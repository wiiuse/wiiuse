#include <check.h>
#include <stdlib.h>
#include <string.h>

/* wiiuse internal headers for struct definitions */
#include "wiiuse_internal.h"
#include "wiiuse.h"

/*
 * Security invariant: wiiuse_read_data must reject read requests where
 * the requested length exceeds the maximum buffer capacity (MAX_PAYLOAD - 1
 * or similar internal limit). A malicious device should not be able to
 * trigger an unbounded memcpy into a fixed-size buffer.
 */

#define WIIUSE_MAX_PAYLOAD 23  /* Standard HID report payload size for Wii */

START_TEST(test_read_data_length_bounds)
{
    /* Invariant: read requests with len > buffer capacity must be rejected or clamped */
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    /* Test lengths: overflow case, boundary, and valid */
    unsigned short test_lens[] = {
        0xFFFF,                    /* Exploit: massive length causing heap overflow */
        WIIUSE_MAX_PAYLOAD + 100,  /* Boundary: exceeds payload buffer */
        16,                        /* Valid: fits in buffer */
    };
    int num_tests = sizeof(test_lens) / sizeof(test_lens[0]);

    for (int i = 0; i < num_tests; i++) {
        unsigned short len = test_lens[i];
        /* wiiuse_read_data returns 0 on failure/invalid params, 1 on success */
        int ret = wiiuse_read_data(wm[0], NULL, 0, 0x0000, len);
        if (len > WIIUSE_MAX_PAYLOAD) {
            /* Oversized requests must not succeed without bounds checking */
            ck_assert_msg(ret == 0,
                "read_data accepted dangerous len=%u without bounds check", len);
        }
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

    tcase_add_test(tc_core, test_read_data_length_bounds);
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