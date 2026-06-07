#include <check.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* wiiuse internal headers for struct definitions */
#include "wiiuse_internal.h"
#include "wiiuse.h"

/* Test seam: handle_wm_accel() is un-static'd for exactly this purpose. */
#include "events.h"

/* Test seam: wiiuse_decode_accel_calibration_lsb() is always compiled. */
#include "io.h"

/* For WIIMOTE_ACCEL_{X,Y,Z}SHIFT -- the calibrated-precision scaling
 * below reuses these real constants rather than re-deriving them. */
#include "dynamics.h"

/*
 * Wiimote extended-precision (10/9/9-bit) accelerometer.
 * Calls handle_wm_accel() directly with hand-built 5-byte msg arrays
 * (msg[0]/msg[1] = Core Buttons bytes, msg[2..4] = accel bytes), as a
 * real WM_RPT_BTN_ACC report would lay them out.
 */

#define MSG_LEN 5

static void make_msg(byte *msg, byte core0, byte core1, byte ax, byte ay, byte az)
{
    memset(msg, 0, MSG_LEN);
    msg[0] = core0;
    msg[1] = core1;
    msg[2] = ax;
    msg[3] = ay;
    msg[4] = az;
}

#ifndef WIIUSE_ACCEL_10BIT

START_TEST(test_flag_off_preserves_raw_bytes)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    byte msg[MSG_LEN];
    /* Core Buttons bytes carry non-zero data in what would be the extra
     * precision bits if the flag were on -- must be ignored here. */
    make_msg(msg, 0x60, 0x60, 0xAB, 0x12, 0x34);

    handle_wm_accel(wm[0], msg);

    ck_assert_uint_eq(wm[0]->accel.x, msg[2]);
    ck_assert_uint_eq(wm[0]->accel.y, msg[3]);
    ck_assert_uint_eq(wm[0]->accel.z, msg[4]);

    wiiuse_cleanup(wm, 1);
}
END_TEST

#else

START_TEST(test_flag_on_extra_bits_all_zero)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    byte msg[MSG_LEN];
    make_msg(msg, 0x00, 0x00, 0xAB, 0x12, 0x34);

    handle_wm_accel(wm[0], msg);

    uint16_t expected_x = ((uint16_t)msg[2] << 2);
    uint16_t expected_y = ((uint16_t)msg[3] << 1);
    uint16_t expected_z = ((uint16_t)msg[4] << 1);

    ck_assert_uint_eq(wm[0]->accel.x, expected_x);
    ck_assert_uint_eq(wm[0]->accel.y, expected_y);
    ck_assert_uint_eq(wm[0]->accel.z, expected_z);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_flag_on_extra_bits_all_one)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    byte msg[MSG_LEN];
    make_msg(msg, 0x60, 0x60, 0xAB, 0x12, 0x34);

    handle_wm_accel(wm[0], msg);

    uint16_t expected_x = ((uint16_t)msg[2] << 2) | 0x3;
    uint16_t expected_y = ((uint16_t)msg[3] << 1) | 0x1;
    uint16_t expected_z = ((uint16_t)msg[4] << 1) | 0x1;

    ck_assert_uint_eq(wm[0]->accel.x, expected_x);
    ck_assert_uint_eq(wm[0]->accel.y, expected_y);
    ck_assert_uint_eq(wm[0]->accel.z, expected_z);

    wiiuse_cleanup(wm, 1);
}
END_TEST

/*
 * Mixed pattern #1: only msg[0] bit5 (X bit0) and msg[1] bit6 (Z bit0)
 * are set. This would catch a mistake that swaps X's bit0/bit1 sources
 * (msg[0] bit5 vs bit6) or confuses Y's source (msg[1] bit5) with Z's
 * (msg[1] bit6).
 */
