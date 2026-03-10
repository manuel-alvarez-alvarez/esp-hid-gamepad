#include "hid_gamepad.h"

#include <string.h>

/* byte helpers */
#define LO8(v) ((uint8_t)((uint16_t)(v) & 0xFF))
#define HI8(v) ((uint8_t)(((uint16_t)(v) >> 8) & 0xFF))

/* HID axis logical range (16-bit signed) */
#define HID_AXIS_MIN  (-32767)
#define HID_AXIS_MAX  ( 32767)
#define HID_AXIS_RANGE (HID_AXIS_MAX - HID_AXIS_MIN) /* 65534 */
#define HID_AXIS_SCALE_SHIFT 16

/* Descriptor section sizes (bytes) */
#define DESC_HEADER_SIZE    6   /* Usage Page + Usage + Collection */
#define DESC_BUTTONS_SIZE  16   /* Button usage page + min/max/size/count + input */
#define DESC_PADDING_SIZE   6   /* report Size + Report Count + Input (Const) */
#define DESC_HAT_HDR_SIZE   2   /* Usage Page (Generic Desktop) */
#define DESC_HAT_EACH_SIZE 19   /* Per-hat: usage + log/phys min/max + unit + size/count + input */
#define DESC_HAT_FOOTER_SIZE 2  /* Unit (None) reset after hats */
#define DESC_AXIS_HDR_SIZE  2   /* Usage Page (Generic Desktop) */
#define DESC_AXIS_USAGE_SIZE 2  /* Per-axis usage entry */
#define DESC_AXIS_TAIL_SIZE 12  /* Logical min/max + report size/count + input */
#define DESC_END_SIZE       1   /* End Collection */

/* ═══════════════════════════════════════════════════════════════════════
 *  Helper functions
 * ═══════════════════════════════════════════════════════════════════════ */

static size_t emit(uint8_t *buf, size_t pos, size_t cap,
                   const uint8_t *src, size_t len) {
    if (buf && pos + len <= cap)
        memcpy(buf + pos, src, len);
    return len;
}

static uint16_t fnv1a_16(const char *str) {
    uint32_t h = 0x811c9dc5;
    for (; *str; str++) {
        h ^= (uint8_t) *str;
        h *= 0x01000193;
    }
    /* XOR-fold 32-bit to 16-bit */
    return (uint16_t) ((h >> 16) ^ (h & 0xFFFF));
}

static bool axis_compute_scale(hid_gamepad_axis_def_t *axis) {
    if (!axis) {
        return false;
    }
    int64_t range = (int64_t) axis->in_max - axis->in_min;
    if (range <= 0) {
        axis->scale_mult = 0;
        return false;
    }
    axis->scale_mult = (uint32_t) (((uint64_t) HID_AXIS_RANGE << HID_AXIS_SCALE_SHIFT) / (uint64_t) range);
    return axis->scale_mult != 0;
}

static int16_t scale_axis(const hid_gamepad_axis_def_t *axis, int32_t val) {
    int32_t clamped = val;
    if (clamped < axis->in_min)
        clamped = axis->in_min;
    else if (clamped > axis->in_max)
        clamped = axis->in_max;

    if (clamped <= axis->in_min)
        return HID_AXIS_MIN;
    if (clamped >= axis->in_max)
        return HID_AXIS_MAX;

    if (axis->scale_mult != 0) {
        int32_t delta = clamped - axis->in_min;
        int32_t scaled = (int32_t) (((int64_t) delta * (int64_t) axis->scale_mult) >> HID_AXIS_SCALE_SHIFT);
        return (int16_t) (scaled + HID_AXIS_MIN);
    }

    int32_t range = axis->in_max - axis->in_min;
    if (range <= 0) {
        return HID_AXIS_MIN;
    }
    return (int16_t) (((int64_t) (clamped - axis->in_min) * HID_AXIS_RANGE) / range + HID_AXIS_MIN);
}

static uint8_t next_bit(const hid_gamepad_layout_t *layout) {
    uint8_t bit = 0;
    for (uint8_t i = 0; i < layout->button_count; i++) {
        uint8_t end = layout->buttons[i].bit_offset + 1;
        if (end > bit) bit = end;
    }
    for (uint8_t i = 0; i < layout->switch_count; i++) {
        uint8_t end = layout->switches[i].button_offset + layout->switches[i].count - 1;
        if (end > bit) bit = end;
    }
    return bit;
}

