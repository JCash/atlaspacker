#include <memory.h>
#include <stdlib.h> // qsort

#define JC_TEST_IMPLEMENTATION
#include <jc_test.h>

extern "C" {
#include <stb_wrappers.h>
#include <atlaspacker/atlaspacker.h>
#include <atlaspacker/binpacker.h>
#include <atlaspacker/tilepacker.h>
#include <atlaspacker/convexhull.h>
}

#include "render.c"

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

static Image* CreateImage(const char* path, uint32_t color, int w, int h, int c)
{
    Image* image = new Image;
    image->data = (uint8_t*)malloc((uint32_t)(w * h * c));
    image->width = w;
    image->height = h;
    image->channels = c;
    uint8_t* p = image->data;
    for (int i = 0; i < w*h; ++i)
    {
        for (int j = 0; j < c; ++j)
        {
            *p++ = (color >> (j*8)) & 0xFF;
        }
    }
    image->path = path;
    return image;
}

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
    int a_w = a->width;
    int a_h = a->height;
    int b_w = b->width;
    int b_h = b->height;
    int area_a = a_w * a_h;
    int area_b = b_w * b_h;

    int max_a = a_w > a_h ? a_w : a_h;
    int min_a = a_w < a_h ? a_w : a_h;
    int max_b = b_w > b_h ? b_w : b_h;
    int min_b = b_w < b_h ? b_w : b_h;

    float square_a = ((float)max_a / (float)min_a) * (float)area_a;
    float square_b = ((float)max_b / (float)min_b) * (float)area_b;
    return (square_a <= square_b) ? 1 : -1;
}

typedef int (*QsortFn)(const void*, const void*);
static void SortImages(Image** images, int num_images)
{
    qsort(images, (size_t)num_images, sizeof(images[0]), (QsortFn)CompareImages);
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

TEST(AtlasPacker, Create) {
    ASSERT_DEATH(apCreate(0,0), "Must have options");

    apOptions options;
    ASSERT_DEATH(apCreate(&options,0), "Must have packer");
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

static void DebugDrawHull(const apPosf* vertices, int num_vertices, uint8_t* color, const Image* image, uint8_t* data)
{
    for (int i = 0; i < num_vertices; ++i)
    {
        int j = (i+1)%num_vertices;
        apPosf p0 = vertices[i];
        apPosf p1 = vertices[j];
        p0.x = (p0.x + 0.5f) * (float)image->width * 0.999f;
        p0.y = (p0.y + 0.5f) * (float)image->height * 0.999f;
        p1.x = (p1.x + 0.5f) * (float)image->width * 0.999f;
        p1.y = (p1.y + 0.5f) * (float)image->height * 0.999f;

        draw_line((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, data, image->width, image->height, image->channels, color);
    }
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

    //int vertex_counts[] = {4, 8, 16, 32};
    int vertex_counts[] = {8};

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
        //printf("Adding image: %s, %d x %d  \t\tarea: %d\n", image->path, image->width, image->height, image->width * image->height);
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

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
