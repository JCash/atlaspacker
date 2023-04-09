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

typedef struct apTileImage
{
    int twidth;         // width in #tiles
    int theight;        // height in #tiles
    int rotation;       // 0, 90, 180, 270
    apRect rect;        // The bounding box of the cells that are valid in this image

    uint32_t bytecount;
    uint8_t* bytes;     // Each byte represents a tile. 0 means it's not occupied
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

    // Hull
    apPosf* vertices; // owned by caller
    int num_vertices;
} apTilePackerImage;

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

// Checks a sub rect of the image to see if it was occupied or not
// Used to create a "tile image"
static int apTilePackCheckSubImage(int tile_size, int x, int y,
                                int width, int height, int channels, int alphathreshold, const uint8_t* data)
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

// Returns 1 if all bits are set (i.e. a solid rectangle)
static int apTilePackerImageIsSolidRect(const apTileImage* image)
{
    int size = image->bytecount;
    for (int i = 0; i < size; ++i)
    {
        if (image->bytes[i] == 0)
            return 0;
    }
    return 1;
}

static apTileImage* apTilePackerCreateTileImage(int width, int height, int tile_size)
{
    apTileImage* image = (apTileImage*)malloc(sizeof(apTileImage));
    memset(image, 0, sizeof(apTileImage));

    int twidth = apMathRoundUp(width, tile_size) / tile_size;
    int theight = apMathRoundUp(height, tile_size) / tile_size;
    image->twidth = twidth;
    image->theight = theight;
    image->bytecount = twidth*theight;
    image->bytes = (uint8_t*)malloc(twidth*theight);
    memset(image->bytes, 0, twidth*theight);

    image->rect.pos.x = 0;
    image->rect.pos.y = 0;
    image->rect.size.width = image->twidth;
    image->rect.size.height = image->theight;

    return image;
}

static void apTilePackerGrowTileImage(apTileImage* image, int width, int height, int tile_size)
{
    int oldtwidth = image->twidth;
    int oldtheight = image->theight;
    int twidth = apMathRoundUp(width, tile_size) / tile_size;
    int theight = apMathRoundUp(height, tile_size) / tile_size;
    image->twidth = twidth;
    image->theight = theight;

    uint8_t* oldbytes = image->bytes;

    image->bytecount = twidth*theight;
    image->bytes = (uint8_t*)malloc(twidth*theight);
    memset(image->bytes, 0, twidth*theight);

    for (int y = 0; y < oldtheight; ++y)
    {
        for (int x = 0; x < oldtwidth; ++x)
        {
            image->bytes[y*twidth + x] = oldbytes[y*oldtwidth + x];
        }
    }
}

static void apTilePackerMakeTileImageFromImageData(apTileImage* image, int tile_size,
                                int width, int height, int channels, const uint8_t* data)
{
    int twidth = image->twidth;

    // Calculate the bounding rect
    int rminx = image->twidth + 1;
    int rminy = image->theight + 1;
    int rmaxx = -1;
    int rmaxy = -1;

    // x/y are in original image space
    for (int y = 0, ty = 0; y < height; y += tile_size, ++ty)
    {
        for (int x = 0, tx = 0; x < width; x += tile_size, ++tx)
        {
            // Check area in image
            int alphathreshold = 8; // TODO: Create a setting for this!
            int nonempty = apTilePackCheckSubImage(tile_size, x, y, width, height, channels, alphathreshold, data);
            image->bytes[ty*twidth+tx] = nonempty;
            if (nonempty)
            {
                rminx = apMathMin(rminx, tx);
                rminy = apMathMin(rminy, ty);
                rmaxx = apMathMax(rmaxx, tx);
                rmaxy = apMathMax(rmaxy, ty);
            }
        }
    }

    image->rect.pos.x = rminx;
    image->rect.pos.y = rminy;
    image->rect.size.width = rmaxx - rminx + 1;
    image->rect.size.height = rmaxy - rminy + 1;

    // printf("IMAGE RECT:\n");
    // printf("  x/y: %d %d  w/h: %d %d\n", image->rect.pos.x, image->rect.pos.y, image->rect.size.width, image->rect.size.height);
}


