#include <memory.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test.h>

extern "C" {
#include <stb_wrappers.h>
#include <atlaspacker/convexhull.h>
#include "utils.h"
}


TEST(HullConvex, CreateHullImage)
{
    Image* image = LoadImage("examples/spineboy/gun.png");

    for (int i = 0; i < 3; ++i)
    {
        uint8_t* hull_image = apCreateHullImage(image->data, (uint32_t)image->width, (uint32_t)image->height, (uint32_t)image->channels, i);

        // we use tga here to remove the compression time from the tests
        char path[64];
        snprintf(path, sizeof(path), "image_%s_dilate_%02d.tga", "convexhull", i);
        int result = STBI_write_tga(path, image->width, image->height, 1, hull_image);
        if (result)
            printf("Wrote %s at %d x %d\n", path, image->width, image->height);

        free((void*)hull_image);
    }

    DestroyImage(image);
}

TEST(HullConvex, CreateHull)
{
    Image* image = LoadImage("examples/spineboy/hoverboard-board.png");
    uint32_t size = (uint32_t)(image->width*image->height*image->channels);
    uint8_t* imagecopy = (uint8_t*)malloc(size);

    int dilate = 0;
    uint8_t* hull_image = apCreateHullImage(image->data, (uint32_t)image->width, (uint32_t)image->height, (uint32_t)image->channels, dilate);

    char path[64];
    snprintf(path, sizeof(path), "image_createhull_%s.tga", "hullimage");
    int result = STBI_write_tga(path, image->width, image->height, 1, hull_image);
    if (result)
        printf("Wrote %s at %d x %d\n", path, image->width, image->height);

    int vertex_counts[] = {4, 8, 16, 32};

    for (int k = 0; k < (int)(sizeof(vertex_counts)/sizeof(vertex_counts[0])); ++k)
    {
        memcpy(imagecopy, image->data, size);

        int num_planes = vertex_counts[k];
        int num_vertices;
        apPosf* vertices = apConvexHullFromImage(num_planes, hull_image, image->width, image->height, &num_vertices);

        ASSERT_NE((apPosf*)0, vertices);

        // for (int v = 0; v < num_vertices; ++v)
        // {
        //     printf("v %d: %f %f  -> %f %f\n", v, vertices[v].x, vertices[v].y, (vertices[v].x + 0.5f)*image->width, (vertices[v].y + 0.5f)*image->height);
        // }
        // printf("\n");


        uint8_t color[] = {127, 64, 0, 255};
        DebugDrawHull(vertices, num_vertices, color, image, imagecopy);

        free((void*)vertices);

        // we use tga here to remove the compression time from the tests
        snprintf(path, sizeof(path), "image_createhull_%s_vertices_%02d.tga", "convexhull", num_planes);
        result = STBI_write_tga(path, image->width, image->height, image->channels, imagecopy);
        if (result)
            printf("Wrote %s at %d x %d\n", path, image->width, image->height);
    }

    free((void*)imagecopy);
    free((void*)hull_image);
    DestroyImage(image);
}

struct StandaloneContext
{
    int width;
    int height;
    uint8_t* data;
};

static void PrintBox(void* _ctx, int x, int y, int width, int height)
{
    StandaloneContext* ctx = (StandaloneContext*)_ctx;
    printf("  BOX: %d, %d %d, %d\n", x, y, width, height);

    DebugPrintTileImage(ctx->width, ctx->height, ctx->data);
}

static int TestStandalone(const char* path)
{
    printf("PATH: %s\n", path);

    Image* image = LoadImage(path);
    int twidth = 0;
    int theight = 0;
    uint32_t tile_size = 16;
    int alphathreshold = 8;
    uint8_t* timage = CreateTileImage(image, tile_size, alphathreshold, &twidth, &theight);

    DebugPrintTileImage(twidth, theight, timage);

    StandaloneContext ctx;
    ctx.width = twidth;
    ctx.height = theight;
    ctx.data = timage;

    uint64_t tstart = GetTime();

    apHullFindLargestBoxes(twidth, theight, timage, PrintBox, &ctx);

    uint64_t tend = GetTime();
    printf("Finding largest boxes took %.2f ms\n", (tend-tstart)/1000.0f);

    DebugPrintTileImage(twidth, theight, timage);

    free((void*)timage);
    DestroyImage(image);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        const char* dir_path = argv[1];
        return TestStandalone(dir_path);
    }

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
