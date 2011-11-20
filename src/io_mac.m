/*
 *	io_mac.m
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
 *	@brief Handles device I/O for Mac.
 */
#ifdef __APPLE__

#import "io_mac.h"

// Used just to retrieve max num of wiimotes in handler functions
static int max_num_wiimotes = 0;

@implementation WiiSearch

#pragma mark -
#pragma mark WiiSearch
- (id) init
{
	self = [super init];
	foundWiimotes = 0;
	isDiscovering = NO;
	if (self != nil) { 
		/* 
		 * Calling IOBluetoothLocalDeviceAvailable has two advantages:
		 * 1. it sets up a event source in the run loop (bug for C version of the bluetooth api)
		 * 2. it checks for the availability of the BT hardware
		 */
		if (![IOBluetoothHostController defaultController])
		{
			[self release];
			self = nil;
		}		
	}

	return self;
}

- (void) dealloc
{	
	inquiry = 0;
	WIIC_DEBUG("Wiimote Discovery released");
	[super dealloc];
}

- (BOOL) isDiscovering
{
	return isDiscovering;
}

- (void) setDiscovering:(BOOL) flag
{
	isDiscovering = flag;
}

- (void) setWiimoteStruct:(wiimote**) wiimote_struct
{
	wiimotes = wiimote_struct;
}

- (int) getFoundWiimotes
{
	return foundWiimotes;
}

- (IOReturn) start:(unsigned int) timeout maxWiimotes:(unsigned int) wiimotesNum
{
	if (![IOBluetoothHostController defaultController]) {
		WIIC_ERROR("Unable to find any bluetooth receiver on your host.");
		return kIOReturnNotAttached;
	}
	
	// If we are currently discovering, we can't start a new discovery right now.
	if ([self isDiscovering]) {
		WIIC_INFO("Wiimote search is already in progress...");
		return kIOReturnSuccess;
	}
	
	[self close];
	maxWiimotes = wiimotesNum;
	foundWiimotes = 0;

	inquiry = [IOBluetoothDeviceInquiry inquiryWithDelegate:self];
	// We set the search timeout
	if(timeout == 0)
		[inquiry setInquiryLength:5];
	else if(timeout < 20)
		[inquiry setInquiryLength:timeout];
	else
		[inquiry setInquiryLength:20];
	[inquiry setSearchCriteria:kBluetoothServiceClassMajorAny majorDeviceClass:WM_DEV_MAJOR_CLASS minorDeviceClass:WM_DEV_MINOR_CLASS];
	[inquiry setUpdateNewDeviceNames:NO];

	IOReturn status = [inquiry start];
	if (status == kIOReturnSuccess) {
		[inquiry retain];
	} else {
		[self close];
		WIIC_ERROR("Unable to search for bluetooth devices.");
	}
	
	return status;
}

- (IOReturn) stop
{
	return [inquiry stop];
}

- (IOReturn) close
{
	IOReturn ret = kIOReturnSuccess;
	
	ret = [inquiry stop];
	[inquiry release];
	inquiry = nil;
	
	WIIC_DEBUG(@"Discovery closed");
	return ret;
}

#pragma mark -
#pragma mark IOBluetoothDeviceInquiry delegates
//*************** HANDLERS FOR WIIC_FIND FOR MACOSX *******************/
- (void) retrieveWiimoteInfo:(IOBluetoothDevice*) device
{	
	// We set the device reference (we must retain it to use it after the search)
	wiimotes[foundWiimotes]->device = [[device retain] getDeviceRef];
	wiimotes[foundWiimotes]->address = (CFStringRef)[[device getAddressString] retain];
	
	// C String (common for Mac and Linux)
	CFStringGetCString(wiimotes[foundWiimotes]->address,wiimotes[foundWiimotes]->bdaddr_str,18,kCFStringEncodingMacRoman);
	
	WIIMOTE_ENABLE_STATE(wiimotes[foundWiimotes], WIIMOTE_STATE_DEV_FOUND);
	WIIC_INFO("Found Wiimote (%s) [id %i].",CFStringGetCStringPtr(wiimotes[foundWiimotes]->address, kCFStringEncodingMacRoman),wiimotes[foundWiimotes]->unid);
	++foundWiimotes;
}

- (void) deviceInquiryStarted:(IOBluetoothDeviceInquiry*) sender
{
	[self setDiscovering:YES];
}

