#include <check.h>
#include <stdlib.h>

/* wiiuse internal headers for struct definitions */
#include "wiiuse_internal.h"
#include "wiiuse.h"

/* Test seam: state_changed() is un-static'd for exactly this purpose,
 * matching the existing precedent for handle_wm_accel(). */
#include "events.h"

/* For WIIMOTE_ACCEL_{X,Y,Z}SHIFT / NUNCHUK_ACCEL_{X,Y,Z}SHIFT /
 * NUNCHUK_PASSTHROUGH_ACCEL_{X,Y,Z}SHIFT -- accel_threshold is rescaled
 * by these same constants rather than re-deriving them. */
#include "dynamics.h"

/*
 * accel_threshold per-axis rescale.
 *
 * state_changed() is exercised directly on a freshly wiiuse_init()'d
 * wiimote so only the accelerometer path under test can report a
 * change. orient_threshold (and, for the nunchuk paths, the nunchuk's
 * own orient_threshold) is set high enough that the always-zero
 * default orientation never crosses it and masks the result.
 */

/* --- Direct wiimote accelerometer ------------------------------------- */

START_TEST(test_wiimote_raw_accel_below_threshold_no_event)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->orient_threshold = 1000.0f;
    WIIMOTE_ENABLE_STATE(wm[0], WIIMOTE_STATE_ACC);
    wm[0]->accel_threshold = 5;

    wm[0]->lstate.accel.x = 1000;
    wm[0]->lstate.accel.y = 1000;
    wm[0]->lstate.accel.z = 1000;

    /* One raw unit short of each axis's own scaled threshold. */
    wm[0]->accel.x = (uint16_t)(1000 + (wm[0]->accel_threshold << WIIMOTE_ACCEL_XSHIFT) - 1);
    wm[0]->accel.y = (uint16_t)(1000 + (wm[0]->accel_threshold << WIIMOTE_ACCEL_YSHIFT) - 1);
    wm[0]->accel.z = (uint16_t)(1000 + (wm[0]->accel_threshold << WIIMOTE_ACCEL_ZSHIFT) - 1);

    ck_assert_int_eq(state_changed(wm[0]), 0);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_wiimote_raw_accel_x_at_threshold_triggers_event)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->orient_threshold = 1000.0f;
    WIIMOTE_ENABLE_STATE(wm[0], WIIMOTE_STATE_ACC);
    wm[0]->accel_threshold = 5;

    wm[0]->lstate.accel.x = 1000;
    wm[0]->lstate.accel.y = 1000;
    wm[0]->lstate.accel.z = 1000;

    wm[0]->accel.x = (uint16_t)(1000 + (wm[0]->accel_threshold << WIIMOTE_ACCEL_XSHIFT));
    wm[0]->accel.y = 1000;
    wm[0]->accel.z = 1000;

    ck_assert_int_eq(state_changed(wm[0]), 1);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_wiimote_raw_accel_y_at_threshold_triggers_event)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->orient_threshold = 1000.0f;
    WIIMOTE_ENABLE_STATE(wm[0], WIIMOTE_STATE_ACC);
    wm[0]->accel_threshold = 5;

    wm[0]->lstate.accel.x = 1000;
    wm[0]->lstate.accel.y = 1000;
    wm[0]->lstate.accel.z = 1000;

    wm[0]->accel.x = 1000;
    wm[0]->accel.y = (uint16_t)(1000 + (wm[0]->accel_threshold << WIIMOTE_ACCEL_YSHIFT));
    wm[0]->accel.z = 1000;

    ck_assert_int_eq(state_changed(wm[0]), 1);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_wiimote_raw_accel_z_at_threshold_triggers_event)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->orient_threshold = 1000.0f;
    WIIMOTE_ENABLE_STATE(wm[0], WIIMOTE_STATE_ACC);
    wm[0]->accel_threshold = 5;

    wm[0]->lstate.accel.x = 1000;
    wm[0]->lstate.accel.y = 1000;
    wm[0]->lstate.accel.z = 1000;

    wm[0]->accel.x = 1000;
    wm[0]->accel.y = 1000;
    wm[0]->accel.z = (uint16_t)(1000 + (wm[0]->accel_threshold << WIIMOTE_ACCEL_ZSHIFT));

    ck_assert_int_eq(state_changed(wm[0]), 1);

    wiiuse_cleanup(wm, 1);
}
END_TEST

/*
 * wiiuse_set_accel_threshold()/wiiuse_set_nunchuk_accel_threshold() accept
 * any int, unvalidated. Left-shifting a negative value directly would be
 * undefined behavior in C; SCALE_ACCEL_THRESHOLD (events.c) shifts through
 * unsigned instead, so this must produce a deterministic result rather
 * than crash. Since accel hasn't moved (diff 0), comparing against a
 * negative threshold always reports a change either way.
 */
