/*
 *	wiiuse
 *
 *	Written By:
 *		pixelomer
 *
 *	Copyright 2026
 *
 *	Adapted from nunchuk.h
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
 *	@brief Tatacon expansion device.
 */

#ifndef TATACON_H_INCLUDED
#define TATACON_H_INCLUDED

#include "wiiuse_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup internal_tatacon Internal: Tatacon */
/** @{ */
int tatacon_handshake(struct wiimote_t *wm, struct tatacon_t *tatacon, byte *data, unsigned short len);

void tatacon_disconnected(struct tatacon_t *tatacon);

void tatacon_event(struct tatacon_t *tatacon, byte *msg);

void tatacon_pressed_buttons(struct tatacon_t *tatacon, byte now);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* TATACON_H_INCLUDED */
