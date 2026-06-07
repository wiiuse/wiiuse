/*
 *	wiiuse
 *
 *	Written By:
 *		Michael Laforest	< para >
 *		Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 *	Copyright 2006-2007
 *
 *	This file is part of wiiuse.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	$Header$
 *
 */

/**
 *	@file
 *	@brief Handles the dynamics of the wiimote.
 *
 *	The file includes functions that handle the dynamics
 *	of the wiimote.  Such dynamics include orientation and
 *	motion sensing.
 */

#include "dynamics.h"

#include <math.h>   /* for atan2f, atanf, sqrt */
#include <stdlib.h> /* for abs */

/**
 *	@brief Scale an 8-bit calibration point up to the raw accel value's
 *	scale, per axis, filling in any real extra-precision bits decoded
 *	for it along the way.
 *
 *	accel_xshift/yshift/zshift is 0 (a no-op) unless WIIUSE_ACCEL_10BIT is
 *	defined, so this always matches the scale handle_wm_accel()/
 *	nunchuk_event() decoded the raw value at - see dynamics.h.
 *
 *	cal_lsb holds real extra-precision bits decoded from EEPROM (Wii
 *	Remote only); it is 0 elsewhere, so ORing it in is then a no-op.
 */
struct vec3i_t wiiuse_scale_calibration(const struct vec3b_t *cal, const struct vec3b_t *cal_lsb, int accel_xshift,
                                        int accel_yshift, int accel_zshift)
{
    struct vec3i_t scaled;

    scaled.x = ((int)cal->x << accel_xshift) | cal_lsb->x;
    scaled.y = ((int)cal->y << accel_yshift) | cal_lsb->y;
    scaled.z = ((int)cal->z << accel_zshift) | cal_lsb->z;
    return scaled;
}

/**
 *	@brief Scale the calibration to the raw accel value's scale and normalize
 *	the reading to +/- 1g per axis.
 *
 *	@param calzero	[out] Scaled zero point.
 *	@param calg		[out] Scaled 0g-to-1g distance.
 *	@param g		[out] Normalized reading.
 */
static void normalize_accel(const struct accel_t *ac, const struct vec3w_t *accel, int accel_xshift, int accel_yshift,
                            int accel_zshift, struct vec3i_t *calzero, struct vec3i_t *calg, struct gforce_t *g)
{
    *calzero = wiiuse_scale_calibration(&ac->cal_zero, &ac->cal_zero_lsb, accel_xshift, accel_yshift, accel_zshift);
    *calg    = wiiuse_scale_calibration(&ac->cal_g, &ac->cal_g_lsb, accel_xshift, accel_yshift, accel_zshift);

    g->x = ((float)accel->x - (float)calzero->x) / (float)calg->x;
    g->y = ((float)accel->y - (float)calzero->y) / (float)calg->y;
    g->z = ((float)accel->z - (float)calzero->z) / (float)calg->z;
}

/**
 *	@brief Calculate the roll, pitch, yaw.
 *
 *	@param ac			An accelerometer (accel_t) structure.
 *	@param accel		[in] Pointer to a vec3w_t structure that holds the raw acceleration data.
 *	@param orient		[out] Pointer to a orient_t structure that will hold the orientation data.
 *	@param rorient		[out] Pointer to a orient_t structure that will hold the non-smoothed
 *orientation data.
 *	@param smooth		If smoothing should be performed on the angles calculated. 1 to enable, 0 to
 *disable.
 *	@param accel_xshift	[in] Per-axis WIIUSE_ACCEL_10BIT decode shift; see dynamics.h.
 *	@param accel_yshift	[in] Per-axis WIIUSE_ACCEL_10BIT decode shift; see dynamics.h.
 *	@param accel_zshift	[in] Per-axis WIIUSE_ACCEL_10BIT decode shift; see dynamics.h.
 *
 *	Given the raw acceleration data from the accelerometer struct, calculate
 *	the orientation of the device and set it in the \a orient parameter.
 */
void calculate_orientation(struct accel_t *ac, struct vec3w_t *accel, struct orient_t *orient, int smooth,
                           int accel_xshift, int accel_yshift, int accel_zshift)
{
    float x, y, z;
    struct vec3i_t calzero, calg;
    struct gforce_t g;

    /*
     *	roll	- use atan(z / x)		[ ranges from -180 to 180 ]
     *	pitch	- use atan(z / y)		[ ranges from -180 to 180 ]
     *	yaw		- impossible to tell without IR
     */

    /* yaw - set to 0, IR will take care of it if it's enabled */
    orient->yaw = 0.0f;

    normalize_accel(ac, accel, accel_xshift, accel_yshift, accel_zshift, &calzero, &calg, &g);
    x = g.x;
    y = g.y;
    z = g.z;

    /* make sure x,y,z are between -1 and 1 for the tan functions */
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

    /*
    If it is over 1g then it is probably accelerating and not reliable
    Formulas from: http://husstechlabs.com/projects/atb1/using-the-accelerometer/
*/

    if (abs((int)accel->x - calzero.x) <= calg.x)
    {
        /* roll */
        float roll = RAD_TO_DEGREE(atan2f(x, z));

        orient->roll   = roll;
        orient->a_roll = roll;
    }

    if (abs((int)accel->y - calzero.y) <= calg.y)
    {
        /* pitch */
        float pitch = RAD_TO_DEGREE(atan2f(y, sqrtf(x * x + z * z)));

        orient->pitch   = pitch;
        orient->a_pitch = pitch;
    }

    /* smooth the angles if enabled */
    if (smooth)
    {
        apply_smoothing(ac, orient, SMOOTH_ROLL);
        apply_smoothing(ac, orient, SMOOTH_PITCH);
    }
}

