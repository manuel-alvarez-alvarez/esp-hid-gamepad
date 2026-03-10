#include "unity.h"
#include "hid_gamepad.h"
#include "hid_gamepad_pack.h"
#include <string.h>

/* Helper: build the standard 32-btn, 6-axis layout via builder API.
 * Axes use identity range [-32767, 32767] so raw == HID value.
 * Buttons: on=1, off=0 (raw >= 1 = pressed, raw <= 0 = released). */
static hid_gamepad_layout_t make_standard(void)
{
    hid_gamepad_layout_t l = {0};
    for (int i = 0; i < 32; i++)
        hid_gamepad_layout_add_button(&l, 1, 0);
    hid_gamepad_layout_add_axis(&l, 0x30, -32767, 32767); /* X  */
    hid_gamepad_layout_add_axis(&l, 0x31, -32767, 32767); /* Y  */
    hid_gamepad_layout_add_axis(&l, 0x32, -32767, 32767); /* Z  */
    hid_gamepad_layout_add_axis(&l, 0x35, -32767, 32767); /* Rz */
    hid_gamepad_layout_add_axis(&l, 0x33, -32767, 32767); /* Rx */
    hid_gamepad_layout_add_axis(&l, 0x34, -32767, 32767); /* Ry */
    return l;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Report tests (identity ranges — raw value passes through)
 * ═══════════════════════════════════════════════════════════════════════ */

void test_builder_report_size_standard_is_16(void)
{
    hid_gamepad_layout_t l = make_standard();
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);
    TEST_ASSERT_EQUAL_UINT16(16, r.size);
}

void test_builder_report_init_zeros(void)
{
    hid_gamepad_layout_t l = make_standard();
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);
    for (uint16_t i = 0; i < r.size; i++)
        TEST_ASSERT_EQUAL_HEX8(0x00, r.data[i]);
}

void test_builder_report_set_button(void)
{
    hid_gamepad_layout_t l = make_standard();
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* on=1, off=0: raw >= 1 = pressed, raw <= 0 = released */
    hid_gamepad_report_set_button(&r, 0, 1);   /* pressed */
    hid_gamepad_report_set_button(&r, 7, 100); /* pressed */
    TEST_ASSERT_EQUAL_HEX8(0x81, r.data[0]); /* bit 0 + bit 7 */

    hid_gamepad_report_set_button(&r, 0, 0);   /* released */
    TEST_ASSERT_EQUAL_HEX8(0x80, r.data[0]); /* bit 7 only */
}

void test_builder_report_set_axis(void)
{
    hid_gamepad_layout_t l = make_standard();
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* Identity range [-32767,32767] → raw 0x1234 passes through */
    hid_gamepad_report_set_axis(&r, 0, 0x1234);
    TEST_ASSERT_EQUAL_HEX8(0x34, r.data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x12, r.data[5]);

    hid_gamepad_report_set_axis(&r, 2, -1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, r.data[8]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, r.data[9]);
}

void test_builder_report_matches_packed_struct(void)
{
    hid_gamepad_layout_t l = make_standard();
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* Set buttons via individual calls */
    hid_gamepad_report_set_button(&r, 0, 1);   /* button 0 */
    hid_gamepad_report_set_button(&r, 2, 1);   /* button 2 */
    hid_gamepad_report_set_axis(&r, 0, 1000);  /* x */
    hid_gamepad_report_set_axis(&r, 1, -2000); /* y */
    hid_gamepad_report_set_axis(&r, 2, 3000);  /* z */
    hid_gamepad_report_set_axis(&r, 3, -4000); /* rz */
    hid_gamepad_report_set_axis(&r, 4, 5000);  /* rx */
    hid_gamepad_report_set_axis(&r, 5, -6000); /* ry */

    /* Build the same data with the packed struct */
    ref_gamepad_wire_t ref;
    memset(&ref, 0, sizeof(ref));
    ref.buttons = 0x00000005;
    ref.x       = 1000;
    ref.y       = -2000;
    ref.z       = 3000;
    ref.rz      = -4000;
    ref.rx      = 5000;
    ref.ry      = -6000;

    TEST_ASSERT_EQUAL_UINT16(sizeof(ref), r.size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t *)&ref, r.data, sizeof(ref));
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Scaling tests (non-identity ranges — verify internal conversion)
 * ═══════════════════════════════════════════════════════════════════════ */

