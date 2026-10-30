/* Wii lightmap preparation. GPL-2.0-or-later. */
#ifndef NZP_LIGHTMAP_WII_H
#define NZP_LIGHTMAP_WII_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
/* Match RGBA8 -> RGBA5551 -> this OpenGX CompoundPixelStream expansion.
 * The backend verifies this mapping against its linked converter at runtime. */
static inline int Wii_LightmapRGBA8(uint8_t *dst, size_t capacity,
    const uint8_t *src, int atlas_width, int atlas_height, int stride,
    int x, int y, int width, int height, int compact)
{
    int row, col;
    if (!dst || !src || atlas_width <= 0 || atlas_height <= 0 ||
        atlas_width > INT_MAX / 4 || stride < atlas_width * 4 ||
        width <= 0 || height <= 0 || width > atlas_width || height > atlas_height ||
        x < 0 || y < 0 || x > atlas_width-width || y > atlas_height-height ||
        (size_t)width > capacity / 4 / (size_t)height)
        return 0;
    for (row=0; row<height; ++row) {
        const uint8_t *line=src+(size_t)(y+row)*stride+(size_t)x*4;
        if (compact) {
            for (col=0; col<width; ++col, line+=4, dst+=4) {
                dst[0]=(line[0]&0xf8)|(line[0]>>5);
                dst[1]=(line[1]&0xf8)|(line[1]>>5);
                dst[2]=(line[2]&0xf8)|(line[2]>>5);
                dst[3]=(line[3]&0x80)?0xc0:0;
            }
        } else {
            for (col=0; col<width*4; ++col) dst[col]=line[col];
            dst+=(size_t)width*4;
        }
    }
    return 1;
}
#endif