static apTileImage* apTilePackerCreateRotatedCopy(const apTileImage* image, int rotation)
{
    apTileImage* outimage = (apTileImage*)malloc(sizeof(apTileImage));
    memset(outimage, 0, sizeof(apTileImage));

    int rotated = rotation == 90 || rotation == 270;
    outimage->twidth = rotated ? image->theight : image->twidth;
    outimage->theight = rotated ? image->twidth : image->theight;
    outimage->rotation = rotation;
    outimage->bytecount = image->bytecount;
    outimage->bytes = (uint8_t*)malloc(outimage->bytecount);

    int twidth = image->twidth;
    int theight = image->theight;
    int outtwidth = outimage->twidth;
    for (int y = 0; y < theight; ++y)
    {
        uint8_t* srcrow = &image->bytes[y * twidth];
        for (int x = 0; x < twidth; ++x)
        {
            apPos outpos = apRotate(x, y, twidth, theight, rotation);

            uint8_t* dstrow = &outimage->bytes[outpos.y * outtwidth];
            dstrow[outpos.x] = srcrow[x];
        }
    }

    apPos p1 = image->rect.pos;
    apPos p2 = { image->rect.pos.x + image->rect.size.width - 1, image->rect.pos.y + image->rect.size.height - 1 };
    apPos rp1 = apRotate(p1.x, p1.y, twidth, theight, rotation);
    apPos rp2 = apRotate(p2.x, p2.y, twidth, theight, rotation);
    apPos new_p1 = { apMathMin(rp1.x, rp2.x), apMathMin(rp1.y, rp2.y) };
    apPos new_p2 = { apMathMax(rp1.x, rp2.x), apMathMax(rp1.y, rp2.y) };

    outimage->rect.pos = new_p1;
    outimage->rect.size.width = new_p2.x - new_p1.x + 1;
    outimage->rect.size.height = new_p2.y - new_p1.y + 1;

    return outimage;
}


static void DebugPrintTileImage(apTileImage* image)
{
    int twidth = image->twidth;
    printf("IMAGE:\n");
    for (int y = 0; y < image->theight; ++y)
    {
        printf("    ");
        for (int x = 0; x < twidth; ++x)
        {
            printf("%d", image->bytes[y*twidth]?1:0);
            if ((x%8) == 7)
                printf(" ");
        }
        printf("\n");
    }
    printf("\n");
}

static void apTilePackerCreateRotatedTileImages(apTilePacker* packer, apTilePackerImage* image)
{
    apTileImage* tile_image = image->images[0];
    assert(tile_image);
    if (!apTilePackerImageIsSolidRect(tile_image))
    {
        image->images[image->num_images++] = apTilePackerCreateRotatedCopy(tile_image, 90);
        image->images[image->num_images++] = apTilePackerCreateRotatedCopy(tile_image, 180);
        image->images[image->num_images++] = apTilePackerCreateRotatedCopy(tile_image, 270);
    } else {
        if (tile_image->twidth != tile_image->theight)
        {
            image->images[image->num_images++] = apTilePackerCreateRotatedCopy(tile_image, 90);
        }
    }
}

static void apTilePackerCreateTileImageFromImageData(apTilePacker* packer, apTilePackerImage* image)
{
    // Create the initial, unrotated tile image
    apTileImage* tile_image = (apTileImage*)malloc(sizeof(apTileImage));
    memset(tile_image, 0, sizeof(apTileImage));

    int tile_size = packer->options.tile_size;
    int twidth = apMathRoundUp(image->super.width, tile_size) / tile_size;
    int theight = apMathRoundUp(image->super.height, tile_size) / tile_size;
    tile_image->twidth = twidth;
    tile_image->theight = theight;
    tile_image->bytecount = twidth*theight;
    tile_image->bytes = (uint8_t*)malloc(twidth*theight);
    apTilePackerMakeTileImageFromImageData(tile_image, tile_size, image->super.width, image->super.height, image->super.channels, image->super.data);

    image->images[image->num_images++] = tile_image;
}

