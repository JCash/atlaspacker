// For reference: http://pds25.egloos.com/pds/201504/21/98/RectangleBinPack.pdf

#include <atlaspacker/binpacker.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // INT_MAX
#include <stdio.h>  // printf
#include <math.h>   // sqrtf

// The term "skyline" refers to the silhouette of a big city skyline
// This structure represents one "rooftop" in the skyline
//
//     +-----+
//     |     +--+
// +---+        
//
typedef struct
{
    int     x;
    int     y;
    int     width;
} apBinPackSkylineNode;

typedef struct
{
    apPage*                 page;
    apBinPackSkylineNode*   skyline;
    int                     skyline_size;
    int                     skyline_capacity;
} apBinPackerPage;

typedef struct
{
    apPacker         super;
    apBinPackOptions options;
    apBinPackerPage  page; // currently just a single page, that grows dynamically
} apBinPacker;

// static void debugSkyline(apBinPackerPage* page)
// {
//     printf("skyline:\n");
//     for(int i = 0; i < page->skyline_size; ++i)
//     {
//         printf("%d: x: %d  y: %d  width: %d\n", i, page->skyline[i].x, page->skyline[i].y, page->skyline[i].width);
//     }
// }


static apImage* apBinPackCreateImage(apPacker* packer, const char* path, int width, int height, int channels, uint8_t* data)
{
    apImage* image = (apImage*)malloc(sizeof(apImage));
    memset(image, 0, sizeof(apImage));
    image->page = 0;
    return image;
}

static void apBinPackDestroyImage(apPacker* packer, apImage* image)
{
    free((void*)image);
}

// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
static uint32_t NextPowerOfTwo(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

#define AP_MAX(_A, _B) ((_A) > (_B) ? (_A) : (_B))

// For the skyline nodes that lie under the "width" of this rect,
// find the max Y value.
// If the remaining width/height of the bin cannot hold the rect at the 
// position of (skyline[index].x, best_height), then it returns 0
static int apBinPackSLBLRectangleFitsAtNode(apBinPackerPage* page, int index, int width, int height,
                                    int bin_width, int bin_height, int* max_height)
{
    apBinPackSkylineNode* skyline = page->skyline;
    int x = skyline[index].x;
    if (x + width > bin_width)
    {
//printf("%s:%d\n", __FUNCTION__, __LINE__);
        return 0;
    }

    int y = skyline[index].y;
    int width_left = width;
    while (width_left > 0)
    {
        y = AP_MAX(y, skyline[index].y);
        if (y + height > bin_height)
        {
//printf("%s:%d\n", __FUNCTION__, __LINE__);
            return 0;
        }

        width_left -= skyline[index].width;
        ++index;
    }
    *max_height = y;
    return 1;
}

#undef AP_MAX

static int apBinPackSkylineBLPackRect(apBinPackerPage* page, int width, int height, int allow_rotate, apRect* out_rect)
{
    apBinPackSkylineNode* skyline = page->skyline;
    int best_height = INT_MAX;
    int best_width = INT_MAX;
    int best_index = -1;
    apRect rect;
    memset(&rect, 0, sizeof(apRect));

    int bin_width = page->page->dimensions.width;
    int bin_height = page->page->dimensions.height;

    for (int i = 0; i < page->skyline_size; ++i)
    {
        int y;
        for (int r = 0; r < 2; ++r)
        {

//printf("    SL: %d  r: %d\n", i, r);

            if (apBinPackSLBLRectangleFitsAtNode(page, i, width, height, bin_width, bin_height, &y))
            {
//printf("    if fit!  y: %d  width: %d   height: %d\n", y, width, height);
                if (y + height < best_height || ((y + height) == best_height && skyline[i].width < best_width))
                {
                    best_index = i;
                    best_height = y + height;
                    best_width = skyline[i].width;
                    rect.pos.x = skyline[i].x;
                    rect.pos.y = y;
                    rect.size.width = width;
                    rect.size.height = height;

//printf("   best_index: %d\n", best_index);
                }
            } // else it didn't fit
            
            if (!allow_rotate || width == height)
                break;

            int tmp = width;
            width = height;
            height = tmp;
        }
    }

    *out_rect = rect;
    return best_index;
}

static void apBinPackEraseSkylineNode(apBinPackerPage* page, int index)
{
    assert(page->skyline_size > 0);
    // Move all items one step to the left
    int num_to_move = page->skyline_size - index - 1;
    memmove(page->skyline+index, page->skyline+index+1, num_to_move * sizeof(apBinPackSkylineNode));
    page->skyline_size -= 1;
}

static void apBinPackInsertSkylineNode(apBinPackerPage* page, int index, const apBinPackSkylineNode* node)
{
    if (page->skyline_size == page->skyline_capacity)
    {
        page->skyline_capacity += 8;
        page->skyline = (apBinPackSkylineNode*)realloc(page->skyline, page->skyline_capacity * sizeof(apBinPackSkylineNode));
    }

    int size = page->skyline_size;
    int num_to_move = size - index;
    // Move all items one step to the right
    memmove(page->skyline+index+1, page->skyline+index, num_to_move * sizeof(apBinPackSkylineNode));
    page->skyline[index] = *node;
    page->skyline_size += 1;
}

static void apBinPackInsertSkylineNodeFromRect(apBinPackerPage* page, int index, const apRect* rect)
{
    apBinPackSkylineNode node;
    node.x = rect->pos.x;
    node.y = rect->pos.y + rect->size.height;
    node.width = rect->size.width;
    apBinPackInsertSkylineNode(page, index, &node);
}

static void apBinPackFixupSkyline(apBinPackerPage* page, int index)
{
    apBinPackSkylineNode* skyline = page->skyline;

    // Now clear up/split any nodes that overlap with this new new
    for (int i = index + 1; i < page->skyline_size; ++i)
    {
		assert(skyline[i-1].x <= skyline[i].x);

        if (skyline[i].x < (skyline[i-1].x + skyline[i-1].width))
        {
			int shrink = skyline[i-1].x + skyline[i-1].width - skyline[i].x;

			skyline[i].x += shrink;
			skyline[i].width -= shrink;

            if (skyline[i].width <= 0)
            {
                apBinPackEraseSkylineNode(page, i);
                --i;
            }
            else 
            {
                break;
            }
        }
        else 
        {
            break;
        }
    }

    // now, if there are segments that are on the same y level, then we need to merge them

    // we don't need to merge items before the start,
    // as they cannot have the same y level due to this merge step
    int start = index - 1;
    if (start < 0)
        start = 0;
    for(int i = start; i < page->skyline_size-1; ++i)
    {
		if (skyline[i].y == skyline[i+1].y)
		{
			skyline[i].width += skyline[i+1].width;
            apBinPackEraseSkylineNode(page, i+1);
			--i;
		}
    }
}

static int apBinPackPackRect(apBinPackerPage* page, int width, int height, int allow_rotate, apRect* rect)
{
	int best_index = apBinPackSkylineBLPackRect(page, width, height, allow_rotate, rect);
    if (best_index != -1)
    {
        apBinPackInsertSkylineNodeFromRect(page, best_index, rect);
        apBinPackFixupSkyline(page, best_index);
        return 1;
    }
    return 0;
}

static void apBinPackPackImages(apPacker* _packer, apContext* ctx)
{
    apBinPacker* packer = (apBinPacker*)_packer;
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
    bin_size = NextPowerOfTwo((uint32_t)bin_size);

    apBinPackerPage* page = &packer->page;
    memset(page, 0, sizeof(apBinPackerPage));
    page->page = apAllocPage(ctx);
    page->page->dimensions.width = bin_size;
    page->page->dimensions.height = bin_size;

    apBinPackSkylineNode node;
    node.x = 0;
    node.y = 0;
    node.width = bin_size;
    apBinPackInsertSkylineNode(page, 0, &node);

// printf("packing...\n");
// printf("  page size: %d x %d\n", page->page->dimensions.width, page->page->dimensions.height);
// debugSkyline(page);

    int allow_rotate = !packer->options.no_rotate;
    for (int i = 0; i < ctx->num_images; ++i)
    {
        apImage* image = ctx->images[i];

// printf("  image: %d x %d %s\n", image->dimensions.width, image->dimensions.height, image->path);

        if (apBinPackPackRect(page, image->dimensions.width, image->dimensions.height, allow_rotate, &image->placement))
        {
            image->rotation = image->placement.size.width == image->dimensions.width ? 0 : 90;
            image->page = page->page->index;

// printf("  rotation: %d   pos: %d, %d  %d, %d\n", image->rotation,
//         image->placement.pos.x, image->placement.pos.y, image->placement.size.width, image->placement.size.height);

        }
        else
        {
            // We need to grow the page size (or switch page)
        }

        //debugSkyline(page);
    }

}

apPacker* apCreateBinPacker(apBinPackOptions* options)
{
    apBinPacker* packer = (apBinPacker*)malloc(sizeof(apBinPacker));
    memset(packer, 0, sizeof(apBinPacker));
    packer->super.pack_type = AP_PT_BIN_PACK;
    packer->super.createImage = apBinPackCreateImage;
    packer->super.destroyImage = apBinPackDestroyImage;
    packer->super.packImages = apBinPackPackImages;
    packer->options = *options;
    return (apPacker*)packer;
}

void apDestroyBinPacker(apPacker* packer)
{
    free((void*)packer);
}