void test_builder_scale_axis_endpoints(void)
{
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_axis(&l, 0x30, 0, 4095);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* raw min → -32767 */
    hid_gamepad_report_set_axis(&r, 0, 0);
    int16_t val = (int16_t)(r.data[0] | (r.data[1] << 8));
    TEST_ASSERT_EQUAL_INT16(-32767, val);

    /* raw max → 32767 */
    hid_gamepad_report_set_axis(&r, 0, 4095);
    val = (int16_t)(r.data[0] | (r.data[1] << 8));
    TEST_ASSERT_EQUAL_INT16(32767, val);
}

void test_builder_scale_axis_midpoint(void)
{
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_axis(&l, 0x30, 0, 1000);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    hid_gamepad_report_set_axis(&r, 0, 500);
    int16_t val = (int16_t)(r.data[0] | (r.data[1] << 8));
    TEST_ASSERT_INT16_WITHIN(1, 0, val);
}

void test_builder_scale_axis_signed_range(void)
{
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_axis(&l, 0x30, -100, 100);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    hid_gamepad_report_set_axis(&r, 0, -100);
    int16_t val = (int16_t)(r.data[0] | (r.data[1] << 8));
    TEST_ASSERT_EQUAL_INT16(-32767, val);

    hid_gamepad_report_set_axis(&r, 0, 100);
    val = (int16_t)(r.data[0] | (r.data[1] << 8));
    TEST_ASSERT_EQUAL_INT16(32767, val);

    hid_gamepad_report_set_axis(&r, 0, 0);
    val = (int16_t)(r.data[0] | (r.data[1] << 8));
    TEST_ASSERT_INT16_WITHIN(1, 0, val);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Hat switch tests
 * ═══════════════════════════════════════════════════════════════════════ */

/* Helper: standard 8-way hat positions (raw values 0–7 = N,NE,E,SE,S,SW,W,NW) */
static const int32_t hat8_positions[] = {0, 1, 2, 3, 4, 5, 6, 7};
#define HAT8_CENTERED 8

void test_builder_hat_report_size(void)
{
    /* 8 buttons (1 byte) + 1 hat (4 bits, padded to 1 byte) + 2 axes (4 bytes) = 6 */
    hid_gamepad_layout_t l = {0};
    for (int i = 0; i < 8; i++)
        hid_gamepad_layout_add_button(&l, 1, 0);
    hid_gamepad_layout_add_hat(&l, HAT8_CENTERED, hat8_positions, 8);
    hid_gamepad_layout_add_axis(&l, 0x30, -32767, 32767);
    hid_gamepad_layout_add_axis(&l, 0x31, -32767, 32767);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);
    TEST_ASSERT_EQUAL_UINT16(6, r.size);
}

void test_builder_hat_init_centered(void)
{
    /* A single 8-way hat should initialize to count (8) = null/centered */
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_hat(&l, HAT8_CENTERED, hat8_positions, 8);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);
    TEST_ASSERT_EQUAL_HEX8(0x08, r.data[0] & 0x0F);
}

void test_builder_hat_set_direction(void)
{
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_hat(&l, HAT8_CENTERED, hat8_positions, 8);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* Set to North (raw 0 → HID position 0) */
    hid_gamepad_report_set_hat(&r, 0, 0);
    TEST_ASSERT_EQUAL_HEX8(0x00, r.data[0] & 0x0F);

    /* Set to East (raw 2 → HID position 2) */
    hid_gamepad_report_set_hat(&r, 0, 2);
    TEST_ASSERT_EQUAL_HEX8(0x02, r.data[0] & 0x0F);

    /* Set to South (raw 4 → HID position 4) */
    hid_gamepad_report_set_hat(&r, 0, 4);
    TEST_ASSERT_EQUAL_HEX8(0x04, r.data[0] & 0x0F);

    /* Set to centered */
    hid_gamepad_report_set_hat(&r, 0, HAT8_CENTERED);
    TEST_ASSERT_EQUAL_HEX8(0x08, r.data[0] & 0x0F);
}

void test_builder_hat_two_hats_packed(void)
{
    /* Two hats share one byte (4 bits each, no padding needed) */
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_hat(&l, HAT8_CENTERED, hat8_positions, 8);
    hid_gamepad_layout_add_hat(&l, HAT8_CENTERED, hat8_positions, 8);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);
    TEST_ASSERT_EQUAL_UINT16(1, r.size); /* 2 hats × 4 bits = 1 byte */

    hid_gamepad_report_set_hat(&r, 0, 3); /* SE in low nibble → HID 3 */
    hid_gamepad_report_set_hat(&r, 1, 7); /* NW in high nibble → HID 7 */
    TEST_ASSERT_EQUAL_HEX8(0x73, r.data[0]);
}

