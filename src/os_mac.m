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

#import <IOBluetooth/IOBluetoothUtilities.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>
#import <IOBluetooth/objc/IOBluetoothDeviceInquiry.h>
#import <IOBluetooth/objc/IOBluetoothL2CAPChannel.h>


#pragma mark -
#pragma mark connect

/**
 *	@brief Connect to a wiimote with a known address.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *
 *	@see wiimote_os_connect()
 *	@see wiimote_os_find()
 *
 *	@return 1 on success, 0 on failure
 */
static short wiiuse_os_connect_single(struct wiimote_t* wm) {
	// Skip if already connected or device not found
	if(!wm) {
		WIIUSE_ERROR("No Wiimote given.");
		return 0;
	} else if(wm && (!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_DEV_FOUND) || wm->device == NULL)) {
		WIIUSE_ERROR("Tried to connect Wiimote without an address.");
		return 0;
	} else if(WIIMOTE_IS_CONNECTED(wm)) {
		WIIUSE_WARNING("Wiimote [id %i] is already connected.", wm->unid);
		return 1;
	}
	
	WIIUSE_DEBUG("Connecting to Wiimote [id %i].", wm->unid);
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	short result = 0;
	
	// connect
	WiiuseWiimote* objc_wm = [[WiiuseWiimote alloc] initWithPtr: wm];
	if([objc_wm connect] == kIOReturnSuccess) {
		WIIUSE_INFO("Connected to Wiimote [id %i].", wm->unid);
		
		// save the connect structure to retrieve data later on
		wm->objc_wm = (void*)objc_wm;

		// save the connection status
		WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
		
		// Do the handshake
		wiiuse_handshake(wm, NULL, 0);
		wiiuse_set_report_type(wm);
		
		result = 1;
	} else {
		[objc_wm release];
	}
	
	[pool drain];
	return result;
}

int wiiuse_os_connect(struct wiimote_t** wm, int wiimotes) {
	int connected = 0;
	
	int i;
	for (i = 0; i < wiimotes; ++i) {
		if(wm[i] == NULL) {
			WIIUSE_ERROR("Trying to connect to non-initialized Wiimote.");
			break;
		}
		
		if (!WIIMOTE_IS_SET(wm[i], WIIMOTE_STATE_DEV_FOUND) || !wm[i]->device) {
			// If the device is not set, skip it
			continue;
		}
		
		if (wiiuse_os_connect_single(wm[i]))
			++connected;
	}
	
	return connected;
}

#pragma mark disconnect

