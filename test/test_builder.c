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
