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
#pragma mark connect, disconnect

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
	} else if(wm && (!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_DEV_FOUND) || wm->objc_wm == NULL)) {
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
	WiiuseWiimote* objc_wm = (WiiuseWiimote*) wm->objc_wm;
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
		
		if (!WIIMOTE_IS_SET(wm[i], WIIMOTE_STATE_DEV_FOUND) || !wm[i]->objc_wm) {
			// If the device is not found, skip it
			continue;
		}
		
		if (wiiuse_os_connect_single(wm[i]))
			++connected;
	}
	
	return connected;
}

void wiiuse_os_disconnect(struct wiimote_t* wm) {
	if (!wm || !WIIMOTE_IS_CONNECTED(wm) || !wm->objc_wm)
		return;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[((WiiuseWiimote*)wm->objc_wm) disconnect];
	[pool drain];
}

#pragma mark -
#pragma mark poll, read, write

int wiiuse_os_poll(struct wiimote_t** wm, int wiimotes) {
	int i, evnt = 0;
	
	if (!wm) return 0;
	
	for (i = 0; i < wiimotes; ++i) {
		wm[i]->event = WIIUSE_NONE;
		
		if (wiiuse_os_read(wm[i])) {
			/* propagate the event, messages should be read as in linux, starting from the second element */
			propagate_event(wm[i], wm[i]->event_buf[1], wm[i]->event_buf+2);
			evnt += (wm[i]->event != WIIUSE_NONE);
			
			/* clear out the event buffer */
			memset(wm[i]->event_buf, 0, sizeof(wm[i]->event_buf));
		} else {
			idle_cycle(wm[i]);
		}
	}
	
	return evnt;
}

int wiiuse_os_read(struct wiimote_t* wm) {
	if(!wm || !wm->objc_wm || !WIIMOTE_IS_CONNECTED(wm)) {
		WIIUSE_ERROR("Attempting to read from NULL or unconnected Wiimote");
		return 0;
	}
	
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	WiiuseWiimote* objc_wm = (WiiuseWiimote*) wm->objc_wm;
	int result = [objc_wm read];
	
	[pool drain];
	return result;
}

int wiiuse_os_write(struct wiimote_t* wm, byte* buf, int len) {
	if(!wm || !wm->objc_wm || !WIIMOTE_IS_CONNECTED(wm)) {
		WIIUSE_ERROR("Attempting to write to NULL or unconnected Wiimote");
		return 0;
	}
	
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	WiiuseWiimote* objc_wm = (WiiuseWiimote*) wm->objc_wm;
	int result = [objc_wm writeBuffer: buf length: (NSUInteger)len];
	
	[pool drain];
	return result;
}

#pragma mark -
#pragma mark platform fields

void wiiuse_init_platform_fields(struct wiimote_t* wm) {
	wm->objc_wm = NULL;
}

void wiiuse_cleanup_platform_fields(struct wiimote_t* wm) {
	if(!wm) return;
	WiiuseWiimote* objc_wm = (WiiuseWiimote*) wm->objc_wm;
	
	// disconnect
	// Note: this should already have happened, because this function
	// is called once the device is disconnected. This is just paranoia.
	[objc_wm disconnect];
	
	// release WiiuseWiimote object
	[objc_wm release];
	wm->objc_wm = NULL;
}

#pragma mark -
#pragma mark WiiuseWiimote

@implementation WiiuseWiimote

#pragma mark init, dealloc

- (id) initWithPtr: (wiimote*) wm_ device:(IOBluetoothDevice *)device_ {
	self = [super init];
	if(self) {
		wm = wm_;
		
		device = device_;
		controlChannel = nil;
		interruptChannel = nil;
		
		disconnectNotification = nil;
		
		receivedData = [[NSMutableArray alloc] initWithCapacity: 2];
		receivedDataLock = [[NSLock alloc] init];
	}
	return self;
}

- (void) dealloc {
	wm = NULL;
	
	[interruptChannel release];
	[controlChannel release];
	[device release];
	
	[disconnectNotification unregister];
	[disconnectNotification release];
	
	[receivedData release];
	
	[super dealloc];
}

#pragma mark connect, disconnect

- (BOOL) connectChannel: (IOBluetoothL2CAPChannel**) pChannel PSM: (BluetoothL2CAPPSM) psm {
	if ([device openL2CAPChannelSync:pChannel withPSM:psm delegate:self] != kIOReturnSuccess) {
		WIIUSE_ERROR("Unable to open L2CAP channel [id %i].", wm->unid);
		*pChannel = nil;
		return NO;
	} else {
		[*pChannel retain];
		return YES;
	}
}

- (IOReturn) connect {
	if(!device) {
		WIIUSE_ERROR("Missing device.");
		return kIOReturnBadArgument;
	}
	
	// open channels
	if(![self connectChannel:&controlChannel PSM:WM_OUTPUT_CHANNEL]) {
		[self disconnect];
		return kIOReturnNotOpen;
	} else if(![self connectChannel:&interruptChannel PSM:WM_INPUT_CHANNEL]) {
		[self disconnect];
		return kIOReturnNotOpen;
	}
	
	// register for device disconnection
	disconnectNotification = [device registerForDisconnectNotification:self selector:@selector(disconnected:fromDevice:)];
	if(!disconnectNotification) {
		WIIUSE_ERROR("Unable to register disconnection handler [id %i].", wm->unid);
		[self disconnect];
		return kIOReturnNotOpen;
	}
	
	return kIOReturnSuccess;
}

