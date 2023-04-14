#include <memory.h>
#include <stdlib.h> // atoi

#define JC_TEST_IMPLEMENTATION
#include <jc_test.h>
#include <array.h>

extern "C" {
#include <stb_wrappers.h>
#include <atlaspacker/atlaspacker.h>
#include <atlaspacker/tilepacker.h>
#include <atlaspacker/convexhull.h>
#include "utils.h"
}

TEST(PackerTilePack, OverlapTest)
{
    apPosf box1[4] = {
        {0, 0},
        {3, 0},
        {3, 3},
        {0, 3},
    };

    apPosf triangle1[3] = {
        {0.5f, 0.5f},
        {2.5f, 0.5f},
        {1.5f, 2.5f},
    };

    ASSERT_EQ(1, apOverlapTest2D(box1, 4, triangle1, 3));
    ASSERT_EQ(1, apOverlapTest2D(triangle1, 3, box1, 4));


    apPosf triangle2[3] = {
        {3.5f, 0.5f},
        {5.5f, 0.5f},
        {4.5f, 2.5f},
    };

    ASSERT_EQ(0, apOverlapTest2D(triangle2, 3, box1, 4));

    apPosf triangle3[3] = {
        {-100, -50},
        {100, -50},
        {0, 50},
    };

    ASSERT_EQ(1, apOverlapTest2D(triangle3, 3, box1, 4));


    // This is upside down
    int expected[9] = {
        1,1,1,
        1,1,1,
        0,1,0
    };

    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            apPosf boxsmall[4] = {
                {x+0.0f, y+0.0f},
                {x+1.0f, y+0.0f},
                {x+1.0f, y+1.0f},
                {x+0.0f, y+1.0f},
            };

            ASSERT_EQ(expected[y*3+x], apOverlapTest2D(triangle1, 3, boxsmall, 4));
        }
    }
}

TEST(PackerTilePack, PackSmall) {
    apTilePackOptions packer_options;
    memset(&packer_options, 0, sizeof(packer_options));
    apPacker* packer = apTilePackerCreate(&packer_options);

    apOptions options;
    apContext* ctx = apCreate(&options, packer);

    const char* paths[] = {
        "examples/basic/shape_L_128.png",
        "examples/basic/box_fill_64.png",
        "examples/basic/triangle_fill_64.png",
        ////"examples/basic/circle_fill_64.png",
    };
    const int num_images = sizeof(paths)/sizeof(paths[0]);

    Image* images[num_images];
    for (int i = 0; i < num_images; ++i)
    {
        Image* image = LoadImage(paths[i]);
        images[i] = image;
        apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);
    }

    apPackImages(ctx);

    ASSERT_TRUE(DebugWriteOutput(ctx, "pack_tile_small"));

    apDestroy(ctx);

    for (int i = 0; i < num_images; ++i)
    {
        DestroyImage(images[i]);
    }
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

TEST(PackerTilePack, PackSpineboyNoVertices) {
    const int num_images = sizeof(spineboy_files)/sizeof(spineboy_files[0]);

    Image** images = new Image*[num_images];
    for (int i = 0; i < num_images; ++i)
    {
        Image* image = LoadImage(spineboy_files[i]);
        images[i] = image;
    }

    SortImages(images, num_images);

    apTilePackOptions packer_options;
    memset(&packer_options, 0, sizeof(packer_options));
    apPacker* packer = apTilePackerCreate(&packer_options);

    apOptions options;
    apContext* ctx = apCreate(&options, packer);

    for (int i = 0; i < num_images; ++i)
    {
        Image* image = images[i];
        //printf("Adding image: %s, %d x %d  \t\tarea: %d\n", image->path, image->width, image->height, image->width * image->height);
        apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);
    }

    apPackImages(ctx);

    ASSERT_TRUE(DebugWriteOutput(ctx, "pack_tile_spineboy_noverts"));

    apDestroy(ctx);

    for (int i = 0; i < num_images; ++i)
    {
        DestroyImage(images[i]);
    }
}