static uint8_t total_button_count(const hid_gamepad_layout_t *layout) {
    return next_bit(layout);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Descriptor builder
 * ═══════════════════════════════════════════════════════════════════════ */

static size_t hid_gamepad_desc_size(const hid_gamepad_layout_t *layout) {
    size_t size = DESC_HEADER_SIZE;
    uint8_t btn_total = total_button_count(layout);
    if (btn_total > 0) {
        size += DESC_BUTTONS_SIZE;
        uint8_t padding = (8 - (btn_total % 8)) % 8;
        if (padding > 0)
            size += DESC_PADDING_SIZE;
    }
    if (layout->hat_count > 0) {
        size += DESC_HAT_HDR_SIZE
                + (size_t) layout->hat_count * DESC_HAT_EACH_SIZE
                + DESC_HAT_FOOTER_SIZE;
        if (layout->hat_count % 2 != 0)
            size += DESC_PADDING_SIZE;
    }
    if (layout->axis_count > 0)
        size += DESC_AXIS_HDR_SIZE
                + (size_t) layout->axis_count * DESC_AXIS_USAGE_SIZE
                + DESC_AXIS_TAIL_SIZE;
    size += DESC_END_SIZE;
    return size;
}

static size_t hid_gamepad_desc_build(const hid_gamepad_layout_t *layout,
                                     uint8_t *buf, size_t buf_size) {
    size_t need = hid_gamepad_desc_size(layout);
    if (buf_size < need)
        return 0;

    size_t p = 0;

    /* Header */
    {
        const uint8_t hdr[] = {
            0x05, 0x01, /* Usage Page (Generic Desktop) */
            0x09, 0x05, /* Usage (Gamepad) */
            0xA1, 0x01, /* Collection (Application) */
        };
        p += emit(buf, p, buf_size, hdr, sizeof(hdr));
    }

    /* Buttons (explicit + switch-derived) */
    uint8_t btn_total = total_button_count(layout);
    if (btn_total > 0) {
        const uint8_t btn[] = {
            0x05, 0x09, /* Usage Page (Button) */
            0x19, 0x01, /* Usage Minimum (1) */
            0x29, btn_total, /* Usage Maximum */
            0x15, 0x00, /* Logical Minimum (0) */
            0x25, 0x01, /* Logical Maximum (1) */
            0x75, 0x01, /* Report Size (1) */
            0x95, btn_total, /* Report Count */
            0x81, 0x02, /* Input (Data,Var,Abs) */
        };
        p += emit(buf, p, buf_size, btn, sizeof(btn));

        /* Pad to byte boundary */
        uint8_t padding = (8 - (btn_total % 8)) % 8;
        if (padding > 0) {
            const uint8_t pad[] = {
                0x75, padding, /* Report Size (padding bits) */
                0x95, 0x01, /* Report Count (1) */
                0x81, 0x01, /* Input (Const) */
            };
            p += emit(buf, p, buf_size, pad, sizeof(pad));
        }
    }

    /* Hats */
    if (layout->hat_count > 0) {
        const uint8_t hat_hdr[] = {0x05, 0x01}; /* Usage Page (Generic Desktop) */
        p += emit(buf, p, buf_size, hat_hdr, sizeof(hat_hdr));

        for (uint8_t h = 0; h < layout->hat_count; h++) {
            uint8_t count = layout->hats[h].count;
            uint16_t phys_max = (count - 1) * 360 / count;
            const uint8_t hat[] = {
                0x09, 0x39, /* Usage (Hat Switch) */
                0x15, 0x00, /* Logical Minimum (0) */
                0x25, (uint8_t) (count - 1), /* Logical Maximum */
                0x35, 0x00, /* Physical Minimum (0) */
                0x46, LO8(phys_max), HI8(phys_max), /* Physical Maximum (degrees) */
                0x65, 0x14, /* Unit (Eng Rotation: Degrees) */
                0x75, 0x04, /* Report Size (4) */
                0x95, 0x01, /* Report Count (1) */
                0x81, 0x42, /* Input (Data,Var,Abs,Null) */
            };
            p += emit(buf, p, buf_size, hat, sizeof(hat));
        }

        /* Reset unit */
        const uint8_t unit_reset[] = {0x65, 0x00}; /* Unit (None) */
        p += emit(buf, p, buf_size, unit_reset, sizeof(unit_reset));

        /* Pad to byte boundary if odd number of hats (each is 4 bits) */
        if (layout->hat_count % 2 != 0) {
            const uint8_t pad[] = {
                0x75, 0x04, /* Report Size (4) */
                0x95, 0x01, /* Report Count (1) */
                0x81, 0x01, /* Input (Const) */
            };
            p += emit(buf, p, buf_size, pad, sizeof(pad));
        }
    }

    /* Axes */
    if (layout->axis_count > 0) {
        const uint8_t ax_hdr[] = {0x05, 0x01};
        p += emit(buf, p, buf_size, ax_hdr, sizeof(ax_hdr));

        for (uint8_t a = 0; a < layout->axis_count; a++) {
            const uint8_t u[] = {0x09, layout->axes[a].usage};
            p += emit(buf, p, buf_size, u, sizeof(u));
        }

        const uint8_t ax_tail[] = {
            0x16, LO8(HID_AXIS_MIN), HI8(HID_AXIS_MIN), /* Logical Minimum */
            0x26, LO8(HID_AXIS_MAX), HI8(HID_AXIS_MAX), /* Logical Maximum */
            0x75, 0x10, /* Report Size (16) */
            0x95, layout->axis_count, /* Report Count */
            0x81, 0x02, /* Input (Data,Var,Abs) */
        };
        p += emit(buf, p, buf_size, ax_tail, sizeof(ax_tail));
    }

    /* End Collection */
    if (buf && p < buf_size)
        buf[p] = 0xC0;
    p++;

    return p;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Layout
 * ═══════════════════════════════════════════════════════════════════════ */

bool hid_gamepad_layout_add_button(hid_gamepad_layout_t *layout,
                                   int32_t on, int32_t off) {
    if (!layout || layout->button_count >= HID_GAMEPAD_MAX_BUTTONS) {
        return false;
    }
    layout->buttons[layout->button_count].on = on;
    layout->buttons[layout->button_count].off = off;
    layout->buttons[layout->button_count].bit_offset = next_bit(layout);
    layout->button_count++;
    return true;
}

bool hid_gamepad_layout_add_hat(hid_gamepad_layout_t *layout,
                                int32_t centered,
                                const int32_t *positions, uint8_t count) {
    if (!layout || !positions)
        return false;
    if (count == 0 || count > HID_GAMEPAD_MAX_HAT_POSITIONS)
        return false;
    if (layout->hat_count >= HID_GAMEPAD_MAX_HATS)
        return false;

    hid_gamepad_hat_def_t *hat = &layout->hats[layout->hat_count];
    hat->count = count;
    hat->centered = centered;
    memcpy(hat->positions, positions, count * sizeof(int32_t));
    layout->hat_count++;
    return true;
}

bool hid_gamepad_layout_add_switch(hid_gamepad_layout_t *layout,
                                   const int32_t *values, uint8_t count) {
    if (!layout || !values)
        return false;
    if (count == 0 || count > HID_GAMEPAD_MAX_SWITCH_POSITIONS)
        return false;
    if (layout->switch_count >= HID_GAMEPAD_MAX_SWITCHES)
        return false;

    hid_gamepad_switch_def_t *sw = &layout->switches[layout->switch_count];
    sw->count = count;
    memcpy(sw->values, values, count * sizeof(int32_t));
    sw->button_offset = next_bit(layout);
    layout->switch_count++;
    return true;
}

bool hid_gamepad_layout_add_axis(hid_gamepad_layout_t *layout,
                                 uint8_t usage, int32_t in_min, int32_t in_max) {
    if (!layout)
        return false;
    if (layout->axis_count >= HID_GAMEPAD_MAX_AXES)
        return false;
    if (in_max <= in_min)
        return false;

    hid_gamepad_axis_def_t *axis = &layout->axes[layout->axis_count];
    axis->usage = usage;
    axis->in_min = in_min;
    axis->in_max = in_max;
    axis_compute_scale(axis);
    layout->axis_count++;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Report
 * ═══════════════════════════════════════════════════════════════════════ */

void hid_gamepad_report_init(hid_gamepad_report_buf_t *report, hid_gamepad_layout_t *layout) {
    memset(report, 0, sizeof(*report));
    report->layout = layout;
    uint8_t btn_bytes = (total_button_count(layout) + 7) / 8;
    uint8_t hat_bytes = (layout->hat_count + 1) / 2; /* 4 bits each, rounded up */
    report->hat_offset = btn_bytes;
    report->axis_offset = btn_bytes + hat_bytes;
    report->size = report->axis_offset + (uint16_t) layout->axis_count * 2;

    for (uint8_t i = 0; i < layout->axis_count; i++) {
        if (layout->axes[i].scale_mult == 0) {
            axis_compute_scale(&layout->axes[i]);
        }
    }

    /* Initialize hats to null/centered (count value = outside logical range) */
    for (uint8_t i = 0; i < layout->hat_count; i++) {
        uint8_t null_val = layout->hats[i].count;
        uint8_t byte_idx = report->hat_offset + i / 2;
        if (i % 2 == 0)
            report->data[byte_idx] = (report->data[byte_idx] & 0xF0) | (null_val & 0x0F);
        else
            report->data[byte_idx] = (report->data[byte_idx] & 0x0F) | ((null_val & 0x0F) << 4);
    }
}

void hid_gamepad_report_set_button(hid_gamepad_report_buf_t *report,
                                   uint8_t index, int32_t raw_value) {
    if (index >= report->layout->button_count)
        return;
    uint8_t bit_pos = report->layout->buttons[index].bit_offset;
    uint8_t byte_idx = bit_pos / 8;
    uint8_t bit_idx = bit_pos % 8;
    if (raw_value >= report->layout->buttons[index].on)
        report->data[byte_idx] |= (1u << bit_idx);
    else if (raw_value <= report->layout->buttons[index].off)
        report->data[byte_idx] &= ~(1u << bit_idx);
}

void hid_gamepad_report_set_hat(hid_gamepad_report_buf_t *report,
                                uint8_t hat_index, int32_t raw_value) {
    if (hat_index >= report->layout->hat_count)
        return;
    const hid_gamepad_hat_def_t *hat = &report->layout->hats[hat_index];

    /* Map raw value to HID position: match positions first, then centered, else null */
    uint8_t hid_val = hat->count; /* null/centered by default */
    if (raw_value == hat->centered) {
        /* already null */
    } else {
        for (uint8_t i = 0; i < hat->count; i++) {
            if (raw_value == hat->positions[i]) {
                hid_val = i;
                break;
            }
        }
    }

    uint8_t byte_idx = report->hat_offset + hat_index / 2;
    if (hat_index % 2 == 0)
        report->data[byte_idx] = (report->data[byte_idx] & 0xF0) | (hid_val & 0x0F);
    else
        report->data[byte_idx] = (report->data[byte_idx] & 0x0F) | ((hid_val & 0x0F) << 4);
}

void hid_gamepad_report_set_switch(hid_gamepad_report_buf_t *report,
                                    uint8_t switch_index, int32_t raw_value) {
    if (switch_index >= report->layout->switch_count)
        return;
    hid_gamepad_switch_def_t *sw = &report->layout->switches[switch_index];

    /* Find which position matches the raw value */
    uint8_t active = 0; /* 0 = no button (values[0] or unknown) */
    for (uint8_t i = 0; i < sw->count; i++) {
        if (raw_value == sw->values[i]) {
            active = i; /* 0 = no button, 1..count-1 = button index */
            break;
        }
    }

    uint8_t btn_start = sw->button_offset;

    for (uint8_t i = 0; i < sw->count - 1; i++) {
        uint8_t btn_idx = btn_start + i;
        uint8_t byte_idx = btn_idx / 8;
        uint8_t bit_idx = btn_idx % 8;
        if (active == i + 1)
            report->data[byte_idx] |= (1u << bit_idx);
        else
            report->data[byte_idx] &= ~(1u << bit_idx);
    }
}

void hid_gamepad_report_set_axis(hid_gamepad_report_buf_t *report,
                                 uint8_t axis_index, int32_t raw_value) {
    if (axis_index >= report->layout->axis_count)
        return;
    const hid_gamepad_axis_def_t *axis = &report->layout->axes[axis_index];
    int16_t scaled = scale_axis(axis, raw_value);
    uint8_t off = report->axis_offset + axis_index * 2;
    report->data[off] = (uint8_t) (scaled & 0xFF);
    report->data[off + 1] = (uint8_t) ((uint16_t) scaled >> 8);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ESP-IDF USB Driver
 * ═══════════════════════════════════════════════════════════════════════ */

#if defined(ESP_PLATFORM)

#include <esp_log.h>
#include <esp_private/usb_phy.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <tusb.h>
#include <class/hid/hid_device.h>

/* Task defaults */
#define DEFAULT_TASK_PRIORITY 5
#define DEFAULT_TASK_STACK    4096
#define DEFAULT_TASK_CORE     tskNO_AFFINITY

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const char *TAG = "hid_gamepad";

/* USB descriptor globals (populated during init) */

enum {
    STR_IDX_LANGID = 0,
    STR_IDX_MANUFACTURER,
    STR_IDX_PRODUCT,
    STR_IDX_SERIAL,
    STR_IDX_COUNT,
};

static tusb_desc_device_t s_device_desc;
static uint8_t s_config_desc[CONFIG_TOTAL_LEN];
static const char *s_strings[STR_IDX_COUNT];
static uint8_t s_report_desc[128];
static uint16_t s_report_desc_size = 0;

/* Device state */

static usb_phy_handle_t s_phy;
static TaskHandle_t s_task;
static TaskHandle_t s_joiner;
static volatile bool s_mounted;
static volatile bool s_running;
static volatile bool s_report;
static volatile bool s_busy;
static bool s_initialized;
static uint16_t s_report_size;
static uint8_t s_report_data[HID_GAMEPAD_MAX_REPORT_LENGTH];

/* ── TinyUSB device task ──────────────────────────────────────────── */

static esp_err_t try_send_report(void) {
    if (s_report_size == 0) {
        return ESP_OK;
    }
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_report || s_busy) {
        return ESP_ERR_TIMEOUT;
    }
    if (!tud_hid_report(0, s_report_data, s_report_size)) {
        return ESP_FAIL;
    }
    s_report = false;
    s_busy = true;
    return ESP_OK;
}

static void usb_device_task(void *arg) {
    (void) arg;
    while (s_running) {
        tud_task_ext(1, false);
        try_send_report();
        taskYIELD();
    }
    if (s_joiner) {
        xTaskNotifyGive(s_joiner);
    }
    vTaskDelete(NULL);
}

/* ── TinyUSB descriptor callbacks ──────────────────────────────────── */

void tud_sof_cb(const uint32_t frame_count) {
    (void) frame_count;
    s_report = true;
}

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *) &s_device_desc;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return s_config_desc;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    static uint16_t buf[32 + 1];
    uint8_t chr_count;

    if (index == STR_IDX_LANGID) {
        buf[1] = 0x0409;
        chr_count = 1;
    } else {
        if (index >= STR_IDX_COUNT || !s_strings[index]) {
            return NULL;
        }
        const char *str = s_strings[index];
        chr_count = (uint8_t) strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }
        for (uint8_t i = 0; i < chr_count; i++) {
            buf[1 + i] = (uint16_t) str[i];
        }
    }

    buf[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return buf;
}

const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return s_report_desc;
}

