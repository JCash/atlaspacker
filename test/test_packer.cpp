#include <memory.h>
#include <stdlib.h> // qsort

#define JC_TEST_IMPLEMENTATION
#include <jc_test.h>

extern "C" {
#include <stb_wrappers.h>
#include <atlaspacker/atlaspacker.h>
#include <atlaspacker/binpacker.h>
#include <atlaspacker/tilepacker.h>
}


#pragma pack(1)
struct Image
{
    int width;
    int height;
    int channels;
    uint8_t* data;
    const char* path;
};
#pragma options align=reset

// static Image* CreateImage(const char* path, uint32_t color, int w, int h, int c)
// {
//     Image* image = new Image;
//     image->data = (uint8_t*)malloc((uint32_t)(w * h * c));
//     image->width = w;
//     image->height = h;
//     image->channels = c;
//     uint8_t* p = image->data;
//     for (int i = 0; i < w*h; ++i)
//     {
//         for (int j = 0; j < c; ++j)
//         {
//             *p++ = (color >> (j*8)) & 0xFF;
//         }
//     }
//     image->path = path;
//     return image;
// }

static Image* LoadImage(const char* path)
{
    Image* image = new Image;
    image->data = STBI_load(path, &image->width, &image->height, &image->channels);
    if (!image->data)
    {
        delete image;
        printf("Failed to load %s\n", path);
        return 0;
    }
    image->path = path;
    return image;
}

static void DestroyImage(Image* image)
{
    free((void*)image->data);
    delete image;
}

static int CompareImages(const Image** _a, const Image** _b)
{
    const Image* a = *_a;
    const Image* b = *_b;
    int area_a = a->width * a->height;
    int area_b = b->width * b->height;
    return (area_a < area_b) - (area_a > area_b);
}

typedef int (*QsortFn)(const void*, const void*);
static Image* SortImages(Image** images, int num_images)
{
    qsort(images, num_images, sizeof(images[0]), (QsortFn)CompareImages);
}

static bool DebugWriteOutput(apContext* ctx, const char* pattern)
{
    int num_pages = apGetNumPages(ctx);
    for (int i = 0; i < num_pages; ++i)
    {
        int width, height, channels;
        uint8_t* output = apRenderPage(ctx, i, &width, &height, &channels);
        if (!output)
        {
            return false;
        }

        // we use tga here to remove the compression time from the tests
        char path[64];
        snprintf(path, sizeof(path), "image_%s_%d.tga", pattern, i);
        int result = STBI_write_tga(path, width, height, channels, output);
        if (result)
            printf("Wrote %s at %d x %d\n", path, width, height);
    }
    return true;
}

// TEST(AtlasPacker, Create) {
//     ASSERT_DEATH(apCreate(0,0), "Must have options");

//     apOptions options;
//     ASSERT_DEATH(apCreate(&options,0), "Must have packer");
// }

// TEST(PackerBinPack, PackSmall) {
//     apBinPackOptions packer_options;
//     memset(&packer_options, 0, sizeof(packer_options));
//     packer_options.mode = AP_BP_MODE_DEFAULT;
//     apPacker* packer = apCreateBinPacker(&packer_options);

//     apOptions options;
//     apContext* ctx = apCreate(&options, packer);

//     Image* image_a = CreateImage("a.png", 0x00FF00FF, 64, 64, 3);
//     Image* image_b = CreateImage("b.png", 0x00FFFF00, 32, 48, 3);    // force it to rotate
//     Image* image_c = CreateImage("c.png", 0x0000FFFF, 64, 128, 3);   // force it to rotate

//     // Creates an internal version of the file, depending on the packer
//     apAddImage(ctx, image_a->path, image_a->width, image_a->height, image_a->channels, image_a->data);
//     apAddImage(ctx, image_b->path, image_b->width, image_b->height, image_b->channels, image_b->data);
//     apAddImage(ctx, image_c->path, image_c->width, image_c->height, image_c->channels, image_c->data);

//     apPackImages(ctx);

//     DebugWriteOutput(ctx, "pack_small");

//     ASSERT_EQ(3, ctx->num_images);

//     apImage* image = ctx->images[0];
//     ASSERT_EQ(0, image->placement.pos.x);
//     ASSERT_EQ(0, image->placement.pos.y);
//     ASSERT_EQ(64, image->placement.size.width);
//     ASSERT_EQ(64, image->placement.size.height);

//     image = ctx->images[1];
//     ASSERT_EQ(64, image->placement.pos.x);
//     ASSERT_EQ(0, image->placement.pos.y);
//     ASSERT_EQ(48, image->placement.size.width);
//     ASSERT_EQ(32, image->placement.size.height);

