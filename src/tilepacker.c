#include <atlaspacker/tilepacker.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>   // sqrtf
#include <stdio.h>   // printf

// To create a concave hull of a set of points
// http://repositorium.sdum.uminho.pt/bitstream/1822/6429/1/ConcaveHull_ACM_MYS.pdf

#if 0
#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
#define DEBUGLOG
#endif

#pragma pack(1)

// Each bit represents a tile. 1 means it's filled
typedef struct
{
    uint8_t* bytes;
    uint32_t bytecount;
    uint32_t capacity:31;
    uint32_t all_set:1;     // 1 if all bits are set
} apTilePackImageRow;

typedef struct
{
    int twidth;         // width in #tiles
    int theight;        // height in #tiles
    int rotation;       // 0, 90, 180, 270
    apRect rect;        // Contains the coordinates which are set
    apTilePackImageRow* rows;
} apTileImage;

typedef struct
{
    apImage super;
    // Instead of writing the for loops in such a way to account for the rotation
    // we keep rotated images here instead
    apTileImage** images;
    // Although we usually keep 4 images, we might keep either 1 for a solid rectangle
    // or 8 for flipped geometry
    int num_images;
    // position after placement
    apPos pos;
} apTilePackImage;

typedef struct
{
    apPage*         page;
    apTileImage*    image;
} apTilePackerPage;

typedef struct
{
    apPacker          super;
    apTilePackOptions options;
    apTilePackerPage  page; // currently just a single page, that grows dynamically
} apTilePacker;

#pragma options align=reset


static int apTilePackCheckSubImage(int tile_size, int x, int y,
                                int width, int height, int channels, int alphathreshold, uint8_t* data)
{
    for (int ty = y; ty < y + tile_size && ty < height; ++ty)
    {
        for (int tx = x; tx < x + tile_size && tx < width; ++tx)
        {
            int index = ty * width * channels + tx * channels;
            if (channels == 4 && data[index+3] <= alphathreshold)
                continue; // Skip texels that are <= the alpha threshold

            for (int c = 0; c < channels; ++c)
            {
                if (data[index+c] != 0)
                    return 1;
            }
        }
    }
    return 0;
}

static inline int apRoundUp(int x, int multiple)
{
    return ((x + multiple - 1) / multiple) * multiple;
}

static inline int apTilePackMax(int a, int b)
{
    return a > b ? a : b;
}

static inline int apTilePackMin(int a, int b)
{
    return a < b ? a : b;
}

// Returns 1 if all bits are set (i.e. a solid rectangle)
static int apTilePackImageIsSolidRect(const apTileImage* image)
{
    int num_rows_all_set = 0;
    uint32_t theight = image->theight;
    for (uint32_t r = 0; r < theight; ++r)
    {
        apTilePackImageRow* row = &image->rows[r];
        num_rows_all_set += row->all_set;
    }
    return num_rows_all_set == theight ? 1 : 0;
}

static apTileImage* apTilePackCreateTileImage(int width, int height, int tile_size)
{
    apTileImage* image = (apTileImage*)malloc(sizeof(apTileImage));
    memset(image, 0, sizeof(apTileImage));

    int theight = apRoundUp(height, tile_size) / tile_size;
    image->twidth = apRoundUp(width, tile_size) / tile_size;
    image->theight = theight;
    image->rows = (apTilePackImageRow*)malloc(sizeof(apTilePackImageRow)*theight);

    image->rect.pos.x = 0;
    image->rect.pos.y = 0;
    image->rect.size.width = image->twidth;
    image->rect.size.height = image->theight;

    int num_bytes = image->twidth;
    for (int r = 0; r < theight; ++r)
    {
        apTilePackImageRow* row = &image->rows[r];
        row->capacity = num_bytes;
        row->bytecount = num_bytes;
        row->bytes = (uint8_t*)malloc(num_bytes);
        row->all_set = 0;
        memset(row->bytes, 0, num_bytes);
    }
    return image;
}

static void apTilePackGrowTileImage(apTileImage* image, int width, int height, int tile_size)
{
    int oldtheight = image->theight;
    int theight = apRoundUp(height, tile_size) / tile_size;
    image->twidth = apRoundUp(width, tile_size) / tile_size;
    image->theight = theight;
    image->rows = (apTilePackImageRow*)realloc(image->rows, sizeof(apTilePackImageRow)*theight);
    // Memset the new rows (if any)
    memset(image->rows + oldtheight, 0, sizeof(apTilePackImageRow)*(theight-oldtheight));

    // Grow the rows to the new size.
    // New rows have a previous capacity of 0 bytes, and a null bits pointer
    int num_bytes = image->twidth;
    for (int r = 0; r < theight; ++r)
    {
        apTilePackImageRow* row = &image->rows[r];
        uint32_t oldcapacity = row->capacity;
        row->capacity = num_bytes;
        row->bytecount = num_bytes;
        row->bytes = (uint8_t*)realloc(row->bytes, num_bytes);
        row->all_set = 0;
        memset(row->bytes+oldcapacity, 0, num_bytes - oldcapacity);
    }
}

