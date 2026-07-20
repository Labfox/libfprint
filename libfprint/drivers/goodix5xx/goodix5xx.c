/*
 * Goodix 55x4 (TLS image sensor) driver for libfprint
 *
 * Copyright (C) 2026 Labfox
 *
 * Protocol ported faithfully from the goodix-fp-dump python reference
 * (goodix.py / driver_55x4.py).
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

#define FP_COMPONENT "goodix5xx"

#include <string.h>
#include <gnutls/gnutls.h>

#include "drivers_api.h"
#include "fpi-image.h"
#include "goodix5xx.h"

/* ------------------------------------------------------------------ *
 *  Device constants (driver_55x4.py)
 * ------------------------------------------------------------------ */

/* The interface and bulk endpoints are discovered from the descriptors at
 * open time (protocol.py does the same), and stored on the device. */
#define GD_USB_CLASS_CDC_DATA   0x0a
#define GD_USB_CLASS_VENDOR     0xff

#define GD_SENSOR_WIDTH         88
#define GD_SENSOR_HEIGHT        108

/* Raster geometry of the decoded frame.  tool.write_pgm() groups the pixel
 * stream into lines of length SENSOR_HEIGHT, so the actual raster is
 * SENSOR_HEIGHT wide by SENSOR_WIDTH tall. */
#define GD_IMAGE_WIDTH          GD_SENSOR_HEIGHT
#define GD_IMAGE_HEIGHT         GD_SENSOR_WIDTH

/* SENSOR_WIDTH * SENSOR_HEIGHT / 4 * 6 + 4 (see tool.decode_image) */
#define GD_IMAGE_BYTES          ((GD_SENSOR_WIDTH * GD_SENSOR_HEIGHT / 4 * 6) + 4)

#define GD_USB_TIMEOUT          5000    /* ms */
#define GD_FINGER_POLL_TIMEOUT  2000    /* ms */
#define GD_READ_BUF_SIZE        0x10000

/* PSK / whitebox key material (driver_55x4.py) */
static const guint8 GD_PSK[32] = { 0 };

static const guint8 GD_PSK_WHITE_BOX[] = {
  0xec, 0x35, 0xae, 0x3a, 0xbb, 0x45, 0xed, 0x3f, 0x12, 0xc4, 0x75, 0x1f,
  0x1e, 0x5c, 0x2c, 0xc0, 0x5b, 0x3c, 0x54, 0x52, 0xe9, 0x10, 0x4d, 0x9f,
  0x2a, 0x31, 0x18, 0x64, 0x4f, 0x37, 0xa0, 0x4b, 0x6f, 0xd6, 0x6b, 0x1d,
  0x97, 0xcf, 0x80, 0xf1, 0x34, 0x5f, 0x76, 0xc8, 0x4f, 0x03, 0xff, 0x30,
  0xbb, 0x51, 0xbf, 0x30, 0x8f, 0x2a, 0x98, 0x75, 0xc4, 0x1e, 0x65, 0x92,
  0xcd, 0x2a, 0x2f, 0x9e, 0x60, 0x80, 0x9b, 0x17, 0xb5, 0x31, 0x60, 0x37,
  0xb6, 0x9b, 0xb2, 0xfa, 0x5d, 0x4c, 0x8a, 0xc3, 0x1e, 0xdb, 0x33, 0x94,
  0x04, 0x6e, 0xc0, 0x6b, 0xbd, 0xac, 0xc5, 0x7d, 0xa6, 0xa7, 0x56, 0xc5,
};

static const guint8 GD_PMK_HASH[32] = {
  0x81, 0xb8, 0xff, 0x49, 0x06, 0x12, 0x02, 0x2a, 0x12, 0x1a, 0x94, 0x49,
  0xee, 0x3a, 0xad, 0x27, 0x92, 0xf3, 0x2b, 0x9f, 0x31, 0x41, 0x18, 0x2c,
  0xd0, 0x10, 0x19, 0x94, 0x5e, 0xe5, 0x03, 0x61,
};

/* DEVICE_CONFIG (driver_55x4.py) - 256 bytes */
static const guint8 GD_DEVICE_CONFIG[] = {
  0x60, 0x11, 0x60, 0x71, 0x24, 0x95, 0x2c, 0xc1, 0x14, 0xd5, 0x10, 0xe5,
  0x00, 0xe5, 0x14, 0xf9, 0x03, 0x04, 0x02, 0x00, 0x00, 0x08, 0x00, 0x11,
  0x11, 0xba, 0x00, 0x01, 0x80, 0xca, 0x00, 0x07, 0x00, 0x84, 0x00, 0xc0,
  0xb3, 0x86, 0x00, 0xbb, 0xc4, 0x88, 0x00, 0xba, 0xba, 0x8a, 0x00, 0xb2,
  0xb2, 0x8c, 0x00, 0xaa, 0xaa, 0x8e, 0x00, 0xc1, 0xc1, 0x90, 0x00, 0xbb,
  0xbb, 0x92, 0x00, 0xb1, 0xb1, 0x94, 0x00, 0x00, 0xa8, 0x96, 0x00, 0x00,
  0xb6, 0x98, 0x00, 0x00, 0xbf, 0x9a, 0x00, 0x00, 0xba, 0x50, 0x00, 0x01,
  0x05, 0xd0, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x72, 0x00, 0x78,
  0x56, 0x74, 0x00, 0x34, 0x12, 0x26, 0x00, 0x00, 0x12, 0x20, 0x00, 0x10,
  0x40, 0x12, 0x00, 0x03, 0x04, 0x2a, 0x01, 0x02, 0x00, 0x22, 0x00, 0x01,
  0x20, 0x24, 0x00, 0x32, 0x00, 0x80, 0x00, 0x01, 0x00, 0x5c, 0x00, 0x80,
  0x00, 0x56, 0x00, 0x08, 0x20, 0x58, 0x00, 0x01, 0x00, 0x32, 0x00, 0x2c,
  0x02, 0x82, 0x00, 0x80, 0x0c, 0xba, 0x00, 0x01, 0x80, 0xca, 0x00, 0x07,
  0x00, 0x2a, 0x01, 0x82, 0x03, 0x20, 0x00, 0x10, 0x40, 0x22, 0x00, 0x01,
  0x20, 0x24, 0x00, 0x14, 0x00, 0x80, 0x00, 0x05, 0x00, 0x5c, 0x00, 0x00,
  0x01, 0x56, 0x00, 0x08, 0x20, 0x58, 0x00, 0x03, 0x00, 0x82, 0x00, 0x80,
  0x14, 0x2a, 0x01, 0x08, 0x00, 0x5c, 0x00, 0x80, 0x00, 0x62, 0x00, 0x09,
  0x03, 0x64, 0x00, 0x18, 0x00, 0x22, 0x00, 0x00, 0x20, 0x2a, 0x01, 0x08,
  0x00, 0x5c, 0x00, 0x00, 0x01, 0x52, 0x00, 0x08, 0x00, 0x54, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x9a, 0x69,
};