//     image = ctx->images[2];
//     ASSERT_EQ(0, image->placement.pos.x);
//     ASSERT_EQ(64, image->placement.pos.y);
//     ASSERT_EQ(128, image->placement.size.width);
//     ASSERT_EQ(64, image->placement.size.height);

//     apDestroy(ctx);

//     DestroyImage(image_a);
//     DestroyImage(image_b);
//     DestroyImage(image_c);
// }

// TEST(PackerTilePack, PackSmall) {
//     apTilePackOptions packer_options;
//     memset(&packer_options, 0, sizeof(packer_options));
//     apPacker* packer = apCreateTilePacker(&packer_options);

//     apOptions options;
//     apContext* ctx = apCreate(&options, packer);

//     const char* paths[] = {
//         "examples/basic/shape_L_128.png",
//         "examples/basic/box_fill_64.png",
//         "examples/basic/triangle_fill_64.png",
//         ////"examples/basic/circle_fill_64.png",
//     };
//     const int num_images = sizeof(paths)/sizeof(paths[0]);

//     Image* images[num_images];
//     for (int i = 0; i < num_images; ++i)
//     {
//         Image* image = LoadImage(paths[i]);
//         images[i] = image;
//         apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);
//     }

//     apPackImages(ctx);

//     ASSERT_TRUE(DebugWriteOutput(ctx, "pack_tile_small"));

//     apDestroy(ctx);

//     for (int i = 0; i < num_images; ++i)
//     {
//         DestroyImage(images[i]);
//     }
// }

// Sorted on size
const char* spineboy_files[] = {
    "examples/spineboy/crosshair.png",
    "examples/spineboy/eye-indifferent.png",
    "examples/spineboy/eye-surprised.png",
    "examples/spineboy/eyes.png",
    "examples/spineboy/front-bracer.png",
    "examples/spineboy/front-fist-closed.png",
    "examples/spineboy/front-fist-open.png",
    "examples/spineboy/front-foot.png",
    "examples/spineboy/front-shin.png",
    "examples/spineboy/front-thigh.png",
    "examples/spineboy/front-upper-arm.png",
    "examples/spineboy/goggles.png",
    "examples/spineboy/gun.png",
    "examples/spineboy/head.png",
    "examples/spineboy/hoverboard-board.png",
    "examples/spineboy/hoverboard-thruster.png",
    "examples/spineboy/hoverglow-small.png",
    "examples/spineboy/mouth-grind.png",
    "examples/spineboy/mouth-oooo.png",
    "examples/spineboy/mouth-smile.png",
    "examples/spineboy/muzzle-glow.png",
    "examples/spineboy/muzzle-ring.png",
    "examples/spineboy/muzzle.png",
    "examples/spineboy/muzzle01.png",
    "examples/spineboy/muzzle02.png",
    "examples/spineboy/muzzle03.png",
    "examples/spineboy/muzzle04.png",
    "examples/spineboy/muzzle05.png",
    "examples/spineboy/neck.png",
    "examples/spineboy/portal-bg.png",
    "examples/spineboy/portal-flare1.png",
    "examples/spineboy/portal-flare2.png",
    "examples/spineboy/portal-flare3.png",
    "examples/spineboy/portal-shade.png",
    "examples/spineboy/portal-streaks1.png",
    "examples/spineboy/portal-streaks2.png",
    "examples/spineboy/rear-bracer.png",
    "examples/spineboy/rear-foot.png",
    "examples/spineboy/rear-shin.png",
    "examples/spineboy/rear-thigh.png",
    "examples/spineboy/rear-upper-arm.png",
    "examples/spineboy/torso.png"
};

TEST(PackerTilePack, PackSpineboy) {
    apTilePackOptions packer_options;
    memset(&packer_options, 0, sizeof(packer_options));
    apPacker* packer = apCreateTilePacker(&packer_options);

    apOptions options;
    apContext* ctx = apCreate(&options, packer);

    const int num_images = sizeof(spineboy_files)/sizeof(spineboy_files[0]);

    Image** images = new Image*[num_images];
    for (int i = 0; i < num_images; ++i)
    {
        Image* image = LoadImage(spineboy_files[i]);
        images[i] = image;
    }

    SortImages(images, num_images);
    
    for (int i = 0; i < num_images; ++i)
    {
        Image* image = images[i];
        //printf("Adding image: %s, %d x %d  \t\tarea: %d\n", image->path, image->width, image->height, image->width * image->height);
        apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);
    }

    apPackImages(ctx);

    ASSERT_TRUE(DebugWriteOutput(ctx, "pack_tile_spineboy"));

    apDestroy(ctx);

    for (int i = 0; i < num_images; ++i)
    {
        DestroyImage(images[i]);
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