- (void) deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry *) sender device:(IOBluetoothDevice *) device
{
	if(foundWiimotes < maxWiimotes)
		[self retrieveWiimoteInfo:device];
	else
		[inquiry stop];
}

- (void) deviceInquiryComplete:(IOBluetoothDeviceInquiry*) sender error:(IOReturn) error aborted:(BOOL) aborted
{	
	// The inquiry has completed, we can now process what we have found
	[self setDiscovering:NO];

	// We stop the search because of errors
	if ((error != kIOReturnSuccess) && !aborted) {
		foundWiimotes = 0;
		[self close];
		WIIC_ERROR("Search not completed, because of unexpected errors. This error can be due to a short search timeout.");
		return;
	}
	
	foundWiimotes = [[inquiry foundDevices] count];
}

@end

@implementation WiiConnect
#pragma mark -
#pragma mark WiiConnect
- (id) init
{
	self = [super init];
	receivedMsg = [[NSData alloc] init];
	msgLength = 0;
	_wm = 0;
	isReading = NO;
	timeout = NO;
	disconnecting = NO;
	return self;
}

- (void) dealloc
{	
	WIIC_DEBUG("Wiimote released");
	if(receivedMsg)
		[receivedMsg release];
	receivedMsg = 0;
	_wm = 0;
	[super dealloc];
}

- (byte*) getNextMsg
{
	if(!receivedMsg)
		return 0;

	return (byte*)[receivedMsg bytes];
}

- (void) deleteMsg
{
	if(receivedMsg) {
		[receivedMsg release];
		msgLength = 0;
	}
}

- (BOOL) isDisconnecting
{
	return disconnecting;
}

- (BOOL) isReading
{
	return isReading;
}

- (void) setReading:(BOOL) flag
{
	isReading = flag;
}

- (BOOL) isTimeout
{
	return timeout;
}

- (void) setTimeout:(BOOL) flag
{
	timeout = flag;
}

- (void) startTimerThread
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	// Timer 
	sleep(1);
	
	[pool drain];
}

- (void) wakeUpMainThreadRunloop:(id)arg
{
    // This method is executed on main thread!
    // It doesn't need to do anything actually, just having it run will
    // make sure the main thread stops running the runloop
}


- (IOBluetoothL2CAPChannel*) openL2CAPChannelWithPSM:(BluetoothL2CAPPSM) psm device:(IOBluetoothDevice*) device delegate:(id) delegate
{
	IOBluetoothL2CAPChannel* channel = nil;
	
	if ([device openL2CAPChannelSync:&channel withPSM:psm delegate:delegate] != kIOReturnSuccess) 
		channel = nil;
	
	return channel;
}

- (IOReturn) connectToWiimote:(wiimote*) wm
{
	IOBluetoothDevice* device = [IOBluetoothDevice withDeviceRef:wm->device];
	IOBluetoothL2CAPChannel* outCh = nil;
	IOBluetoothL2CAPChannel* inCh = nil;
	
	if(!device) {
		WIIC_ERROR("Non existent device or already connected.");
		return kIOReturnBadArgument;
	}
	
	outCh = [self openL2CAPChannelWithPSM:WM_OUTPUT_CHANNEL device:device delegate:self];
	if (!outCh) {
		WIIC_ERROR("Unable to open L2CAP output channel (id %i).", wm->unid);
		[device closeConnection];
		return kIOReturnNotOpen;
	}
	wm->outputCh = [[outCh retain] getL2CAPChannelRef];
	usleep(20000);
	
	inCh = [self openL2CAPChannelWithPSM:WM_INPUT_CHANNEL device:device delegate:self];
	if (!inCh) {
		WIIC_ERROR("Unable to open L2CAP input channel (id %i).", wm->unid);
		[device closeConnection];
		return kIOReturnNotOpen;
	}
	wm->inputCh = [[inCh retain] getL2CAPChannelRef];
	usleep(20000);
	
	IOBluetoothUserNotification* disconnectNotification = [device registerForDisconnectNotification:self selector:@selector(disconnected:fromDevice:)];
	if(!disconnectNotification) {
		WIIC_ERROR("Unable to register disconnection handler (id %i).", wm->unid);
		[device closeConnection];
		return kIOReturnNotOpen;
	}
	
	// We store the reference to its relative structure (Used for completing the handshake step)
	_wm = wm;
	
	return kIOReturnSuccess;
}