static apImage* apTilePackerCreateImage(apPacker* _packer, const char* path, int width, int height, int channels, const uint8_t* data)
{
    (void)_packer;

    apTilePackerImage* image = (apTilePackerImage*)malloc(sizeof(apTilePackerImage));
    memset(image, 0, sizeof(apTilePackerImage));
    image->super.page = -1;

    image->images = (apTileImage**)malloc(sizeof(apTileImage*)*8);
    memset(image->images, 0, sizeof(apTileImage*)*8);
    image->num_images = 0;

    // for (int i = 0; i < image->num_images; ++i)
    // {
    //     apTileImage* tile_image = image->images[i];
    //     DebugPrintTileImage(tile_image);
    // }

    return (apImage*)image;
}

static void apTilePackerDestroyImage(apPacker* packer, apImage* image)
{
    free((void*)image);
}

void apTilePackerCreateTileImageFromTriangles(apPacker* _packer, apImage* _image, apPosf* triangles, int num_vertices)
{
    apTilePacker* packer = (apTilePacker*)_packer;
    apTilePackerImage* image = (apTilePackerImage*)_image;
    apTileImage* tile_image = (apTileImage*)malloc(sizeof(apTileImage));
    memset(tile_image, 0, sizeof(apTileImage));


    int debug = 0;
    // if (strstr(image->super.path, "hoverboard-board") != 0)
    // {
    //     debug = 1;

    //     apTilePackerCreateTileImageFromImageData(packer, image);

    //     printf("apTilePackerCreateTileImage (reference)\n");
    //     DebugPrintTileImage(image->images[0]);
    //     printf("\n");
    //     image->num_images = 0;
    // }

    int tile_size = packer->options.tile_size;
    int twidth = apMathRoundUp(image->super.width, tile_size) / tile_size;
    int theight = apMathRoundUp(image->super.height, tile_size) / tile_size;
    tile_image->twidth = twidth;
    tile_image->theight = theight;
    tile_image->bytecount = twidth*theight;
    tile_image->bytes = (uint8_t*)malloc(twidth*theight);

    apPosf half = {0.5f, 0.5f};
    apPosf tsize = {twidth, theight};

    if (debug)
    {
        // apPosf imagesize = {image->super.width, image->super.height};
        // printf("image path: %s\n", image->super.path);
        // printf("image size: %d x %d  tile image size: %d x %d\n", image->super.width, image->super.height, twidth, theight);
        // for (int i = 0; i < num_vertices; ++i)
        // {
        //     apPosf v = apMathMul(apMathAdd(triangles[i], half), tsize);

        //     apPosf vi = apMathMul(apMathAdd(triangles[i], half), imagesize);
        //     printf("v %d: %f, %f  -> %f, %f  image: %f %f\n", i, triangles[i].x, triangles[i].y, v.x, v.y, vi.x, vi.y);
        // }
    }

    // Currently, I see two ways to create the image:
    // 1) Texel overlap tests
    // 2) Triangle rasterization

    apPos rect_min = {  100000.0f,  100000.0f };
    apPos rect_max = { -100000.0f, -100000.0f };

    for (int y = 0; y < theight; ++y)
    {
        // if (debug)
        //     printf("y: %2d\t\t", y);

        for (int x = 0; x < twidth; ++x)
        {
            apPosf corners[4] = {
                { x + 0, y + 0 },
                { x + 1, y + 0 },
                { x + 1, y + 1 },
                { x + 0, y + 1 }
            };

            tile_image->bytes[y*twidth+x] = 0;

            //int debug2 = debug && (x == 9 && (y==0 || y == (theight-1)));

            // if (debug2)
            // {
            //     printf("  c0: %f, %f\n", corners[0].x, corners[0].y);
            //     printf("  c1: %f, %f\n", corners[1].x, corners[1].y);
            //     printf("  c2: %f, %f\n", corners[2].x, corners[2].y);
            //     printf("  c3: %f, %f\n", corners[3].x, corners[3].y);
            //     printf("\n");
            // }

            // We neeed to check all triangles
            for (int t = 0, ti = 0; t < num_vertices/3; ++t, ti += 3)
            {
                apPosf triangle[3] = { triangles[ti+0], triangles[ti+1], triangles[ti+2] };
                // Convert from [(-0.5, -0.5), (0.5, 0.5)] to [(0,0), (twidth, theight)]
                triangle[0] = apMathMul(apMathAdd(triangle[0], half), tsize);
                triangle[1] = apMathMul(apMathAdd(triangle[1], half), tsize);
                triangle[2] = apMathMul(apMathAdd(triangle[2], half), tsize);

                // if (debug2)
                // {
                //     printf("  t0: %f, %f   -> %f, %f\n", triangle[0].x, triangle[0].y, (triangles[ti+0].x+0.5f)*image->super.width, (triangles[ti+0].y+0.5f)*image->super.height);
                //     printf("  t1: %f, %f   -> %f, %f\n", triangle[1].x, triangle[1].y, (triangles[ti+1].x+0.5f)*image->super.width, (triangles[ti+1].y+0.5f)*image->super.height);
                //     printf("  t2: %f, %f   -> %f, %f\n", triangle[2].x, triangle[2].y, (triangles[ti+2].x+0.5f)*image->super.width, (triangles[ti+2].y+0.5f)*image->super.height);
                //     printf("\n");
                // }

                int overlap = apOverlapTest2D(triangle, 3, corners, 4);
                if (overlap)
                {
                    tile_image->bytes[y*twidth+x] = 1;

                    rect_min.x = apMathMin(rect_min.x, x);
                    rect_min.y = apMathMin(rect_min.y, y);
                    rect_max.x = apMathMax(rect_max.x, x);
                    rect_max.y = apMathMax(rect_max.y, y);
                    break;
                }
            }
        }

        // if (debug)
        //     printf("\n");

    }

    tile_image->rect.pos = rect_min;
    tile_image->rect.size.width = rect_max.x - rect_min.x + 1;
    tile_image->rect.size.height = rect_max.y - rect_min.y + 1;

    // printf("%s\n", image->super.path);
    // printf("rect: %d, %d w/h: %d %d\n", tile_image->rect.pos.x, tile_image->rect.pos.y, tile_image->rect.size.width, tile_image->rect.size.height);

    // if (debug)
    // {
    //     printf("apTilePackerCreateTileImage\n");
    //     DebugPrintTileImage(tile_image);
    //     printf("\n");
    // }

    image->images[0] = tile_image;
    image->num_images = 1;
}