/* FDT payloads (driver_55x4.py) */
static const guint8 GD_FDT_MODE[] = {
  0x0d, 0x01, 0x80, 0x12, 0x80, 0x12, 0x80, 0x98, 0x80, 0x82, 0x80, 0x12,
  0x80, 0xa0, 0x80, 0x99, 0x80, 0x7f, 0x80, 0x12, 0x80, 0x9f, 0x80, 0x93,
  0x80, 0x7e,
};

static const guint8 GD_FDT_DOWN[] = {
  0x0c, 0x01, 0x80, 0xb0, 0x80, 0xc4, 0x80, 0xba, 0x80, 0xa6, 0x80, 0xb7,
  0x80, 0xc7, 0x80, 0xc0, 0x80, 0xaa, 0x80, 0xb4, 0x80, 0xc4, 0x80, 0xba,
  0x80, 0xa6,
};

/* ------------------------------------------------------------------ */

struct _FpiDeviceGoodix5xx
{
  FpImageDevice             parent;

  GThread                  *worker;
  gboolean                  deactivating;

  GUsbDevice               *usb;
  guint8                   *read_buf;      /* GD_READ_BUF_SIZE */

  guint8                    iface;         /* claimed interface number */
  guint8                    ep_in;         /* bulk IN endpoint address */
  guint8                    ep_out;        /* bulk OUT endpoint address */

  /* TLS */
  gnutls_session_t          tls_session;
  gnutls_psk_server_credentials_t tls_creds;
  gboolean                  tls_ready;

  /* GnuTLS pull buffering (raw TLS bytes unwrapped from message packs) */
  guint8                   *pull_buf;
  gsize                     pull_len;
  gsize                     pull_pos;
};

G_DEFINE_TYPE (FpiDeviceGoodix5xx, fpi_device_goodix5xx, FP_TYPE_IMAGE_DEVICE)

/* ================================================================== *
 *  Raw USB message framing
 * ================================================================== */

/* Build encode_message_pack(payload, flags) into a freshly allocated,
 * 0x40-padded buffer.  *out_len is the padded length. */
static guint8 *
gd_build_pack (guint8 flags, const guint8 *payload, gsize payload_len,
               gsize *out_len)
{
  gsize raw = 4 + payload_len;
  gsize padded = (raw + 0x3f) & ~((gsize) 0x3f);
  guint8 *buf = g_malloc0 (padded);

  buf[0] = flags;
  buf[1] = payload_len & 0xff;
  buf[2] = (payload_len >> 8) & 0xff;
  buf[3] = (buf[0] + buf[1] + buf[2]) & 0xff;
  if (payload_len)
    memcpy (buf + 4, payload, payload_len);

  *out_len = padded;
  return buf;
}

/* encode_message_protocol(payload, command, checksum=True) */
static guint8 *
gd_build_protocol (guint8 command, const guint8 *payload, gsize payload_len,
                   gsize *out_len)
{
  gsize len = 3 + payload_len + 1;
  guint8 *buf = g_malloc0 (len);
  guint sum = 0;
  gsize i;

  buf[0] = command;
  buf[1] = (payload_len + 1) & 0xff;
  buf[2] = ((payload_len + 1) >> 8) & 0xff;
  if (payload_len)
    memcpy (buf + 3, payload, payload_len);

  for (i = 0; i < 3 + payload_len; i++)
    sum += buf[i];
  buf[3 + payload_len] = (0xaa - sum) & 0xff;

  *out_len = len;
  return buf;
}

static gboolean
gd_usb_write (FpiDeviceGoodix5xx *self, const guint8 *data, gsize len,
              GError **error)
{
  return g_usb_device_bulk_transfer (self->usb, self->ep_out,
                                     (guint8 *) data, len, NULL,
                                     GD_USB_TIMEOUT, NULL, error);
}

/* Send a raw message pack (flags + payload). */
static gboolean
gd_send_pack (FpiDeviceGoodix5xx *self, guint8 flags,
              const guint8 *payload, gsize payload_len, GError **error)
{
  g_autofree guint8 *buf = NULL;
  gsize len;

  buf = gd_build_pack (flags, payload, payload_len, &len);
  return gd_usb_write (self, buf, len, error);
}