#pragma mark -
#pragma mark IOBluetoothL2CAPChannel delegates
- (void) disconnected:(IOBluetoothUserNotification*) notification fromDevice:(IOBluetoothDevice*) device
{
	[self deleteMsg];
	[self setReading:NO];
	disconnecting = YES;
		
	// The wiimote_t struct must be re-initialized due to the disconnection
	wiic_disconnected(_wm) ;	
}

//*************** HANDLERS FOR WIIC_IO_READ FOR MACOSX *******************/
- (void) l2capChannelData:(IOBluetoothL2CAPChannel*) channel data:(byte *) data length:(NSUInteger) length 
{
	// This is done in case the output channel woke up this handler
	if(!data) {
	    [self setReading:NO];
		length = 0;
		return;
	}

	/* 
	 * This is called if we are receiving data before completing
	 * the handshaking, hence before calling wiic_poll
	 */
	if(WIIMOTE_IS_SET(_wm, WIIMOTE_STATE_HANDSHAKE))
		propagate_event(_wm, data[1], data+2);

	receivedMsg = [[NSData dataWithBytes:data length:length] retain]; 
	msgLength = length;
	
    // This is done when a message is successfully received. Stop the main loop after reading
    [self setReading:NO];
}

- (unsigned int) getMsgLength
{
	return msgLength;
}

- (void) l2capChannelReconfigured:(IOBluetoothL2CAPChannel*) l2capChannel
{
      //NSLog(@"l2capChannelReconfigured");
}

- (void) l2capChannelWriteComplete:(IOBluetoothL2CAPChannel*) l2capChannel refcon:(void*) refcon status:(IOReturn) error
{
      //NSLog(@"l2capChannelWriteComplete");
}

- (void) l2capChannelQueueSpaceAvailable:(IOBluetoothL2CAPChannel*) l2capChannel
{
      //NSLog(@"l2capChannelQueueSpaceAvailable");
}

- (void) l2capChannelOpenComplete:(IOBluetoothL2CAPChannel*) l2capChannel status:(IOReturn) error
{
	//NSLog(@"l2capChannelOpenComplete (PSM:0x%x)", [l2capChannel getPSM]);
}


@end

#pragma mark -
#pragma mark Wiiuse
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
int wiic_find(struct wiimote_t** wm, int max_wiimotes, int timeout) 
{
	int found_wiimotes = 0;

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	WiiSearch* search = [[WiiSearch alloc] init];
	[search setWiimoteStruct:wm];
	
	if(timeout) { // Single search
		[search start:timeout maxWiimotes:max_wiimotes];
	
		NSRunLoop *theRL = [NSRunLoop currentRunLoop];
		while ([search isDiscovering] && [theRL runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]);
	
		found_wiimotes = [search getFoundWiimotes];
	}
	else { // Unlimited search
		found_wiimotes = 0;
		while(!found_wiimotes) {
			[search start:timeout maxWiimotes:max_wiimotes];
	
			NSRunLoop *theRL = [NSRunLoop currentRunLoop];
			while ([search isDiscovering] && [theRL runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]);
	
			found_wiimotes = [search getFoundWiimotes];	
		}
	}
	
	WIIC_INFO("Found %i Wiimote device(s).", found_wiimotes);
	
	[search release];
	[pool drain];
	
	return found_wiimotes;
}


//*************** HANDLERS FOR WIIC_DISCONNECT FOR MACOSX *******************/
/**
 *	@brief Disconnect a wiimote.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *
 *	@see wiic_connect()
 *
 *	Note that this will not free the wiimote structure.
 */
void wiic_disconnect(struct wiimote_t* wm) 
{
	IOReturn error;

    if (!wm || !WIIMOTE_IS_CONNECTED(wm))
            return;

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	// Input Channel
	if(wm->inputCh) {
		IOBluetoothL2CAPChannel* inCh = [IOBluetoothL2CAPChannel withL2CAPChannelRef:wm->inputCh];
		error = [inCh closeChannel];
		[inCh setDelegate:nil];
		if(error != kIOReturnSuccess) 
			WIIC_ERROR("Unable to close input channel (id %i).", wm->unid);			
		usleep(10000);
		[inCh release];
		inCh = nil;
		wm->inputCh = 0;
	}
	
	// Output Channel
	if(wm->outputCh) {
		IOBluetoothL2CAPChannel* outCh = [IOBluetoothL2CAPChannel withL2CAPChannelRef:wm->outputCh];
		error = [outCh closeChannel];
		[outCh setDelegate:nil];
		if(error != kIOReturnSuccess) 
			WIIC_ERROR("Unable to close output channel (id %i).", wm->unid);			
		usleep(10000);
		[outCh release];
		outCh = nil;
		wm->outputCh = 0;
	}
	
	// Device
	if(wm->device) {
		IOBluetoothDevice* device = [IOBluetoothDevice withDeviceRef:wm->device];
		error = [device closeConnection];

		if(error != kIOReturnSuccess) 
			WIIC_ERROR("Unable to close the device connection (id %i).", wm->unid);			
		usleep(10000);
		[device release];
		device = nil;
		wm->device = 0;
	}
	
	[pool drain];
	
	wm->event = WIIC_NONE;

    WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
    WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
}