// In order to keep the test/write logic as contained as possible, we use the same function
// for both testing and writing
static int apTilePackerFitImageAtPos(apTileImage* page_image, int px, int py, int pwidth, int pheight, const apTileImage* image, int do_write)
{
    int twidth = image->rect.size.width;
    int theight = image->rect.size.height;

    // The tight rect of the image says it won't fit here
    if (pwidth < twidth || pheight < theight)
        return 0;

    int start_x = image->rect.pos.x;
    int end_x = start_x + twidth;
    int start_y = image->rect.pos.y;
    int end_y = start_y + theight;

    int page_twidth = page_image->twidth;

    for (int sy = start_y, dy = py; sy < end_y; ++sy, ++dy)
    {
        uint8_t* dstrow = &page_image->bytes[dy*page_twidth+0];
        uint8_t* srcrow = &image->bytes[sy*twidth+0];

        for (int sx = start_x, dx = px; sx < end_x; ++sx, ++dx)
        {
            if (!do_write)
            {
                uint8_t dst_bit = dstrow[dx];
                uint8_t src_bit = srcrow[sx];
                // we only fail if we wish to put a byte there, and it's already occupied
                if (dst_bit & src_bit)
                {
                    return 0;
                }
            }
            else
            {
                dstrow[dx] |= srcrow[sx];
            }
        }
    }
    return 1;
}


