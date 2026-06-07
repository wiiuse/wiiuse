#include <check.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* wiiuse internal headers for struct definitions */
#include "wiiuse_internal.h"
#include "wiiuse.h"
#include "nunchuk.h" /* for nunchuk_event() */

/* For NUNCHUK_ACCEL_{X,Y,Z}SHIFT -- the calibrated-precision scaling
 * below reuses these real constants rather than re-deriving them. */
#include "dynamics.h"

/*
 * Nunchuk extended-precision (10-bit) accelerometer tests. Compiled twice
 * by CI (WIIUSE_ACCEL_10BIT off and on); assertions that only apply to one
 * configuration are guarded with #ifdef WIIUSE_ACCEL_10BIT.
 *
 * Test seam: construct a wiimote_t via wiiuse_init(1), reach into
 * wm[0]->exp.nunchuk, and call nunchuk_event() directly with a hand-built
 * 6-byte report buffer, without needing a real Bluetooth transport.
 */

/*
 * Nunchuk calibrated-output precision.
 *
 * Plain, ungated reimplementation of calculate_orientation()/
 * calculate_gforce()'s math (src/dynamics.c): scale the byte-range
 * calibration data up to raw_{x,y,z}'s own scale by the given per-axis
 * shift before doing the existing roll/pitch/gforce math against the
 * full-precision raw value. Passing the real NUNCHUK_ACCEL_*SHIFT
 * constants models a flag-on build; passing 0 models what a flag-off
 * build (or the pre-extended-precision decode) would have computed
 * from the same base byte values.
 */
static void reference_orientation_gforce(int raw_x, int raw_y, int raw_z, byte cal_zero_x, byte cal_zero_y,
                                          byte cal_zero_z, byte cal_g_x, byte cal_g_y, byte cal_g_z, int shift_x,
                                          int shift_y, int shift_z, float *out_roll, float *out_pitch, float *out_gx,
                                          float *out_gy, float *out_gz)
{
    int calzero_x = (int)cal_zero_x << shift_x;
    int calzero_y = (int)cal_zero_y << shift_y;
    int calzero_z = (int)cal_zero_z << shift_z;
    int calg_x    = (int)cal_g_x << shift_x;
    int calg_y    = (int)cal_g_y << shift_y;
    int calg_z    = (int)cal_g_z << shift_z;

    float xg = (float)calg_x;
    float yg = (float)calg_y;
    float zg = (float)calg_z;

    float x = ((float)raw_x - (float)calzero_x) / xg;
    float y = ((float)raw_y - (float)calzero_y) / yg;
    float z = ((float)raw_z - (float)calzero_z) / zg;

    if (x < -1.0f)
    {
        x = -1.0f;
    } else if (x > 1.0f)
    {
        x = 1.0f;
    }
    if (y < -1.0f)
    {
        y = -1.0f;
    } else if (y > 1.0f)
    {
        y = 1.0f;
    }
    if (z < -1.0f)
    {
        z = -1.0f;
    } else if (z > 1.0f)
    {
        z = 1.0f;
    }

    *out_roll  = NAN;
    *out_pitch = NAN;

    if (abs(raw_x - calzero_x) <= calg_x)
    {
        *out_roll = (atan2f(x, z) * 180.0f) / (float)M_PI;
    }

    if (abs(raw_y - calzero_y) <= calg_y)
    {
        *out_pitch = (atan2f(y, sqrtf(x * x + z * z)) * 180.0f) / (float)M_PI;
    }

    *out_gx = ((float)raw_x - (float)calzero_x) / xg;
    *out_gy = ((float)raw_y - (float)calzero_y) / yg;
    *out_gz = ((float)raw_z - (float)calzero_z) / zg;
}

/* Build a fresh wiimote_t with a nunchuk attached and known-nonzero
 * calibration data (wiiuse_init() leaves accel_calib zeroed, which would
 * divide by zero in calculate_orientation()/calculate_gforce()). */
