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

/* Dial / Slider input range */
#define DIAL_MIN   0
#define DIAL_MAX   100
#define SLIDER_MIN 0
#define SLIDER_MAX 100

#define HAT_CENTERED 100
#define HAT_N 200
#define HAT_NE 300
#define HAT_E 400
#define HAT_SE 500
#define HAT_S 600
#define HAT_SW 700
#define HAT_W 800
#define HAT_NW 900

static const int32_t hat_positions[] = {HAT_N, HAT_NE, HAT_E, HAT_SE, HAT_S, HAT_SW, HAT_W, HAT_NW};

/* ── Application ───────────────────────────────────────────────────── */

void app_main(void) {
    /* Build layout */
    hid_gamepad_layout_t layout = {0};
    hid_gamepad_layout_add_button(&layout, BUTTON_ON, BUTTON_OFF);
    static const int32_t profile_switch[] = {0, 100, 200};    /* 3-position selector */
    hid_gamepad_layout_add_switch(&layout, profile_switch, 3);
    hid_gamepad_layout_add_button(&layout, BUTTON_ON, BUTTON_OFF);
    hid_gamepad_layout_add_hat(&layout, HAT_CENTERED, hat_positions, 8);
    hid_gamepad_layout_add_axis(&layout, HID_USAGE_DESKTOP_X, AXIS_MIN, AXIS_MAX);
    hid_gamepad_layout_add_axis(&layout, HID_USAGE_DESKTOP_Y, AXIS_MIN, AXIS_MAX);
    hid_gamepad_layout_add_axis(&layout, HID_USAGE_DESKTOP_DIAL, DIAL_MIN, DIAL_MAX);
    hid_gamepad_layout_add_axis(&layout, HID_USAGE_DESKTOP_SLIDER, SLIDER_MIN, SLIDER_MAX);

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

        /* Dial ramps 0→100 over one revolution */
        int ticks_per_rev = (int)(1000.0f / POLL_MS);
        int dial = (tick % ticks_per_rev) * DIAL_MAX / ticks_per_rev;
        hid_gamepad_report_set_axis(&report, 2, dial);
        hid_gamepad_report_set_axis(&report, 3, dial);

        /* Hat follows the stick rotation: divide circle into 8 sectors.
         * Offset by 4 because Y=cos(0)=+1 is South in gamepad coords (+Y=down),
         * and reverse direction because the visual rotation is counterclockwise. */
        int sector = ((int)roundf(rad / (2.0f * 3.14159265f) * 8.0f)) % 8;
        if (sector < 0) sector += 8;
        int hat_idx = (12 - sector) % 8;
        hid_gamepad_report_set_hat(&report, 0, hat_positions[hat_idx]);

        /* Alternate buttons */
        hid_gamepad_report_set_button(&report, 0, (tick / 50) % 2);
        hid_gamepad_report_set_button(&report, 1, ((tick / 50) + 1) % 2);

        /* Switch */
        int switch_idx = tick % 3;
        hid_gamepad_report_set_switch(&report, 0, profile_switch[switch_idx]);

        hid_gamepad_send_report(&report);

        tick = (tick + 1) % 36000;

        /* Ensure at least 1 tick delay even if POLL_MS < portTICK_PERIOD_MS */
        TickType_t ticks = pdMS_TO_TICKS(POLL_MS);
        vTaskDelay(ticks > 0 ? ticks : 1);
    }
}
