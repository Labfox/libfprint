/*
 * Goodix 55x4 (TLS image sensor) driver for libfprint
 *
 * Copyright (C) 2026 Labfox
 *
 * Protocol ported faithfully from the goodix-fp-dump python reference
 * (goodix.py / driver_55x4.py). Everything that could be read from that
 * reference is taken verbatim; the only pieces that are not expressible in
 * the python (which delegates TLS to an external `openssl s_server`) are the
 * in-process GnuTLS PSK session and the USB endpoint numbers.
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

#include "fpi-device.h"

G_DECLARE_FINAL_TYPE (FpiDeviceGoodix5xx, fpi_device_goodix5xx, FPI,
                      DEVICE_GOODIX5XX, FpDevice)

/* ------------------------------------------------------------------ *
 *  Message framing (goodix.py)
 * ------------------------------------------------------------------ */

/* encode_message_pack() flags */
#define GD_FLAGS_MESSAGE_PROTOCOL             0xa0
#define GD_FLAGS_TLS                          0xb0
#define GD_FLAGS_TLS_DATA                     0xb2

/* message-protocol command bytes (already fully encoded in goodix.py) */
#define GD_CMD_NOP                            0x00
#define GD_CMD_MCU_GET_IMAGE                  0x20
#define GD_CMD_MCU_SWITCH_TO_FDT_DOWN         0x32
#define GD_CMD_MCU_SWITCH_TO_FDT_UP           0x34
#define GD_CMD_MCU_SWITCH_TO_FDT_MODE         0x36
#define GD_CMD_MCU_SWITCH_TO_IDLE_MODE        0x70
#define GD_CMD_READ_SENSOR_REGISTER           0x82
#define GD_CMD_UPLOAD_CONFIG_MCU              0x90
#define GD_CMD_SWITCH_TO_SLEEP_MODE           0x92
#define GD_CMD_RESET                          0xa2
#define GD_CMD_READ_OTP                       0xa6
#define GD_CMD_FIRMWARE_VERSION               0xa8
#define GD_CMD_ACK                            0xb0
#define GD_CMD_REQUEST_TLS_CONNECTION         0xd0
#define GD_CMD_PRESET_PSK_WRITE               0xe0
#define GD_CMD_PRESET_PSK_READ                0xe4