START_TEST(test_flag_on_extra_bits_mixed_pattern_1)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    byte msg[MSG_LEN];
    make_msg(msg, 0x20 /* bit5 only */, 0x40 /* bit6 only */, 0xAB, 0x12, 0x34);

    handle_wm_accel(wm[0], msg);

    uint16_t expected_x = ((uint16_t)msg[2] << 2) | 0x1;
    uint16_t expected_y = ((uint16_t)msg[3] << 1) | 0x0;
    uint16_t expected_z = ((uint16_t)msg[4] << 1) | 0x1;

    ck_assert_uint_eq(wm[0]->accel.x, expected_x);
    ck_assert_uint_eq(wm[0]->accel.y, expected_y);
    ck_assert_uint_eq(wm[0]->accel.z, expected_z);

    wiiuse_cleanup(wm, 1);
}
END_TEST

/*
 * Mixed pattern #2: the mirror image of pattern #1 -- msg[0] bit6 (X
 * bit1) and msg[1] bit5 (Y bit0) are set instead.
 */
START_TEST(test_flag_on_extra_bits_mixed_pattern_2)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    byte msg[MSG_LEN];
    make_msg(msg, 0x40 /* bit6 only */, 0x20 /* bit5 only */, 0xAB, 0x12, 0x34);

    handle_wm_accel(wm[0], msg);

    uint16_t expected_x = ((uint16_t)msg[2] << 2) | 0x2;
    uint16_t expected_y = ((uint16_t)msg[3] << 1) | 0x1;
    uint16_t expected_z = ((uint16_t)msg[4] << 1) | 0x0;

    ck_assert_uint_eq(wm[0]->accel.x, expected_x);
    ck_assert_uint_eq(wm[0]->accel.y, expected_y);
    ck_assert_uint_eq(wm[0]->accel.z, expected_z);

    wiiuse_cleanup(wm, 1);
}
END_TEST

#endif /* WIIUSE_ACCEL_10BIT */

/* Wiimote accelerometer calibration-data extra-precision bits. */

START_TEST(test_decode_calibration_lsb)
{
    /* Zero point 10, one-g point 110 on every axis, so the whole-byte delta
     * is 100 and the full-precision delta is 400 (X, 2 extra bits) / 200
     * (Y, Z, 1 extra bit), minus the zero point's extra bits plus the
     * one-g point's. Each row pins down which physical bit feeds which
     * axis, that Y/Z's unused low bit (bits 2 and 0) is ignored, that
     * all bits at once don't overlap, and that the two bytes don't bleed
     * into each other. */
    static const struct
    {
        byte zero_bits, g_bits;
        byte zero_lsb_x, zero_lsb_y, zero_lsb_z;
        int delta_x, delta_y, delta_z;
    } cases[] = {
        {0x01, 0x00, 0, 0, 0, 400, 200, 200}, /* z lower bit - ignored */
        {0x02, 0x00, 0, 0, 1, 400, 200, 199}, /* z upper bit */
        {0x04, 0x00, 0, 0, 0, 400, 200, 200}, /* y lower bit - ignored */
        {0x08, 0x00, 0, 1, 0, 400, 199, 200}, /* y upper bit */
        {0x10, 0x00, 1, 0, 0, 399, 200, 200}, /* x bit 0 */
        {0x20, 0x00, 2, 0, 0, 398, 200, 200}, /* x bit 1 */
        {0x3F, 0x3F, 3, 1, 1, 400, 200, 200}, /* all bits, both bytes */
        {0x30, 0x08, 3, 0, 0, 397, 201, 200}, /* different bytes: X=3 / Y=1 */
        {0x00, 0x20, 0, 0, 0, 402, 200, 200}, /* one-g bits alone */
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        struct accel_t accel;
        byte buf[8] = {10, 10, 10, 0, 110, 110, 110, 0};

        memset(&accel, 0, sizeof(accel));
        buf[3] = cases[i].zero_bits;
        buf[7] = cases[i].g_bits;

        wiiuse_decode_accel_calibration_lsb(&accel, buf);

        ck_assert_uint_eq(accel.cal_zero.x, 10);
        ck_assert_uint_eq(accel.cal_zero.y, 10);
        ck_assert_uint_eq(accel.cal_zero.z, 10);
        ck_assert_uint_eq(accel.cal_zero_lsb.x, cases[i].zero_lsb_x);
        ck_assert_uint_eq(accel.cal_zero_lsb.y, cases[i].zero_lsb_y);
        ck_assert_uint_eq(accel.cal_zero_lsb.z, cases[i].zero_lsb_z);

        /* cal_g/cal_g_lsb together must hold the full-precision delta. */
        ck_assert_int_eq((accel.cal_g.x << WIIMOTE_ACCEL_XBITS) | accel.cal_g_lsb.x, cases[i].delta_x);
        ck_assert_int_eq((accel.cal_g.y << WIIMOTE_ACCEL_YBITS) | accel.cal_g_lsb.y, cases[i].delta_y);
        ck_assert_int_eq((accel.cal_g.z << WIIMOTE_ACCEL_ZBITS) | accel.cal_g_lsb.z, cases[i].delta_z);
    }
}
END_TEST

