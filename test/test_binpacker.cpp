#include <memory.h>
#include <stdlib.h> // qsort

#define JC_TEST_IMPLEMENTATION
#include <jc_test.h>
#include <array.h>

extern "C" {
#include <stb_wrappers.h>
#include <atlaspacker/atlaspacker.h>
#include <atlaspacker/binpacker.h>
#include "utils.h"
}

TEST(PackerBinPack, PackSmall) {
    apBinPackOptions packer_options;
    memset(&packer_options, 0, sizeof(packer_options));
    packer_options.mode = AP_BP_MODE_DEFAULT;
    apPacker* packer = apCreateBinPacker(&packer_options);

    apOptions options;
    apContext* ctx = apCreate(&options, packer);

    Image* image_a = CreateImage("a.png", 0x00FF00FF, 64, 64, 3);
    Image* image_b = CreateImage("b.png", 0x00FFFF00, 32, 48, 3);    // force it to rotate
    Image* image_c = CreateImage("c.png", 0x0000FFFF, 64, 128, 3);   // force it to rotate

    // Creates an internal version of the file, depending on the packer
    apAddImage(ctx, image_a->path, image_a->width, image_a->height, image_a->channels, image_a->data);
    apAddImage(ctx, image_b->path, image_b->width, image_b->height, image_b->channels, image_b->data);
    apAddImage(ctx, image_c->path, image_c->width, image_c->height, image_c->channels, image_c->data);

    apPackImages(ctx);

    DebugWriteOutput(ctx, "pack_small");

    ASSERT_EQ(3, ctx->num_images);

    apImage* image = ctx->images[0];
    ASSERT_EQ(0, image->placement.pos.x);
    ASSERT_EQ(0, image->placement.pos.y);
    ASSERT_EQ(64, image->placement.size.width);
    ASSERT_EQ(64, image->placement.size.height);

    image = ctx->images[1];
    ASSERT_EQ(64, image->placement.pos.x);
    ASSERT_EQ(0, image->placement.pos.y);
    ASSERT_EQ(48, image->placement.size.width);
    ASSERT_EQ(32, image->placement.size.height);

    image = ctx->images[2];
    ASSERT_EQ(0, image->placement.pos.x);
    ASSERT_EQ(64, image->placement.pos.y);
    ASSERT_EQ(128, image->placement.size.width);
    ASSERT_EQ(64, image->placement.size.height);

    apDestroy(ctx);

    DestroyImage(image_a);
    DestroyImage(image_b);
    DestroyImage(image_c);
}

// Sorted on size
static const char* spineboy_files[] = {
"examples/spineboy/rear-thigh.png",
"examples/spineboy/hoverglow-small.png",
"examples/spineboy/rear-upper-arm.png",
"examples/spineboy/rear-bracer.png",
"examples/spineboy/portal-streaks1.png",
"examples/spineboy/front-fist-open.png",
"examples/spineboy/portal-flare2.png",
"examples/spineboy/portal-flare3.png",
"examples/spineboy/portal-streaks2.png",
"examples/spineboy/portal-flare1.png",
"examples/spineboy/muzzle-ring.png",
"examples/spineboy/front-thigh.png",
"examples/spineboy/eye-surprised.png",
"examples/spineboy/gun.png",
"examples/spineboy/portal-shade.png",
"examples/spineboy/crosshair.png",
"examples/spineboy/portal-bg.png",
"examples/spineboy/front-foot.png",
"examples/spineboy/front-upper-arm.png",
"examples/spineboy/mouth-oooo.png",
"examples/spineboy/rear-shin.png",
"examples/spineboy/head.png",
"examples/spineboy/front-bracer.png",
"examples/spineboy/eye-indifferent.png",
"examples/spineboy/hoverboard-board.png",
"examples/spineboy/torso.png",
"examples/spineboy/muzzle05.png",
"examples/spineboy/muzzle04.png",
"examples/spineboy/front-shin.png",
"examples/spineboy/neck.png",
"examples/spineboy/mouth-smile.png",
"examples/spineboy/muzzle-glow.png",
"examples/spineboy/goggles.png",
"examples/spineboy/muzzle03.png",
"examples/spineboy/muzzle02.png",
"examples/spineboy/mouth-grind.png",
"examples/spineboy/rear-foot.png",
"examples/spineboy/hoverboard-thruster.png",
"examples/spineboy/front-fist-closed.png",
"examples/spineboy/muzzle01.png",
};

