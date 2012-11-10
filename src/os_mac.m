/*
 *  wiiuse
 *
 *  Written By:
 *    Michael Laforest  < para >
 *    Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 *  Copyright 2006-2007
 *
 *  This file is part of wiiuse.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  $Header$
 *
 */

/**
 *  @file
 *  @brief Handles device I/O for Mac OS X.
 */

#ifdef __APPLE__

#include "io.h"

#if defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7
#define WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE 1
#else
#define WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE 0
#endif


void wiiuse_init_platform_fields(struct wiimote_t* wm) {

}

void wiiuse_cleanup_platform_fields(struct wiimote_t* wm) {

}

int wiiuse_os_find(struct wiimote_t** wm, int max_wiimotes, int timeout) {
  return 0;
}

int wiiuse_os_connect(struct wiimote_t** wm, int wiimotes) {
  return 0;
}

void wiiuse_os_disconnect(struct wiimote_t* wm) {

}

int wiiuse_os_read(struct wiimote_t* wm) {
  return 0;
}

int wiiuse_os_write(struct wiimote_t* wm, byte* buf, int len) {
  return 0;
}

#endif // __APPLE__