static struct wiimote_t **make_wiimote_with_nunchuk_calib(byte cal_zero_x, byte cal_zero_y, byte cal_zero_z,
                                                            byte cal_g_x, byte cal_g_y, byte cal_g_z)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    /* nunchuk_event() reads through nc->flags, only populated by a real
     * handshake; point it at the wiimote's own flags word like
     * nunchuk_handshake() does, and disable smoothing so a single call
     * produces the unsmoothed angle directly. */
    wm[0]->exp.nunchuk.flags = &wm[0]->flags;
    wm[0]->flags &= ~WIIUSE_SMOOTHING;

    wm[0]->exp.nunchuk.accel_calib.cal_zero.x = cal_zero_x;
    wm[0]->exp.nunchuk.accel_calib.cal_zero.y = cal_zero_y;
    wm[0]->exp.nunchuk.accel_calib.cal_zero.z = cal_zero_z;
    wm[0]->exp.nunchuk.accel_calib.cal_g.x    = cal_g_x;
    wm[0]->exp.nunchuk.accel_calib.cal_g.y    = cal_g_y;
    wm[0]->exp.nunchuk.accel_calib.cal_g.z    = cal_g_z;

    return wm;
}

#ifndef WIIUSE_ACCEL_10BIT

START_TEST(test_flag_off_preserves_byte_values)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->exp.nunchuk.flags = &wm[0]->flags;

    byte msg[6] = {0x80, 0x80, 0x12, 0x34, 0x56, 0xFF};

    nunchuk_event(&wm[0]->exp.nunchuk, msg);

    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.x, msg[2]);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.y, msg[3]);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.z, msg[4]);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_flag_off_preserves_byte_values_all_zero)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->exp.nunchuk.flags = &wm[0]->flags;

    byte msg[6] = {0x80, 0x80, 0x00, 0x00, 0x00, 0x00};

    nunchuk_event(&wm[0]->exp.nunchuk, msg);

    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.x, 0);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.y, 0);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.z, 0);

    wiiuse_cleanup(wm, 1);
}
END_TEST

#else /* WIIUSE_ACCEL_10BIT */

START_TEST(test_flag_on_extra_bits_all_zero)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->exp.nunchuk.flags = &wm[0]->flags;

    byte msg[6] = {0x80, 0x80, 0xAB, 0xCD, 0xEF, 0x00};

    nunchuk_event(&wm[0]->exp.nunchuk, msg);

    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.x, ((uint16_t)0xAB << 2) | 0);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.y, ((uint16_t)0xCD << 2) | 0);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.z, ((uint16_t)0xEF << 2) | 0);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_flag_on_extra_bits_all_one)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->exp.nunchuk.flags = &wm[0]->flags;

    byte msg[6] = {0x80, 0x80, 0x00, 0x00, 0x00, 0xFF};

    nunchuk_event(&wm[0]->exp.nunchuk, msg);

    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.x, ((uint16_t)0x00 << 2) | 0x3);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.y, ((uint16_t)0x00 << 2) | 0x3);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.z, ((uint16_t)0x00 << 2) | 0x3);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_flag_on_mixed_bit_pattern)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->exp.nunchuk.flags = &wm[0]->flags;

    /* msg[5] = 0b10011001:
     *   bits 0-1 (button bits, ignored by accel decode) = 0b01
     *   bits 2-3 (X lsb)                                = 0b10
     *   bits 4-5 (Y lsb)                                = 0b01
     *   bits 6-7 (Z lsb)                                = 0b10
     */
    byte msg5   = 0x99; /* 0b10011001 */
    byte msg[6] = {0x80, 0x80, 0x01, 0x02, 0x03, msg5};

    uint16_t expected_x = ((uint16_t)0x01 << 2) | ((msg5 >> 2) & 0x3);
    uint16_t expected_y = ((uint16_t)0x02 << 2) | ((msg5 >> 4) & 0x3);
    uint16_t expected_z = ((uint16_t)0x03 << 2) | ((msg5 >> 6) & 0x3);

    nunchuk_event(&wm[0]->exp.nunchuk, msg);

    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.x, expected_x);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.y, expected_y);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.z, expected_z);

    wiiuse_cleanup(wm, 1);
}
END_TEST

/*
 * Confirm the Nunchuk's 10/10/10 layout (every axis <<2, from its own
 * buttons byte msg[5]) is distinct from the Wiimote's 10/9/9 (<<1 on
 * Y/Z, extra bits from msg[0]/msg[1]) - would fail if someone
 * accidentally reused the Wiimote's bit positions/shift amounts.
 */
