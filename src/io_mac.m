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
 *	@brief Handles device I/O for Mac OS X.
 */

#ifdef __APPLE__

#include "io.h"

#include <stdlib.h>
#include <unistd.h>

#define BLUETOOTH_VERSION_USE_CURRENT
#import <IOBluetooth/IOBluetooth.h>

#define MAX_WIIMOTES 4
wiimote * g_wiimotes[MAX_WIIMOTES] = {NULL, NULL, NULL, NULL};

@interface SearchBT: NSObject {
@public
	unsigned int maxDevices;
}
@end

@implementation SearchBT
- (void) deviceInquiryComplete: (IOBluetoothDeviceInquiry *) sender
	error: (IOReturn) error
	aborted: (BOOL) aborted
{
	CFRunLoopStop(CFRunLoopGetCurrent());
}

- (void) deviceInquiryDeviceFound: (IOBluetoothDeviceInquiry *) sender
	device: (IOBluetoothDevice *) device
{
	WIIUSE_INFO("Discovered bluetooth device at %s: %s",
		[[device getAddressString] UTF8String],
		[[device getName] UTF8String]);

	if ([[sender foundDevices] count] == maxDevices)
		[sender stop];
}
@end

@interface ConnectBT: NSObject {}
@end

@implementation ConnectBT
- (void) l2capChannelData: (IOBluetoothL2CAPChannel *) l2capChannel
	data: (unsigned char *) data
	length: (NSUInteger) length
{
	IOBluetoothDevice *device = [l2capChannel getDevice];
	wiimote *wm = NULL;
	int i;

	for (i = 0; i < MAX_WIIMOTES; i++) {
		if (g_wiimotes[i] == NULL)
			continue;
		if ([device isEqual: g_wiimotes[i]->btd] == TRUE)
			wm = g_wiimotes[i];
	}

	if (wm == NULL) {
		WIIUSE_INFO("Received packet for unknown wiimote");
		return;
	}

	if (length > MAX_PAYLOAD) {
		WIIUSE_INFO("Dropping packet for wiimote %i, too large",
				wm->unid);
		return;
	}

	if (wm->inputlen != 0) {
		WIIUSE_INFO("Dropping packet for wiimote %i, queue full",
				wm->unid);
		return;
	}

	memcpy(wm->input, data, length);
	wm->inputlen = length;

	wiiuse_io_read(wm);

	/*(void)UpdateSystemActivity(UsrActivity);*/
}

- (void) l2capChannelClosed: (IOBluetoothL2CAPChannel *) l2capChannel
{
	IOBluetoothDevice *device = [l2capChannel getDevice];
	wiimote *wm = NULL;

	int i;

	for (i = 0; i < MAX_WIIMOTES; i++) {
		if (g_wiimotes[i] == NULL)
			continue;
		if ([device isEqual: g_wiimotes[i]->btd] == TRUE)
			wm = g_wiimotes[i];
	}

	if (wm == NULL) {
		WIIUSE_INFO("Channel for unknown wiimote was closed");
		return;
	}

	WIIUSE_INFO("Lost channel to wiimote %i", wm->unid);

	wiiuse_disconnect(wm);
}
@end

static int wiiuse_connect_single(struct wiimote_t* wm, char* address);

/**
 *	@brief Find a wiimote or wiimotes.
 *
 *	@param wm			An array of wiimote_t structures.
 *	@param max_wiimotes	The number of wiimote structures in \a wm.
 *	@param timeout		The number of seconds before the search times out.
 *
 *	@return The number of wiimotes found.
 *
 *	@see wiimote_connect()
 *
 *	This function will only look for wiimote devices.						\n
 *	When a device is found the address in the structures will be set.		\n
 *	You can then call wiimote_connect() to connect to the found				\n
 *	devices.
 */