/**
 *	@brief Connect to a wiimote with a known address.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param address	The address of the device to connect to.
 *					If NULL, use the address in the struct set by wiic_find().
 *  @param autoreconnect	Re-connects the device in case of unexpected disconnection.
 *
 *	@return 1 on success, 0 on failure
 */
int wiic_connect_single(struct wiimote_t* wm, char* address, int autoreconnect) 
{
	// Skip if already connected or device not found
	if(!wm || WIIMOTE_IS_CONNECTED(wm) || wm->device == 0) {
		WIIC_ERROR("Non existent device or already connected.");
		return 0;	
	}

	// Convert the IP address
	// FIXME - see if it is possible

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	WiiConnect* connect = [[[WiiConnect alloc] init] autorelease];
	if([connect connectToWiimote:wm] == kIOReturnSuccess) {
		WIIC_INFO("Connected to wiimote [id %i].", wm->unid);
		// This is stored to retrieve incoming data
		wm->connectionHandler = (void*)([connect retain]);

		// Do the handshake 
		WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
		wiic_handshake(wm, NULL, 0);
		wiic_set_report_type(wm);
		
		/* autoreconnect flag */
		wm->autoreconnect = autoreconnect;
		
		[pool drain];
	}
	else {
		[pool drain];
		return 0;
	}
	
	return 1;
}

/**
 *	@brief Connect to a wiimote or wiimotes once an address is known.
 *
 *	@param wm			An array of wiimote_t structures.
 *	@param wiimotes		The number of wiimote structures in \a wm.
 *	@param autoreconnect	Re-connect to the device in case of unexpected disconnection.
 *
 *	@return The number of wiimotes that successfully connected.
 *
 *	@see wiic_find()
 *	@see wiic_connect_single()
 *	@see wiic_disconnect()
 *
 *	Connect to a number of wiimotes when the address is already set
 *	in the wiimote_t structures.  These addresses are normally set
 *	by the wiic_find() function, but can also be set manually.
 */
int wiic_connect(struct wiimote_t** wm, int wiimotes, int autoreconnect) 
{
	int connected = 0;
	int i = 0;
	
	for (; i < wiimotes; ++i) {
		if(!(wm[i])) {
			WIIC_ERROR("Trying to connect more Wiimotes than initialized");
			return;
		}
		
		if (!WIIMOTE_IS_SET(wm[i], WIIMOTE_STATE_DEV_FOUND))
			// If the device address is not set, skip it 
			continue;

		if (wiic_connect_single(wm[i], NULL, autoreconnect))
			++connected;
	}

	return connected;
}

/**
 *	@brief Load Wii devices registered in the wiimotes.config file.
 *
 *	@param wm			An array of wiimote_t structures.
 *
 *	@return The number of wiimotes successfully loaded.
 *
 *	@see wiic_find()
 *  @see wiic_connect()
 *	@see wiic_connect_single()
 *	@see wiic_disconnect()
 *
 *	From version 0.53, it is possible to register the MAC address of your 
 *  Wii devices. This allows to automatically load them, without waiting for any
 *  search timeout. To register a new device, go to: <HOME_DIR>/.wiic/ and
 *  edit the file wiimotes.config, by adding the MAC address of the device 
 *  you want to register (one line per MAC address).
 */
