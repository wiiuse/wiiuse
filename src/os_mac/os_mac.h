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


#define BLUETOOTH_VERSION_USE_CURRENT

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothL2CAPChannel.h>

#import "../wiiuse_internal.h"


#if defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7
#define WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE 1
#else
#define WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE 0
#endif


// WIIUSE_IOBluetoothDevice_to_IOBluetoothDeviceRef
#if WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE
#define WIIUSE_IOBluetoothDevice_to_IOBluetoothDeviceRef(device) \
	((IOBluetoothDeviceRef) (device))
#else
#define WIIUSE_IOBluetoothDevice_to_IOBluetoothDeviceRef(device) \
	[(device) getDeviceRef]
#endif

// WIIUSE_IOBluetoothDeviceRef_to_IOBluetoothDevice
#if WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE
#define WIIUSE_IOBluetoothDeviceRef_to_IOBluetoothDevice(ref) \
	((IOBluetoothDevice*) (ref))
#else
#define WIIUSE_IOBluetoothDeviceRef_to_IOBluetoothDevice(ref) \
	[IOBluetoothDevice withDeviceRef: (ref)]
#endif

// WIIUSE_IOBluetoothL2CAPChannel_to_IOBluetoothL2CAPChannelRef
#if WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE
#define WIIUSE_IOBluetoothL2CAPChannel_to_IOBluetoothL2CAPChannelRef(channel) \
	((IOBluetoothL2CAPChannelRef) (channel))
#else
#define WIIUSE_IOBluetoothL2CAPChannel_to_IOBluetoothL2CAPChannelRef(channel) \
	[(channel) getL2CAPChannelRef]
#endif

// WIIUSE_IOBluetoothL2CAPChannelRef_to_IOBluetoothL2CAPChannel
#if WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE
#define WIIUSE_IOBluetoothL2CAPChannelRef_to_IOBluetoothL2CAPChannel(ref) \
	((IOBluetoothL2CAPChannel*) (ref))
#else
#define WIIUSE_IOBluetoothL2CAPChannelRef_to_IOBluetoothL2CAPChannel(ref) \
	[IOBluetoothL2CAPChannel withChanneleRef: (ref)]
#endif


@interface WiiuseWiimote : NSObject<IOBluetoothL2CAPChannelDelegate> {
	wiimote* wm; // reference to the C wiimote struct
	IOBluetoothUserNotification* disconnectNotification;
}

- (id) initWithPtr: (wiimote*) wm;
- (IOReturn) connect;

@end


#endif // __APPLE__
