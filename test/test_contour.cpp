#include <memory.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test.h>

extern "C" {
#include <stb_wrappers.h>
#include <atlaspacker/atlaspacker.h>
#include <atlaspacker/tilepacker.h>
#include <atlaspacker/contour.h>
#include "utils.h"
}


// TEST(Contour, CreateHullImage)
// {
//     Image* image = LoadImage("examples/spineboy/gun.png");

//     for (int i = 0; i < 3; ++i)
//     {
//         uint8_t* hull_image = apCreateHullImage(image->data, (uint32_t)image->width, (uint32_t)image->height, (uint32_t)image->channels, i);

//         // we use tga here to remove the compression time from the tests
//         char path[64];
//         snprintf(path, sizeof(path), "image_%s_dilate_%02d.tga", "convexhull", i);
//         int result = STBI_write_tga(path, image->width, image->height, 1, hull_image);
//         if (result)
//             printf("Wrote %s at %d x %d\n", path, image->width, image->height);

//         free((void*)hull_image);
//     }

//     DestroyImage(image);
// }

static int TestStandalone(const char* path, const char* outname)
{
    printf("PATH: %s\n", path);

    //uint64_t tstart = GetTime();

    Image* image = LoadImage(path);

    // Convert to a 1-channel, black and white image
    int twidth = 0;
    int theight = 0;
    uint8_t* timage = apTilePackerDebugCreateTileImageFromImage(16, image->width, image->height, image->channels, image->data, &twidth, &theight);

    // Get the contour from the tile image
    int num_triangles = 0;
    apPosf* triangles = apContourFromImage(timage, (uint32_t)twidth, (uint32_t)theight, &num_triangles);

    uint8_t color[] = {255,255,255,128};
    DebugDrawTriangles(triangles, num_triangles, color, image, image->data);

    // we use tga here to remove the compression time from the tests
    char outpath[64];
    snprintf(outpath, sizeof(outpath), "image_%s_standalone.tga", "contour");
    int result = STBI_write_tga(outpath, image->width, image->height, 1, image->data);
    if (result)
        printf("Wrote %s at %d x %d\n", outpath, image->width, image->height);

    free((void*)triangles);
    free((void*)timage);

    DestroyImage(image);
    return 0;

    //uint64_t tend = GetTime();

    // if (images.Empty())
    // {
    //     printf("Failed to find any images!\n");
    //     return 1;
    // }

    // printf("Loaded %u images in %.2f ms\n", (uint32_t)images.Size(), (tend - tstart)/1000.0f);

    // SortImages(images.Begin(), images.Size());

    // tstart = GetTime();

    // apBinPackOptions packer_options;
    // memset(&packer_options, 0, sizeof(packer_options));
    // packer_options.mode = AP_BP_MODE_DEFAULT;
    // apPacker* packer = apCreateBinPacker(&packer_options);

    // apOptions options;
    // apContext* ctx = apCreate(&options, packer);

    // for (int i = 0; i < images.Size(); ++i)
    // {
    //     Image* image = images[i];
    //     apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);
    // }

    // apPackImages(ctx);

    // tend = GetTime();

    // printf("Packing %u images took %.2f ms\n", (uint32_t)images.Size(), (tend - tstart)/1000.0f);

    // tstart = GetTime();

    // DebugWriteOutput(ctx, outname);

    // tend = GetTime();
    // printf("Writing packed image took %.2f ms\n", (tend-tstart)/1000.0f);

    // apDestroy(ctx);

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
