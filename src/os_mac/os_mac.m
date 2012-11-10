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

#import "os_mac.h"

#import "../events.h"

#import <IOBluetooth/IOBluetoothUtilities.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>
#import <IOBluetooth/objc/IOBluetoothDeviceInquiry.h>
#import <IOBluetooth/objc/IOBluetoothL2CAPChannel.h>


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