START_TEST(test_wiimote_negative_accel_threshold_is_well_defined)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->orient_threshold = 1000.0f;
    WIIMOTE_ENABLE_STATE(wm[0], WIIMOTE_STATE_ACC);
    wm[0]->accel_threshold = -5;

    wm[0]->lstate.accel.x = 1000;
    wm[0]->lstate.accel.y = 1000;
    wm[0]->lstate.accel.z = 1000;
    wm[0]->accel.x         = 1000;
    wm[0]->accel.y         = 1000;
    wm[0]->accel.z         = 1000;

    ck_assert_int_eq(state_changed(wm[0]), 1);

    wiiuse_cleanup(wm, 1);
}
END_TEST

/* --- Direct (non-passthrough) nunchuk accelerometer -------------------- */

START_TEST(test_direct_nunchuk_raw_accel_below_threshold_no_event)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->orient_threshold          = 1000.0f;
    wm[0]->exp.type                  = EXP_NUNCHUK;
    wm[0]->exp.nunchuk.orient_threshold = 1000.0f;
    wm[0]->exp.nunchuk.accel_threshold  = 5;

    wm[0]->lstate.exp_accel.x = 1000;
    wm[0]->lstate.exp_accel.y = 1000;
    wm[0]->lstate.exp_accel.z = 1000;

    wm[0]->exp.nunchuk.accel.x = (uint16_t)(1000 + (wm[0]->exp.nunchuk.accel_threshold << NUNCHUK_ACCEL_XSHIFT) - 1);
    wm[0]->exp.nunchuk.accel.y = (uint16_t)(1000 + (wm[0]->exp.nunchuk.accel_threshold << NUNCHUK_ACCEL_YSHIFT) - 1);
    wm[0]->exp.nunchuk.accel.z = (uint16_t)(1000 + (wm[0]->exp.nunchuk.accel_threshold << NUNCHUK_ACCEL_ZSHIFT) - 1);

    ck_assert_int_eq(state_changed(wm[0]), 0);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_direct_nunchuk_raw_accel_at_threshold_triggers_event)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->orient_threshold          = 1000.0f;
    wm[0]->exp.type                  = EXP_NUNCHUK;
    wm[0]->exp.nunchuk.orient_threshold = 1000.0f;
    wm[0]->exp.nunchuk.accel_threshold  = 5;

    wm[0]->lstate.exp_accel.x = 1000;
    wm[0]->lstate.exp_accel.y = 1000;
    wm[0]->lstate.exp_accel.z = 1000;

    /* Nunchuk precision is uniform across axes, unlike the wiimote's
     * asymmetric split -- one representative axis is enough. */
    wm[0]->exp.nunchuk.accel.x = (uint16_t)(1000 + (wm[0]->exp.nunchuk.accel_threshold << NUNCHUK_ACCEL_XSHIFT));
    wm[0]->exp.nunchuk.accel.y = 1000;
    wm[0]->exp.nunchuk.accel.z = 1000;

    ck_assert_int_eq(state_changed(wm[0]), 1);

    wiiuse_cleanup(wm, 1);
}
END_TEST

/* --- Motion+ nunchuk-passthrough accelerometer -------------------------
 *
 * This path never decodes extra precision bits, so its threshold must
 * never be rescaled -- provably, regardless of whether the library was
 * built with WIIUSE_ACCEL_10BIT.
 */

START_TEST(test_motion_plus_passthrough_accel_threshold_never_rescaled)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->orient_threshold          = 1000.0f;
    wm[0]->exp.type                  = EXP_MOTION_PLUS_NUNCHUK;
    wm[0]->exp.nunchuk.orient_threshold = 1000.0f;
    wm[0]->exp.nunchuk.accel_threshold  = 5;

    wm[0]->lstate.exp_accel.x = 1000;

    /* One below the *unscaled* threshold: never triggers, on any build. */
    wm[0]->exp.nunchuk.accel.x =
        (uint16_t)(1000 + (wm[0]->exp.nunchuk.accel_threshold << NUNCHUK_PASSTHROUGH_ACCEL_XSHIFT) - 1);
    ck_assert_int_eq(state_changed(wm[0]), 0);

    /* Exactly at the unscaled threshold: always triggers, on any build --
     * NUNCHUK_PASSTHROUGH_ACCEL_XSHIFT is 0 unconditionally. */
    wm[0]->exp.nunchuk.accel.x = (uint16_t)(1000 + wm[0]->exp.nunchuk.accel_threshold);
    ck_assert_int_eq(state_changed(wm[0]), 1);

    wiiuse_cleanup(wm, 1);
}
END_TEST

Suite *events_accel_threshold_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s       = suite_create("EventsAccelThreshold");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_wiimote_raw_accel_below_threshold_no_event);
    tcase_add_test(tc_core, test_wiimote_raw_accel_x_at_threshold_triggers_event);
    tcase_add_test(tc_core, test_wiimote_raw_accel_y_at_threshold_triggers_event);
    tcase_add_test(tc_core, test_wiimote_raw_accel_z_at_threshold_triggers_event);
    tcase_add_test(tc_core, test_wiimote_negative_accel_threshold_is_well_defined);
    tcase_add_test(tc_core, test_direct_nunchuk_raw_accel_below_threshold_no_event);
    tcase_add_test(tc_core, test_direct_nunchuk_raw_accel_at_threshold_triggers_event);
    tcase_add_test(tc_core, test_motion_plus_passthrough_accel_threshold_never_rescaled);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s  = events_accel_threshold_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
