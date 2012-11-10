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

#import "os_mac/os_mac.h"

#import "io.h"
#import "events.h"
#import "os.h"


#define BLUETOOTH_VERSION_USE_CURRENT
#import <IOBluetooth/IOBluetoothUtilities.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>
#import <IOBluetooth/objc/IOBluetoothDeviceInquiry.h>
#import <IOBluetooth/objc/IOBluetoothL2CAPChannel.h>


#pragma mark -
#pragma mark connect, disconnect

int wiiuse_os_connect(struct wiimote_t** wm, int wiimotes) {
	return 0;
}

void wiiuse_os_disconnect(struct wiimote_t* wm) {

}

#pragma mark -
#pragma mark poll, read, write

int wiiuse_os_poll(struct wiimote_t** wm, int wiimotes) {
	int i;
	
	if (!wm) return 0;
	
	for (i = 0; i < wiimotes; ++i) {
		wm[i]->event = WIIUSE_NONE;
		idle_cycle(wm[i]);
	}
	
	return 0;
}

int wiiuse_os_read(struct wiimote_t* wm) {
	return 0;
}

int wiiuse_os_write(struct wiimote_t* wm, byte* buf, int len) {
	return 0;
}

#pragma mark platform fields

void wiiuse_init_platform_fields(struct wiimote_t* wm) {
	wm->device = NULL;
}

void wiiuse_cleanup_platform_fields(struct wiimote_t* wm) {
	[WIIUSE_IOBluetoothDeviceRef_to_IOBluetoothDevice(wm->device) release];
	wm->device = NULL;
}

#endif // __APPLE__