/* Send a message-protocol command wrapped in a 0xa0 pack. */
static gboolean
gd_send_cmd (FpiDeviceGoodix5xx *self, guint8 command,
             const guint8 *payload, gsize payload_len, GError **error)
{
  g_autofree guint8 *proto = NULL;
  gsize proto_len;

  proto = gd_build_protocol (command, payload, payload_len, &proto_len);
  return gd_send_pack (self, GD_FLAGS_MESSAGE_PROTOCOL, proto, proto_len, error);
}

/* Discard any pending IN data so the next exchange starts in sync
 * (goodix.py Device.empty_buffer). */
static void
gd_drain (FpiDeviceGoodix5xx *self)
{
  gsize actual;
  GError *error = NULL;

  while (g_usb_device_bulk_transfer (self->usb, self->ep_in, self->read_buf,
                                     GD_READ_BUF_SIZE, &actual, 100,
                                     NULL, &error))
    ;                             /* keep reading until it times out / errors */
  g_clear_error (&error);
}

/* Read one message pack.  Returns the pack payload in self->read_buf+4;
 * *flags and *payload_len describe it.  The pointer stays valid until the
 * next read. */
static const guint8 *
gd_read_pack (FpiDeviceGoodix5xx *self, guint timeout, guint8 *flags,
              gsize *payload_len, GError **error)
{
  gsize actual = 0;

  if (!g_usb_device_bulk_transfer (self->usb, self->ep_in, self->read_buf,
                                   GD_READ_BUF_SIZE, &actual, timeout,
                                   NULL, error))
    return NULL;

  if (actual < 4)
    {
      g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                           "Short message pack");
      return NULL;
    }

  if (((self->read_buf[0] + self->read_buf[1] + self->read_buf[2]) & 0xff)
      != self->read_buf[3])
    {
      g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                           "Bad message pack checksum");
      return NULL;
    }

  *flags = self->read_buf[0];
  *payload_len = self->read_buf[1] | (self->read_buf[2] << 8);
  if (*payload_len > actual - 4)
    *payload_len = actual - 4;

  return self->read_buf + 4;
}

/* Read and validate the ACK for a given command. */
static gboolean
gd_read_ack (FpiDeviceGoodix5xx *self, guint8 command, GError **error)
{
  const guint8 *payload;
  guint8 flags;
  gsize plen;
  guint16 body_len;

  payload = gd_read_pack (self, GD_USB_TIMEOUT, &flags, &plen, error);
  if (!payload)
    return FALSE;

  /* payload = message-protocol packet: [cmd][len16][body][cksum] */
  if (plen < 4 || payload[0] != GD_CMD_ACK)
    {
      g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                           "Expected ACK packet");
      return FALSE;
    }

  body_len = payload[1] | (payload[2] << 8);
  if (body_len < 2 || plen < 3 + (gsize) body_len)
    {
      g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                           "Malformed ACK packet");
      return FALSE;
    }

  /* body = [acked_command][flags]; flags bit0 must be set. */
  if (payload[3] != command || !(payload[4] & 0x1))
    {
      g_set_error (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                   "Unexpected ACK for command 0x%02x (got 0x%02x)",
                   command, payload[3]);
      return FALSE;
    }

  return TRUE;
}

/* Read a message-protocol reply for @command; returns a copy of the body. */
static guint8 *
gd_read_cmd_reply (FpiDeviceGoodix5xx *self, guint8 command,
                   gsize *body_len_out, GError **error)
{
  const guint8 *payload;
  guint8 flags;
  gsize plen;
  guint16 body_len;

  payload = gd_read_pack (self, GD_USB_TIMEOUT, &flags, &plen, error);
  if (!payload)
    return NULL;

  if (plen < 3 || payload[0] != command)
    {
      g_set_error (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                   "Unexpected reply command 0x%02x (want 0x%02x)",
                   plen ? payload[0] : 0, command);
      return NULL;
    }

  body_len = payload[1] | (payload[2] << 8);
  if (body_len < 1)
    body_len = 1;
  body_len -= 1;                 /* decode_message_protocol length - 1 */
  if (body_len > plen - 3)
    body_len = plen - 3;

  *body_len_out = body_len;
  return g_memdup2 (payload + 3, body_len);
}

/* Send a command, read its ACK, then read its reply body. */
static guint8 *
gd_exec (FpiDeviceGoodix5xx *self, guint8 command,
         const guint8 *payload, gsize payload_len,
         gsize *reply_len, GError **error)
{
  if (!gd_send_cmd (self, command, payload, payload_len, error))
    return NULL;
  if (!gd_read_ack (self, command, error))
    return NULL;
  return gd_read_cmd_reply (self, command, reply_len, error);
}

/* Send a command and only read the ACK (no reply body expected). */
static gboolean
gd_exec_ack_only (FpiDeviceGoodix5xx *self, guint8 command,
                  const guint8 *payload, gsize payload_len, GError **error)
{
  if (!gd_send_cmd (self, command, payload, payload_len, error))
    return FALSE;
  return gd_read_ack (self, command, error);
}

/* ================================================================== *
 *  GnuTLS transport (host acts as PSK-TLS server, device is client)
 * ================================================================== */

static ssize_t
gd_tls_push (gnutls_transport_ptr_t ptr, const void *data, size_t len)
{
  FpiDeviceGoodix5xx *self = ptr;
  GError *error = NULL;

  if (!gd_send_pack (self, GD_FLAGS_TLS, data, len, &error))
    {
      fp_warn ("TLS push failed: %s", error->message);
      g_clear_error (&error);
      gnutls_transport_set_errno (self->tls_session, EIO);
      return -1;
    }
  return len;
}