/*
 * Wiimote calibrated-output precision.
 *
 * Plain, ungated reimplementation of calculate_orientation()/
 * calculate_gforce()'s math (src/dynamics.c): scale the byte-range
 * calibration data up to raw_{x,y,z}'s own scale by the given per-axis
 * shift before doing the existing roll/pitch/gforce math against the
 * full-precision raw value. Passing the real WIIMOTE_ACCEL_*SHIFT
 * constants models a flag-on build; passing 0 models what a flag-off
 * build (or the pre-extended-precision decode) would have computed
 * from the same base byte values.
 */
static void reference_orientation_gforce(struct accel_t *ac, int raw_x, int raw_y, int raw_z, int shift_x,
                                          int shift_y, int shift_z, struct orient_t *orient, struct gforce_t *gforce)
{
    float xg, yg, zg;
    float x, y, z;
    int calzero_x = ac->cal_zero.x << shift_x;
    int calzero_y = ac->cal_zero.y << shift_y;
    int calzero_z = ac->cal_zero.z << shift_z;
    int calg_x    = ac->cal_g.x << shift_x;
    int calg_y    = ac->cal_g.y << shift_y;
    int calg_z    = ac->cal_g.z << shift_z;

    orient->yaw = 0.0f;

    xg = (float)calg_x;
    yg = (float)calg_y;
    zg = (float)calg_z;

    x = ((float)raw_x - (float)calzero_x) / xg;
    y = ((float)raw_y - (float)calzero_y) / yg;
    z = ((float)raw_z - (float)calzero_z) / zg;

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

    if (abs(raw_x - calzero_x) <= calg_x)
    {
        float roll     = RAD_TO_DEGREE(atan2f(x, z));
        orient->roll   = roll;
        orient->a_roll = roll;
    }

    if (abs(raw_y - calzero_y) <= calg_y)
    {
        float pitch     = RAD_TO_DEGREE(atan2f(y, sqrtf(x * x + z * z)));
        orient->pitch   = pitch;
        orient->a_pitch = pitch;
    }

    gforce->x = ((float)raw_x - (float)calzero_x) / xg;
    gforce->y = ((float)raw_y - (float)calzero_y) / yg;
    gforce->z = ((float)raw_z - (float)calzero_z) / zg;
}

/*
 * wiiuse_scale_calibration() ORs cal_zero_lsb/cal_g_lsb into the shifted
 * calibration bytes; calculate_orientation()/calculate_gforce() must treat
 * a byte+lsb pair like one pre-combined value (reference run with shift 0).
 */
