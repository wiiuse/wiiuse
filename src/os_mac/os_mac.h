/*
 * macOS transport for wiiuse, using macOS-managed Bluetooth HID.
 *
 * This replaces the direct IOBluetoothL2CAPChannel transport. Discovery remains
 * in os_mac_find.m; pairing and HID reports are handled by IOBluetoothDevicePair
 * and IOHIDManager respectively.
 */
#ifdef __APPLE__

/* Preserve the SDK compatibility setup used by wiiuse's original macOS files. */
#define BLUETOOTH_VERSION_USE_CURRENT

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothDevicePair.h>
#import <IOKit/hid/IOHIDLib.h>

#import "../wiiuse_internal.h"

#if defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7
#define WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE 1
#else
#define WIIUSE_MAC_OS_X_VERSION_10_7_OR_ABOVE 0
#endif

@interface WiiuseWiimote : NSObject<IOBluetoothDevicePairDelegate> {
    wiimote *wm;

    IOBluetoothDevice *device;
    IOBluetoothDevicePair *pairingAgent;

    /* Retained to initialise the system pairing coordinator's XPC path. */
    id coreBluetoothManager;

    IOHIDManagerRef hidManager;
    IOHIDDeviceRef hidDevice;
    uint8_t *inputReportBuffer;
    CFIndex inputReportBufferSize;

    BOOL pairingFinished;
    IOReturn pairingError;
    BOOL hidReady;

    NSMutableArray *receivedData; /* WiiuseReceivedMessage objects */
    NSLock *receivedDataLock;
}

- (id)initWithPtr:(wiimote *)wm device:(IOBluetoothDevice *)device;
- (IOReturn)connect;
- (void)disconnect;

- (int)readBuffer:(byte *)buffer length:(NSUInteger)bufferLength;
- (int)writeReport:(byte)report_type buffer:(byte *)buffer length:(NSUInteger)length;

/* Called by the IOHID C callbacks. */
- (void)didMatchHIDDevice:(IOHIDDeviceRef)hid;
- (void)didRemoveHIDDevice:(IOHIDDeviceRef)hid;
- (void)didReceiveInputReport:(const uint8_t *)report length:(CFIndex)length;
@end

@protocol WiiuseReceivedMessage <NSObject>
- (int)applyToStruct:(wiimote *)wm buffer:(byte *)buffer length:(NSUInteger)bufferLength;
@end

@interface WiiuseReceivedData : NSObject<WiiuseReceivedMessage> {
    NSData *data;
}
- (id)initWithBytes:(void *)bytes length:(NSUInteger)length;
- (id)initWithData:(NSData *)data;
@end

@interface WiiuseDisconnectionMessage : NSObject<WiiuseReceivedMessage>
@end

#endif /* __APPLE__ */