static ssize_t
gd_tls_pull (gnutls_transport_ptr_t ptr, void *data, size_t len)
{
  FpiDeviceGoodix5xx *self = ptr;

  if (self->pull_pos >= self->pull_len)
    {
      GError *error = NULL;
      const guint8 *payload;
      guint8 flags;
      gsize plen;
      gsize skip;

      payload = gd_read_pack (self, GD_USB_TIMEOUT, &flags, &plen, &error);
      if (!payload)
        {
          g_clear_error (&error);
          gnutls_transport_set_errno (self->tls_session, EIO);
          return -1;
        }

      /* 0xb0 packs carry raw TLS records; 0xb2 image-data packs prefix the
       * TLS record with a 9-byte header (see driver_55x4.py "[9:]"). */
      skip = (flags == GD_FLAGS_TLS_DATA) ? 9 : 0;
      if (plen < skip)
        plen = skip;

      g_free (self->pull_buf);
      self->pull_len = plen - skip;
      self->pull_buf = g_memdup2 (payload + skip, self->pull_len);
      self->pull_pos = 0;
    }

  {
    gsize avail = self->pull_len - self->pull_pos;
    gsize n = MIN (avail, len);
    memcpy (data, self->pull_buf + self->pull_pos, n);
    self->pull_pos += n;
    return n;
  }
}

static int
gd_tls_psk_creds_cb (gnutls_session_t session, const char *username,
                     gnutls_datum_t *key)
{
  key->data = gnutls_malloc (sizeof (GD_PSK));
  if (!key->data)
    return -1;
  memcpy (key->data, GD_PSK, sizeof (GD_PSK));
  key->size = sizeof (GD_PSK);
  return 0;
}

static gboolean
gd_tls_handshake (FpiDeviceGoodix5xx *self, GError **error)
{
  int ret;

  gnutls_psk_allocate_server_credentials (&self->tls_creds);
  gnutls_psk_set_server_credentials_function (self->tls_creds,
                                              gd_tls_psk_creds_cb);

  gnutls_init (&self->tls_session, GNUTLS_SERVER);
  gnutls_priority_set_direct (self->tls_session,
                              "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.2:"
                              "-KX-ALL:+PSK:+ECDHE-PSK:+DHE-PSK", NULL);
  gnutls_credentials_set (self->tls_session, GNUTLS_CRD_PSK, self->tls_creds);

  gnutls_transport_set_ptr (self->tls_session, self);
  gnutls_transport_set_push_function (self->tls_session, gd_tls_push);
  gnutls_transport_set_pull_function (self->tls_session, gd_tls_pull);

  /* Tell the device to start its TLS client handshake. */
  if (!gd_exec_ack_only (self, GD_CMD_REQUEST_TLS_CONNECTION,
                         (const guint8 *) "\x00\x00", 2, error))
    return FALSE;

  do
    ret = gnutls_handshake (self->tls_session);
  while (ret < 0 && gnutls_error_is_fatal (ret) == 0);

  if (ret < 0)
    {
      g_set_error (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                   "TLS handshake failed: %s", gnutls_strerror (ret));
      return FALSE;
    }

  self->tls_ready = TRUE;
  return TRUE;
}

static void
gd_tls_deinit (FpiDeviceGoodix5xx *self)
{
  if (self->tls_ready)
    {
      gnutls_bye (self->tls_session, GNUTLS_SHUT_RDWR);
      self->tls_ready = FALSE;
    }
  if (self->tls_session)
    {
      gnutls_deinit (self->tls_session);
      self->tls_session = NULL;
    }
  if (self->tls_creds)
    {
      gnutls_psk_free_server_credentials (self->tls_creds);
      self->tls_creds = NULL;
    }
  g_clear_pointer (&self->pull_buf, g_free);
  self->pull_len = self->pull_pos = 0;
}

/* ================================================================== *
 *  High level protocol operations (driver_55x4.py)
 * ================================================================== */

static gboolean
gd_op_nop (FpiDeviceGoodix5xx *self, GError **error)
{
  /* NOP has no checksum and returns nothing meaningful; just fire it. */
  g_autofree guint8 *proto = NULL;
  gsize proto_len;
  static const guint8 body[4] = { 0 };

  /* encode_message_protocol(..., checksum=False) => trailing 0x88 */
  proto = gd_build_protocol (GD_CMD_NOP, body, sizeof (body), &proto_len);
  proto[proto_len - 1] = 0x88;
  return gd_send_pack (self, GD_FLAGS_MESSAGE_PROTOCOL, proto, proto_len, error);
}

static gboolean
gd_op_firmware_version (FpiDeviceGoodix5xx *self, gchar **version, GError **error)
{
  g_autofree guint8 *reply = NULL;
  gsize len = 0;

  reply = gd_exec (self, GD_CMD_FIRMWARE_VERSION,
                   (const guint8 *) "\x00\x00", 2, &len, error);
  if (!reply)
    return FALSE;

  *version = g_strndup ((const gchar *) reply, len);
  return TRUE;
}