TEST(PackerTilePack, PackSpineboyVertices) {
    const int num_images = sizeof(spineboy_files)/sizeof(spineboy_files[0]);

    Image** images = new Image*[num_images];
    for (int i = 0; i < num_images; ++i)
    {
        Image* image = LoadImage(spineboy_files[i]);
        images[i] = image;
    }

    printf("Loaded %u images\n", num_images);

    SortImages(images, num_images);

    apTilePackOptions packer_options;
    memset(&packer_options, 0, sizeof(packer_options));
    apPacker* packer = apTilePackerCreate(&packer_options);

    apOptions options;
    apContext* ctx = apCreate(&options, packer);

    for (int i = 0; i < num_images; ++i)
    {
        Image* image = images[i];
        //printf("Adding image: %s, %d x %d  \t\tarea: %d\n", image->path, image->width, image->height, image->width * image->height);
        apImage* apimage = apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);

        int num_planes = 8;

    //int debug = i == 0;

        int dilate = 0;
        uint8_t* hull_image = apCreateHullImage(image->data, (uint32_t)image->width, (uint32_t)image->height, (uint32_t)image->channels, dilate);

    // if (debug)
    // {
    //     char path[64];
    //     snprintf(path, sizeof(path), "image_tilepack_%s_%02d.tga", "hullimage", i);
    //     int result = STBI_write_tga(path, image->width, image->height, 1, hull_image);
    //     if (result)
    //         printf("Wrote %s at %d x %d\n", path, image->width, image->height);
    // }

        int num_vertices;
        apPosf* vertices = apConvexHullFromImage(num_planes, hull_image, image->width, image->height, &num_vertices);

    // if (debug)
    // {
    //     // for(int v = 0; v < num_vertices; ++v)
    //     // {
    //     //     printf("v%d: %f, %f\n", v, vertices[v].x, vertices[v].y);
    //     // }
    // }

        // Triangulate a convex hull
        int num_triangles = num_vertices - 2;
        apPosf* triangles = (apPosf*)malloc(sizeof(apPosf) * (size_t)num_triangles * 3);
        for (int t = 0; t < num_triangles; ++t)
        {
            triangles[t*3+0] = vertices[0];
            triangles[t*3+1] = vertices[1+t+0];
            triangles[t*3+2] = vertices[1+t+1];
        }

        apTilePackerCreateTileImageFromTriangles(packer, apimage, triangles, num_triangles*3);

    // if (debug)
    // {
    //     int tile_size = apTilePackerGetTileSize(packer);
    //     uint8_t* dbgimage = apTilePackerDebugCreateImageFromTileImage(apimage, 0, tile_size);

    //     char path[64];
    //     snprintf(path, sizeof(path), "image_tilepack_%s_%02d.tga", "tileimage", i);
    //     int result = STBI_write_tga(path, image->width, image->height, 1, dbgimage);
    //     if (result)
    //         printf("Wrote %s at %d x %d\n", path, image->width, image->height);

    //     free((void*)dbgimage);
    // }

        //uint8_t color[] = {127, 64, 0, 255};
        uint8_t color[] = {255,255,255, 255};
        DebugDrawHull(vertices, num_vertices, color, image, image->data);

        free((void*)vertices);
        free((void*)hull_image);
    }

    apPackImages(ctx);

    ASSERT_TRUE(DebugWriteOutput(ctx, "pack_tile_spineboy"));

    apDestroy(ctx);

    for (int i = 0; i < num_images; ++i)
    {
        DestroyImage(images[i]);
    }
}

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

    if (strstr(path, "spineboy.png") != 0)
        return 1;
    if (strstr(path, "spineboy_2x.png") != 0)
        return 1;

    //  if (strcmp(path, "/Users/mathiaswesterdahl/work/projects/agulev/BigAtlases/main/images/tile_0484 copy.png") != 0)
    //     return 1;

    jc::Array<Image*>* images = (jc::Array<Image*>*)_ctx;
    if (images->Full())
        images->SetCapacity(images->Capacity() + 32);

    Image* image = LoadImage(path);
    images->Push(image);
    return 1;
}

