/* SPDX-License-Identifier: GPL-3.0-or-later */
/* DM2Mac's C interface to the unmodified xBRZ implementation. */
#ifndef DM2_XBRZ_C_H
#define DM2_XBRZ_C_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool xbrz_scale(const uint32_t *src, int w, int h, int factor, uint32_t *dst);

#ifdef __cplusplus
}
#endif
#endif