- (void) disconnectChannel: (IOBluetoothL2CAPChannel**) pChannel {
	if(!pChannel) return;
	
	if([*pChannel closeChannel] != kIOReturnSuccess)
		WIIUSE_ERROR("Unable to close channel [id %i].", wm ? wm->unid : -1);
	[*pChannel release];
	*pChannel = nil;
}

- (void) disconnect {
	// channels
	[self disconnectChannel:&interruptChannel];
	[self disconnectChannel:&controlChannel];
	
	// device
	if([device closeConnection] != kIOReturnSuccess)
		WIIUSE_ERROR("Unable to close the device connection [id %i].", wm ? wm->unid : -1);
	[device release];
	device = nil;
}

- (void) disconnected:(IOBluetoothUserNotification*) notification fromDevice:(IOBluetoothDevice*) device {
	
	wiiuse_disconnected(wm);
}

#pragma mark read, write

- (int) checkForAvailableData {
	unsigned int length = 0;
	
	[receivedDataLock lock];
	if([receivedData count]) {
		// pop first item from queue
		NSData* firstData = [receivedData objectAtIndex:0];
		[receivedData removeObjectAtIndex:0];
		
		byte* data = (byte*) [firstData bytes];
		length = [firstData length];
		
		// forward event data to C struct
		memcpy(wm->event_buf, data, sizeof(wm->event_buf));
		
		// clear local buffer
		[firstData release];
	}
	[receivedDataLock unlock];
	
	return length;
}

- (void) waitForInclomingData: (NSTimeInterval) duration {
	NSDate* timeoutDate = [NSDate dateWithTimeIntervalSinceNow: duration];
	NSRunLoop *theRL = [NSRunLoop currentRunLoop];
	while (true) {
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]; // This is used for fast release of NSDate, otherwise it leaks
		
		if(![theRL runMode:NSDefaultRunLoopMode beforeDate:timeoutDate]) {
			WIIUSE_ERROR("Could not start run loop while waiting for read [id %i].", wm->unid);
			break;
		}
		
		[receivedDataLock lock];
		NSUInteger count = [receivedData count];
		[receivedDataLock unlock];
		if(count) {
			// received some data, stop waiting
			break;
		}
		
		if([timeoutDate isLessThanOrEqualTo:[NSDate date]]) {
			// timeout
			break;
		}
		
		[pool drain];
	}
}

- (int) read {
	// is there already some data to read?
	int result = [self checkForAvailableData];
	if(!result) {
		// wait a short amount of time, until data becomes available
		[self waitForInclomingData:1];
		
		// check again
		result = [self checkForAvailableData];
	}
	
	return result;
}

- (int) writeBuffer: (byte*) buffer length: (NSUInteger) length {
	if(controlChannel == nil) {
		WIIUSE_ERROR("Attempted to write to nil control channel [id %i].", wm->unid);
		return 0;
	}
	
	IOReturn error = [controlChannel writeSync:buffer length:length];
	if (error != kIOReturnSuccess) {
		WIIUSE_ERROR("Error writing to control channel [id %i].", wm->unid);
		
		WIIUSE_DEBUG("Attempting to reopen the control channel [id %i].", wm->unid);
		[self disconnectChannel:&controlChannel];
		[self connectChannel:&controlChannel PSM:WM_OUTPUT_CHANNEL];
		if(!controlChannel) {
			WIIUSE_ERROR("Error reopening the control channel [id %i].", wm->unid);
			[self disconnect];
		} else {
			WIIUSE_DEBUG("Attempting to write again to the control channel [id %i].", wm->unid);
			error = [controlChannel writeSync:buffer length:length];
			if (error != kIOReturnSuccess)
				WIIUSE_ERROR("Unable to write again to the control channel [id %i].", wm->unid);
		}
	}
	
	return (error == kIOReturnSuccess) ? length : 0;
}

#pragma mark IOBluetoothL2CAPChannelDelegate

- (void) l2capChannelData:(IOBluetoothL2CAPChannel*)channel data:(void*)data_ length:(NSUInteger)length {
	
	byte* data = (byte*) data_;
	
	// This is done in case the output channel woke up this handler
	if(!data || ([channel PSM] == WM_OUTPUT_CHANNEL)) {
		return;
	}
	
	// log the received data
#ifdef WITH_WIIUSE_DEBUG
	{
		printf("[DEBUG] (id %i) RECV: (%x) ", wm->unid, data[0]);
		int x;
		for (x = 1; x < length; ++x)
			printf("%.2x ", data[x]);
		printf("\n");
	}
#endif
	
	/*
	 * This is called if we are receiving data before completing
	 * the handshaking, hence before calling wiiuse_poll
	 */
	if(WIIMOTE_IS_SET(wm, WIIMOTE_STATE_HANDSHAKE)) {
		propagate_event(wm, data[1], data+2);
		return;
	}
	
	// copy the data into the buffer
	[receivedDataLock lock];
	[receivedData addObject: [[NSData alloc] initWithBytes:data length:length]];
	[receivedDataLock unlock];
}

@end

#endif // __APPLE__
