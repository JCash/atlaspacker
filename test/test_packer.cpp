#include <memory.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test.h>

extern "C" {
#include <stb_wrappers.h>
#include <atlaspacker/atlaspacker.h>
#include <atlaspacker/binpacker.h>
}

TEST(AtlasPacker, Create) {
    ASSERT_DEATH(apCreate(0,0), "Must have options");

    apOptions options;
    ASSERT_DEATH(apCreate(&options,0), "Must have packer");
}

#pragma pack(1)
struct Image
{
    int width;
    int height;
    int channels;
    uint8_t* data;
};
#pragma options align=reset

static Image* CreateImage(uint32_t color, int w, int h, int c)
{
    Image* image = new Image;
    image->data = new uint8_t[w*h*c];
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
    return image;
}

static void DestroyImage(Image* image)
{
    delete[] image->data;
    delete image;
}

static void DebugWriteOutput(apContext* ctx, const char* pattern)
{
    int num_pages = apGetNumPages(ctx);
    for (int i = 0; i < num_pages; ++i)
    {
        int width, height, channels;
        uint8_t* output = apRenderPage(ctx, i, &width, &height, &channels);

        char path[64];
        snprintf(path, sizeof(path), "image_%s_%d.png", pattern, i);
        int result = STBI_write_png(path, width, height, channels, output, width*channels);
        if (result)
            printf("Wrote %s\n", path);
    }
}

TEST(PackerBinPack, PackSmall) {
    apBinPackOptions packer_options;
    memset(&packer_options, 0, sizeof(packer_options));
    packer_options.mode = AP_BP_MODE_DEFAULT;
    apPacker* packer = apCreateBinPacker(&packer_options);

    apOptions options;
    apContext* ctx = apCreate(&options, packer);

    Image* image_a = CreateImage(0x00FF00FF, 64, 64, 3);
    Image* image_b = CreateImage(0x00FFFF00, 32, 48, 3);    // force it to rotate
    Image* image_c = CreateImage(0x0000FFFF, 64, 128, 3);   // force it to rotate

    // Creates an internal version of the file, depending on the packer
    apAddImage(ctx, "a.png", image_a->width, image_a->height, image_a->channels, image_a->data);
    apAddImage(ctx, "b.png", image_b->width, image_b->height, image_b->channels, image_b->data);
    apAddImage(ctx, "c.png", image_c->width, image_c->height, image_c->channels, image_c->data);

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


int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
