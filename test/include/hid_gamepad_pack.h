#pragma once

/*
 * Reference standard gamepad wire-format struct and descriptor.
 *
 * Layout (16 bytes):
 *   [0..3]   buttons  uint32_t  — bit 0 = button 1 … bit 31 = button 32
 *   [4..5]   x        int16_t   — left  stick X  (-32767 … 32767)
 *   [6..7]   y        int16_t   — left  stick Y
 *   [8..9]   z        int16_t   — right stick X
 *   [10..11] rz       int16_t   — right stick Y
 *   [12..13] rx       int16_t   — left  trigger
 *   [14..15] ry       int16_t   — right trigger
 */

#include <stdint.h>
#include <stddef.h>
#include "hid_gamepad.h"

/* Wire-format packed struct — must match the descriptor byte-for-byte */
typedef struct __attribute__((packed)) {
    uint32_t buttons;
    int16_t  x;
    int16_t  y;
    int16_t  z;
    int16_t  rz;
    int16_t  rx;
    int16_t  ry;
} ref_gamepad_wire_t;