START_TEST(test_flag_on_nunchuk_layout_distinct_from_wiimote)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);
    wm[0]->exp.nunchuk.flags = &wm[0]->flags;

    /* msg[0]/msg[1] carry a recognizable non-zero pattern - the Wiimote's
     * would-be Core Buttons bytes - to prove the Nunchuk decode never
     * reads from them. */
    byte msg[6] = {0xFF, 0xFF, 0x40, 0x40, 0x40, 0x54}; /* 0x54 = 0b01010100 */

    nunchuk_event(&wm[0]->exp.nunchuk, msg);

    uint16_t expected_x = ((uint16_t)0x40 << 2) | 0x1;
    uint16_t expected_y = ((uint16_t)0x40 << 2) | 0x1;
    uint16_t expected_z = ((uint16_t)0x40 << 2) | 0x1;

    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.x, expected_x);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.y, expected_y);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.z, expected_z);

    /* Changing only msg[0]/msg[1] must not change the decoded value. */
    byte msg_diff_core[6] = {0x00, 0x00, 0x40, 0x40, 0x40, 0x54};
    nunchuk_event(&wm[0]->exp.nunchuk, msg_diff_core);

    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.x, expected_x);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.y, expected_y);
    ck_assert_uint_eq(wm[0]->exp.nunchuk.accel.z, expected_z);

    wiiuse_cleanup(wm, 1);
}
END_TEST

#endif /* WIIUSE_ACCEL_10BIT */

/*
 * calculate_orientation()/calculate_gforce() parity between the library
 * and an independent reference implementation, for the same underlying
 * raw input. Not gated by #ifdef - both builds run this same test; the
 * shift passed to the reference is the real NUNCHUK_ACCEL_*SHIFT, which
 * collapses to 0 in a flag-off build.
 */
START_TEST(test_orientation_gforce_parity_with_reference)
{
    byte cal_zero_x = 0x80, cal_zero_y = 0x80, cal_zero_z = 0x80;
    byte cal_g_x = 0x28, cal_g_y = 0x28, cal_g_z = 0x28;

    struct wiimote_t **wm =
        make_wiimote_with_nunchuk_calib(cal_zero_x, cal_zero_y, cal_zero_z, cal_g_x, cal_g_y, cal_g_z);

    byte ax = 0x90, ay = 0x70, az = 0xA0;
    byte msg[6] = {0x80, 0x80, ax, ay, az, 0x00};

    nunchuk_event(&wm[0]->exp.nunchuk, msg);

    float ref_roll, ref_pitch, ref_gx, ref_gy, ref_gz;
    reference_orientation_gforce((int)wm[0]->exp.nunchuk.accel.x, (int)wm[0]->exp.nunchuk.accel.y,
                                  (int)wm[0]->exp.nunchuk.accel.z, cal_zero_x, cal_zero_y, cal_zero_z, cal_g_x,
                                  cal_g_y, cal_g_z, NUNCHUK_ACCEL_XSHIFT, NUNCHUK_ACCEL_YSHIFT, NUNCHUK_ACCEL_ZSHIFT,
                                  &ref_roll, &ref_pitch, &ref_gx, &ref_gy, &ref_gz);

    if (isnan(ref_roll))
    {
        ck_assert(isnan(wm[0]->exp.nunchuk.orient.roll) || wm[0]->exp.nunchuk.orient.roll == 0.0f);
    } else
    {
        ck_assert_float_eq_tol(wm[0]->exp.nunchuk.orient.roll, ref_roll, 0.0001f);
    }

    if (isnan(ref_pitch))
    {
        ck_assert(isnan(wm[0]->exp.nunchuk.orient.pitch) || wm[0]->exp.nunchuk.orient.pitch == 0.0f);
    } else
    {
        ck_assert_float_eq_tol(wm[0]->exp.nunchuk.orient.pitch, ref_pitch, 0.0001f);
    }

    ck_assert_float_eq_tol(wm[0]->exp.nunchuk.gforce.x, ref_gx, 0.0001f);
    ck_assert_float_eq_tol(wm[0]->exp.nunchuk.gforce.y, ref_gy, 0.0001f);
    ck_assert_float_eq_tol(wm[0]->exp.nunchuk.gforce.z, ref_gz, 0.0001f);

    wiiuse_cleanup(wm, 1);
}
END_TEST

