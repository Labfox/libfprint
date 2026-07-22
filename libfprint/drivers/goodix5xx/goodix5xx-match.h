/*
 * Goodix 55x4 driver for libfprint — SIGFM template format and matching
 *
 * Copyright (C) 2026 Labfox
 *
 * The SIGFM (SIFT-based) matcher and this glue are adapted from the
 * goodix-fp-linux-dev goodix53x5 driver, which pioneered SIFT matching for
 * these small Goodix image sensors (the 88x108 sensor yields far too few
 * NBIS minutiae for bozorth3 to score).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/* Opaque handle for extracted SIGFM features (struct SigfmImgInfo). Only the
 * match module talks to the SIGFM/OpenCV implementation directly. */
typedef struct SigfmImgInfo GoodixMatchInfo;

typedef enum {
  GOODIX_SIGFM_TEMPLATE_OK,
  GOODIX_SIGFM_TEMPLATE_INCOMPATIBLE,
  GOODIX_SIGFM_TEMPLATE_INVALID,
} GoodixSigfmTemplateStatus;

/* Extract SIGFM features (CLAHE + SIFT) from a processed 8-bit sensor frame of
 * @width x @height pixels. Free the result with goodix_match_free_info(). */
GoodixMatchInfo *goodix_match_extract (const guint8 *image, int width, int height);

int  goodix_match_keypoints_count (GoodixMatchInfo *info);

void goodix_match_free_info (GoodixMatchInfo *info);

/* Serialize extracted features into a driver-owned template (magic + version
 * header + serialized features). Returns NULL on serialization failure. */
GBytes *goodix_match_serialize_template (GoodixMatchInfo *info);

/* Score @probe_info against one serialized enrolled sample. On
 * GOODIX_SIGFM_TEMPLATE_OK, *score holds the SIGFM match score. */
GoodixSigfmTemplateStatus goodix_match_serialized_feature (GoodixMatchInfo *probe_info,
                                                           const guint8    *feature,
                                                           gsize            feature_len,
                                                           int             *score);

G_END_DECLS
