# esp-hid-gamepad

[![Component Registry](https://components.espressif.com/components/manuel-alvarez-alvarez/esp-hid-gamepad/badge.svg)](https://components.espressif.com/components/manuel-alvarez-alvarez/esp-hid-gamepad)

Runtime-configurable USB HID gamepad component for [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/) using [TinyUSB](https://github.com/hathach/tinyusb).

## Features

- Dynamic layout builder for buttons (up to 32), hat switches (up to 4), and axes (up to 8)
- Automatic HID report descriptor generation with proper byte-aligned padding
- Per-button hysteresis thresholds (on/off) for noise-free input
- Hat switches with raw device value mapping (positions[0]=N, clockwise) and null/centered state
- 16-bit signed axes with configurable input range scaled (and clamped) to [-32767, 32767]
- Configurable USB polling rate (default 1000 Hz)
- VID/PID from config or auto-derived from manufacturer/product strings via FNV-1a hash
- Async SOF-based report sending for minimal latency
- Supports ESP32-S2 and ESP32-S3

## Requirements

- ESP-IDF >= 5.0

## Installation

### ESP-IDF Component Registry

```bash
idf.py add-dependency "manuel-alvarez-alvarez/esp-hid-gamepad"
```

### Manual

Clone or add this repository as a git submodule under your project's `components/` directory.

## Quick Start

```c
#include "hid_gamepad.h"
#include "class/hid/hid.h"

void app_main(void) {
    hid_gamepad_layout_t layout = {0};
    hid_gamepad_layout_add_button(&layout, 1, 0);
    hid_gamepad_layout_add_button(&layout, 1, 0);
    static const int32_t hat_pos[] = {0, 1, 2, 3, 4, 5, 6, 7}; /* N, NE, E, SE, S, SW, W, NW */
    hid_gamepad_layout_add_hat(&layout, /*centered=*/8, hat_pos, 8);
    static const int32_t profile_switch[] = {0, 100, 200};    /* 3-position selector */
    hid_gamepad_layout_add_switch(&layout, profile_switch, 3);
    hid_gamepad_layout_add_axis(&layout, HID_USAGE_DESKTOP_X, -1000, 1000);
    hid_gamepad_layout_add_axis(&layout, HID_USAGE_DESKTOP_Y, -1000, 1000);

    hid_gamepad_config_t cfg = HID_GAMEPAD_DEFAULT_CONFIG();
    cfg.layout = &layout;

    ESP_ERROR_CHECK(hid_gamepad_init(&cfg));

    hid_gamepad_report_buf_t report;
    hid_gamepad_report_init(&report, &layout);

    for (;;) {
        hid_gamepad_report_set_button(&report, 0, my_read_btn0());
        hid_gamepad_report_set_button(&report, 1, my_read_btn1());
        hid_gamepad_report_set_hat(&report, 0, my_read_dpad());
        hid_gamepad_report_set_switch(&report, 0, my_read_profile());
        hid_gamepad_report_set_axis(&report, 0, my_read_x());
        hid_gamepad_report_set_axis(&report, 1, my_read_y());
        hid_gamepad_send_report(&report);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

A full example is available in [`examples/basic`](examples/basic).

## API

### Configuration

Use `HID_GAMEPAD_DEFAULT_CONFIG()` and override only the fields you need.

| Field | Type | Default | Description |
|---|---|---|---|
| `vid` | `uint16_t` | `0` | USB vendor ID (`0` = derived from `manufacturer` via FNV-1a hash) |
| `pid` | `uint16_t` | `0` | USB product ID (`0` = derived from `product` via FNV-1a hash) |
| `manufacturer` | `const char *` | `"Manuel Alvarez Alvarez"` | USB manufacturer string |
| `product` | `const char *` | `"HID Gamepad"` | USB product string |
| `serial` | `const char *` | `"000000"` | USB serial string |
| `layout` | `const hid_gamepad_layout_t *` | `NULL` | HID report layout (**required**) |
| `task_priority` | `int` | `5` | FreeRTOS task priority |
| `task_core` | `int` | no affinity | Pin task to core (`-1` = any) |
| `task_stack_size` | `size_t` | `4096` | Task stack size in bytes |
| `poll_interval_ms` | `uint8_t` | `1` | USB endpoint polling interval in ms (1 = 1000 Hz) |

Fields set to `0` (or `-1` for `task_core`) use their defaults.

### Layout Builder

All builder helpers now return `true` on success and `false` when the layout is already full or the parameters are invalid (e.g., zero hat positions or an axis whose min/max are equal). Invalid inputs are rejected up front instead of producing malformed HID descriptors later.

| Function | Description |
|---|---|
| `hid_gamepad_layout_add_button(layout, on, off)` | Add a button with hysteresis thresholds (raw >= `on` → pressed, raw <= `off` → released) |
| `hid_gamepad_layout_add_hat(layout, centered, positions, count)` | Add a hat switch with raw values for each direction (`positions[0]`=N, clockwise) and a centered/null value |
| `hid_gamepad_layout_add_switch(layout, values, count)` | Add an n-way switch whose raw values map onto HID buttons |
| `hid_gamepad_layout_add_axis(layout, usage, in_min, in_max)` | Add a 16-bit signed axis with HID usage ID and device raw range |

### Functions

| Function | Description |
|---|---|
| `hid_gamepad_init(config)` | Initialize the USB HID device; starts TinyUSB and the background task |
| `hid_gamepad_update(config)` | Rebuild descriptors and force USB re-enumeration with a new configuration |
| `hid_gamepad_deinit()` | Stop the task and release USB resources |
| `hid_gamepad_is_mounted()` | Returns `true` if a USB host is connected |
| `hid_gamepad_send_report(report)` | Send the current report to the host |

### Report Buffer

Axis setters clamp raw inputs into the declared `[in_min, in_max]` range before conversion so devices that overshoot never corrupt the HID stream. Setters are void; they update the report buffer in-place and ignore out-of-range indices.

| Function | Description |
|---|---|
| `hid_gamepad_report_init(report, layout)` | Initialize a report buffer for the given layout |
| `hid_gamepad_report_set_button(report, index, raw_value)` | Set a button state from a raw device value |
| `hid_gamepad_report_set_hat(report, hat_index, raw_value)` | Set a hat switch from a raw device value (mapped automatically) |
| `hid_gamepad_report_set_axis(report, axis_index, raw_value)` | Set an axis value from a raw device value (scaled automatically) |

## Testing

Unit tests run on the host (no hardware required):

```bash
cmake -B test/build test
cmake --build test/build
ctest --test-dir test/build --output-on-failure
```

## License

[MIT](LICENSE)
