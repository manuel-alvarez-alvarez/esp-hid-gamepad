#include "unity.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* ── CLI filter: -n <test_name> runs only that test ─────────────────── */
static const char *g_filter = NULL;

#define RUN_FILTERED_TEST(func)                              \
    do {                                                     \
        if (!g_filter || strcmp(g_filter, #func) == 0)        \
            RUN_TEST(func);                                  \
    } while (0)

/* ── test_report.c ──────────────────────────────────────────────────── */

/* Wire struct field offsets */
extern void test_wire_total_size(void);
extern void test_wire_buttons_at_offset_0(void);
extern void test_wire_x_at_offset_4(void);
extern void test_wire_y_at_offset_6(void);
extern void test_wire_z_at_offset_8(void);
extern void test_wire_rz_at_offset_10(void);
extern void test_wire_rx_at_offset_12(void);
extern void test_wire_ry_at_offset_14(void);

/* ── test_builder.c (runtime builder API) ──────────────────────────── */

extern void test_builder_report_size_standard_is_16(void);
extern void test_builder_report_init_zeros(void);
extern void test_builder_report_set_button(void);
extern void test_builder_report_set_axis(void);
extern void test_builder_report_matches_packed_struct(void);
extern void test_builder_hat_report_size(void);
extern void test_builder_hat_init_centered(void);
extern void test_builder_hat_set_direction(void);
extern void test_builder_hat_two_hats_packed(void);
extern void test_builder_hat_with_buttons_offsets(void);
extern void test_builder_hat_out_of_range_is_noop(void);
extern void test_builder_hat_custom_raw_values(void);

extern void test_builder_button_hysteresis(void);

extern void test_builder_scale_axis_endpoints(void);
extern void test_builder_scale_axis_midpoint(void);
extern void test_builder_scale_axis_signed_range(void);
extern void test_builder_scale_button_threshold(void);

/* Switch tests */
extern void test_builder_switch_adds_buttons(void);
extern void test_builder_switch_set_positions(void);
extern void test_builder_switch_no_match_clears_all(void);
extern void test_builder_switch_with_buttons_offsets(void);
extern void test_builder_switch_out_of_range_is_noop(void);
extern void test_builder_two_switches(void);

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            g_filter = argv[++i];
    }

    UNITY_BEGIN();

    /* Wire struct layout */
    RUN_FILTERED_TEST(test_wire_total_size);
    RUN_FILTERED_TEST(test_wire_buttons_at_offset_0);
    RUN_FILTERED_TEST(test_wire_x_at_offset_4);
    RUN_FILTERED_TEST(test_wire_y_at_offset_6);
    RUN_FILTERED_TEST(test_wire_z_at_offset_8);
    RUN_FILTERED_TEST(test_wire_rz_at_offset_10);
    RUN_FILTERED_TEST(test_wire_rx_at_offset_12);
    RUN_FILTERED_TEST(test_wire_ry_at_offset_14);

    /* Builder: report */
    RUN_FILTERED_TEST(test_builder_report_size_standard_is_16);
    RUN_FILTERED_TEST(test_builder_report_init_zeros);
    RUN_FILTERED_TEST(test_builder_report_set_button);
    RUN_FILTERED_TEST(test_builder_report_set_axis);
    RUN_FILTERED_TEST(test_builder_report_matches_packed_struct);

    /* Builder: hat switches */
    RUN_FILTERED_TEST(test_builder_hat_report_size);
    RUN_FILTERED_TEST(test_builder_hat_init_centered);
    RUN_FILTERED_TEST(test_builder_hat_set_direction);
    RUN_FILTERED_TEST(test_builder_hat_two_hats_packed);
    RUN_FILTERED_TEST(test_builder_hat_with_buttons_offsets);
    RUN_FILTERED_TEST(test_builder_hat_out_of_range_is_noop);
    RUN_FILTERED_TEST(test_builder_hat_custom_raw_values);

    /* Builder: hysteresis */
    RUN_FILTERED_TEST(test_builder_button_hysteresis);

    /* Builder: scaling */
    RUN_FILTERED_TEST(test_builder_scale_axis_endpoints);
    RUN_FILTERED_TEST(test_builder_scale_axis_midpoint);
    RUN_FILTERED_TEST(test_builder_scale_axis_signed_range);
    RUN_FILTERED_TEST(test_builder_scale_button_threshold);

    /* Builder: switches */
    RUN_FILTERED_TEST(test_builder_switch_adds_buttons);
    RUN_FILTERED_TEST(test_builder_switch_set_positions);
    RUN_FILTERED_TEST(test_builder_switch_no_match_clears_all);
    RUN_FILTERED_TEST(test_builder_switch_with_buttons_offsets);
    RUN_FILTERED_TEST(test_builder_switch_out_of_range_is_noop);
    RUN_FILTERED_TEST(test_builder_two_switches);

    return UNITY_END();
}