void test_builder_hat_with_buttons_offsets(void)
{
    /* 32 buttons (4 bytes) + 1 hat (4 bits + 4 pad = 1 byte) + 1 axis (2 bytes) = 7 */
    hid_gamepad_layout_t l = {0};
    for (int i = 0; i < 32; i++)
        hid_gamepad_layout_add_button(&l, 1, 0);
    hid_gamepad_layout_add_hat(&l, HAT8_CENTERED, hat8_positions, 8);
    hid_gamepad_layout_add_axis(&l, 0x30, -32767, 32767);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    TEST_ASSERT_EQUAL_UINT8(4, r.hat_offset);
    TEST_ASSERT_EQUAL_UINT8(5, r.axis_offset);
    TEST_ASSERT_EQUAL_UINT16(7, r.size);

    /* Set hat to S (raw 4 → HID 4) and verify it's in the right byte */
    hid_gamepad_report_set_hat(&r, 0, 4);
    TEST_ASSERT_EQUAL_HEX8(0x04, r.data[4] & 0x0F);

    /* Set axis and verify it doesn't clobber hat */
    hid_gamepad_report_set_axis(&r, 0, 0x1234);
    TEST_ASSERT_EQUAL_HEX8(0x04, r.data[4] & 0x0F);
    TEST_ASSERT_EQUAL_HEX8(0x34, r.data[5]);
    TEST_ASSERT_EQUAL_HEX8(0x12, r.data[6]);
}

void test_builder_hat_out_of_range_is_noop(void)
{
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_hat(&l, HAT8_CENTERED, hat8_positions, 8);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    hid_gamepad_report_set_hat(&r, 0, 3);
    TEST_ASSERT_EQUAL_HEX8(0x03, r.data[0] & 0x0F);
    /* index 1 doesn't exist — should be a no-op */
    hid_gamepad_report_set_hat(&r, 1, 0);
    TEST_ASSERT_EQUAL_HEX8(0x03, r.data[0] & 0x0F);
}

void test_builder_hat_custom_raw_values(void)
{
    /* Hat with non-sequential raw values (e.g. device sends 100,200,300,400 for N,E,S,W) */
    const int32_t positions[] = {100, 200, 300, 400};
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_hat(&l, -1, positions, 4);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* Centered on init (count=4 → HID null value) */
    TEST_ASSERT_EQUAL_HEX8(0x04, r.data[0] & 0x0F);

    /* raw 100 → position 0 (N) */
    hid_gamepad_report_set_hat(&r, 0, 100);
    TEST_ASSERT_EQUAL_HEX8(0x00, r.data[0] & 0x0F);

    /* raw 300 → position 2 (S) */
    hid_gamepad_report_set_hat(&r, 0, 300);
    TEST_ASSERT_EQUAL_HEX8(0x02, r.data[0] & 0x0F);

    /* raw -1 → centered */
    hid_gamepad_report_set_hat(&r, 0, -1);
    TEST_ASSERT_EQUAL_HEX8(0x04, r.data[0] & 0x0F);

    /* Unknown raw value → centered (null) */
    hid_gamepad_report_set_hat(&r, 0, 999);
    TEST_ASSERT_EQUAL_HEX8(0x04, r.data[0] & 0x0F);
}

void test_builder_hat_rejects_zero_count(void)
{
    hid_gamepad_layout_t l = {0};
    TEST_ASSERT_FALSE(hid_gamepad_layout_add_hat(&l, 0, hat8_positions, 0));
    TEST_ASSERT_EQUAL_UINT8(0, l.hat_count);
}

void test_builder_hat_rejects_excess_positions(void)
{
    hid_gamepad_layout_t l = {0};
    int32_t positions[HID_GAMEPAD_MAX_HAT_POSITIONS + 1] = {0};
    TEST_ASSERT_FALSE(
        hid_gamepad_layout_add_hat(&l, 0, positions, HID_GAMEPAD_MAX_HAT_POSITIONS + 1));
    TEST_ASSERT_EQUAL_UINT8(0, l.hat_count);
}

