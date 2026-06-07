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

#ifndef DYNAMICS_H_INCLUDED
#define DYNAMICS_H_INCLUDED

#include "wiiuse_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup internal_dynamics Internal: Dynamics Functions */
/** @{ */

/* Extra precision bits the Wii Remote's accelerometer has per axis (10-bit X / 9-bit Y / 9-bit Z);
 * always defined, unlike the shifts below. */
#define WIIMOTE_ACCEL_XBITS 2
#define WIIMOTE_ACCEL_YBITS 1
#define WIIMOTE_ACCEL_ZBITS 1

/*
 *	Per-axis shift calculate_orientation()/calculate_gforce() apply to the
 *	8-bit calibration data (accel_t.cal_zero/cal_g) to bring it up to the
 *	same scale as the raw accel value they're compared against. Differs
 *	per device: the Wiimote is a 10-bit X / 9-bit Y / 9-bit Z split, the
 *	Nunchuk is a full 10 bits on every axis - matching each decode site
 *	(handle_wm_accel(), nunchuk_event()).
 *
 *	Under WIIUSE_ACCEL_10BIT the shift is the decode width above;
 *	without it raw accel is a plain byte and the shift collapses to 0, so
 *	callers (including the accel_threshold comparison in events.c) reduce
 *	to byte-only arithmetic.
 *
 *	The Wii Remote's own calibration also gets its real EEPROM extra bits
 *	ORed in by wiiuse_scale_calibration(); other sources are zero-padded.
 */
#ifdef WIIUSE_ACCEL_10BIT
#define WIIMOTE_ACCEL_XSHIFT WIIMOTE_ACCEL_XBITS
#define WIIMOTE_ACCEL_YSHIFT WIIMOTE_ACCEL_YBITS
#define WIIMOTE_ACCEL_ZSHIFT WIIMOTE_ACCEL_ZBITS

#define NUNCHUK_ACCEL_XSHIFT 2
#define NUNCHUK_ACCEL_YSHIFT 2
#define NUNCHUK_ACCEL_ZSHIFT 2
#else
#define WIIMOTE_ACCEL_XSHIFT 0
#define WIIMOTE_ACCEL_YSHIFT 0
#define WIIMOTE_ACCEL_ZSHIFT 0

#define NUNCHUK_ACCEL_XSHIFT 0
#define NUNCHUK_ACCEL_YSHIFT 0
#define NUNCHUK_ACCEL_ZSHIFT 0
#endif

/* The Motion+ Nunchuk-passthrough path (motion_plus.c) never decodes
 * extra precision bits, so it must never be shifted either, regardless
 * of WIIUSE_ACCEL_10BIT. */
#define NUNCHUK_PASSTHROUGH_ACCEL_XSHIFT 0
#define NUNCHUK_PASSTHROUGH_ACCEL_YSHIFT 0
#define NUNCHUK_PASSTHROUGH_ACCEL_ZSHIFT 0

struct vec3i_t
{
    int x, y, z;
};

/* Test seam: un-static'd so tests can exercise the real scaling. */
struct vec3i_t wiiuse_scale_calibration(const struct vec3b_t *cal, const struct vec3b_t *cal_lsb, int accel_xshift,
                                 int accel_yshift, int accel_zshift);
void calculate_orientation(struct accel_t *ac, struct vec3w_t *accel, struct orient_t *orient, int smooth,
                           int accel_xshift, int accel_yshift, int accel_zshift);
void calculate_gforce(struct accel_t *ac, struct vec3w_t *accel, struct gforce_t *gforce, int accel_xshift,
                      int accel_yshift, int accel_zshift);
void calc_joystick_state(struct joystick_t *js, float x, float y);
void apply_smoothing(struct accel_t *ac, struct orient_t *orient, int type);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* DYNAMICS_H_INCLUDED */
