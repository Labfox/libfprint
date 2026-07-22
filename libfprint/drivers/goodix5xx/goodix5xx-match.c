/*
 * Goodix 55x4 driver for libfprint — SIGFM template format and matching
 *
 * Copyright (C) 2026 Labfox
 *
 * Adapted from the goodix-fp-linux-dev goodix53x5 driver.
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

#include "goodix5xx-match.h"
#include "sigfm/sigfm.hpp"

#include <string.h>

/* Driver-owned wrapper for serialized SIGFM features. Bump the version when
 * preprocessing, extraction, or matching semantics make old templates unsafe
 * to compare against newly enrolled templates. */
#define GOODIX_SIGFM_TEMPLATE_MAGIC      "GD5X"
#define GOODIX_SIGFM_TEMPLATE_MAGIC_LEN  4
#define GOODIX_SIGFM_TEMPLATE_VERSION    1
#define GOODIX_SIGFM_TEMPLATE_HEADER_LEN \
  (GOODIX_SIGFM_TEMPLATE_MAGIC_LEN + sizeof (guint16))
#define GOODIX_SIGFM_TEMPLATE_MAX_LEN    (1024 * 1024)

GoodixMatchInfo *
goodix_match_extract (const guint8 *image, int width, int height)
{
  return sigfm_extract (image, width, height);
}

int
goodix_match_keypoints_count (GoodixMatchInfo *info)
{
  return sigfm_keypoints_count (info);
}

void
goodix_match_free_info (GoodixMatchInfo *info)
{
  sigfm_free_info (info);
}

GBytes *
goodix_match_serialize_template (GoodixMatchInfo *info)
{
  guint8 *feature;
  guint8 *tmpl;
  guint16 version;
  int feature_len;

  feature = sigfm_serialize_binary (info, &feature_len);
  if (feature == NULL || feature_len <= 0 ||
      feature_len > GOODIX_SIGFM_TEMPLATE_MAX_LEN - (int) GOODIX_SIGFM_TEMPLATE_HEADER_LEN)
    {
      g_free (feature);
      return NULL;
    }

  tmpl = g_malloc (GOODIX_SIGFM_TEMPLATE_HEADER_LEN + feature_len);
  memcpy (tmpl, GOODIX_SIGFM_TEMPLATE_MAGIC, GOODIX_SIGFM_TEMPLATE_MAGIC_LEN);
  version = GUINT16_TO_LE (GOODIX_SIGFM_TEMPLATE_VERSION);
  memcpy (tmpl + GOODIX_SIGFM_TEMPLATE_MAGIC_LEN, &version, sizeof (version));
  memcpy (tmpl + GOODIX_SIGFM_TEMPLATE_HEADER_LEN, feature, feature_len);
  g_free (feature);

  return g_bytes_new_take (tmpl,
                           GOODIX_SIGFM_TEMPLATE_HEADER_LEN + feature_len);
}

static SigfmImgInfo *
goodix_match_deserialize_template (const guint8 *tmpl,
                                   gsize         tmpl_len,
                                   GoodixSigfmTemplateStatus *status)
{
  SigfmImgInfo *info;
  guint16 version;
  gsize feature_len;

  *status = GOODIX_SIGFM_TEMPLATE_INVALID;

  if (tmpl_len <= GOODIX_SIGFM_TEMPLATE_HEADER_LEN ||
      tmpl_len > GOODIX_SIGFM_TEMPLATE_MAX_LEN ||
      memcmp (tmpl, GOODIX_SIGFM_TEMPLATE_MAGIC,
              GOODIX_SIGFM_TEMPLATE_MAGIC_LEN) != 0)
    {
      *status = GOODIX_SIGFM_TEMPLATE_INCOMPATIBLE;
      return NULL;
    }

  memcpy (&version, tmpl + GOODIX_SIGFM_TEMPLATE_MAGIC_LEN, sizeof (version));
  if (GUINT16_FROM_LE (version) != GOODIX_SIGFM_TEMPLATE_VERSION)
    {
      *status = GOODIX_SIGFM_TEMPLATE_INCOMPATIBLE;
      return NULL;
    }

  feature_len = tmpl_len - GOODIX_SIGFM_TEMPLATE_HEADER_LEN;
  if (feature_len > G_MAXINT)
    return NULL;

  info = sigfm_deserialize_binary (tmpl + GOODIX_SIGFM_TEMPLATE_HEADER_LEN,
                                   (int) feature_len);
  if (info != NULL)
    *status = GOODIX_SIGFM_TEMPLATE_OK;

  return info;
}

GoodixSigfmTemplateStatus
goodix_match_serialized_feature (GoodixMatchInfo *probe_info,
                                 const guint8    *feature,
                                 gsize            feature_len,
                                 int             *score)
{
  SigfmImgInfo *tmpl_info;
  GoodixSigfmTemplateStatus status;

  tmpl_info = goodix_match_deserialize_template (feature, feature_len, &status);
  if (tmpl_info == NULL)
    return status;

  *score = sigfm_match_score (probe_info, tmpl_info);
  sigfm_free_info (tmpl_info);
  if (*score < 0)
    return GOODIX_SIGFM_TEMPLATE_INVALID;

  return GOODIX_SIGFM_TEMPLATE_OK;
}
