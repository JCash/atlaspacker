#include <atlaspacker/convexhull.h>
#include <atlaspacker/atlaspacker.h>

#include <stdlib.h> // malloc
#include <stdio.h> // printf
#include <math.h> // cosf/sinf
#ifndef  M_PI
#define  M_PI  3.1415926535897932384626433
#endif

typedef struct
{
    int num_planes;
    apPosf* normals;
    float* distances;   // plane distances
} apConvexHull;


static int apConvexHullSetupPlanes(apConvexHull* hull, uint8_t* image, int width, int height)
{
    int empty = 1;

    int num_planes = hull->num_planes;
    for (int i = 0; i < num_planes; ++i)
    {
        hull->distances[i] = -1000000.0f;
        
        float angle = i * 2.0f * M_PI / (float)num_planes;
        hull->normals[i].x = cosf(angle);
        hull->normals[i].y = sinf(angle);
        hull->normals[i] = apMathNormalize(hull->normals[i]);
    }

    apPos center = { width / 2.0f, height / 2.0f };
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (!image[y * width + x])
                continue;
        
            empty = 0;

            // project each corner of the texel along the plane normal
            apPosf corners[4] ={{ x + 0 - center.x, y + 0 - center.y },
                                { x + 1 - center.x, y + 0 - center.y },
                                { x + 1 - center.x, y + 1 - center.y },
                                { x + 0 - center.x, y + 1 - center.y }};

            for (int p = 0; p < num_planes; ++p)
            {
                apPosf dir = hull->normals[p];

                for (int c = 0; c < 4; ++c)
                {
                    float distance = apMathDot(dir, corners[c]);
                    if (distance > hull->distances[p])
                        hull->distances[p] = distance;
                }
            }
        }
    }

    return empty ? 0 : 1;
}

static inline apPosf apConvexHullCalculatePoint(apPosf dir, float distance)
{
    apPosf p = { dir.x * distance, dir.y * distance};
    return p;
}

static inline float apConvexHullRoundEdge(float v)
{
    if (v >= -0.5f && v <= 0.5f)
        return v;
    const float epsilon = 0.001f;
    if (v < -0.5f && (v + 0.5f) > -epsilon)
        return -0.5f;
    if (v > 0.5f && (v - 0.5f) < epsilon)
        return 0.5f;
    return v; // outside of [-0.5, 0.5] range
}

static apPosf* apConvexHullCalculateVertices(apConvexHull* hull, int width, int height)
{
    int num_planes = hull->num_planes;

    apPosf* vertices = (apPosf*)malloc(sizeof(apPosf)*num_planes);

    float half_width = width/2.0f;
    float half_height = height/2.0f;

    for (int i = 0; i < num_planes; ++i)
    {
        int j = (i+1)%num_planes;

        // in texel space
        apPosf p0 = apConvexHullCalculatePoint(hull->normals[i], hull->distances[i]);
        apPosf p1 = apConvexHullCalculatePoint(hull->normals[j], hull->distances[j]);

        // directions for the lines
        apPosf d0 = { -hull->normals[i].y, hull->normals[i].x };
        apPosf d1 = { -hull->normals[j].y, hull->normals[j].x };

        // Calculate the line intersection
        float t = ((p0.y - p1.y) * d1.x - (p0.x - p1.x) * d1.y) / (d0.x * d1.y - d1.x * d0.y);
        apPosf vertex = { p0.x + d0.x * t, p0.y + d0.y * t };

        // to [(-0.5,-0.5), (0.5,0.5)]
        vertex.x = (vertex.x / half_width) * 0.5f;
        vertex.y = (vertex.y / half_height) * 0.5f;

        vertex.x = apConvexHullRoundEdge(vertex.x);
        vertex.y = apConvexHullRoundEdge(vertex.y);

        vertices[num_planes - i - 1] = vertex;
    }

    return vertices;
}

apPosf* apConvexHullFromImage(int num_planes, uint8_t* image, int width, int height, int* num_vertices)
{
    apConvexHull hull;
    hull.num_planes = num_planes;
    hull.normals = (apPosf*)malloc(sizeof(apPosf)*num_planes);
    hull.distances = (float*)malloc(sizeof(float)*num_planes);

    int valid = apConvexHullSetupPlanes(&hull, image, width, height);
    if (!valid)
        return 0;

    apPosf* vertices = apConvexHullCalculateVertices(&hull, width, height);

    free((void*)hull.normals);
    free((void*)hull.distances);

    *num_vertices = num_planes; // until we simplify the hull
    return vertices;
}
