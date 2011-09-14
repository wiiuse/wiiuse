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
 *	@brief Handles device I/O (non-OS specific).
 */
#include "io.h"
#include "ir.h"                         /* for wiiuse_set_ir_mode */

#include <stdlib.h>                     /* for free, malloc */


static void wiiuse_disable_motion_plus2(struct wiimote_t *wm,byte *data,unsigned short len)
{
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_FAILED);
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_HANDSHAKE);
	wiiuse_set_ir_mode(wm);

	wm->handshake_state++;
	wiiuse_handshake(wm, NULL, 0);

}

static void wiiuse_disable_motion_plus1(struct wiimote_t *wm,byte *data,unsigned short len)
{
	byte val = 0x00;
	wiiuse_write_data_cb(wm, WM_EXP_MEM_ENABLE1, &val, 1, wiiuse_disable_motion_plus2);
}

 /**
 *	@brief Get initialization data from the wiimote.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param data		unused
 *	@param len		unused
 *
 *	When first called for a wiimote_t structure, a request
 *	is sent to the wiimote for initialization information.
 *	This includes factory set accelerometer data.
 *	The handshake will be concluded when the wiimote responds
 *	with this data.
 */
void wiiuse_handshake(struct wiimote_t* wm, byte* data, uint16_t len) {
	if (!wm)	return;

	switch (wm->handshake_state) {
		case 0:
		{
			/* send request to wiimote for accelerometer calibration */
			byte* buf;

			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
			wiiuse_set_leds(wm, WIIMOTE_LED_NONE);

			buf = (byte*)malloc(sizeof(byte) * 8);
			wiiuse_read_data_cb(wm, wiiuse_handshake, buf, WM_MEM_OFFSET_CALIBRATION, 7);
			wm->handshake_state++;

			wiiuse_set_leds(wm, WIIMOTE_LED_NONE);

			break;
		}

		case 1:
		{
			struct read_req_t* req = wm->read_req;
			struct accel_t* accel = &wm->accel_calib;
			byte val;

			/* received read data */
			accel->cal_zero.x = req->buf[0];
			accel->cal_zero.y = req->buf[1];
			accel->cal_zero.z = req->buf[2];

			accel->cal_g.x = req->buf[4] - accel->cal_zero.x;
			accel->cal_g.y = req->buf[5] - accel->cal_zero.y;
			accel->cal_g.z = req->buf[6] - accel->cal_zero.z;

			/* done with the buffer */
			free(req->buf);

			/* handshake is done */
			WIIUSE_DEBUG("Handshake finished. Calibration: Idle: X=%x Y=%x Z=%x\t+1g: X=%x Y=%x Z=%x",
					accel->cal_zero.x, accel->cal_zero.y, accel->cal_zero.z,
					accel->cal_g.x, accel->cal_g.y, accel->cal_g.z);

			/* M+ off */
			val = 0x55;
			wiiuse_write_data_cb(wm, WM_EXP_MEM_ENABLE1, &val, 1, wiiuse_disable_motion_plus1);

			break;
		}

		case 2:
		{
			/* request the status of the wiimote to check for any expansion */
			WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);
			wm->handshake_state++;

			/* now enable IR if it was set before the handshake completed */
			if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR)) {
				WIIUSE_DEBUG("Handshake finished, enabling IR.");
				WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_IR);
				wiiuse_set_ir(wm, 1);
			}

			wm->event = WIIUSE_CONNECT;
			wiiuse_status(wm);

			break;
		}

		default:
		{
			break;
		}
	}
}
