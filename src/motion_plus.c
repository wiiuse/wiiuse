/*
 *	wiiuse
 *
 *	Written By:
 *		Michal Wiedenbauer	< shagkur >
 *		Dave Murphy			< WinterMute >
 *		Hector Martin		< marcan >
 * 		Radu Andries		<admiral0>
 *
 *	Copyright 2009
 *
 *	This file is part of wiiuse and fWIIne.
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


#include "wiiuse_internal.h"
#include "dynamics.h"
#include "events.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

void wiiuse_motion_plus_check(struct wiimote_t *wm,byte *data,unsigned short len)
{
	int val;
	if(data == NULL)
	{
	    wiiuse_read_data_cb(wm, wiiuse_motion_plus_check, wm->motion_plus_id, WM_EXP_ID, 6);
	}
	else
	{
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_FAILED);
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_HANDSHAKE);
		val = (data[3] << 16) | (data[2] << 24) | (data[4] << 8) | data[5];
		if(val == EXP_ID_CODE_MOTION_PLUS)
		{
			/* handshake done */
			wm->event = WIIUSE_MOTION_PLUS_ACTIVATED;
			wm->exp.type = EXP_MOTION_PLUS;
			WIIUSE_DEBUG("Motion plus connected");			
			WIIMOTE_ENABLE_STATE(wm,WIIMOTE_STATE_EXP);
			wiiuse_set_ir_mode(wm);
		}
	}
}

static void wiiuse_set_motion_plus_clear2(struct wiimote_t *wm,byte *data,unsigned short len)
{
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_FAILED);
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_HANDSHAKE);
	wiiuse_set_ir_mode(wm);
	wiiuse_status(wm);
}

static void wiiuse_set_motion_plus_clear1(struct wiimote_t *wm,byte *data,unsigned short len)
{
	unsigned char val = 0x00;
	wiiuse_write_data_cb(wm, WM_EXP_MEM_ENABLE1, &val, 1, wiiuse_set_motion_plus_clear2);
}


void wiiuse_set_motion_plus(struct wiimote_t *wm, int status)
{
	unsigned char val;
	
	if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP_HANDSHAKE))
		return;
	
	WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_EXP_HANDSHAKE);
	if(status)
	{
		val = 0x04;
		wiiuse_write_data_cb(wm, WM_EXP_MOTION_PLUS_ENABLE, &val, 1, wiiuse_motion_plus_check);
	}
	else
	{
		disable_expansion(wm);
		val = 0x55;
		wiiuse_write_data_cb(wm, WM_EXP_MEM_ENABLE1, &val, 1, wiiuse_set_motion_plus_clear1);
	}
}

void motion_plus_disconnected(struct motion_plus_t* mp)
{
	WIIUSE_DEBUG("Motion plus disconnected");
	memset(mp, 0, sizeof(struct motion_plus_t));
}

void motion_plus_event(struct motion_plus_t* mp, byte* msg)
{
	mp->rx = ((msg[5] & 0xFC) << 6) | msg[2]; // Pitch
	mp->ry = ((msg[4] & 0xFC) << 6) | msg[1]; // Roll
	mp->rz = ((msg[3] & 0xFC) << 6) | msg[0]; // Yaw

	mp->ext = msg[4] & 0x1;
	mp->status = (msg[3] & 0x3) | ((msg[4] & 0x2) << 1); // roll, yaw, pitch
}