START_TEST(test_calibration_lsb_combined_with_shift)
{
    struct accel_t ac, combined;
    struct vec3i_t zero, g;
    struct vec3w_t raw;
    struct orient_t orient, ref_orient;
    struct gforce_t gforce, ref_gforce;

    /* cal_g chosen so that (cal_g << shift) | cal_g_lsb still fits the
     * vec3b_t byte range below, since that's what "combined" has to
     * hold to be fed back into reference_orientation_gforce(). */
    memset(&ac, 0, sizeof(ac));
    ac.cal_zero.x = 10;
    ac.cal_zero.y = 20;
    ac.cal_zero.z = 30;
    ac.cal_g.x    = 50;
    ac.cal_g.y    = 100;
    ac.cal_g.z    = 100;
    ac.cal_zero_lsb.x = 3;
    ac.cal_zero_lsb.y = 1;
    ac.cal_zero_lsb.z = 1;
    ac.cal_g_lsb.x    = 2;
    ac.cal_g_lsb.y    = 1;
    ac.cal_g_lsb.z    = 0;

    raw.x = 400;
    raw.y = 300;
    raw.z = 400;

    memset(&orient, 0, sizeof(orient));
    memset(&gforce, 0, sizeof(gforce));
    calculate_orientation(&ac, &raw, &orient, 0, WIIMOTE_ACCEL_XSHIFT, WIIMOTE_ACCEL_YSHIFT, WIIMOTE_ACCEL_ZSHIFT);
    calculate_gforce(&ac, &raw, &gforce, WIIMOTE_ACCEL_XSHIFT, WIIMOTE_ACCEL_YSHIFT, WIIMOTE_ACCEL_ZSHIFT);

    memset(&combined, 0, sizeof(combined));
    zero = wiiuse_scale_calibration(&ac.cal_zero, &ac.cal_zero_lsb, WIIMOTE_ACCEL_XSHIFT, WIIMOTE_ACCEL_YSHIFT,
                             WIIMOTE_ACCEL_ZSHIFT);
    g    = wiiuse_scale_calibration(&ac.cal_g, &ac.cal_g_lsb, WIIMOTE_ACCEL_XSHIFT, WIIMOTE_ACCEL_YSHIFT,
                             WIIMOTE_ACCEL_ZSHIFT);
#ifdef WIIUSE_ACCEL_10BIT
    /* Literal expectations, so dropping the LSB OR can't hide behind the function under test. */
    ck_assert_int_eq(zero.x, (10 << 2) | 3);
    ck_assert_int_eq(zero.y, (20 << 1) | 1);
    ck_assert_int_eq(zero.z, (30 << 1) | 1);
    ck_assert_int_eq(g.x, (50 << 2) | 2);
    ck_assert_int_eq(g.y, (100 << 1) | 1);
    ck_assert_int_eq(g.z, 100 << 1);
#endif
    combined.cal_zero.x = (byte)zero.x;
    combined.cal_zero.y = (byte)zero.y;
    combined.cal_zero.z = (byte)zero.z;
    combined.cal_g.x    = (byte)g.x;
    combined.cal_g.y    = (byte)g.y;
    combined.cal_g.z    = (byte)g.z;

    memset(&ref_orient, 0, sizeof(ref_orient));
    memset(&ref_gforce, 0, sizeof(ref_gforce));
    reference_orientation_gforce(&combined, (int)raw.x, (int)raw.y, (int)raw.z, 0, 0, 0, &ref_orient, &ref_gforce);

    ck_assert_float_eq_tol(orient.roll, ref_orient.roll, 1e-4f);
    ck_assert_float_eq_tol(orient.pitch, ref_orient.pitch, 1e-4f);
    ck_assert_float_eq_tol(gforce.x, ref_gforce.x, 1e-4f);
    ck_assert_float_eq_tol(gforce.y, ref_gforce.y, 1e-4f);
    ck_assert_float_eq_tol(gforce.z, ref_gforce.z, 1e-4f);
}
END_TEST

