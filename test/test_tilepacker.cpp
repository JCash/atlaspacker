#include <memory.h>

#define JC_TEST_USE_DEFAULT_MAIN
#include <jc_test.h>

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

    int debug = i == 0;

        int dilate = 0;
        uint8_t* hull_image = apCreateHullImage(image->data, (uint32_t)image->width, (uint32_t)image->height, (uint32_t)image->channels, dilate);
        
    if (debug)
    {
        char path[64];
        snprintf(path, sizeof(path), "image_tilepack_%s_%02d.tga", "hullimage", i);
        int result = STBI_write_tga(path, image->width, image->height, 1, hull_image);
        if (result)
            printf("Wrote %s at %d x %d\n", path, image->width, image->height);
    }
    
        int num_vertices;
        apPosf* vertices = apConvexHullFromImage(num_planes, hull_image, image->width, image->height, &num_vertices);

    if (debug)
    {
        // for(int v = 0; v < num_vertices; ++v)
        // {
        //     printf("v%d: %f, %f\n", v, vertices[v].x, vertices[v].y);
        // }
    }

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

    if (debug)
    {
        int tile_size = apTilePackerGetTileSize(packer);
        uint8_t* dbgimage = apTilePackerDebugCreateImageFromTileImage(apimage, 0, tile_size);
        
        char path[64];
        snprintf(path, sizeof(path), "image_tilepack_%s_%02d.tga", "tileimage", i);
        int result = STBI_write_tga(path, image->width, image->height, 1, dbgimage);
        if (result)
            printf("Wrote %s at %d x %d\n", path, image->width, image->height);

        free((void*)dbgimage);
    }

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