/* ── TinyUSB device state callbacks ─────────────────────────────────── */

void tud_mount_cb(void) {
    s_mounted = true;
    s_busy = false;
    s_report = false;
    tud_sof_cb_enable(true);
    ESP_LOGI(TAG, "mounted");
}

void tud_umount_cb(void) {
    s_mounted = false;
    ESP_LOGI(TAG, "unmounted");
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    s_mounted = false;
    ESP_LOGD(TAG, "suspended");
}

void tud_resume_cb(void) {
    s_mounted = true;
    s_busy = false;
    s_report = false;
    tud_sof_cb_enable(true);
    ESP_LOGD(TAG, "resumed");
}

/* ── TinyUSB HID class callbacks ────────────────────────────────────── */

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    if (reqlen < s_report_size || s_report_size == 0) {
        return 0;
    }
    memcpy(buffer, s_report_data, s_report_size);
    return s_report_size;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           const uint8_t *buffer, uint16_t bufsize) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {
    (void) instance;
    (void) report;
    (void) len;
    s_busy = false;
}

// Invoked when a transfer wasn't successful
void tud_hid_report_failed_cb(uint8_t instance, hid_report_type_t report_type, uint8_t const *report,
                              uint16_t xferred_bytes) {
    (void) instance;
    (void) report;
    (void) xferred_bytes;
    s_report = true;
    s_busy = false;
}