#ifdef WIIUSE_ACCEL_10BIT

START_TEST(test_calibrated_output_matches_flagoff_when_extra_bits_zero)
{
    byte cal_zero_x = 0x80, cal_zero_y = 0x80, cal_zero_z = 0x80;
    byte cal_g_x = 0x28, cal_g_y = 0x28, cal_g_z = 0x28;

    struct wiimote_t **wm =
        make_wiimote_with_nunchuk_calib(cal_zero_x, cal_zero_y, cal_zero_z, cal_g_x, cal_g_y, cal_g_z);

    byte ax = 0x90, ay = 0x70, az = 0xA0;
    /* No extra-precision bits set in the Nunchuk buttons byte. */
    byte msg[6] = {0x80, 0x80, ax, ay, az, 0x00};

    nunchuk_event(&wm[0]->exp.nunchuk, msg);

    float flagoff_roll, flagoff_pitch, flagoff_gx, flagoff_gy, flagoff_gz;
    reference_orientation_gforce((int)ax, (int)ay, (int)az, cal_zero_x, cal_zero_y, cal_zero_z, cal_g_x, cal_g_y,
                                  cal_g_z, 0, 0, 0, &flagoff_roll, &flagoff_pitch, &flagoff_gx, &flagoff_gy,
                                  &flagoff_gz);

    ck_assert_float_eq_tol(wm[0]->exp.nunchuk.gforce.x, flagoff_gx, 0.0001f);
    ck_assert_float_eq_tol(wm[0]->exp.nunchuk.gforce.y, flagoff_gy, 0.0001f);
    ck_assert_float_eq_tol(wm[0]->exp.nunchuk.gforce.z, flagoff_gz, 0.0001f);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_calibrated_output_diverges_when_extra_bits_nonzero)
{
    byte cal_zero_x = 0x80, cal_zero_y = 0x80, cal_zero_z = 0x80;
    byte cal_g_x = 0x28, cal_g_y = 0x28, cal_g_z = 0x28;

    struct wiimote_t **wm =
        make_wiimote_with_nunchuk_calib(cal_zero_x, cal_zero_y, cal_zero_z, cal_g_x, cal_g_y, cal_g_z);

    byte ax = 0x90, ay = 0x70, az = 0xA0;
    /* All extra-precision bits set (msg[5] bits 2-7). */
    byte msg[6] = {0x80, 0x80, ax, ay, az, 0xFC};

    nunchuk_event(&wm[0]->exp.nunchuk, msg);

    float expected_roll, expected_pitch, expected_gx, expected_gy, expected_gz;
    reference_orientation_gforce((int)wm[0]->exp.nunchuk.accel.x, (int)wm[0]->exp.nunchuk.accel.y,
                                  (int)wm[0]->exp.nunchuk.accel.z, cal_zero_x, cal_zero_y, cal_zero_z, cal_g_x,
                                  cal_g_y, cal_g_z, NUNCHUK_ACCEL_XSHIFT, NUNCHUK_ACCEL_YSHIFT, NUNCHUK_ACCEL_ZSHIFT,
                                  &expected_roll, &expected_pitch, &expected_gx, &expected_gy, &expected_gz);

    ck_assert_float_eq_tol(wm[0]->exp.nunchuk.gforce.x, expected_gx, 0.0001f);
    ck_assert_float_eq_tol(wm[0]->exp.nunchuk.gforce.y, expected_gy, 0.0001f);
    ck_assert_float_eq_tol(wm[0]->exp.nunchuk.gforce.z, expected_gz, 0.0001f);

    /* And prove that output actually diverges from what a flag-off build
     * would have computed from the same base byte values. */
    float flagoff_roll, flagoff_pitch, flagoff_gx, flagoff_gy, flagoff_gz;
    reference_orientation_gforce((int)ax, (int)ay, (int)az, cal_zero_x, cal_zero_y, cal_zero_z, cal_g_x, cal_g_y,
                                  cal_g_z, 0, 0, 0, &flagoff_roll, &flagoff_pitch, &flagoff_gx, &flagoff_gy,
                                  &flagoff_gz);

    ck_assert(fabsf(wm[0]->exp.nunchuk.gforce.x - flagoff_gx) > 1e-3f);
    ck_assert(fabsf(wm[0]->exp.nunchuk.gforce.y - flagoff_gy) > 1e-3f);
    ck_assert(fabsf(wm[0]->exp.nunchuk.gforce.z - flagoff_gz) > 1e-3f);

    wiiuse_cleanup(wm, 1);
}
END_TEST