int wiiuse_find(struct wiimote_t** wm, int max_wiimotes, int timeout) {
	IOBluetoothHostController *bth;
	IOBluetoothDeviceInquiry *bti;
	SearchBT *sbt;
	NSEnumerator *en;
	int i;
	int found_devices = 0, found_wiimotes = 0;

	// Count the number of already found wiimotes
	for (i = 0; i < MAX_WIIMOTES; ++i)
		if (wm[i])
			found_wiimotes++;

	bth = [[IOBluetoothHostController alloc] init];
	if ([bth addressAsString] == nil) {
		WIIUSE_INFO("No bluetooth host controller");
		[bth release];
		return found_wiimotes;
	}

	sbt = [[SearchBT alloc] init];
	sbt->maxDevices = max_wiimotes - found_wiimotes;
	bti = [[IOBluetoothDeviceInquiry alloc] init];
	[bti setDelegate: sbt];
	[bti setInquiryLength: 5];
	[bti setSearchCriteria: kBluetoothServiceClassMajorAny
		majorDeviceClass: kBluetoothDeviceClassMajorPeripheral
		minorDeviceClass: kBluetoothDeviceClassMinorPeripheral2Joystick
		];
	[bti setUpdateNewDeviceNames: NO];

	if ([bti start] == kIOReturnSuccess)
		[bti retain];
	else
		WIIUSE_INFO("Unable to do bluetooth discovery");

	CFRunLoopRun();

	[bti stop];
	found_devices = [[bti foundDevices] count];

	WIIUSE_INFO("Found %i bluetooth device%c", found_devices,
		found_devices == 1 ? '\0' : 's');

	en = [[bti foundDevices] objectEnumerator];
	
	for (; (i < found_devices) && (found_wiimotes < max_wiimotes); ++i) {
			/* found a device */
			wm[found_wiimotes]->btd = [en nextObject];

			WIIUSE_INFO("Found wiimote (%s) [id %i].",
				[[wm[found_wiimotes]->btd getAddressString] UTF8String],
				wm[found_wiimotes]->unid);

			WIIMOTE_ENABLE_STATE(wm[found_wiimotes], WIIMOTE_STATE_DEV_FOUND);
			++found_wiimotes;
	}
	[bth release];
	[bti release];
	[sbt release];

	return found_wiimotes;
}


/**
 *	@brief Connect to a wiimote or wiimotes once an address is known.
 *
 *	@param wm			An array of wiimote_t structures.
 *	@param wiimotes		The number of wiimote structures in \a wm.
 *
 *	@return The number of wiimotes that successfully connected.
 *
 *	@see wiiuse_find()
 *	@see wiiuse_connect_single()
 *	@see wiiuse_disconnect()
 *
 *	Connect to a number of wiimotes when the address is already set
 *	in the wiimote_t structures.  These addresses are normally set
 *	by the wiiuse_find() function, but can also be set manually.
 */
int wiiuse_connect(struct wiimote_t** wm, int wiimotes) {
	int connected = 0;
	int i = 0;

	for (; i < wiimotes; ++i) {
		if (!WIIMOTE_IS_SET(wm[i], WIIMOTE_STATE_DEV_FOUND))
			/* if the device address is not set, skip it */
			continue;

		if (wiiuse_connect_single(wm[i], NULL))
			++connected;
	}

	return connected;
}


/**
 *	@brief Connect to a wiimote with a known address.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param address	The address of the device to connect to.
 *					If NULL, use the address in the struct set by wiiuse_find().
 *
 *	@return 1 on success, 0 on failure
 */
static int wiiuse_connect_single(struct wiimote_t* wm, char* address) {

	if (!wm || WIIMOTE_IS_CONNECTED(wm))
		return 0;

	int i;
	ConnectBT *cbt = [[ConnectBT alloc] init];
	[wm->btd openL2CAPChannelSync: wm->cchan
		withPSM: kBluetoothL2CAPPSMHIDControl delegate: cbt];
	[wm->btd openL2CAPChannelSync: wm->ichan
		withPSM: kBluetoothL2CAPPSMHIDInterrupt delegate: cbt];
	if (wm->cchan == NULL || wm->ichan == NULL) {
		WIIUSE_INFO("Unable to open L2CAP channels "
			"for wiimote %i", wm->unid);
		[cbt release];
		return 0;
	}

	for (i = 0; i < MAX_WIIMOTES; i++) {
		if (g_wiimotes[i] == NULL) {
			g_wiimotes[i] = wm;
			break;
		}
	}

	WIIUSE_INFO("Connected to wiimote [id %i].", wm->unid);

	/* do the handshake */
	WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
	wiiuse_handshake(wm, NULL, 0);

	wiiuse_set_report_type(wm);

	[cbt release];
	return 1;
}


/**
 *	@brief Disconnect a wiimote.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *
 *	@see wiiuse_connect()
 *
 *	Note that this will not free the wiimote structure.
 */
void wiiuse_disconnect(struct wiimote_t* wm) {
	if (!wm || WIIMOTE_IS_CONNECTED(wm))
		return;
	int i;
	for (i = 0; i < MAX_WIIMOTES; i++) {
		if (wm == g_wiimotes[i])
			g_wiimotes[i] = NULL;
	}
	[wm->btd closeConnection];
	[wm->btd release];
	wm->btd = NULL;
	wm->cchan = NULL;
	wm->ichan = NULL;

	wm->event = WIIUSE_NONE;

	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
}


int wiiuse_io_read(struct wiimote_t* wm) {
	/* not used */
	return 0;
}


int wiiuse_io_write(struct wiimote_t* wm, byte* buf, int len) {
	IOReturn ret;

	if (!wm || WIIMOTE_IS_CONNECTED(wm))
		return 0;

	ret = [wm->cchan writeAsync: buf length: len refcon: nil];

	if (ret == kIOReturnSuccess)
		return len;
	else
		return 0;
}



#endif /* ifdef __APPLE__ */