static gboolean
gd_op_check_psk (FpiDeviceGoodix5xx *self, gboolean *valid, GError **error)
{
  g_autofree guint8 *reply = NULL;
  gsize len = 0;
  guint8 payload[8];
  guint32 psk_len, flags;

  /* preset_psk_read(0xbb020007): le32(flags) + le32(0) */
  payload[0] = 0x07; payload[1] = 0x00; payload[2] = 0x02; payload[3] = 0xbb;
  payload[4] = payload[5] = payload[6] = payload[7] = 0x00;

  reply = gd_exec (self, GD_CMD_PRESET_PSK_READ, payload, sizeof (payload),
                   &len, error);
  if (!reply)
    return FALSE;

  *valid = FALSE;
  if (len < 9 || reply[0] != 0x00)
    return TRUE;

  /* check_psk() also requires the returned flags to match. */
  flags = reply[1] | (reply[2] << 8) | (reply[3] << 16) | (reply[4] << 24);
  if (flags != 0xbb020007)
    return TRUE;

  psk_len = reply[5] | (reply[6] << 8) | (reply[7] << 16) | (reply[8] << 24);
  if (len < 9 + psk_len)
    return TRUE;

  if (psk_len == sizeof (GD_PMK_HASH) &&
      memcmp (reply + 9, GD_PMK_HASH, sizeof (GD_PMK_HASH)) == 0)
    *valid = TRUE;

  return TRUE;
}

static gboolean
gd_op_write_psk (FpiDeviceGoodix5xx *self, GError **error)
{
  g_autofree guint8 *reply = NULL;
  g_autofree guint8 *payload = NULL;
  gsize len = 0;
  guint32 flags = 0xbb010003;
  guint32 wblen = sizeof (GD_PSK_WHITE_BOX);
  gsize plen = 8 + wblen;

  /* preset_psk_write(0xbb010003, PSK_WHITE_BOX):
   *   le32(flags) + le32(len) + payload */
  payload = g_malloc0 (plen);
  memcpy (payload + 0, &flags, 4);       /* host is LE */
  memcpy (payload + 4, &wblen, 4);
  memcpy (payload + 8, GD_PSK_WHITE_BOX, wblen);

  reply = gd_exec (self, GD_CMD_PRESET_PSK_WRITE, payload, plen, &len, error);
  if (!reply)
    return FALSE;

  if (len < 1 || reply[0] != 0x00)
    {
      g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                           "Failed to write PSK");
      return FALSE;
    }
  return TRUE;
}

static gboolean
gd_op_reset (FpiDeviceGoodix5xx *self, GError **error)
{
  g_autofree guint8 *reply = NULL;
  gsize len = 0;
  /* reset(reset_sensor=True, soft_reset_mcu=False, sleep_time=20) */
  const guint8 payload[2] = { 0x05, 20 };

  reply = gd_exec (self, GD_CMD_RESET, payload, sizeof (payload), &len, error);
  if (!reply)
    return FALSE;
  if (len < 1 || reply[0] != 0x01)
    {
      g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                           "Reset failed");
      return FALSE;
    }
  return TRUE;
}

static gboolean
gd_op_read_sensor_register (FpiDeviceGoodix5xx *self, guint16 addr,
                            guint8 length, GError **error)
{
  g_autofree guint8 *reply = NULL;
  gsize len = 0;
  guint8 payload[4] = { 0x00, addr & 0xff, (addr >> 8) & 0xff, length };

  reply = gd_exec (self, GD_CMD_READ_SENSOR_REGISTER, payload,
                   sizeof (payload), &len, error);
  return reply != NULL;
}

static gboolean
gd_op_read_otp (FpiDeviceGoodix5xx *self, GError **error)
{
  g_autofree guint8 *reply = NULL;
  gsize len = 0;

  reply = gd_exec (self, GD_CMD_READ_OTP, (const guint8 *) "\x00\x00", 2,
                   &len, error);
  return reply != NULL;
}

static gboolean
gd_op_upload_config (FpiDeviceGoodix5xx *self, GError **error)
{
  g_autofree guint8 *reply = NULL;
  gsize len = 0;

  reply = gd_exec (self, GD_CMD_UPLOAD_CONFIG_MCU, GD_DEVICE_CONFIG,
                   sizeof (GD_DEVICE_CONFIG), &len, error);
  if (!reply)
    return FALSE;
  if (len < 1 || reply[0] != 0x01)
    {
      g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                           "Failed to upload config");
      return FALSE;
    }
  return TRUE;
}

static gboolean
gd_op_fdt_mode (FpiDeviceGoodix5xx *self, GError **error)
{
  g_autofree guint8 *reply = NULL;
  gsize len = 0;

  reply = gd_exec (self, GD_CMD_MCU_SWITCH_TO_FDT_MODE, GD_FDT_MODE,
                   sizeof (GD_FDT_MODE), &len, error);
  return reply != NULL;
}

static gboolean
gd_op_fdt_down (FpiDeviceGoodix5xx *self, guint timeout, GError **error)
{
  const guint8 *payload;
  guint8 flags;
  gsize plen;

  if (!gd_send_cmd (self, GD_CMD_MCU_SWITCH_TO_FDT_DOWN, GD_FDT_DOWN,
                    sizeof (GD_FDT_DOWN), error))
    return FALSE;
  if (!gd_read_ack (self, GD_CMD_MCU_SWITCH_TO_FDT_DOWN, error))
    return FALSE;

  /* Reply only arrives once a finger is on the sensor. */
  payload = gd_read_pack (self, timeout, &flags, &plen, error);
  return payload != NULL;
}

static gboolean
gd_op_idle_mode (FpiDeviceGoodix5xx *self, guint8 sleep_time, GError **error)
{
  guint8 payload[2] = { sleep_time, 0x00 };

  return gd_exec_ack_only (self, GD_CMD_MCU_SWITCH_TO_IDLE_MODE,
                           payload, sizeof (payload), error);
}

