#include <math.h>
#include "hid_gamepad.h"
#include "class/hid/hid.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "example";

/* Polling rate */
#define POLL_MS 1

/* Axis input ranges */
#define AXIS_MIN  (-1000)
#define AXIS_MAX  ( 1000)

/* Button thresholds (raw >= on → pressed, raw <= off → released) */
#define BUTTON_ON   1
#define BUTTON_OFF  0

/* ── Application ───────────────────────────────────────────────────── */

void app_main(void) {
    /* Build layout */
    hid_gamepad_layout_t layout = {0};
    hid_gamepad_layout_add_button(&layout, BUTTON_ON, BUTTON_OFF);
    hid_gamepad_layout_add_button(&layout, BUTTON_ON, BUTTON_OFF);
    hid_gamepad_layout_add_axis(&layout, HID_USAGE_DESKTOP_X, AXIS_MIN, AXIS_MAX);
    hid_gamepad_layout_add_axis(&layout, HID_USAGE_DESKTOP_Y, AXIS_MIN, AXIS_MAX);

    hid_gamepad_config_t cfg = HID_GAMEPAD_DEFAULT_CONFIG();
    cfg.task_core = 1;
    cfg.task_priority = configMAX_PRIORITIES - 1;
    cfg.layout = &layout;
    cfg.poll_interval_ms = POLL_MS;

    ESP_ERROR_CHECK(hid_gamepad_init(&cfg));

    ESP_LOGI(TAG, "Waiting for USB host...");
    while (!hid_gamepad_is_mounted()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "Host connected — sending reports");

    hid_gamepad_report_buf_t report;
    hid_gamepad_report_init(&report, &layout);
    int tick = 0;

    for (;;) {
        float rad = (float) tick * (2.0f * 3.14159265f / (1000.0f / POLL_MS)); /* 1 rev/sec */

        /* Move X and Y axes in a circle */
        hid_gamepad_report_set_axis(&report, 0, (int)(sinf(rad) * AXIS_MAX));
        hid_gamepad_report_set_axis(&report, 1, (int)(cosf(rad) * AXIS_MAX));

        /* Alternate buttons */
        hid_gamepad_report_set_button(&report, 0, (tick / 50) % 2);
        hid_gamepad_report_set_button(&report, 1, ((tick / 50) + 1) % 2);
        hid_gamepad_send_report(&report);
        tick = (tick + 1) % 36000;
        /* Ensure at least 1 tick delay even if POLL_MS < portTICK_PERIOD_MS */
        TickType_t ticks = pdMS_TO_TICKS(POLL_MS);
        vTaskDelay(ticks > 0 ? ticks : 1);
    }
}
