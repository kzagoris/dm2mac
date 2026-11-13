/* SPDX-License-Identifier: GPL-3.0-or-later */
/* DM2Mac adapter: opaque RGB, replicated edges, and no exceptions across the C ABI. */
#include "xbrz_c.h"
#include "xbrz.h"
#include <climits>

extern "C" bool xbrz_scale(const uint32_t *src, int w, int h, int factor, uint32_t *dst)
{
    if (!src || !dst || w <= 0 || h <= 0 || factor < 2 || factor > 4 ||
        w > INT_MAX / factor || h > INT_MAX / factor ||
        w * factor > INT_MAX / (h * factor))
        return false;
    try {
        // RGB mode repeats the edges of the screen rather than adding transparent pixels.
        xbrz::scale(factor, src, dst, w, h, xbrz::ColorFormat::RGB);
        // RGB blending ignores alpha. SDL and the caller receive an entirely opaque frame.
        const size_t count = static_cast<size_t>(w) * h * factor * factor;
        for (size_t i = 0; i < count; ++i)
            dst[i] |= 0xff000000u;
        return true;
    } catch (...) {
        return false;
    }
}
