#include <atlaspacker/tilepacker.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>   // sqrtf
#include <stdio.h>   // printf

#pragma pack(1)
typedef struct
{
    apPage* page;
} apTilePackerPage;

typedef struct
{
    apPacker          super;
    apTilePackOptions options;
    apTilePackerPage  page; // currently just a single page, that grows dynamically
} apTilePacker;

// Each bit represents a tile. 1 means it's filled
typedef struct
{
    uint8_t* bits;
    uint32_t bitcount;
    uint32_t capacity:31;
    uint32_t all_set:1;     // 1 if all bits are set
} apBitArray;


typedef struct
{
    int twidth;         // width in #tiles
    int theight;        // height in #tiles
    apBitArray* rows;
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
} apTilePackImage;

#pragma options align=reset


static int apTilePackCheckSubImage(int tile_size, int x, int y,
                                int width, int height, int channels, uint8_t* data)
{
    for (int ty = y; ty < y + tile_size && ty < height; ++ty)
    {
        for (int tx = x; tx < x + tile_size && tx < width; ++tx)
        {
            int index = ty * width * channels + tx * channels;
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

// Go through all bytes and set the flag "all_set" if all bits are set
static int apTilePackBitArrayAllSet(const apBitArray* row)
{
    uint32_t size = row->capacity;
    uint32_t bitcount = row->bitcount;
    for (uint32_t i = 0; i < size; ++i)
    {
        uint8_t b = row->bits[i];
        uint8_t mask = bitcount >= 8 ? 0xFF : ((1<<bitcount)-1);

        if ((b & mask) != mask)
        {
            return 0;
        }
        bitcount -= 8;
    }
    return 1;
}

static int apTilePackBitArrayGet(apBitArray* row, int index)
{
    uint32_t byte = index / 8;
    uint32_t bit = index - byte*8;
    int value = row->bits[byte] & (1 << bit);
    return value != 0 ? 1 : 0;
}

static void apTilePackBitArraySet(apBitArray* row, int index, int value)
{
    uint32_t byte = index / 8;
    uint32_t bit = index - byte*8;
    if (value)
        row->bits[byte] |= 1 << bit;
    else
        row->bits[byte] &= ~(1 << bit);
}

// Returns 1 if all bits are set (i.e. a solid rectangle)
static int apTilePackImageIsSolidRect(const apTileImage* image)
{
    int num_rows_all_set = 0;
    uint32_t theight = image->theight;
    for (uint32_t r = 0; r < theight; ++r)
    {
        apBitArray* row = &image->rows[r];
        num_rows_all_set += row->all_set;
    }
    return num_rows_all_set == theight ? 1 : 0;
}

static void apTilePackMakeTilesImage(apTileImage* image, int tile_size,
                                int width, int height, int channels, uint8_t* data)
{
    int num_bytes = apRoundUp(image->twidth, 8) / 8;

    for (int y = 0, r = 0; y < height; y += tile_size, ++r)
    {
        apBitArray* row = &image->rows[r];
        row->capacity = num_bytes;
        row->bitcount = image->twidth;
        row->bits = (uint8_t*)malloc(num_bytes);
        memset(row->bits, 0, num_bytes);

        for (int x = 0, t = 0; x < width; x += tile_size, ++t)
        {
            // Check area in image
            int nonempty = apTilePackCheckSubImage(tile_size, x, y, width, height, channels, data);
            if (nonempty)
            {
                int byte = t / 8;
                int bit = 1 << t;
                row->bits[byte] |= bit;
            }
        }
    }

    uint32_t theight = image->theight;
    for (uint32_t r = 0; r < theight; ++r)
    {
        apBitArray* row = &image->rows[r];
        row->all_set = apTilePackBitArrayAllSet(row);
    }
}


static apTileImage* apTilePackCreateRotatedCopy(const apTileImage* image, int rotation)
{
    apTileImage* outimage = (apTileImage*)malloc(sizeof(apTileImage));
    memset(outimage, 0, sizeof(apTileImage));

    int rotated = rotation == 90 || rotation == 270;
    outimage->twidth = rotated ? image->theight : image->twidth;
    outimage->theight = rotated ? image->twidth : image->theight;
    outimage->rows = (apBitArray*)malloc(sizeof(apBitArray)*outimage->theight);
    memset(outimage->rows, 0, sizeof(apBitArray)*outimage->theight);
    
    int num_bytes = apRoundUp(outimage->twidth, 8) / 8;

    uint32_t twidth = image->twidth;
    uint32_t theight = image->theight;
    for (int y = 0; y < theight; ++y)
    {
        apBitArray* srcrow = &image->rows[y];
        for (int x = 0; x < twidth; ++x)
        {
            apPos outpos = apRotate(x, y, twidth, theight, rotation);
            
            apBitArray* dstrow = &outimage->rows[outpos.y];
            if (!dstrow->bits)
            {
                dstrow->capacity = num_bytes;
                dstrow->bitcount = outimage->twidth;
                dstrow->bits = (uint8_t*)malloc(num_bytes);
                memset(dstrow->bits, 0, num_bytes);
            }
            int value = apTilePackBitArrayGet(srcrow, x);
            apTilePackBitArraySet(dstrow, outpos.x, value);
        }
    }

    return outimage;
}


static void DebugPrintTiles(apBitArray* rows, int num_rows)
{
    printf("IMAGE:\n");
    for (int i = 0; i < num_rows; ++i)
    {
        printf("    ");
        apBitArray* row = &rows[i];
        for (int b = 0; b < row->capacity; ++b)
        {
            uint8_t byte = row->bits[b];

            // TODO: make sure to not print more bits than we have
            for (int bit = 0; bit < 8; ++bit)
            {
                printf("%d", (byte>>bit)&0x1);
            }
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
    image->super.page = 0;
    image->super.page = 0;

    image->images = (apTileImage**)malloc(sizeof(apTileImage*)*8);

    // Create the initial, unrotated tile image
    apTileImage* tile_image = (apTileImage*)malloc(sizeof(apTileImage));
    memset(tile_image, 0, sizeof(apTileImage));

    int tile_size = packer->options.tile_size;
    tile_image->twidth = apRoundUp(width, tile_size) / tile_size;
    tile_image->theight = apRoundUp(height, tile_size) / tile_size;
    tile_image->rows = (apBitArray*)malloc(sizeof(apBitArray)*tile_image->theight);
    apTilePackMakeTilesImage(tile_image, tile_size, width, height, channels, data);

    image->images[image->num_images++] = tile_image;

    if (!apTilePackImageIsSolidRect(tile_image))
    {
        printf("Generating images...\n");
        image->images[image->num_images++] = apTilePackCreateRotatedCopy(tile_image, 90);
        image->images[image->num_images++] = apTilePackCreateRotatedCopy(tile_image, 180);
        image->images[image->num_images++] = apTilePackCreateRotatedCopy(tile_image, 270);
    }

    for (int i = 0; i < image->num_images; ++i)
    {
        apTileImage* tile_image = image->images[i];
        DebugPrintTiles(tile_image->rows, tile_image->theight);
    }

    return (apImage*)image;
}

static void apTilePackDestroyImage(apPacker* packer, apImage* image)
{
    free((void*)image);
}

static void apTilePackPackImages(apPacker* _packer, apContext* ctx)
{
    apTilePacker* packer = (apTilePacker*)_packer;
    int totalArea = 0;
    for (int i = 0; i < ctx->num_images; ++i)
    {
        apImage* image = ctx->images[i];
        int area = image->dimensions.width * image->dimensions.height;
        totalArea += area;
    }

    int bin_size = (int)sqrtf(totalArea);
    if (bin_size == 0)
    {
        bin_size = 128;
    }
    // Make sure the size is a power of two
    bin_size = apNextPowerOfTwo((uint32_t)bin_size);

    apTilePackerPage* page = &packer->page;
    memset(page, 0, sizeof(apTilePackerPage));
    page->page = apAllocPage(ctx);
    page->page->dimensions.width = bin_size;
    page->page->dimensions.height = bin_size;

    // apBinPackSkylineNode node;
    // node.x = 0;
    // node.y = 0;
    // node.width = bin_size;
    // apBinPackInsertSkylineNode(page, 0, &node);

// printf("packing...\n");
// printf("  page size: %d x %d\n", page->page->dimensions.width, page->page->dimensions.height);

//     int allow_rotate = !packer->options.no_rotate;
//     for (int i = 0; i < ctx->num_images; ++i)
//     {
//         apImage* image = ctx->images[i];

// // printf("  image: %d x %d %s\n", image->dimensions.width, image->dimensions.height, image->path);

//         if (apBinPackPackRect(page, image->dimensions.width, image->dimensions.height, allow_rotate, &image->placement))
//         {
//             image->rotation = image->placement.size.width == image->dimensions.width ? 0 : 90;
//             image->page = page->page->index;

// // printf("  rotation: %d   pos: %d, %d  %d, %d\n", image->rotation,
// //         image->placement.pos.x, image->placement.pos.y, image->placement.size.width, image->placement.size.height);

//         }
//         else
//         {
//             // We need to grow the page size (or switch page)
//         }
//     }

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