static int apTilePackerFitImageInRect(apTileImage* page_image, apTileImage* image, apRect* area, apPos* pos)
{
    int px = area->pos.x;
    int py = area->pos.y;
    int pwidth = area->pos.x + area->size.width;
    int pheight = area->pos.y + area->size.height;

    for (int dy = py; dy < pheight; ++dy)
    {
        int pheight_left = pheight - dy;
        for (int dx = px; dx < pwidth; ++dx)
        {
            int pwidth_left = pwidth - dx;
            int fit = apTilePackerFitImageAtPos(page_image, dx, dy, pwidth_left, pheight_left, image, 0);

            if (fit) {
                // It fit, so now we write it to the page
                apTilePackerFitImageAtPos(page_image, dx, dy, pwidth_left, pheight_left, image, 1);

                pos->x = dx - image->rect.pos.x;
                pos->y = dy - image->rect.pos.y;
                return 1;
            }
        }
    }
    return 0;
}

static int apTilePackerFitImage(apTileImage* page_image, apTileImage* image, apRect* prio_area, apPos* pos)
{
    apRect rect;
    rect.pos.x = 0;
    rect.pos.y = 0;
    rect.size.width = page_image->twidth;
    rect.size.height = page_image->theight;

    // If we have a prio area, it's the area of the page image before the last resize.
    // By searching this area first, we may fit smaller images in this area first and thus save more coherent space in the
    // lastly allocated page image
    if (prio_area)
    {
        int r = apTilePackerFitImageInRect(page_image, image, prio_area, pos);
        if (r)
            return 1;

        // Make a rect of the rest of the area
        // The prio area is assumed to extend fully in one of the directions
        if (prio_area->size.height == rect.size.height)
        {
            rect.pos.x = prio_area->size.width;
            rect.size.width = page_image->twidth - prio_area->size.width;
        }
        else
        {
            rect.pos.y = prio_area->size.height;
            rect.size.height = page_image->theight - prio_area->size.height;
        }
    }

    return apTilePackerFitImageInRect(page_image, image, &rect, pos);
}

static int apTilePackerPackImage(apTileImage* page_image, apTilePackerImage* image, apRect* prio_area, int allow_rotate)
{
    //printf("Packing image %s\n", image->super.path);

    // Try the rotated variations variation of the image
    for (int i = 0; i < image->num_images; ++i)
    {
        if (apTilePackerFitImage(page_image, image->images[i], prio_area, &image->pos))
        {
            return i;
        }
        if (!allow_rotate)
            break;
    }
    return -1;
}

static void apTilePackerPackImages(apPacker* _packer, apContext* ctx)
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

printf("Creating page: %d x %d\n", page->page->dimensions.width, page->page->dimensions.height);

    page->image = 0;