static gboolean
gd_op_switch_to_sleep (FpiDeviceGoodix5xx *self, guint8 number, GError **error)
{
  g_autofree guint8 *reply = NULL;
  gsize len = 0;
  guint8 payload[1] = { number };

  reply = gd_exec (self, GD_CMD_SWITCH_TO_SLEEP_MODE, payload, 1, &len, error);
  if (!reply)
    return FALSE;
  if (len < 1 || reply[0] != 0x01)
    {
      g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                           "Failed to switch to sleep mode");
      return FALSE;
    }
  return TRUE;
}

/* mcu_get_image: trigger a capture and read the (TLS encrypted) image. */
static gboolean
gd_op_get_image (FpiDeviceGoodix5xx *self, guint8 *out, gsize out_len,
                 GError **error)
{
  gsize got = 0;

  if (!gd_send_cmd (self, GD_CMD_MCU_GET_IMAGE,
                    (const guint8 *) "\x01\x00", 2, error))
    return FALSE;
  if (!gd_read_ack (self, GD_CMD_MCU_GET_IMAGE, error))
    return FALSE;

  while (got < out_len)
    {
      int ret = gnutls_record_recv (self->tls_session, out + got, out_len - got);
      if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
        continue;
      if (ret < 0)
        {
          g_set_error (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                       "TLS image read failed: %s", gnutls_strerror (ret));
          return FALSE;
        }
      if (ret == 0)
        break;
      got += ret;
    }

  if (got < out_len)
    {
      g_set_error (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                   "Short image: got %zu of %zu bytes", got, out_len);
      return FALSE;
    }
  return TRUE;
}

/* tool.decode_image: 6 packed bytes -> 4 twelve-bit pixels. */
static FpImage *
gd_decode_image (const guint8 *data, gsize len)
{
  FpImage *img = fp_image_new (GD_IMAGE_WIDTH, GD_IMAGE_HEIGHT);
  guint npix = GD_IMAGE_WIDTH * GD_IMAGE_HEIGHT;
  guint8 *out = img->data;
  guint p = 0;
  gsize i;

  for (i = 0; i + 6 <= len && p + 4 <= npix; i += 6)
    {
      const guint8 *c = data + i;
      guint16 v0 = ((c[0] & 0x0f) << 8) | c[1];
      guint16 v1 = (c[3] << 4) | (c[0] >> 4);
      guint16 v2 = ((c[5] & 0x0f) << 8) | c[2];
      guint16 v3 = (c[4] << 4) | (c[5] >> 4);

      /* 12-bit -> 8-bit */
      out[p++] = v0 >> 4;
      out[p++] = v1 >> 4;
      out[p++] = v2 >> 4;
      out[p++] = v3 >> 4;
    }

  return img;
}

/* ================================================================== *
 *  Worker thread: full activation + capture
 * ================================================================== */

typedef struct
{
  FpiDeviceGoodix5xx *self;
  FpImage            *image;
  GError             *error;
  gboolean            finger_present;
} GdMainCall;

static gboolean
gd_idle_finger_on (gpointer data)
{
  GdMainCall *c = data;
  fpi_image_device_report_finger_status (FP_IMAGE_DEVICE (c->self), TRUE);
  g_object_unref (c->self);
  g_free (c);
  return G_SOURCE_REMOVE;
}

static gboolean
gd_idle_finger_off (gpointer data)
{
  GdMainCall *c = data;
  fpi_image_device_report_finger_status (FP_IMAGE_DEVICE (c->self), FALSE);
  g_object_unref (c->self);
  g_free (c);
  return G_SOURCE_REMOVE;
}

static gboolean
gd_idle_image (gpointer data)
{
  GdMainCall *c = data;
  fpi_image_device_image_captured (FP_IMAGE_DEVICE (c->self), c->image);
  g_object_unref (c->self);
  g_free (c);
  return G_SOURCE_REMOVE;
}

static gboolean
gd_idle_activate_done (gpointer data)
{
  GdMainCall *c = data;
  fpi_image_device_activate_complete (FP_IMAGE_DEVICE (c->self), NULL);
  g_object_unref (c->self);
  g_free (c);
  return G_SOURCE_REMOVE;
}

static gboolean
gd_idle_deactivate_done (gpointer data)
{
  GdMainCall *c = data;
  fpi_image_device_deactivate_complete (FP_IMAGE_DEVICE (c->self), NULL);
  g_object_unref (c->self);
  g_free (c);
  return G_SOURCE_REMOVE;
}

static gboolean
gd_idle_session_error (gpointer data)
{
  GdMainCall *c = data;
  fpi_image_device_session_error (FP_IMAGE_DEVICE (c->self), c->error);
  g_object_unref (c->self);
  g_free (c);
  return G_SOURCE_REMOVE;
}

static void
gd_marshal (FpiDeviceGoodix5xx *self, GSourceFunc fn, FpImage *img, GError *err)
{
  GdMainCall *c = g_new0 (GdMainCall, 1);

  c->self = g_object_ref (self);
  c->image = img;
  c->error = err;
  g_idle_add (fn, c);
}