void test_builder_button_hysteresis(void)
{
    /* on=100, off=50: values in the dead zone (51–99) don't change state */
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_button(&l, 100, 50);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* Start released; value in dead zone doesn't change anything */
    hid_gamepad_report_set_button(&r, 0, 75);
    TEST_ASSERT_EQUAL_HEX8(0x00, r.data[0] & 0x01);
    /* Press it */
    hid_gamepad_report_set_button(&r, 0, 100);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0] & 0x01);
    /* Dead zone again — stays pressed */
    hid_gamepad_report_set_button(&r, 0, 75);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0] & 0x01);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Scaling tests (non-identity ranges — verify internal conversion)
 * ═══════════════════════════════════════════════════════════════════════ */

void test_builder_scale_button_threshold(void)
{
    /* on=2048, off=2047 (midpoint of 0..4095) */
    hid_gamepad_layout_t l = {0};
    for (int i = 0; i < 8; i++)
        hid_gamepad_layout_add_button(&l, 2048, 2047);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    hid_gamepad_report_set_button(&r, 0, 1000);  /* below off */
    TEST_ASSERT_EQUAL_HEX8(0x00, r.data[0] & 0x01);

    hid_gamepad_report_set_button(&r, 0, 3000);  /* above on */
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0] & 0x01);

    hid_gamepad_report_set_button(&r, 0, 0);     /* at min */
    TEST_ASSERT_EQUAL_HEX8(0x00, r.data[0] & 0x01);

    hid_gamepad_report_set_button(&r, 0, 4095);  /* at max */
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0] & 0x01);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Switch tests
 * ═══════════════════════════════════════════════════════════════════════ */

/* Switch with 4 positions: values[0]=off, values[1..3]=buttons */
static const int32_t sw4_values[] = {0, 10, 20, 30};

void test_builder_switch_adds_buttons(void)
{
    /* A switch with 4 positions adds 3 buttons to the report */
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_switch(&l, sw4_values, 4);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);
    /* 3 buttons → 1 byte (3 bits + 5 padding) */
    TEST_ASSERT_EQUAL_UINT16(1, r.size);
}

void test_builder_switch_set_positions(void)
{
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_switch(&l, sw4_values, 4);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* Position 0 (raw 0) = no button */
    hid_gamepad_report_set_switch(&r, 0, 0);
    TEST_ASSERT_EQUAL_HEX8(0x00, r.data[0]);

    /* Position 1 (raw 10) = button 0 */
    hid_gamepad_report_set_switch(&r, 0, 10);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0]);

    /* Position 2 (raw 20) = button 1 */
    hid_gamepad_report_set_switch(&r, 0, 20);
    TEST_ASSERT_EQUAL_HEX8(0x02, r.data[0]);

    /* Position 3 (raw 30) = button 2 */
    hid_gamepad_report_set_switch(&r, 0, 30);
    TEST_ASSERT_EQUAL_HEX8(0x04, r.data[0]);
}

void test_builder_switch_no_match_clears_all(void)
{
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_switch(&l, sw4_values, 4);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* Set to position 1, then feed unknown value → all cleared */
    hid_gamepad_report_set_switch(&r, 0, 10);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0]);
    hid_gamepad_report_set_switch(&r, 0, 999);
    TEST_ASSERT_EQUAL_HEX8(0x00, r.data[0]);
}

void test_builder_switch_rejects_zero_count(void)
{
    hid_gamepad_layout_t l = {0};
    TEST_ASSERT_FALSE(hid_gamepad_layout_add_switch(&l, sw4_values, 0));
    TEST_ASSERT_EQUAL_UINT8(0, l.switch_count);
}

void test_builder_switch_with_buttons_offsets(void)
{
    /* 8 explicit buttons (1 byte) + switch with 4 positions (3 buttons) + 1 axis */
    hid_gamepad_layout_t l = {0};
    for (int i = 0; i < 8; i++)
        hid_gamepad_layout_add_button(&l, 1, 0);
    hid_gamepad_layout_add_switch(&l, sw4_values, 4);
    hid_gamepad_layout_add_axis(&l, 0x30, -32767, 32767);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* 8+3=11 buttons → 2 bytes, axes at offset 2, total = 4 */
    TEST_ASSERT_EQUAL_UINT16(4, r.size);
    TEST_ASSERT_EQUAL_UINT8(2, r.axis_offset);

    /* Set explicit button 7 */
    hid_gamepad_report_set_button(&r, 7, 1);
    TEST_ASSERT_EQUAL_HEX8(0x80, r.data[0]);

    /* Set switch to position 2 → button index 9 (bit 1 of byte 1) */
    hid_gamepad_report_set_switch(&r, 0, 20);
    TEST_ASSERT_EQUAL_HEX8(0x02, r.data[1] & 0x07);

    /* Explicit button still intact */
    TEST_ASSERT_EQUAL_HEX8(0x80, r.data[0]);
}