static void apTilePackMakeTilesImage(apTileImage* image, int tile_size,
                                int width, int height, int channels, uint8_t* data)
{
    int twidth = image->twidth;

    // Calculate the bounding rect
    int rminx = image->twidth + 1;
    int rminy = image->theight + 1;
    int rmaxx = -1;
    int rmaxy = -1;

    // x/y are in original image space
    for (int y = 0, r = 0; y < height; y += tile_size, ++r)
    {
        apTilePackImageRow* row = &image->rows[r];
        row->capacity = twidth;
        row->bytecount = twidth;
        row->bytes = (uint8_t*)malloc(twidth);

        int count_set = 0;
        for (int x = 0, t = 0; x < width; x += tile_size, ++t)
        {
            // Check area in image
            int alphathreshold = 8;
            int nonempty = apTilePackCheckSubImage(tile_size, x, y, width, height, channels, alphathreshold, data);
            row->bytes[t] = nonempty;
            if (nonempty)
            {
                rminx = apTilePackMin(rminx, t);
                rminy = apTilePackMin(rminy, r);
                rmaxx = apTilePackMax(rmaxx, t);
                rmaxy = apTilePackMax(rmaxy, r);
                ++count_set;
            }
        }

        row->all_set = count_set == twidth;
    }

    image->rect.pos.x = rminx;
    image->rect.pos.y = rminy;
    image->rect.size.width = rmaxx - rminx + 1;
    image->rect.size.height = rmaxy - rminy + 1;

    // printf("IMAGE RECT:\n");
    // printf("  x/y: %d %d  w/h: %d %d\n", image->rect.pos.x, image->rect.pos.y, image->rect.size.width, image->rect.size.height);
}


static apTileImage* apTilePackCreateRotatedCopy(const apTileImage* image, int rotation)
{
    apTileImage* outimage = (apTileImage*)malloc(sizeof(apTileImage));
    memset(outimage, 0, sizeof(apTileImage));

    int rotated = rotation == 90 || rotation == 270;
    outimage->twidth = rotated ? image->theight : image->twidth;
    outimage->theight = rotated ? image->twidth : image->theight;
    outimage->rotation = rotation;
    outimage->rows = (apTilePackImageRow*)malloc(sizeof(apTilePackImageRow)*outimage->theight);
    memset(outimage->rows, 0, sizeof(apTilePackImageRow)*outimage->theight);

    uint32_t twidth = image->twidth;
    uint32_t theight = image->theight;
    for (int y = 0; y < theight; ++y)
    {
        apTilePackImageRow* srcrow = &image->rows[y];
        for (int x = 0; x < twidth; ++x)
        {
            apPos outpos = apRotate(x, y, twidth, theight, rotation);
            
            apTilePackImageRow* dstrow = &outimage->rows[outpos.y];
            if (!dstrow->bytes)
            {
                dstrow->capacity = outimage->twidth;
                dstrow->bytecount = outimage->twidth;
                dstrow->bytes = (uint8_t*)malloc(outimage->twidth);
                memset(dstrow->bytes, 0, outimage->twidth);
            }
            dstrow->bytes[outpos.x] = srcrow->bytes[x];
        }
    }

    apPos p1 = image->rect.pos;
    apPos p2 = { image->rect.pos.x + image->rect.size.width - 1, image->rect.pos.y + image->rect.size.height - 1 };
    apPos rp1 = apRotate(p1.x, p1.y, twidth, theight, rotation);
    apPos rp2 = apRotate(p2.x, p2.y, twidth, theight, rotation);
    apPos new_p1 = { apTilePackMin(rp1.x, rp2.x), apTilePackMin(rp1.y, rp2.y) };
    apPos new_p2 = { apTilePackMax(rp1.x, rp2.x), apTilePackMax(rp1.y, rp2.y) };

    outimage->rect.pos = new_p1;
    outimage->rect.size.width = new_p2.x - new_p1.x + 1;
    outimage->rect.size.height = new_p2.y - new_p1.y + 1;

    return outimage;
}