static gboolean
gd_full_init (FpiDeviceGoodix5xx *self, GError **error)
{
  g_autofree gchar *fw = NULL;
  gboolean psk_valid = FALSE;

  /* goodix.py Device.__init__: empty_buffer() then nop().  Drain again after
   * the NOP so its (checksum-less) ACK cannot desync the next exchange. */
  gd_drain (self);
  gd_op_nop (self, NULL);
  gd_drain (self);

  if (!gd_op_firmware_version (self, &fw, error))
    return FALSE;
  fp_dbg ("Firmware: %s", fw);

  /* We only drive the application firmware; flashing/IAP is handled by the
   * goodix-fp-dump python tool. */
  if (!g_str_has_prefix (fw, "GF32") || !strstr (fw, "_RTSEC_APP_"))
    {
      g_set_error (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_NOT_SUPPORTED,
                   "Unexpected firmware '%s'; flash the app firmware with "
                   "goodix-fp-dump first", fw);
      return FALSE;
    }

  if (!gd_op_check_psk (self, &psk_valid, error))
    return FALSE;
  if (!psk_valid)
    {
      fp_dbg ("PSK not set, writing whitebox PSK");
      if (!gd_op_write_psk (self, error))
        return FALSE;
      /* write_psk() re-reads the PSK to confirm it took. */
      if (!gd_op_check_psk (self, &psk_valid, error))
        return FALSE;
      if (!psk_valid)
        {
          g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_IO,
                               "PSK verification failed after write");
          return FALSE;
        }
    }

  fp_dbg ("init: reset");
  if (!gd_op_reset (self, error))
    return FALSE;
  fp_dbg ("init: read chip id");
  if (!gd_op_read_sensor_register (self, 0x0000, 4, error))   /* chip id */
    return FALSE;
  fp_dbg ("init: read otp");
  if (!gd_op_read_otp (self, error))
    return FALSE;

  fp_dbg ("init: TLS handshake");
  if (!gd_tls_handshake (self, error))
    return FALSE;

  fp_dbg ("init: upload config");
  if (!gd_op_upload_config (self, error))
    return FALSE;

  /* Sensor calibration, matching driver_55x4.py run_driver(): capture and
   * discard two background ("clear") frames, then put the MCU to sleep so the
   * finger-down detector can arm. */
  {
    g_autofree guint8 *clear = g_malloc0 (GD_IMAGE_BYTES);

    fp_dbg ("init: fdt mode + clear frame 0");
    if (!gd_op_fdt_mode (self, error))
      return FALSE;
    if (!gd_op_get_image (self, clear, GD_IMAGE_BYTES, error))     /* clear-0 */
      return FALSE;

    fp_dbg ("init: fdt mode + idle + clear frame 1");
    if (!gd_op_fdt_mode (self, error))
      return FALSE;
    if (!gd_op_idle_mode (self, 20, error))
      return FALSE;
    if (!gd_op_read_sensor_register (self, 0x0082, 2, error))
      return FALSE;
    if (!gd_op_get_image (self, clear, GD_IMAGE_BYTES, error))     /* clear-1 */
      return FALSE;

    fp_dbg ("init: fdt mode + sleep");
    if (!gd_op_fdt_mode (self, error))
      return FALSE;
    if (!gd_op_switch_to_sleep (self, 0x6c, error))
      return FALSE;
  }
  fp_dbg ("init: done");

  return TRUE;
}

static gpointer
gd_worker (gpointer data)
{
  FpiDeviceGoodix5xx *self = data;
  GError *error = NULL;
  guint8 *raw = NULL;
  FpImage *img = NULL;

  if (!gd_full_init (self, &error))
    goto err;

  gd_marshal (self, gd_idle_activate_done, NULL, NULL);

  /* Capture loop: one image per finger press. */
  while (!g_atomic_int_get (&self->deactivating))
    {
      /* Wait for a finger, polling so we can honour deactivation. */
      gboolean finger = FALSE;
      while (!g_atomic_int_get (&self->deactivating))
        {
          GError *poll_err = NULL;
          if (gd_op_fdt_down (self, GD_FINGER_POLL_TIMEOUT, &poll_err))
            {
              finger = TRUE;
              break;
            }
          if (poll_err &&
              !g_error_matches (poll_err, G_USB_DEVICE_ERROR,
                                G_USB_DEVICE_ERROR_TIMED_OUT))
            {
              error = poll_err;
              goto err;
            }
          g_clear_error (&poll_err);
        }

      if (!finger)
        break;

      gd_marshal (self, gd_idle_finger_on, NULL, NULL);

      raw = g_malloc0 (GD_IMAGE_BYTES);
      if (!gd_op_get_image (self, raw, GD_IMAGE_BYTES, &error))
        {
          g_free (raw);
          goto err;
        }

      /* Drop the trailing 4 bytes (tool.decode_image uses data[:-4]). */
      img = gd_decode_image (raw, GD_IMAGE_BYTES - 4);
      g_free (raw);
      raw = NULL;

      gd_marshal (self, gd_idle_image, img, NULL);
      img = NULL;

      gd_marshal (self, gd_idle_finger_off, NULL, NULL);

      /* One capture per activation; the image device re-activates as needed. */
      break;
    }

  gd_marshal (self, gd_idle_deactivate_done, NULL, NULL);
  return NULL;

err:
  /* libfprint discards (and replaces with a generic message) any error that
   * is not in the FpDeviceError / FpDeviceRetry domain, which would hide the
   * real cause.  Re-wrap while preserving the original message. */
  if (error &&
      error->domain != FP_DEVICE_ERROR && error->domain != FP_DEVICE_RETRY)
    {
      GError *wrapped = fpi_device_error_new_msg (FP_DEVICE_ERROR_PROTO,
                                                  "%s", error->message);
      g_error_free (error);
      error = wrapped;
    }
  gd_marshal (self, gd_idle_session_error, NULL, error);
  return NULL;
}

/* ================================================================== *
 *  FpImageDevice vfuncs
 * ================================================================== */

/* Find the interface exposing bulk IN/OUT endpoints, as protocol.py does at
 * connect time.  We prefer a data/vendor-class interface but fall back to any
 * interface that has a bulk IN + bulk OUT pair.  Everything enumerated is
 * logged so a mismatch is diagnosable from the debug output. */