void test_builder_switch_out_of_range_is_noop(void)
{
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_switch(&l, sw4_values, 4);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    hid_gamepad_report_set_switch(&r, 0, 10);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0]);
    /* index 1 doesn't exist — should be a no-op */
    hid_gamepad_report_set_switch(&r, 1, 0);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0]);
}

void test_builder_two_switches(void)
{
    /* Two switches: first has 3 positions (2 buttons), second has 4 positions (3 buttons) */
    const int32_t sw_a[] = {0, 1, 2};
    const int32_t sw_b[] = {0, 100, 200, 300};
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_switch(&l, sw_a, 3);
    hid_gamepad_layout_add_switch(&l, sw_b, 4);
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* 2+3=5 buttons → 1 byte */
    TEST_ASSERT_EQUAL_UINT16(1, r.size);

    /* Switch A position 1 → bit 0 */
    hid_gamepad_report_set_switch(&r, 0, 1);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0]);

    /* Switch B position 2 → bit 3 (btn_start=2, i=1 → idx=3) */
    hid_gamepad_report_set_switch(&r, 1, 200);
    TEST_ASSERT_EQUAL_HEX8(0x09, r.data[0]); /* bit 0 + bit 3 */

    /* Switch A off → only switch B remains */
    hid_gamepad_report_set_switch(&r, 0, 0);
    TEST_ASSERT_EQUAL_HEX8(0x08, r.data[0]);
}

void test_builder_switch_between_buttons(void)
{
    /* button 0, switch (3 positions = 2 buttons), button 1 — interleaved */
    const int32_t sw_vals[] = {0, 10, 20};
    hid_gamepad_layout_t l = {0};
    hid_gamepad_layout_add_button(&l, 1, 0);     /* button 0 → bit 0 */
    hid_gamepad_layout_add_switch(&l, sw_vals, 3); /* switch → bits 1,2 */
    hid_gamepad_layout_add_button(&l, 1, 0);     /* button 1 → bit 3 */

    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    /* 1 + 2 + 1 = 4 total button bits → 1 byte */
    TEST_ASSERT_EQUAL_UINT16(1, r.size);

    /* Press button 0 → bit 0 */
    hid_gamepad_report_set_button(&r, 0, 1);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.data[0]);

    /* Switch position 1 → bit 1 */
    hid_gamepad_report_set_switch(&r, 0, 10);
    TEST_ASSERT_EQUAL_HEX8(0x03, r.data[0]); /* bits 0+1 */

    /* Press button 1 → bit 3 (NOT bit 1!) */
    hid_gamepad_report_set_button(&r, 1, 1);
    TEST_ASSERT_EQUAL_HEX8(0x0B, r.data[0]); /* bits 0+1+3 */

    /* Release button 0 */
    hid_gamepad_report_set_button(&r, 0, 0);
    TEST_ASSERT_EQUAL_HEX8(0x0A, r.data[0]); /* bits 1+3 */

    /* Switch off */
    hid_gamepad_report_set_switch(&r, 0, 0);
    TEST_ASSERT_EQUAL_HEX8(0x08, r.data[0]); /* bit 3 only */
}

void test_builder_axis_rejects_invalid_range(void)
{
    hid_gamepad_layout_t l = {0};
    TEST_ASSERT_FALSE(hid_gamepad_layout_add_axis(&l, 0x30, 100, 100));
    TEST_ASSERT_EQUAL_UINT8(0, l.axis_count);
    TEST_ASSERT_FALSE(hid_gamepad_layout_add_axis(&l, 0x30, 10, -10));
    TEST_ASSERT_EQUAL_UINT8(0, l.axis_count);
}

void test_builder_axis_clamps_inputs(void)
{
    hid_gamepad_layout_t l = {0};
    TEST_ASSERT_TRUE(hid_gamepad_layout_add_axis(&l, 0x30, -100, 100));
    hid_gamepad_report_buf_t r;
    hid_gamepad_report_init(&r, &l);

    hid_gamepad_report_set_axis(&r, 0, 500);
    int16_t val = (int16_t)(r.data[0] | (r.data[1] << 8));
    TEST_ASSERT_EQUAL_INT16(32767, val);

    hid_gamepad_report_set_axis(&r, 0, -500);
    val = (int16_t)(r.data[0] | (r.data[1] << 8));
    TEST_ASSERT_EQUAL_INT16(-32767, val);
}
