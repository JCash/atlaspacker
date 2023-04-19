#include <stdint.h>
#include <atlaspacker/atlaspacker.h>
#include <stdlib.h> // abs

static void plot(int x, int y, uint8_t* image, int width, int height, int nchannels, uint8_t* color)
{
    if( x < 0 || y < 0 || x > (width-1) || y > (height-1) )
        return;
    int index = y * width * nchannels + x * nchannels;
    for( int i = 0; i < nchannels; ++i )
    {
        image[index+i] = color[i];
    }
}

// http://members.chello.at/~easyfilter/bresenham.html
void draw_line(int x0, int y0, int x1, int y1, uint8_t* image, int width, int height, int nchannels, const uint8_t* color)
{
    int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2; // error value e_xy

    for(;;)
    {  // loop
        plot(x0,y0, image, width, height, nchannels, color);
        if (x0==x1 && y0==y1) break;
        e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; } // e_xy+e_x > 0
        if (e2 <= dx) { err += dx; y0 += sy; } // e_xy+e_y < 0
    }
}

// // http://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/
// static inline int orient2d(const apPosf* a, const apPosf* b, const apPosf* c)
// {
//     return ((int)b->x - (int)a->x)*((int)c->y - (int)a->y) - ((int)b->y - (int)a->y)*((int)c->x - (int)a->x);
// }

// static inline int min2(int a, int b)
// {
//     return (a < b) ? a : b;
// }

// static inline int max2(int a, int b)
// {
//     return (a > b) ? a : b;
// }

// static inline int min3(int a, int b, int c)
// {
//     return min2(a, min2(b, c));
// }
// static inline int max3(int a, int b, int c)
// {
//     return max2(a, max2(b, c));
// }

// static void draw_triangle(const apPosf* v0, const apPosf* v1, const apPosf* v2, uint8_t* image, int width, int height, int nchannels, uint8_t* color)
// {
//     int area = orient2d(v0, v1, v2);
//     if( area == 0 )
//         return;

//     // Compute triangle bounding box
//     int minX = min3((int)v0->x, (int)v1->x, (int)v2->x);
//     int minY = min3((int)v0->y, (int)v1->y, (int)v2->y);
//     int maxX = max3((int)v0->x, (int)v1->x, (int)v2->x);
//     int maxY = max3((int)v0->y, (int)v1->y, (int)v2->y);

//     // Clip against screen bounds
//     minX = max2(minX, 0);
//     minY = max2(minY, 0);
//     maxX = min2(maxX, width - 1);
//     maxY = min2(maxY, height - 1);

//     // Rasterize
//     apPosf p;
//     for (p.y = (float)minY; p.y <= (float)maxY; p.y++) {
//         for (p.x = (float)minX; p.x <= (float)maxX; p.x++) {
//             // Determine barycentric coordinates
//             int w0 = orient2d(v1, v2, &p);
//             int w1 = orient2d(v2, v0, &p);
//             int w2 = orient2d(v0, v1, &p);

//             // If p is on or inside all edges, render pixel.
//             if (w0 >= 0 && w1 >= 0 && w2 >= 0)
//             {
//                 plot((int)p.x, (int)p.y, image, width, height, nchannels, color);
//             }
//         }
//     }
// }