TEST(PackerBinPack, PackSpineboy) {
    const int num_images = sizeof(spineboy_files)/sizeof(spineboy_files[0]);

    Image** images = new Image*[num_images];
    for (int i = 0; i < num_images; ++i)
    {
        Image* image = LoadImage(spineboy_files[i]);
        images[i] = image;
    }

    SortImages(images, num_images);

    apBinPackOptions packer_options;
    memset(&packer_options, 0, sizeof(packer_options));
    packer_options.mode = AP_BP_MODE_DEFAULT;
    apPacker* packer = apCreateBinPacker(&packer_options);

    apOptions options;
    apContext* ctx = apCreate(&options, packer);

    for (int i = 0; i < num_images; ++i)
    {
        Image* image = images[i];
        apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);
    }

    apPackImages(ctx);

    ASSERT_TRUE(DebugWriteOutput(ctx, "pack_bin_spineboy"));

    apDestroy(ctx);

    for (int i = 0; i < num_images; ++i)
    {
        DestroyImage(images[i]);
    }
}


/*
TEST(PackerBinPack, DefoldAtlas) {
    const int num_images = 400;

    Image** images = new Image*[num_images];
    for (int i = 0; i < num_images; ++i)
    {
        const char* dirpath = "/Users/mawe/work/projects/users/mawe/AtlasGeneration/atlases";
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%s/test01_%03d.png", dirpath, i);
        Image* image = LoadImage(buffer);
        ASSERT_TRUE(image != 0);
        images[i] = image;
    }

    SortImages(images, num_images);

    apBinPackOptions packer_options;
    memset(&packer_options, 0, sizeof(packer_options));
    packer_options.mode = AP_BP_MODE_DEFAULT;
    apPacker* packer = apCreateBinPacker(&packer_options);

    apOptions options;
    apContext* ctx = apCreate(&options, packer);

    for (int i = 0; i < num_images; ++i)
    {
        Image* image = images[i];
        //printf("Adding image: %s, %d x %d  \t\tarea: %d\n", image->path, image->width, image->height, image->width * image->height);
        apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);
    }

    apPackImages(ctx);

    ASSERT_TRUE(DebugWriteOutput(ctx, "pack_bin_atlas"));

    apDestroy(ctx);

    for (int i = 0; i < num_images; ++i)
    {
        DestroyImage(images[i]);
    }
}
*/

static int IsImageSuffix(const char* suffix)
{
    return strcmp(suffix, ".png") == 0 || strcmp(suffix, ".PNG") == 0;
}

static int FileIterator(void* _ctx, const char* path)
{
    const char* suffix = strrchr(path, '.');
    if (!suffix)
        return 1;

    //printf("Path: %s %s %d\n", path, suffix?suffix:"", IsImageSuffix(suffix));

    if (!IsImageSuffix(suffix))
        return 1;

    jc::Array<Image*>* images = (jc::Array<Image*>*)_ctx;
    if (images->Full())
        images->SetCapacity(images->Capacity() + 32);

    Image* image = LoadImage(path);
    images->Push(image);
    return 1;
}

static int TestStandalone(const char* dir_path, const char* outname)
{
    printf("DIR PATH: %s\n", dir_path);

    uint64_t tstart = GetTime();

    jc::Array<Image*> images;
    IterateFiles(dir_path, true, FileIterator, &images);

    uint64_t tend = GetTime();

    if (images.Empty())
    {
        printf("Failed to find any images!\n");
        return 1;
    }

    printf("Loaded %u images in %.2f ms\n", (uint32_t)images.Size(), (tend - tstart)/1000.0f);

    SortImages(images.Begin(), images.Size());

    tstart = GetTime();

    apBinPackOptions packer_options;
    memset(&packer_options, 0, sizeof(packer_options));
    packer_options.mode = AP_BP_MODE_DEFAULT;
    apPacker* packer = apCreateBinPacker(&packer_options);

    apOptions options;
    apContext* ctx = apCreate(&options, packer);

    for (int i = 0; i < images.Size(); ++i)
    {
        Image* image = images[i];
        apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);
    }

    apPackImages(ctx);

    tend = GetTime();

    printf("Packing %u images took %.2f ms\n", (uint32_t)images.Size(), (tend - tstart)/1000.0f);

    tstart = GetTime();

    DebugWriteOutput(ctx, outname);

    tend = GetTime();
    printf("Writing packed image took %.2f ms\n", (tend-tstart)/1000.0f);

    apDestroy(ctx);

    for (int i = 0; i < images.Size(); ++i)
    {
        DestroyImage(images[i]);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        const char* dir_path = argv[1];
        const char* outname = "standalone";
        if (argc > 2)
            outname = argv[2];

        return TestStandalone(dir_path, outname);
    }

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