static gboolean
gd_discover_endpoints (FpiDeviceGoodix5xx *self, GError **error)
{
  g_autoptr(GPtrArray) ifaces = NULL;
  guint i, j;
  gboolean found = FALSE;
  gboolean found_preferred = FALSE;

  ifaces = g_usb_device_get_interfaces (self->usb, error);
  if (!ifaces)
    return FALSE;

  fp_dbg ("Enumerating %u interface(s)", ifaces->len);

  for (i = 0; i < ifaces->len; i++)
    {
      GUsbInterface *iface = g_ptr_array_index (ifaces, i);
      guint8 cls = g_usb_interface_get_class (iface);
      guint8 num = g_usb_interface_get_number (iface);
      gboolean preferred = (cls == GD_USB_CLASS_CDC_DATA ||
                            cls == GD_USB_CLASS_VENDOR);
      GPtrArray *eps;
      guint8 ep_in = 0, ep_out = 0;

      eps = g_usb_interface_get_endpoints (iface);
      fp_dbg ("  iface #%u class 0x%02x subclass 0x%02x proto 0x%02x, %u endpoint(s)",
              num, cls, g_usb_interface_get_subclass (iface),
              g_usb_interface_get_protocol (iface), eps ? eps->len : 0);

      for (j = 0; eps && j < eps->len; j++)
        {
          GUsbEndpoint *ep = g_ptr_array_index (eps, j);
          guint8 addr = g_usb_endpoint_get_address (ep);
          gboolean is_in = (g_usb_endpoint_get_direction (ep) ==
                            G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST);

          fp_dbg ("    ep 0x%02x dir %s", addr, is_in ? "IN" : "OUT");

          /* gusb does not expose the transfer type (get_kind returns the
           * descriptor type, always 0x05), so we select purely by direction;
           * the goodix data interface carries exactly one bulk IN and one
           * bulk OUT endpoint. */
          if (is_in)
            ep_in = addr;
          else
            ep_out = addr;
        }

      if (!ep_in || !ep_out)
        continue;

      /* Take the first match; upgrade to a preferred-class one if we find it. */
      if (!found || (preferred && !found_preferred))
        {
          self->iface = num;
          self->ep_in = ep_in;
          self->ep_out = ep_out;
          found = TRUE;
          found_preferred = preferred;
        }
    }

  if (!found)
    {
      g_set_error_literal (error, G_USB_DEVICE_ERROR,
                           G_USB_DEVICE_ERROR_NO_DEVICE,
                           "No interface with bulk IN+OUT endpoints found");
      return FALSE;
    }

  fp_dbg ("Using interface %u, bulk IN 0x%02x, bulk OUT 0x%02x",
          self->iface, self->ep_in, self->ep_out);
  return TRUE;
}

static void
dev_open (FpImageDevice *dev)
{
  FpiDeviceGoodix5xx *self = FPI_DEVICE_GOODIX5XX (dev);
  GError *error = NULL;

  self->usb = fpi_device_get_usb_device (FP_DEVICE (dev));
  self->read_buf = g_malloc0 (GD_READ_BUF_SIZE);

  if (!gd_discover_endpoints (self, &error))
    {
      fpi_image_device_open_complete (dev, error);
      return;
    }

  if (!g_usb_device_claim_interface (self->usb, self->iface, 0, &error))
    {
      fpi_image_device_open_complete (dev, error);
      return;
    }

  fpi_image_device_open_complete (dev, NULL);
}

static void
dev_close (FpImageDevice *dev)
{
  FpiDeviceGoodix5xx *self = FPI_DEVICE_GOODIX5XX (dev);
  GError *error = NULL;

  gd_tls_deinit (self);
  g_clear_pointer (&self->read_buf, g_free);

  g_usb_device_release_interface (self->usb, self->iface, 0, &error);

  fpi_image_device_close_complete (dev, error);
}

static void
dev_activate (FpImageDevice *dev)
{
  FpiDeviceGoodix5xx *self = FPI_DEVICE_GOODIX5XX (dev);

  g_atomic_int_set (&self->deactivating, FALSE);
  self->worker = g_thread_new ("goodix5xx-worker", gd_worker, self);
}

static void
dev_deactivate (FpImageDevice *dev)
{
  FpiDeviceGoodix5xx *self = FPI_DEVICE_GOODIX5XX (dev);

  g_atomic_int_set (&self->deactivating, TRUE);
  if (self->worker)
    {
      g_thread_join (self->worker);
      self->worker = NULL;
    }
}

static const FpIdEntry id_table[] = {
  { .vid = 0x27c6, .pid = 0x55a4, },
  { .vid = 0, .pid = 0, .driver_data = 0 },
};

static void
fpi_device_goodix5xx_init (FpiDeviceGoodix5xx *self)
{
}

static void
fpi_device_goodix5xx_class_init (FpiDeviceGoodix5xxClass *klass)
{
  FpDeviceClass *dev_class = FP_DEVICE_CLASS (klass);
  FpImageDeviceClass *img_class = FP_IMAGE_DEVICE_CLASS (klass);

  dev_class->id = "goodix5xx";
  dev_class->full_name = "Goodix 55x4 Fingerprint Sensor";
  dev_class->type = FP_DEVICE_TYPE_USB;
  dev_class->id_table = id_table;
  dev_class->scan_type = FP_SCAN_TYPE_PRESS;

  img_class->img_open = dev_open;
  img_class->img_close = dev_close;
  img_class->activate = dev_activate;
  img_class->deactivate = dev_deactivate;

  img_class->img_width = GD_IMAGE_WIDTH;
  img_class->img_height = GD_IMAGE_HEIGHT;
  img_class->bz3_threshold = 24;
}