/* ── Public API ────────────────────────────────────────────────────── */

esp_err_t hid_gamepad_init(const hid_gamepad_config_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!config->layout) {
        ESP_LOGE(TAG, "layout must be provided");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_initialized) {
        ESP_LOGE(TAG, "already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* 1. Build descriptor from layout */
    s_report_desc_size = (uint16_t) hid_gamepad_desc_build(
        config->layout, s_report_desc, sizeof(s_report_desc));
    if (s_report_desc_size == 0) {
        ESP_LOGE(TAG, "descriptor build failed");
        return ESP_FAIL;
    }

    uint16_t vid = config->vid ? config->vid : fnv1a_16(config->manufacturer);
    uint16_t pid = config->pid ? config->pid : fnv1a_16(config->product);

    s_device_desc = (tusb_desc_device_t){
        .bLength = sizeof(tusb_desc_device_t),
        .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB = 0x0200,
        .bDeviceClass = 0x00,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
        .idVendor = vid,
        .idProduct = pid,
        .bcdDevice = 0x0100,
        .iManufacturer = STR_IDX_MANUFACTURER,
        .iProduct = STR_IDX_PRODUCT,
        .iSerialNumber = STR_IDX_SERIAL,
        .bNumConfigurations = 1,
    };

    s_strings[STR_IDX_LANGID] = NULL;
    s_strings[STR_IDX_MANUFACTURER] = config->manufacturer;
    s_strings[STR_IDX_PRODUCT] = config->product;
    s_strings[STR_IDX_SERIAL] = config->serial;

    uint8_t poll_ms = config->poll_interval_ms > 0 ? config->poll_interval_ms : 1; {
        enum { ITF_NUM_HID = 0, ITF_NUM_TOTAL };
        uint8_t desc[] = {
            TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0,
                                  CONFIG_TOTAL_LEN,
                                  TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
            TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                               s_report_desc_size,
                               0x81, CFG_TUD_HID_EP_BUFSIZE, poll_ms),
        };
        memcpy(s_config_desc, desc, sizeof(desc));
    }

    /* 2. USB PHY — must be done before tusb_init() on ESP32-S2/S3 */

    const usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .otg_speed = USB_PHY_SPEED_FULL,
    };
    esp_err_t phy_err = usb_new_phy(&phy_conf, &s_phy);
    if (phy_err != ESP_OK) {
        ESP_LOGE(TAG, "usb_new_phy: %s", esp_err_to_name(phy_err));
        return phy_err;
    }

    /* 3. TinyUSB */

    const tusb_rhport_init_t tinyusb_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO,
    };
    if (!tusb_init(0, &tinyusb_init)) {
        ESP_LOGE(TAG, "tusb_init failed");
        usb_del_phy(s_phy);
        return ESP_FAIL;
    }

    /* 4. USB device task */

    int priority = config->task_priority > 0 ? config->task_priority : DEFAULT_TASK_PRIORITY;
    size_t stack = config->task_stack_size > 0 ? config->task_stack_size : DEFAULT_TASK_STACK;
    BaseType_t core = config->task_core >= 0 ? config->task_core : DEFAULT_TASK_CORE;

    s_running = true;
    s_initialized = true;
    s_report = false;
    s_busy = false;

    BaseType_t ret = xTaskCreatePinnedToCore(usb_device_task, "usb_hid",
                                             (uint32_t) stack, NULL,
                                             priority, &s_task, core);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "failed to create USB task");
        s_initialized = false;
        s_running = false;
        usb_del_phy(s_phy);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "initialized (VID=%04X PID=%04X desc=%u bytes)",
             vid, pid, s_report_desc_size);
    return ESP_OK;
}

esp_err_t hid_gamepad_deinit(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_joiner = xTaskGetCurrentTaskHandle();
    s_running = false;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));

    usb_del_phy(s_phy);
    s_initialized = false;
    s_mounted = false;
    s_report_desc_size = 0;
    s_report_size = 0;
    s_report = false;
    s_busy = false;

    ESP_LOGI(TAG, "deinitialized");
    return ESP_OK;
}

bool hid_gamepad_is_mounted(void) {
    return s_mounted;
}

esp_err_t hid_gamepad_send_report(hid_gamepad_report_buf_t *report) {
    if (!report) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    s_report_size = report->size;
    memcpy(s_report_data, report->data, report->size);
    return ESP_OK;
}

#endif /* ESP_PLATFORM */