#endif /* WIIUSE_ACCEL_10BIT */

/*
 * The MotionPlus-Nunchuk-passthrough path (src/motion_plus.c) never
 * decodes extra precision bits, so its calibrated output must stay
 * unextended. Calls calculate_gforce() the way
 * motion_plus.c does - with NUNCHUK_PASSTHROUGH_ACCEL_*SHIFT, not
 * NUNCHUK_ACCEL_*SHIFT - against the flag-off (shift 0) reference; since
 * that shift is unconditionally 0, they must always agree. Not gated by
 * #ifdef - both builds run this same test.
 */
START_TEST(test_passthrough_calibration_never_extended_regardless_of_flag)
{
    byte cal_zero_x = 0x80, cal_zero_y = 0x80, cal_zero_z = 0x80;
    byte cal_g_x = 0x28, cal_g_y = 0x28, cal_g_z = 0x28;
    byte ax = 0x90, ay = 0x70, az = 0xA0;

    struct accel_t cal;
    memset(&cal, 0, sizeof(cal));
    cal.cal_zero.x = cal_zero_x;
    cal.cal_zero.y = cal_zero_y;
    cal.cal_zero.z = cal_zero_z;
    cal.cal_g.x    = cal_g_x;
    cal.cal_g.y    = cal_g_y;
    cal.cal_g.z    = cal_g_z;

    struct vec3w_t raw = {ax, ay, az};
    struct gforce_t passthrough_gforce;
    calculate_gforce(&cal, &raw, &passthrough_gforce, NUNCHUK_PASSTHROUGH_ACCEL_XSHIFT,
                     NUNCHUK_PASSTHROUGH_ACCEL_YSHIFT, NUNCHUK_PASSTHROUGH_ACCEL_ZSHIFT);

    float flagoff_roll, flagoff_pitch, flagoff_gx, flagoff_gy, flagoff_gz;
    reference_orientation_gforce((int)ax, (int)ay, (int)az, cal_zero_x, cal_zero_y, cal_zero_z, cal_g_x, cal_g_y,
                                  cal_g_z, 0, 0, 0, &flagoff_roll, &flagoff_pitch, &flagoff_gx, &flagoff_gy,
                                  &flagoff_gz);

    ck_assert_float_eq_tol(passthrough_gforce.x, flagoff_gx, 0.0001f);
    ck_assert_float_eq_tol(passthrough_gforce.y, flagoff_gy, 0.0001f);
    ck_assert_float_eq_tol(passthrough_gforce.z, flagoff_gz, 0.0001f);
}
END_TEST

Suite *nunchuk_accel_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s       = suite_create("NunchukAccel");
    tc_core = tcase_create("Core");

#ifndef WIIUSE_ACCEL_10BIT
    tcase_add_test(tc_core, test_flag_off_preserves_byte_values);
    tcase_add_test(tc_core, test_flag_off_preserves_byte_values_all_zero);
#else
    tcase_add_test(tc_core, test_flag_on_extra_bits_all_zero);
    tcase_add_test(tc_core, test_flag_on_extra_bits_all_one);
    tcase_add_test(tc_core, test_flag_on_mixed_bit_pattern);
    tcase_add_test(tc_core, test_flag_on_nunchuk_layout_distinct_from_wiimote);
#endif
    tcase_add_test(tc_core, test_orientation_gforce_parity_with_reference);

#ifdef WIIUSE_ACCEL_10BIT
    tcase_add_test(tc_core, test_calibrated_output_matches_flagoff_when_extra_bits_zero);
    tcase_add_test(tc_core, test_calibrated_output_diverges_when_extra_bits_nonzero);
#endif
    tcase_add_test(tc_core, test_passthrough_calibration_never_extended_regardless_of_flag);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s  = nunchuk_accel_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