static int TestStandalone(const char* dir_path, const char* outname, apOptions* options, apTilePackOptions* packer_options)
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

    int num_images = (int)images.Size();
    printf("Loaded %d images in %.2f ms\n", num_images, (tend - tstart)/1000.0f);

    SortImages(images.Begin(), num_images);

    tstart = GetTime();

    apPacker* packer = apTilePackerCreate(packer_options);
    apContext* ctx = apCreate(options, packer);

    uint64_t t_add_images = 0;
    uint64_t t_create_hull_images = 0;
    uint64_t t_convex_hulls = 0;
    uint64_t t_tile_image_from_triangles = 0;

    int mode = 1; // 0: convex hull, 1: boxes
    for (int i = 0; i < num_images; ++i)
    {
        Image* image = images[(uint32_t)i];

        uint64_t tsubstart = GetTime();

        //printf("Adding image: %s, %d x %d  \t\tarea: %d\n", image->path, image->width, image->height, image->width * image->height);
        apImage* apimage = apAddImage(ctx, image->path, image->width, image->height, image->channels, image->data);

        t_add_images += (GetTime() - tsubstart);

        int num_planes = 8;

    int debug = i == 0;
    //int debug = 0;

        uint8_t* hull_image = 0;

        int num_vertices = 0;
        apPosf* vertices = 0;

        int num_triangles = 0;
        apPosf* triangles = 0;

        if (mode == 0)
        {
            tsubstart = GetTime();

            int dilate = 0;
            hull_image = apCreateHullImage(image->data, (uint32_t)image->width, (uint32_t)image->height, (uint32_t)image->channels, dilate);

            t_create_hull_images += (GetTime() - tsubstart);

            tsubstart = GetTime();

            vertices = apConvexHullFromImage(num_planes, hull_image, image->width, image->height, &num_vertices);
            if (!vertices)
            {
                printf("Failed to generate hull for %s\n", image->path);

                char path[64];
                snprintf(path, sizeof(path), "image_tilepack_%s_%02d.tga", "hullimage", i);
                int result = STBI_write_tga(path, image->width, image->height, 1, hull_image);
                if (result)
                    printf("Wrote %s at %d x %d\n", path, image->width, image->height);

                return 1;
            }

            t_convex_hulls += (GetTime() - tsubstart);

            // Triangulate a convex hull
            num_triangles = num_vertices - 2;
            triangles = (apPosf*)malloc(sizeof(apPosf) * (size_t)num_triangles * 3);
            for (int t = 0; t < num_triangles; ++t)
            {
                triangles[t*3+0] = vertices[0];
                triangles[t*3+1] = vertices[1+t+0];
                triangles[t*3+2] = vertices[1+t+1];
            }

            tsubstart = GetTime();

            apTilePackerCreateTileImageFromTriangles(packer, apimage, triangles, num_triangles*3);

            t_tile_image_from_triangles += (GetTime() - tsubstart);

        }

        if (debug)
        {
            // printf("MODE: %d\n", mode);
            // for (int t = 0; t < num_triangles; ++t)
            // {
            //     printf("  t: %.2f, %.2f\n", triangles[t].x, triangles[t].y);
            // }
        }

    // if (debug)
    // {
    //     char path[64];
    //     snprintf(path, sizeof(path), "image_tilepack_%s_%02d.tga", "hullimage", i);
    //     int result = STBI_write_tga(path, image->width, image->height, 1, hull_image);
    //     if (result)
    //         printf("Wrote %s at %d x %d\n", path, image->width, image->height);
    // }

    // if (debug)
    // {
    //     // for(int v = 0; v < num_vertices; ++v)
    //     // {
    //     //     printf("v%d: %f, %f\n", v, vertices[v].x, vertices[v].y);
    //     // }
    // }

    // if (debug)
    // {
    //     int tile_size = apTilePackerGetTileSize(packer);
    //     uint8_t* dbgimage = apTilePackerDebugCreateImageFromTileImage(apimage, 0, tile_size);

    //     char path[64];
    //     snprintf(path, sizeof(path), "image_tilepack_%s_%02d.tga", "tileimage", i);
    //     int result = STBI_write_tga(path, image->width, image->height, 1, dbgimage);
    //     if (result)
    //         printf("Wrote %s at %d x %d\n", path, image->width, image->height);

    //     free((void*)dbgimage);
    // }

        free((void*)triangles);
        free((void*)vertices);
        free((void*)hull_image);
    }

    tend = GetTime();
    printf("Creating atlas images took %.2f ms, where\n", (tend-tstart)/1000.0f);

    printf("   Add images       %.2f ms\n", t_add_images/1000.0f);
    printf("   Hull images      %.2f ms\n", t_create_hull_images/1000.0f);
    printf("   Create hulls     %.2f ms\n", t_convex_hulls/1000.0f);
    printf("   Tiles from tris  %.2f ms\n", t_tile_image_from_triangles/1000.0f);

    // uint64_t t_add_images = 0;
    // uint64_t t_create_hull_images = 0;
    // uint64_t t_convex_hulls = 0;
    // uint64_t t_tile_image_from_triangles = 0;

    tstart = GetTime();

    apPackImages(ctx);

    tend = GetTime();
    printf("Packing atlas images took %.2f ms\n", (tend-tstart)/1000.0f);

    tstart = GetTime();

    DebugWriteOutput(ctx, outname);

    tend = GetTime();
    printf("Writing packed image took %.2f ms\n", (tend-tstart)/1000.0f);

    apDestroy(ctx);

    for (uint32_t i = 0; i < (uint32_t)num_images; ++i)
    {
        DestroyImage(images[i]);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        const char* dir_path = 0;
        const char* outname = "standalone";

        apOptions options;
        memset(&options, 0, sizeof(options));

        apTilePackOptions packer_options;
        memset(&packer_options, 0, sizeof(packer_options));

#define CHECK_NAME(_SHORT, _LONG)      if (strcmp(_SHORT, argv[i])==0 || strcmp(_LONG, argv[i])==0)

        for (int i = 1; i < argc; ++i)
        {
            printf("%d %s\n", i, argv[i]);

            CHECK_NAME("-t", "--tile_size") { packer_options.tile_size       = atoi(argv[++i]); continue; }
            CHECK_NAME("-a", "--alpha")     { packer_options.alpha_threshold = atoi(argv[++i]); continue; }
            CHECK_NAME("-p", "--padding")   { packer_options.padding         = atoi(argv[++i]); continue; }
            CHECK_NAME("-d", "--dir")       { dir_path        = argv[++i]; continue; }
            CHECK_NAME("-o", "--output")    { outname         = argv[++i]; continue; }
            dir_path = argv[i];
        }

#undef CHECK_NAME

        printf("Dir:                %s\n", dir_path?dir_path:"none");
        printf("Output:             %s\n", outname);
        printf("tile size:          %d\n", packer_options.tile_size);
        printf("padding:            %d\n", packer_options.padding);
        printf("alpha threshold:    %d\n", packer_options.alpha_threshold);

        if (!dir_path)
        {
            printf("No directory specified!");
            return 1;
        }
        return TestStandalone(dir_path, outname, &options, &packer_options);
    }

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
