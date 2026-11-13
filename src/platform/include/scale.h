/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM2_PLATFORM_SCALE_H
#define DM2_PLATFORM_SCALE_H
/* Prefixes: scale */

/* The pixel upscale filters of the window: Scale2x and Scale3x on indices, xBRZ on colours. */

#include <stdbool.h>
#include <stdint.h>

/* AdvMAME Scale2x on palette indices; dst is (w*2) x (h*2), edges replicate. */
void scale2x(const uint8_t *src, int w, int h, uint8_t *dst);
/* AdvMAME Scale3x on palette indices; dst is (w*3) x (h*3), edges replicate. */
void scale3x(const uint8_t *src, int w, int h, uint8_t *dst);
/* xBRZ on opaque ARGB8888 colours; dst is (w*factor) x (h*factor), edges replicate. False on an invalid
   size, a factor outside 2 through 4, or allocation failure. */
bool scale_xbrz(const uint32_t *src, int w, int h, int factor, uint32_t *dst);

#endif /* DM2_PLATFORM_SCALE_H */
