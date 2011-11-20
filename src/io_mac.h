/*
 *	io_mac.h
 *
 *	Written By:
 *		Gabriele Randelli	
 *		Email: < randelli (--AT--) dis [--DOT--] uniroma1 [--DOT--] it >
 *
 *	Copyright 2010
 *
 *	This file is part of wiiC.
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
 *	@brief I/O header file for MacOS.
 */
#ifndef IO_MAC_H
#define IO_MAC_H

#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>

#define BLUETOOTH_VERSION_USE_CURRENT 

#import <arpa/inet.h>				/* htons() */
#import <IOBluetooth/IOBluetoothUtilities.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>
#import <IOBluetooth/objc/IOBluetoothDeviceInquiry.h>
#import <IOBluetooth/objc/IOBluetoothL2CAPChannel.h>

#import "wiic_internal.h"
#import "io.h"

@interface WiiSearch : NSObject
{
	IOBluetoothDeviceInquiry* inquiry;
	BOOL isDiscovering;
	// Number of found wiimotes
	int foundWiimotes;
	// Maximum number of wiimotes to be searched
	int maxWiimotes;
	// The Wiimotes structure
	wiimote** wiimotes;
}

- (BOOL) isDiscovering;
- (void) setDiscovering:(BOOL) flag;
- (void) setWiimoteStruct:(wiimote**) wiimote_struct;
- (int) getFoundWiimotes;
- (IOReturn) start:(unsigned int) timeout maxWiimotes:(unsigned int) wiimotesNum;
- (IOReturn) stop;
- (IOReturn) close;
- (void) retrieveWiimoteInfo:(IOBluetoothDevice*) device;
- (void) deviceInquiryStarted:(IOBluetoothDeviceInquiry*) sender;
- (void) deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry *) sender device:(IOBluetoothDevice *) device;
- (void) deviceInquiryComplete:(IOBluetoothDeviceInquiry*) sender error:(IOReturn) error aborted:(BOOL) aborted;

@end

@interface WiiConnect : NSObject
{
	// Buffer to store incoming data from the Wiimote
	NSData* receivedMsg;
	unsigned int msgLength;
	
	// Reference to the relative wiimote struct (used only to complete handshaking)
	wiimote* _wm;
	BOOL isReading;
	BOOL timeout;
	BOOL disconnecting;
}

- (IOBluetoothL2CAPChannel *) openL2CAPChannelWithPSM:(BluetoothL2CAPPSM) psm device:(IOBluetoothDevice*) device delegate:(id) delegate;
- (IOReturn) connectToWiimote:(wiimote*) wm;
- (void) l2capChannelData:(IOBluetoothL2CAPChannel*) channel data:(byte *) data length:(NSUInteger) length;
- (byte*) getNextMsg;
- (unsigned int) getMsgLength;
- (void) deleteMsg;
- (void) disconnected:(IOBluetoothUserNotification*) notification fromDevice:(IOBluetoothDevice*) device;
- (BOOL) isReading;
- (void) setReading:(BOOL) flag;
- (BOOL) isTimeout;
- (void) setTimeout:(BOOL) flag;
- (void) startTimerThread;
- (void) wakeUpMainThreadRunloop:(id)arg;
- (BOOL) isDisconnecting;
@end

#endif /* IO_MAC_H */