int wiic_load(struct wiimote_t** wm) 
{
	int loaded = 0;
	int i = 0;
	char str[200];
	char configPath[100];
	char* tmp = 0;
	
	// Retrieve the HOME environment variable
	tmp = getenv("HOME");
	strcpy(configPath,tmp);
	strncat(configPath,"/.wiic/wiimotes.config",22);

	// Open the config file
	FILE* fd = 0;
	fd = fopen(configPath,"r");
	if(!fd) 
		return loaded;

	// Read line by line
	while(fgets(str,sizeof(str),fd) != NULL && loaded < 1) {
		int len = strlen(str)-1;
      	if(str[len] == '\n') 
        	str[len] = 0;
		loaded++;
	}
	
	// We initialize the device structure
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	for (; i < loaded; ++i) {
		NSString* string = [NSString stringWithCString:str encoding:[NSString defaultCStringEncoding]];
		BluetoothDeviceAddress deviceAddr;
		IOBluetoothNSStringToDeviceAddress(string, &deviceAddr);
		IOBluetoothDevice* device = [IOBluetoothDevice withAddress:&deviceAddr];
		wm[i]->device = [[device retain] getDeviceRef];
		wm[i]->address = (CFStringRef)[[device getAddressString] retain];
		WIIMOTE_ENABLE_STATE(wm[i], WIIMOTE_STATE_DEV_FOUND);
		WIIC_INFO("Loaded Wiimote (%s) [id %i].",CFStringGetCStringPtr(wm[i]->address, kCFStringEncodingMacRoman),wm[i]->unid);
	}
	[pool drain];

	return loaded;
}

int wiic_io_read(struct wiimote_t* wm) 
{
    if (!WIIMOTE_IS_SET(wm, WIIMOTE_STATE_CONNECTED))
            return 0;

    /* If this wiimote is not connected, skip it */
    if (!WIIMOTE_IS_CONNECTED(wm))
            return 0;
			
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	WiiConnect* deviceHandler = 0;
	deviceHandler = (WiiConnect*)(wm->connectionHandler);

	/* If this wiimote is disconnecting, skip it */
	if (!deviceHandler || [deviceHandler isDisconnecting]) {
		[pool drain];
		return 0;
	}

    // Run the main loop to get bt data
	[deviceHandler setReading:YES];
	[deviceHandler setTimeout:NO];

	// We start the thread which manages the timeout to implement a non-blocking read 
	[NSThread detachNewThreadSelector:@selector(startTimerThread) toTarget:deviceHandler withObject:nil];

	NSRunLoop *theRL = [NSRunLoop currentRunLoop];
	// Two possible events: we receive and incoming message or there is a timeout
	while([deviceHandler isReading] && ![deviceHandler isTimeout]) {
		NSAutoreleasePool *pool_loop = [[NSAutoreleasePool alloc] init]; // This is used for fast release of NSDate, otherwise it leaks
		[theRL runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
		[pool_loop drain];
	}
	
	// In this case we have no incoming data (TIMEOUT)
	if([deviceHandler isTimeout]) {
		[pool drain];
		return 0;
	}

	if(!(wm->connectionHandler)) {
		WIIC_ERROR("Unable to find the connection handler (id %i).", wm->unid);
		[pool drain];
		return 0;
	}

	// Read next message
	byte* buffer = 0;
	unsigned int length = 0;
	if(![deviceHandler isDisconnecting]) {
		buffer = [deviceHandler getNextMsg];
		length = [deviceHandler getMsgLength];
	}

	if(!buffer || !length) {
		[pool drain];
		return 0;
	}

	// Forward to WiiC
	if(length < sizeof(wm->event_buf)) 
		memcpy(wm->event_buf,buffer,length);
	else {
		WIIC_DEBUG("Received data are more than the buffer.... strange! (id %i)", wm->unid);
		memcpy(wm->event_buf,buffer,sizeof(wm->event_buf));
	}

	// Release the consumed message
	[deviceHandler deleteMsg];

	[pool drain];
		
    return 1;
}


int wiic_io_write(struct wiimote_t* wm, byte* buf, int len) 
{
	unsigned int length = (unsigned int)len;
	
	// Small check before writing
	if(!wm || !(wm->outputCh)) {
		WIIC_ERROR("Attempt to write over non-existent channel (id %i).", wm->unid);
		perror("Error Details");
		return 0;
	}

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	IOBluetoothL2CAPChannel* channel = [IOBluetoothL2CAPChannel withL2CAPChannelRef:wm->outputCh];
    IOReturn error = [channel writeSync:buf length:length];
	if (error != kIOReturnSuccess) 
		WIIC_ERROR("Unable to write over the output channel (id %i).", wm->unid);		
    usleep(10000);
	
    [pool drain];

	return (error == kIOReturnSuccess ? len : 0);
}

#endif 