START_TEST(test_orientation_and_gforce_parity)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    /* wiiuse_init() leaves accel_calib zeroed, which would divide by
     * zero in calculate_orientation()/calculate_gforce() -- set known
     * nonzero calibration values first. */
    wm[0]->accel_calib.cal_zero.x = 10;
    wm[0]->accel_calib.cal_zero.y = 20;
    wm[0]->accel_calib.cal_zero.z = 30;
    wm[0]->accel_calib.cal_g.x    = 100;
    wm[0]->accel_calib.cal_g.y    = 150;
    wm[0]->accel_calib.cal_g.z    = 200;

    /* wiiuse_init() enables WIIUSE_SMOOTHING by default, which would
     * blend orient->roll/pitch with the previous (zeroed) smoothed
     * value rather than reporting the freshly computed angle. Disable
     * it so the library's output is directly comparable to the
     * reference function below, which does not model smoothing. */
    wm[0]->flags &= ~WIIUSE_SMOOTHING;

    byte msg[MSG_LEN];
    byte ax = 100, ay = 150, az = 200;
    /* Non-zero Core Buttons bytes: whatever extra bits a flag-on build
     * decodes out of them end up folded into wm[0]->accel itself, which
     * the reference call below reads back out. */
    make_msg(msg, 0x60, 0x60, ax, ay, az);

    handle_wm_accel(wm[0], msg);

    struct orient_t ref_orient;
    struct gforce_t ref_gforce;
    memset(&ref_orient, 0, sizeof(ref_orient));
    memset(&ref_gforce, 0, sizeof(ref_gforce));
    reference_orientation_gforce(&wm[0]->accel_calib, (int)wm[0]->accel.x, (int)wm[0]->accel.y, (int)wm[0]->accel.z,
                                  WIIMOTE_ACCEL_XSHIFT, WIIMOTE_ACCEL_YSHIFT, WIIMOTE_ACCEL_ZSHIFT, &ref_orient,
                                  &ref_gforce);

    ck_assert_float_eq_tol(wm[0]->orient.roll, ref_orient.roll, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->orient.pitch, ref_orient.pitch, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->orient.yaw, ref_orient.yaw, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->gforce.x, ref_gforce.x, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->gforce.y, ref_gforce.y, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->gforce.z, ref_gforce.z, 1e-4f);

    wiiuse_cleanup(wm, 1);
}
END_TEST

#ifdef WIIUSE_ACCEL_10BIT

START_TEST(test_calibrated_output_matches_flagoff_when_extra_bits_zero)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    wm[0]->accel_calib.cal_zero.x = 10;
    wm[0]->accel_calib.cal_zero.y = 20;
    wm[0]->accel_calib.cal_zero.z = 30;
    wm[0]->accel_calib.cal_g.x    = 100;
    wm[0]->accel_calib.cal_g.y    = 150;
    wm[0]->accel_calib.cal_g.z    = 200;
    wm[0]->flags &= ~WIIUSE_SMOOTHING;

    byte msg[MSG_LEN];
    byte ax = 100, ay = 150, az = 200;
    /* No extra-precision bits set in the Core Buttons bytes. */
    make_msg(msg, 0x00, 0x00, ax, ay, az);

    handle_wm_accel(wm[0], msg);

    struct orient_t flagoff_orient;
    struct gforce_t flagoff_gforce;
    memset(&flagoff_orient, 0, sizeof(flagoff_orient));
    memset(&flagoff_gforce, 0, sizeof(flagoff_gforce));
    reference_orientation_gforce(&wm[0]->accel_calib, (int)ax, (int)ay, (int)az, 0, 0, 0, &flagoff_orient,
                                  &flagoff_gforce);

    ck_assert_float_eq_tol(wm[0]->orient.roll, flagoff_orient.roll, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->orient.pitch, flagoff_orient.pitch, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->gforce.x, flagoff_gforce.x, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->gforce.y, flagoff_gforce.y, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->gforce.z, flagoff_gforce.z, 1e-4f);

    wiiuse_cleanup(wm, 1);
}
END_TEST

