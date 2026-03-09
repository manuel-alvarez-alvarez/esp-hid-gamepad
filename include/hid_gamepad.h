#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {

#endif

/* ═══════════════════════════════════════════════════════════════════════
 *  Layout & Report Builder
 * ═══════════════════════════════════════════════════════════════════════ */

#define HID_GAMEPAD_MAX_AXES              8
#define HID_GAMEPAD_MAX_BUTTONS           32
#define HID_GAMEPAD_MAX_HATS              4
#define HID_GAMEPAD_MAX_HAT_POSITIONS     8
#define HID_GAMEPAD_MAX_SWITCHES          4
#define HID_GAMEPAD_MAX_SWITCH_POSITIONS  8

#define HID_GAMEPAD_MAX_REPORT_LENGTH 64

/* ── Layout definition types ───────────────────────────────────────── */

typedef struct {
    int32_t on; /* raw value >= on → pressed */
    int32_t off; /* raw value <= off → released */
} hid_gamepad_button_def_t;

typedef struct {
    uint8_t usage; /* HID usage ID (e.g. HID_USAGE_DESKTOP_X) */
    int32_t in_min; /* device raw minimum */
    int32_t in_max; /* device raw maximum */
} hid_gamepad_axis_def_t;

typedef struct {
    uint8_t count; /* number of direction positions */
    int32_t centered; /* raw value for centered/null */
    int32_t positions[HID_GAMEPAD_MAX_HAT_POSITIONS]; /* raw values: [0]=N, [1]=NE, … clockwise */
} hid_gamepad_hat_def_t;

typedef struct {
    uint8_t count; /* total positions including "no button" (position 0) */
    int32_t values[HID_GAMEPAD_MAX_SWITCH_POSITIONS]; /* [0]=no button, [1..count-1]=button N */
} hid_gamepad_switch_def_t;

typedef struct {
    uint8_t button_count; /* 0–32 */
    hid_gamepad_button_def_t buttons[HID_GAMEPAD_MAX_BUTTONS];
    uint8_t hat_count; /* 0–4 */
    hid_gamepad_hat_def_t hats[HID_GAMEPAD_MAX_HATS];
    uint8_t switch_count; /* 0–4 */
    hid_gamepad_switch_def_t switches[HID_GAMEPAD_MAX_SWITCHES];
    uint8_t axis_count; /* 0–8 */
    hid_gamepad_axis_def_t axes[HID_GAMEPAD_MAX_AXES];
} hid_gamepad_layout_t;

/* ── Layout construction ───────────────────────────────────────────── */

/** Add a button with per-button on/off thresholds (raw >= on → pressed, raw <= off → released). */
void hid_gamepad_layout_add_button(hid_gamepad_layout_t *layout,
                                   int32_t on, int32_t off);

/** Add a hat switch. positions[0]=N, [1]=NE, … clockwise. centered = raw value for null state. */
void hid_gamepad_layout_add_hat(hid_gamepad_layout_t *layout,
                                int32_t centered,
                                const int32_t *positions, uint8_t count);

/** Add a switch. values[0]=no button, values[1..count-1]=button N. Maps to HID buttons. */
void hid_gamepad_layout_add_switch(hid_gamepad_layout_t *layout,
                                    const int32_t *values, uint8_t count);

/** Add a 16-bit signed axis with device raw range [in_min, in_max] scaled to [-32767, 32767]. */
void hid_gamepad_layout_add_axis(hid_gamepad_layout_t *layout,
                                 uint8_t usage, int32_t in_min, int32_t in_max);

/* ── Report buffer ─────────────────────────────────────────────────── */

typedef struct {
    hid_gamepad_layout_t *layout;
    uint16_t size;
    uint8_t hat_offset;
    uint8_t axis_offset;
    uint8_t data[HID_GAMEPAD_MAX_REPORT_LENGTH];
} hid_gamepad_report_buf_t;

void hid_gamepad_report_init(hid_gamepad_report_buf_t *report, hid_gamepad_layout_t *layout);

/* ── Field setters (take raw device values) ────────────────────────── */

void hid_gamepad_report_set_button(hid_gamepad_report_buf_t *report,
                                   uint8_t index, int32_t raw_value);

void hid_gamepad_report_set_hat(hid_gamepad_report_buf_t *report,
                                uint8_t hat_index, int32_t raw_value);

void hid_gamepad_report_set_switch(hid_gamepad_report_buf_t *report,
                                    uint8_t switch_index, int32_t raw_value);

void hid_gamepad_report_set_axis(hid_gamepad_report_buf_t *report,
                                 uint8_t axis_index, int32_t raw_value);

/* ═══════════════════════════════════════════════════════════════════════
 *  ESP-IDF USB Driver
 * ═══════════════════════════════════════════════════════════════════════ */

#if defined(ESP_PLATFORM)
#include <esp_err.h>

typedef struct {
    uint16_t vid; /* USB vendor  ID  (0 = hash of manufacturer) */
    uint16_t pid; /* USB product ID  (0 = hash of product) */
    const char *manufacturer; /* Manufacturer string (default: "Manuel Alvarez Alvarez") */
    const char *product; /* Product string      (default: "HID Gamepad") */
    const char *serial; /* Serial string       (default: "000000") */
    const hid_gamepad_layout_t *layout; /* Required: HID report layout */
    int task_priority; /* 0 = default (5) */
    int task_core; /* -1 = no affinity */
    size_t task_stack_size; /* 0 = default (4096) */
    uint8_t poll_interval_ms; /* 0 = default (1 = 1000 Hz) */
} hid_gamepad_config_t;

#define HID_GAMEPAD_DEFAULT_CONFIG() {                      \
    .vid                    = 0,                            \
    .pid                    = 0,                            \
    .manufacturer           = "Manuel Alvarez Alvarez",     \
    .product                = "HID Gamepad",                \
    .serial                 = "000000",                     \
    .layout                 = NULL,                         \
    .task_priority          = 0,                            \
    .task_core              = -1,                           \
    .task_stack_size        = 0,                            \
    .poll_interval_ms       = 0,                            \
}

esp_err_t hid_gamepad_init(const hid_gamepad_config_t *config);

esp_err_t hid_gamepad_deinit(void);

bool hid_gamepad_is_mounted(void);

esp_err_t hid_gamepad_send_report(hid_gamepad_report_buf_t *report);

#endif /* ESP_PLATFORM */

#ifdef __cplusplus
}
#endif