// printf("packing...\n");
// printf("  page size: %d x %d\n", page->page->dimensions.width, page->page->dimensions.height);
    for (int i = 0; i < ctx->num_images; ++i)
    {
        apTilePackerImage* image = (apTilePackerImage*)ctx->images[i];
        if (!image->num_images)
        {
            apTilePackerCreateTileImageFromImageData(packer, image);
        }
        apTilePackerCreateRotatedTileImages(packer, image);
    }

    // In order to fill in all tiny gaps, we keep the previous area in the page image
    apRect prio_area = { {0, 0}, {0, 0} };
    apRect* p_prio_area = 0;

    int tile_size = packer->options.tile_size;
    int allow_rotate = !packer->options.no_rotate;
    for (int i = 0; i < ctx->num_images; ++i)
    {
        apTilePackerImage* image = (apTilePackerImage*)ctx->images[i];

        if (!page->image)
        {
            int width = apNextPowerOfTwo(image->super.dimensions.width);
            int height = apNextPowerOfTwo(image->super.dimensions.width);
            page->image = apTilePackerCreateTileImage(width, height, tile_size);

            prio_area.pos.x = 0;
            prio_area.pos.y = 0;
            prio_area.size.width = width/tile_size;
            prio_area.size.height = height/tile_size;
            printf("Creating page image: %d x %d\n", width, height);
        }

        int image_index = apTilePackerPackImage(page->image, image, p_prio_area, allow_rotate);
        if (image_index == -1)
        {
            // Use the previous area as the first search area
            prio_area.size.width = page->image->twidth;
            prio_area.size.height = page->image->theight;
            p_prio_area = &prio_area;

            // grow the page, or find a next page that will fit this image
            int width = page->image->twidth * tile_size;
            int height = page->image->theight * tile_size;
            if (width <= height)
                width *= 2;
            else
                height *= 2;
            apTilePackerGrowTileImage(page->image, width, height, tile_size);
            printf("Growing page image to %d x %d\n", width, height);

            // TODO: Handle max size according to user settings
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

        //DebugPrintTileImage(page->image);
    }
}

uint8_t* apTilePackerDebugCreateImageFromTileImage(apImage* _image, int tile_image_index, int tile_size)
{
    apTilePackerImage* image = (apTilePackerImage*)_image;
    int outwidth = image->super.width;
    int outheight = image->super.height;

    if (tile_image_index >= image->num_images)
        return 0;

    apTileImage* tile_image = image->images[tile_image_index];
    int twidth = tile_image->twidth;
    int theight = tile_image->theight;

    // printf("tile_size: %d (fix a proper getter here!)\n", tile_size);
    // DebugPrintTileImage(tile_image);

    uint8_t* mem = (uint8_t*)malloc(outwidth*outheight*1);
    memset(mem, 0, outwidth*outheight*1);

    for (int y = 0; y < theight; ++y)
    {
        for (int x = 0; x < twidth; ++x)
        {
            uint8_t value = tile_image->bytes[y * twidth + x] ? 255 : 0;

            // Fill in the rectangle in the output image
            for (int yy = 0; yy < tile_size; ++yy)
            {
                for (int xx = 0; xx < tile_size; ++xx)
                {
                    int outy = y * tile_size + yy;
                    int outx = x * tile_size + xx;
                    // The tile image size is "rounded up", which means it doesn't match precisely with the output image
                    if (outx < outwidth && outy < outheight)
                    {
                        int index = outy * outwidth + outx;
                        mem[index] = value;
                    }
                }
            }
        }
    }

    return mem;
}

apPacker* apTilePackerCreate(apTilePackOptions* options)
{
    apTilePacker* packer = (apTilePacker*)malloc(sizeof(apTilePacker));
    memset(packer, 0, sizeof(apTilePacker));
    packer->super.packer_type = "apTilePacker";
    packer->super.createImage = apTilePackerCreateImage;
    packer->super.destroyImage = apTilePackerDestroyImage;
    packer->super.packImages = apTilePackerPackImages;
    packer->options = *options;
    if (packer->options.tile_size == 0)
        packer->options.tile_size = 16;
    return (apPacker*)packer;
}

void apTilePackerDestroy(apPacker* packer)
{
    free((void*)packer);
}

int apTilePackerGetTileSize(apPacker* _packer)
{
    apTilePacker* packer = (apTilePacker*)_packer;
    return packer->options.tile_size;
}
