#include "unity.h"
#include "hid_gamepad_pack.h"
#include <stddef.h>

/* ── Wire struct size and field-offset tests ─────────────────────────── */

void test_wire_total_size(void)
{
    /* 4 (buttons) + 6*2 (axes) = 16 bytes */
    TEST_ASSERT_EQUAL_size_t(16, sizeof(ref_gamepad_wire_t));
}

void test_wire_buttons_at_offset_0(void)
{
    TEST_ASSERT_EQUAL_size_t(0, offsetof(ref_gamepad_wire_t, buttons));
}

void test_wire_x_at_offset_4(void)
{
    TEST_ASSERT_EQUAL_size_t(4, offsetof(ref_gamepad_wire_t, x));
}

void test_wire_y_at_offset_6(void)
{
    TEST_ASSERT_EQUAL_size_t(6, offsetof(ref_gamepad_wire_t, y));
}

void test_wire_z_at_offset_8(void)
{
    TEST_ASSERT_EQUAL_size_t(8, offsetof(ref_gamepad_wire_t, z));
}

void test_wire_rz_at_offset_10(void)
{
    TEST_ASSERT_EQUAL_size_t(10, offsetof(ref_gamepad_wire_t, rz));
}

void test_wire_rx_at_offset_12(void)
{
    TEST_ASSERT_EQUAL_size_t(12, offsetof(ref_gamepad_wire_t, rx));
}

void test_wire_ry_at_offset_14(void)
{
    TEST_ASSERT_EQUAL_size_t(14, offsetof(ref_gamepad_wire_t, ry));
}