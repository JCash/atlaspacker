#include <memory.h>
#include <stdlib.h> // qsort

#define JC_TEST_USE_DEFAULT_MAIN
#include <jc_test.h>

extern "C" {
#include <stb_wrappers.h>
#include <atlaspacker/atlaspacker.h>
#include "utils.h"
}


TEST(AtlasPacker, Create) {
    ASSERT_DEATH(apCreate(0,0), "Must have options");

    apOptions options;
    ASSERT_DEATH(apCreate(&options,0), "Must have packer");
}

TEST(AtlasPacker, OverlapTest)
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