START_TEST(test_calibrated_output_diverges_when_extra_bits_nonzero)
{
    struct wiimote_t **wm = wiiuse_init(1);
    ck_assert_ptr_nonnull(wm);

    wm[0]->accel_calib.cal_zero.x = 10;
    wm[0]->accel_calib.cal_zero.y = 20;
    wm[0]->accel_calib.cal_zero.z = 30;
    wm[0]->accel_calib.cal_g.x    = 100;
    wm[0]->accel_calib.cal_g.y    = 150;
    wm[0]->accel_calib.cal_g.z    = 200;
    wm[0]->flags &= ~WIIUSE_SMOOTHING;

    byte msg[MSG_LEN];
    byte ax = 100, ay = 150, az = 200;
    /* All extra-precision bits set (see test_flag_on_extra_bits_all_one
     * above for the exact bit positions this exercises). */
    make_msg(msg, 0x60, 0x60, ax, ay, az);

    handle_wm_accel(wm[0], msg);

    struct orient_t expected_orient;
    struct gforce_t expected_gforce;
    memset(&expected_orient, 0, sizeof(expected_orient));
    memset(&expected_gforce, 0, sizeof(expected_gforce));
    reference_orientation_gforce(&wm[0]->accel_calib, (int)wm[0]->accel.x, (int)wm[0]->accel.y, (int)wm[0]->accel.z,
                                  WIIMOTE_ACCEL_XSHIFT, WIIMOTE_ACCEL_YSHIFT, WIIMOTE_ACCEL_ZSHIFT, &expected_orient,
                                  &expected_gforce);

    ck_assert_float_eq_tol(wm[0]->orient.roll, expected_orient.roll, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->orient.pitch, expected_orient.pitch, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->gforce.x, expected_gforce.x, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->gforce.y, expected_gforce.y, 1e-4f);
    ck_assert_float_eq_tol(wm[0]->gforce.z, expected_gforce.z, 1e-4f);

    /* And prove that output actually diverges from what a flag-off
     * build would have computed from the same base byte values. */
    struct orient_t flagoff_orient;
    struct gforce_t flagoff_gforce;
    memset(&flagoff_orient, 0, sizeof(flagoff_orient));
    memset(&flagoff_gforce, 0, sizeof(flagoff_gforce));
    reference_orientation_gforce(&wm[0]->accel_calib, (int)ax, (int)ay, (int)az, 0, 0, 0, &flagoff_orient,
                                  &flagoff_gforce);

    ck_assert(fabsf(wm[0]->gforce.x - flagoff_gforce.x) > 1e-3f);
    ck_assert(fabsf(wm[0]->gforce.y - flagoff_gforce.y) > 1e-3f);
    ck_assert(fabsf(wm[0]->gforce.z - flagoff_gforce.z) > 1e-3f);

    wiiuse_cleanup(wm, 1);
}
END_TEST

#endif /* WIIUSE_ACCEL_10BIT */

Suite *wiimote_accel_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s       = suite_create("WiimoteAccel");
    tc_core = tcase_create("Core");

#ifndef WIIUSE_ACCEL_10BIT
    tcase_add_test(tc_core, test_flag_off_preserves_raw_bytes);
#else
    tcase_add_test(tc_core, test_flag_on_extra_bits_all_zero);
    tcase_add_test(tc_core, test_flag_on_extra_bits_all_one);
    tcase_add_test(tc_core, test_flag_on_extra_bits_mixed_pattern_1);
    tcase_add_test(tc_core, test_flag_on_extra_bits_mixed_pattern_2);
#endif

    tcase_add_test(tc_core, test_decode_calibration_lsb);
    tcase_add_test(tc_core, test_calibration_lsb_combined_with_shift);

    tcase_add_test(tc_core, test_orientation_and_gforce_parity);

#ifdef WIIUSE_ACCEL_10BIT
    tcase_add_test(tc_core, test_calibrated_output_matches_flagoff_when_extra_bits_zero);
    tcase_add_test(tc_core, test_calibrated_output_diverges_when_extra_bits_nonzero);
#endif

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s  = wiimote_accel_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