/**
 *	@brief Calculate the gravity forces on each axis.
 *
 *	@param ac			An accelerometer (accel_t) structure.
 *	@param accel		[in] Pointer to a vec3w_t structure that holds the raw acceleration data.
 *	@param gforce		[out] Pointer to a gforce_t structure that will hold the gravity force data.
 *	@param accel_xshift	[in] See calculate_orientation().
 *	@param accel_yshift	[in] See calculate_orientation().
 *	@param accel_zshift	[in] See calculate_orientation().
 */
void calculate_gforce(struct accel_t *ac, struct vec3w_t *accel, struct gforce_t *gforce, int accel_xshift,
                      int accel_yshift, int accel_zshift)
{
    struct vec3i_t calzero, calg;

    normalize_accel(ac, accel, accel_xshift, accel_yshift, accel_zshift, &calzero, &calg, gforce);
}

static float applyCalibration(float inval, float minval, float maxval, float centerval)
{
    float ret;
    /* We don't use the exact ranges but the ranges + 1 in case we get bad calibration data - avoid div0 */
    if (inval == centerval)
    {
        ret = 0;
    } else if (inval < centerval)
    {
        ret = (inval - minval) / (centerval - minval + 1.0f) - 1.0f;
    } else
    {
        ret = (inval - centerval) / (maxval - centerval + 1.0f);
    }
    return ret;
}

/**
 *	@brief Calculate the angle and magnitude of a joystick.
 *
 *	@param js	[out] Pointer to a joystick_t structure.
 *	@param x	The raw x-axis value.
 *	@param y	The raw y-axis value.
 */
void calc_joystick_state(struct joystick_t *js, float x, float y)
{
    float rx, ry, ang;

    /*
     *	Since the joystick center may not be exactly:
     *		(min + max) / 2
     *	Then the range from the min to the center and the center to the max
     *	may be different.
     *	Because of this, depending on if the current x or y value is greater
     *	or less than the associated axis center value, it needs to be interpolated
     *	between the center and the minimum or maxmimum rather than between
     *	the minimum and maximum.
     *
     *	So we have something like this:
     *		(x min) [-1] ---------*------ [0] (x center) [0] -------- [1] (x max)
     *	Where the * is the current x value.
     *	The range is therefore -1 to 1, 0 being the exact center rather than
     *	the middle of min and max.
     */
    rx    = applyCalibration(x, js->min.x, js->max.x, js->center.x);
    ry    = applyCalibration(y, js->min.y, js->max.y, js->center.y);
    js->x = rx;
    js->y = ry;
    /* calculate the joystick angle and magnitude */
    ang     = RAD_TO_DEGREE(atan2f(ry, rx));
    js->ang = ang + 180.0f;
    js->mag = sqrtf((rx * rx) + (ry * ry));
}

void apply_smoothing(struct accel_t *ac, struct orient_t *orient, int type)
{
    switch (type)
    {
    case SMOOTH_ROLL:
    {
        /* it's possible last iteration was nan or inf, so set it to 0 if that happened */
        if (isnan(ac->st_roll) || isinf(ac->st_roll))
        {
            ac->st_roll = 0.0f;
        }

        /*
         *	If the sign changes (which will happen if going from -180 to 180)
         *	or from (-1 to 1) then don't smooth, just use the new angle.
         */
        if (((ac->st_roll < 0) && (orient->roll > 0)) || ((ac->st_roll > 0) && (orient->roll < 0)))
        {
            ac->st_roll = orient->roll;
        } else
        {
            orient->roll = ac->st_roll + (ac->st_alpha * (orient->a_roll - ac->st_roll));
            ac->st_roll  = orient->roll;
        }

        return;
    }

    case SMOOTH_PITCH:
    {
        if (isnan(ac->st_pitch) || isinf(ac->st_pitch))
        {
            ac->st_pitch = 0.0f;
        }

        if (((ac->st_pitch < 0) && (orient->pitch > 0)) || ((ac->st_pitch > 0) && (orient->pitch < 0)))
        {
            ac->st_pitch = orient->pitch;
        } else
        {
            orient->pitch = ac->st_pitch + (ac->st_alpha * (orient->a_pitch - ac->st_pitch));
            ac->st_pitch  = orient->pitch;
        }

        return;
    }
    }
}
