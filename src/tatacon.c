/*
 *	wiiuse
 *
 *	Written By:
 *		pixelomer
 *
 *	Copyright 2026
 *
 *	Adapted from nunchuk.c
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
 *	@brief tatacon expansion device.
 */

#include "tatacon.h"
#include "events.h"   /* for handshake_expansion */

#include <stdlib.h> /* for malloc */
#include <string.h> /* for memset */

/**
 *	@brief Handle the handshake data from the tatacon.
 *
 *	@param tatacon	A pointer to a tatacon_t structure.
 *	@param data		The data read in from the device.
 *	@param len		The length of the data block, in bytes.
 *
 *	@return	Returns 1 if handshake was successful, 0 if not.
 */
#define HANDSHAKE_BYTES_USED 14
int tatacon_handshake(struct wiimote_t *wm, struct tatacon_t *tatacon, byte *data, unsigned short len)
{
    tatacon->btns          = 0;

    /* handshake done (nothing to do) */
    wm->exp.type = EXP_TATACON;

#ifdef WIIUSE_WIN32
    wm->timeout = WIIMOTE_DEFAULT_TIMEOUT;
#endif

    return 1;
}

/**
 *	@brief The tatacon disconnected.
 *
 *	@param tatacon	A pointer to a tatacon_t structure.
 */
void tatacon_disconnected(struct tatacon_t *tatacon) {
    memset(tatacon, 0, sizeof(struct tatacon_t));
}

/**
 *	@brief Handle tatacon event.
 *
 *	@param tatacon	A pointer to a tatacon_t structure.
 *	@param msg		The message specified in the event packet.
 */
void tatacon_event(struct tatacon_t *tatacon, byte *msg)
{
    /* get button states */
    tatacon_pressed_buttons(tatacon, msg[5]);
}

/**
 *	@brief Find what buttons are pressed.
 *
 *	@param tatacon	Pointer to a tatacon_t structure.
 *	@param now		The buttons byte in the event packet.
 */
void tatacon_pressed_buttons(struct tatacon_t *tatacon, byte now)
{
    /* message is inverted (0 is active, 1 is inactive) */
    now = ~now & TATACON_BUTTON_ALL;

    /* buttons pressed now */
    tatacon->btns = now;
}
