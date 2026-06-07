#include <check.h>
#include <stdlib.h>
#include <string.h>

/* wiiuse internal headers for struct definitions */
#include "wiiuse_internal.h"
#include "wiiuse.h"

/*
 * Security invariant: wiiuse_write_data must reject writes longer than the
 * 16 bytes a write report can carry, instead of copying them into its
 * fixed-size report buffer.
 *
 * The wiimote is flagged connected so the call gets past the connection
 * check and actually reaches the length check; oversized requests are
 * rejected before anything is sent, so no I/O happens.
 */

#define WIIUSE_MAX_WRITE_LEN 16

START_TEST(test_write_data_length_bounds)
{
    struct wiimote_t **wm = wiiuse_init(1);
    byte data[255];
    byte test_lens[] = {WIIUSE_MAX_WRITE_LEN + 1, 32, 255};
    size_t i;

    ck_assert_ptr_nonnull(wm);
    memset(data, 0xAA, sizeof(data));
    WIIMOTE_ENABLE_STATE(wm[0], WIIMOTE_STATE_CONNECTED);

    for (i = 0; i < sizeof(test_lens) / sizeof(test_lens[0]); i++)
    {
        ck_assert_msg(wiiuse_write_data(wm[0], 0x0000, data, test_lens[i]) == 0,
                      "write_data accepted oversized len=%u without bounds check", test_lens[i]);
    }

    wiiuse_cleanup(wm, 1);
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_write_data_length_bounds);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}