#define WIIUSE_MAC_CLOSE_CHANNEL(wm, channel) \
	{ \
		IOBluetoothL2CAPChannel* channel = WIIUSE_IOBluetoothL2CAPChannelRef_to_IOBluetoothL2CAPChannel((wm)->channel##Channel); \
		if([channel closeChannel] != kIOReturnSuccess) \
			WIIUSE_ERROR("Unable to close " #channel " channel [id %i].", (wm)->unid); \
		[channel release]; \
		(wm)->channel##Channel = NULL; \
	}

#define WIIUSE_MAC_CLOSE_DEVICE_CONNECTION(wm) \
	{ \
		IOBluetoothDevice* device = WIIUSE_IOBluetoothDeviceRef_to_IOBluetoothDevice((wm)->device); \
		if([device closeConnection] != kIOReturnSuccess) \
			WIIUSE_ERROR("Unable to close the device connection [id %i].", (wm)->unid); \
		[device release]; \
		(wm)->device = NULL; \
	}

#define WIIUSE_MAC_DISCONNECT(wm) \
	{ \
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]; \
		WIIUSE_MAC_CLOSE_CHANNEL(wm, interrupt); \
		WIIUSE_MAC_CLOSE_CHANNEL(wm, control); \
		WIIUSE_MAC_CLOSE_DEVICE_CONNECTION(wm); \
		[pool drain]; \
	}

void wiiuse_os_disconnect(struct wiimote_t* wm) {
	if (!wm || !WIIMOTE_IS_CONNECTED(wm))
		return;
	
	// disconnect
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	{ \
		IOBluetoothL2CAPChannel* channel = WIIUSE_IOBluetoothL2CAPChannelRef_to_IOBluetoothL2CAPChannel((wm)->interruptChannel); \
		if([channel closeChannel] != kIOReturnSuccess) \
			WIIUSE_ERROR("Unable to close " "interrupt" " channel [id %i].", (wm)->unid); \
		[channel release]; \
		(wm)->interruptChannel = NULL; \
	}
	WIIUSE_MAC_CLOSE_CHANNEL(wm, control);
	WIIUSE_MAC_CLOSE_DEVICE_CONNECTION(wm);
	[pool drain];
	
	// clean up C struct
	wm->event = WIIUSE_NONE;
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
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

#pragma mark -
#pragma mark platform fields

void wiiuse_init_platform_fields(struct wiimote_t* wm) {
	wm->device = NULL;
	wm->objc_wm = NULL;
	wm->controlChannel = NULL;
	wm->interruptChannel = NULL;
}

void wiiuse_cleanup_platform_fields(struct wiimote_t* wm) {
	// disconnect & release device and channels
	// Note: this should already have happened, because this function is called
	// once the device is disconnected. This is just paranoia.
	WIIUSE_MAC_DISCONNECT(wm);
	
	// this is also done on disconnect, so it's even more paranoia.
	wm->device = NULL;
	wm->controlChannel = NULL;
	wm->interruptChannel = NULL;
	
	// release WiiuseWiimote object
	[((WiiuseWiimote*) wm->objc_wm) release];
	wm->objc_wm = NULL;
}

#pragma mark -
#pragma mark WiiuseWiimote

@implementation WiiuseWiimote

- (id) initWithPtr: (wiimote*) wm_ {
	self = [super init];
	if(self) {
		wm = wm_;
		disconnectNotification = nil;
	}
	return self;
}

- (void) dealloc {
	wm = NULL;
	
	[disconnectNotification release];
	disconnectNotification = nil;
	
	[super dealloc];
}

- (IOReturn) connect {
	IOBluetoothDevice* device = WIIUSE_IOBluetoothDeviceRef_to_IOBluetoothDevice(wm->device);
	if(!device) {
		WIIUSE_ERROR("Missing device.");
		return kIOReturnBadArgument;
	}
	
	// open channels
	IOBluetoothL2CAPChannel* controlChannel = nil, *interruptChannel = nil;
	if ([device openL2CAPChannelSync:&controlChannel withPSM:WM_OUTPUT_CHANNEL delegate:self] != kIOReturnSuccess) {
		WIIUSE_ERROR("Unable to open L2CAP control channel [id %i].", wm->unid);
		[device closeConnection];
		return kIOReturnNotOpen;
	} else if([device openL2CAPChannelSync:&interruptChannel withPSM:WM_INPUT_CHANNEL delegate:self] != kIOReturnSuccess) {
		WIIUSE_ERROR("Unable to open L2CAP interrupt channel [id %i].", wm->unid);
		[controlChannel closeChannel];
		[device closeConnection];
		return kIOReturnNotOpen;
	}
	
	// register for device disconnection
	disconnectNotification = [device registerForDisconnectNotification:self selector:@selector(disconnected:fromDevice:)];
	if(!disconnectNotification) {
		WIIUSE_ERROR("Unable to register disconnection handler [id %i].", wm->unid);
		[interruptChannel closeChannel];
		[controlChannel closeChannel];
		[device closeConnection];
		return kIOReturnNotOpen;
	}
	
	// retain channels, and save them to the C struct
	[controlChannel retain]; [interruptChannel retain];
	wm->controlChannel = WIIUSE_IOBluetoothL2CAPChannel_to_IOBluetoothL2CAPChannelRef(controlChannel);
	wm->interruptChannel = WIIUSE_IOBluetoothL2CAPChannel_to_IOBluetoothL2CAPChannelRef(interruptChannel);
	
	return kIOReturnSuccess;
}

#pragma mark IOBluetoothL2CAPChannel delegates

- (void) disconnected:(IOBluetoothUserNotification*) notification fromDevice:(IOBluetoothDevice*) device {
	
	wiiuse_disconnected(wm);
}

@end

#endif // __APPLE__