static void DebugPrintTiles(apTileImage* image)
{
    apTilePackImageRow* rows = image->rows;
    int num_rows = image->theight;
    printf("IMAGE:\n");
    for (int i = 0; i < num_rows; ++i)
    {
        printf("    ");
        apTilePackImageRow* row = &rows[i];
        for (int b = 0; b < row->bytecount; ++b)
        {
            printf("%d", row->bytes[b]?1:0);
            if ((b%8)== 7)
                printf(" ");
        }
        printf("\n");
    }
    printf("\n");
}


static apImage* apTilePackCreateImage(apPacker* _packer, const char* path, int width, int height, int channels, uint8_t* data)
{
    apTilePacker* packer = (apTilePacker*)_packer;

    apTilePackImage* image = (apTilePackImage*)malloc(sizeof(apTilePackImage));
    memset(image, 0, sizeof(apTilePackImage));
    image->super.page = -1;

    image->images = (apTileImage**)malloc(sizeof(apTileImage*)*8);

    // Create the initial, unrotated tile image
    apTileImage* tile_image = (apTileImage*)malloc(sizeof(apTileImage));
    memset(tile_image, 0, sizeof(apTileImage));

    int tile_size = packer->options.tile_size;
    tile_image->twidth = apRoundUp(width, tile_size) / tile_size;
    tile_image->theight = apRoundUp(height, tile_size) / tile_size;
    tile_image->rows = (apTilePackImageRow*)malloc(sizeof(apTilePackImageRow)*tile_image->theight);
    apTilePackMakeTilesImage(tile_image, tile_size, width, height, channels, data);

    image->images[image->num_images++] = tile_image;

    if (!apTilePackImageIsSolidRect(tile_image))
    {
        image->images[image->num_images++] = apTilePackCreateRotatedCopy(tile_image, 90);
        image->images[image->num_images++] = apTilePackCreateRotatedCopy(tile_image, 180);
        image->images[image->num_images++] = apTilePackCreateRotatedCopy(tile_image, 270);
    } else {
        if (tile_image->twidth != tile_image->theight)
        {
            image->images[image->num_images++] = apTilePackCreateRotatedCopy(tile_image, 90);
        }
    }

    // for (int i = 0; i < image->num_images; ++i)
    // {
    //     apTileImage* tile_image = image->images[i];
    //     DebugPrintTiles(tile_image);
    // }

    return (apImage*)image;
}

static void apTilePackDestroyImage(apPacker* packer, apImage* image)
{
    free((void*)image);
}

// In order to keep the test/write logic as contained as possible, we use the same function
static int apTilePackFitImageAtPos(apTileImage* page_image, int px, int py, int pwidth, int pheight, const apTileImage* image, int do_write)
{
    int iwidth = image->rect.size.width;
    int iheight = image->rect.size.height;

    // The tight rect of the image says it won't fit here
    if (pwidth < iwidth || pheight < iheight)
        return 0;

    int start_x = image->rect.pos.x;
    int end_x = start_x + iwidth;
    int start_y = image->rect.pos.y;
    int end_y = start_y + iheight;

    for (int sy = start_y, dy = py; sy < end_y; ++sy, ++dy)
    {
        apTilePackImageRow* dstrow = &page_image->rows[dy];
        apTilePackImageRow* srcrow = &image->rows[sy];

        for (int sx = start_x, dx = px; sx < end_x; ++sx, ++dx)
        {
            if (!do_write)
            {
                uint8_t dst_bit = dstrow->bytes[dx];
                uint8_t src_bit = srcrow->bytes[sx];
                // we only fail if we wish to put a byte there, and it's already occupied
                if (dst_bit & src_bit)
                {
                    return 0;
                }
            }
            else
            {
                //printf("dst[%d]: %u", dx, (uint32_t)dstrow->bytes[dx], sx, (uint32_t)srcrow->bytes[sx]);
                dstrow->bytes[dx] |= srcrow->bytes[sx];
            }
        }
    }
    return 1;
}

static int apTilePackFitImage(apTileImage* page_image, apTileImage* image, apPos* pos)
{
    int pwidth = page_image->twidth;
    int pheight = page_image->theight;
    for (int dy = 0; dy < pheight; ++dy)
    {
        apTilePackImageRow* dstrow = &page_image->rows[dy];
        if (dstrow->all_set) // TODO: unless we're actually setting this in the target, it's useless
            continue;

        int pheight_left = pheight - dy;
        for (int dx = 0; dx < pwidth; ++dx)
        {
            int pwidth_left = pwidth - dx;
            int fit = apTilePackFitImageAtPos(page_image, dx, dy, pwidth_left, pheight_left, image, 0);
            if (fit) {
                apTilePackFitImageAtPos(page_image, dx, dy, pwidth_left, pheight_left, image, 1);
                
                pos->x = dx - image->rect.pos.x;
                pos->y = dy - image->rect.pos.y;
                return 1;
            }
        }
    }
    return 0;
}

static int apTilePackPackImage(apTileImage* page_image, apTilePackImage* image, int allow_rotate)
{
    int debug = 0;
    if (strstr(image->super.path, "head.png") != 0)
        debug = 1;
    for (int i = 0; i < image->num_images; ++i)
    {
        if (apTilePackFitImage(page_image, image->images[i], &image->pos))
        {
            return i;
        }
        if (!allow_rotate)
            break;
    }
    return -1;
}

static void apTilePackPackImages(apPacker* _packer, apContext* ctx)
{
    apTilePacker* packer = (apTilePacker*)_packer;
    // int totalArea = 0;
    // for (int i = 0; i < ctx->num_images; ++i)
    // {
    //     apImage* image = ctx->images[i];
    //     int area = image->dimensions.width * image->dimensions.height;
    //     totalArea += area;
    // }

    // int bin_size = (int)sqrtf(totalArea);
    // if (bin_size == 0)
    // {
    //     bin_size = 128;
    // }
    // // Make sure the size is a power of two
    // bin_size = apNextPowerOfTwo((uint32_t)bin_size);

    // Ideas:
    //      * for each image calculate number of occupied cells, and derive area used from that

    int bin_size = 256; // TODO: make it an option

    apTilePackerPage* page = &packer->page;
    memset(page, 0, sizeof(apTilePackerPage));
    page->page = apAllocPage(ctx);
    page->page->index = 0;
    page->page->dimensions.width = bin_size;
    page->page->dimensions.height = bin_size;

printf("Creating page width %d x %d\n", page->page->dimensions.width, page->page->dimensions.height);

    page->image = 0;

// printf("packing...\n");
// printf("  page size: %d x %d\n", page->page->dimensions.width, page->page->dimensions.height);

    int tile_size = packer->options.tile_size;
    int allow_rotate = !packer->options.no_rotate;
    for (int i = 0; i < ctx->num_images; ++i)
    {
        apTilePackImage* image = (apTilePackImage*)ctx->images[i];

        if (!page->image)
        {
            int width = apNextPowerOfTwo(image->super.dimensions.width);
            int height = apNextPowerOfTwo(image->super.dimensions.width);
            page->image = apTilePackCreateTileImage(width, height, tile_size);
            printf("Creating page image %d x %d\n", width, height);
        }

        int image_index = apTilePackPackImage(page->image, image, allow_rotate);
        if (image_index == -1)
        {
            // grow the page, or find a next page that will fit this image
            int width = page->image->twidth * tile_size;
            int height = page->image->theight * tile_size;
            if (width <= height)
                width *= 2;
            else
                height *= 2;
            apTilePackGrowTileImage(page->image, width, height, tile_size);
            printf("Growing page to %d x %d\n", width, height);

            // if (width > 16384 || height > 16384)
            //     create new page

            page->page->dimensions.width = width;
            page->page->dimensions.height = height;

            --i;
            continue;
        }

        apTileImage* fit_image = image->images[image_index];

        image->super.page = page->page->index;
        image->super.placement.pos.x = image->pos.x * tile_size;
        image->super.placement.pos.y = image->pos.y * tile_size;
        image->super.rotation = fit_image->rotation;

        //printf("Image %s:%d (i:%d) fit at pos %d %d\n", image->super.path, image->super.rotation, image_index, image->pos.x, image->pos.y);
            
        //DebugPrintTiles(page->image);
    }
}

apPacker* apCreateTilePacker(apTilePackOptions* options)
{
    apTilePacker* packer = (apTilePacker*)malloc(sizeof(apTilePacker));
    memset(packer, 0, sizeof(apTilePacker));
    packer->super.packer_type = "apTilePacker";
    packer->super.createImage = apTilePackCreateImage;
    packer->super.destroyImage = apTilePackDestroyImage;
    packer->super.packImages = apTilePackPackImages;
    packer->options = *options;
    if (packer->options.tile_size == 0)
        packer->options.tile_size = 16;
    return (apPacker*)packer;
}

void apDestroyTilePacker(apPacker* packer)
{
    free((void*)packer);
}