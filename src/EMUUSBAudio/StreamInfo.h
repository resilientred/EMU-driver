//
//  StreamInfo.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 25/11/14.
//  Copyright (c) 2014 com.emu. All rights reserved.
//

#ifndef EMUUSBAudio_StreamInfo_h
#define EMUUSBAudio_StreamInfo_h

#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/IOSubMemoryDescriptor.h>


//	-----------------------------------------------------------------
#define	kSampleRate_44100				44100
#define kDefaultSamplingRate			kSampleRate_44100
#define	kBitDepth_16bits				16
#define kBitDepth_24bits				24
#define	kChannelCount_MONO				1
#define	kChannelCount_STEREO			2
#define kChannelCount_QUAD				4
#define kChannelCount_10				10 // this is stupid

/*! initial time in ms for USB callback timer. Normally kRefreshInterval is used. */
#define kMinimumInterval				1
#define kMinimumFrameOffset				1
#define kUSB2FrameOffset				1// additional offset for high speed USB 2.0
#define kWallTimeExtraPrecision         10000
/*! something nanoseconds used in waitForFirstUSBFrameCompletion, maybe for initial sync?*/
#define kWallTimeConstant				1000000
/*! main rate for USB callback timer (ms) */
#define kRefreshInterval				128
/*! after this number of reads from the USB, the time is re-anchored. See getAnchorFrameAndTimeStamp*/
#define kRefreshCount					8
/*! params for very simple lowpass filter in jitter filter */
#define kInvariantCoeff					1024
#define kInvariantCoeffM1				1023
#define kInvariantCoeffDiv2				512


// these should be dynamic based on poll interval
/* Wouter: I think this confusion between 1 and 8 is caused by differences in USB 1 vs 2.  Earlier versions of the specification divide bus time into 1-millisecond frames, each of which can carry multiple transactions to multiple destinations. (A transaction contains two or more packets: a token packet and one or more data packets, a handshake packet, or both.) The USB 2.0 specification divides the 1-millisecond frame into eight, 125-microsecond microframes, each of which can carry multiple transactions to multiple destinations.
 
 This driver seems hard coded to do 1000 requests per second (see CalculateSamplesPerFrame.). What puzzles me is that this is the maximum.
 
 Note on the docu that I added: A number of functions here are obligatory implementations of the IOAudioEngine. Others are support functions for our convenience. It is unfortunate that the distinction is unclear and also that I have to document functions that should (and probably do) already have documentation in the interface definition.  Maybe I'm missing something?
 */

/*! Number of USB frames per millisecond for USB2 */
#define kNumberOfFramesPerMillisecond 8
#define kPollIntervalShift 3 // log2 of kNumberOfFramesPerMillisecond
/*! Number of frames that we transfer in a request to USB. USB transfers frames once per millisecond
 But we prepare NUMBER_FRAMES so that we do not have to deal with that every ms but only every NUMBER_FRAMES ms.
 These frames are grouped into a single request, the larger this number the larger the chunks we get from USB.
 
 Technically this size should be irrelevant, because the array is refreshed in memory anyway and because the
 timestamps we need are stored in the USB frames as long as we need them.
 
 However, it seems that the exact time at which we call takeTimeStamp is critical as well.
 
 
 */

#define NUMBER_FRAMES 64
//#define NUMBER_FRAMES 16



#define RECORD_NUM_USB_FRAME_LISTS				4
#define RECORD_FRAME_LISTS_LIMIT				RECORD_NUM_USB_FRAME_LISTS - 1
#define RECORD_NUM_USB_FRAMES_PER_LIST			NUMBER_FRAMES
#define RECORD_NUM_USB_FRAME_LISTS_TO_QUEUE		4

#define PLAY_NUM_USB_FRAME_LISTS				4
#define PLAY_NUM_USB_FRAMES_PER_LIST			NUMBER_FRAMES
#define PLAY_NUM_USB_FRAME_LISTS_TO_QUEUE		2
// was 2
#define kMaxAttempts							3


// max size of the globally unique descriptor ID. See getGlobalUniqueID()
#define MAX_ID_SIZE 128

// size of FrameSizeQueue
#define FRAMESIZE_QUEUE_SIZE				    128



/*!
 @abstract  the state or our mInput/mOutput USB stream.
 @discussion To handle an USB stream the program joggles with a set of USB frames,
 callback handlers etc. These are grouped in this object.
 
 See also the USB Device interface guide.
 
 The functions used to read from and write to isochronous endpoints are ReadIsochPipeAsync and WriteIsochPipeAsync. Both functions include the following two parameters:
 
 * numFrames—The number of frames for which to transfer data
 
 * frameList—A pointer to an array of structures that describe the frames. specifies the list of transfers you want to occur.
 */
struct StreamInfo {
    /*! The point where the next raw USB byte can be written in bufferPtr. Always in [0, bufferSize> */
    UInt32		bufferOffset;
    
    /*! =numChannels * #bytes per sample for this stream =  frame size.
     this will be 6 for stereo 24 bit audio. */
    UInt32		multFactor;
    
    /*! The max number of bytes that we can receive in a single frame which is part of a USB frameList.
     Is set to (averageFrameSamples + 1) * mInput.multFactor. Eg 97*6=582 for 96kHz/24 stereo  */
    UInt32		maxFrameSize;
    
    /*! This is the length of the bufferDescriptors array and usbIsocFrames.
     // I think sizes of 4 and 8 are usual.*/
    UInt32		numUSBFrameLists;
    /*! The number of usb frames in our lists. Hard set to RECORD_NUM_USB_FRAMES_PER_LIST
     or PLAY_NUM_USB_FRAMES_PER_LIST (64 usually). USB calls back to us only after 'completion'
     (after all frames were read, or something went wrong), calling back to readHandler.
     */
    UInt32		numUSBFramesPerList;
    /*! = mInput.numUSBFramesPerList / kNumberOfFramesPerMillisecond = 8 usually.
     Used as increment for usbFrameToQueueAt*/
    UInt32		numUSBTimeFrames;
    /*!
     @abstract Number of frames we have in use for reading USB.
     @discussion Hard set to RECORD_NUM_USB_FRAME_LISTS_TO_QUEUE or PLAY_NUM_USB_FRAME_LISTS_TO_QUEUE. Typically 2 or 4.  */
    UInt32		numUSBFrameListsToQueue;
    
    /*!
     size of the bufferPtr array. See bufferMemoryDescriptor.
     @discussion
     numSamplesInBuffer * multFactor = # bytes in the buffer.
     where numSamplesInBuffer =PAGE_SIZE * (2 + (sampleRate.whole > 48000) + (sampleRate.whole > 96000))
     and PAGE_SIZE=4096 bytes.
     */
    UInt32		bufferSize;
    
    /*! The nummer of channels coming in. Typically 1 (mono) or 2 (stereo). */
    UInt32		numChannels;
    UInt32		frameOffset;
    UInt8		streamDirection;
    /*! The interface number associated with this stream. */
    UInt8		interfaceNumber;
    UInt8		alternateSettingID;
    
    /*! the framelist that we are expecting to complete from next.
     Basically runs from 0 to numUSBFrameListsToQueue-1 and then
     restarts at 0. Updated after readHandler handled the block. */
    volatile UInt32						currentFrameList;
    
    IOUSBInterface				  *streamInterface;
    IOAudioStream				  *audioStream;
    /*! IOUSBPipe used for isochronous reading input streams from USB*/
    IOUSBPipe					  *pipe;
    IOUSBPipe					  *associatedPipe;
    
    /*! @discussion array of IOUSBLowLatencyIsocFrame containing USB status for a frame.
     size (for mInput) = numUSBFrameLists * numUSBFramesPerList */
    IOUSBLowLatencyIsocFrame	  *usbIsocFrames;
    
    /*! @abstract
     array IOUSBLowLatencyIsocCompletion[numUSBFrameLists]
     @discussion for each frameList there is this callback on completion.
     each one also contains a parameter for the callback.
     The parameter is:
     - for write buffer: number of bytes from the 0 wrap, 0 if this buffer didn't wrap
     - for read buffer; the frameListNum
     */
    IOUSBLowLatencyIsocCompletion *usbCompletion;
    
    // you want ddescriptors? we got descriptors!
    /*!  Big mem block to store all data from reading USB data according to the framelists.
     size mOutput.bufferSize or mInput.numUSBFrameLists * readUSBFrameListSize bytes */
    IOBufferMemoryDescriptor	*usbBufferDescriptor;
    
    /*! The memory descriptor for the The intermediate ring buffer of size bufferSize.*/
    IOBufferMemoryDescriptor	*bufferMemoryDescriptor;
    
    /*! array of pointers to IOMemoryDescriptor of length [frameListnum]. This is where raw USB data will come in.
     @discussion Contains copy of the received USB data.
     When a framelist is complete, readhHandler copies the data from the frame list
     to the mInput buffer so that the frameList can be redeployed.
     We need this (1) to have a fixed ring buffer as HAL is expecting us to have
     (2) to free up the framelist so that we can redeploy it to continue reading
     (3) so that we can do int-to-float conversion 'offline'.
     However it seems that these are no strong reasons, probably we could just simulate a ring buffer
     and redeploy the frameList only after conversion for the HAL.
     */
    IOSubMemoryDescriptor		**bufferDescriptors;
    
    /*! shortcut to bufferMemoryDescriptor actual buffer bytes. Really UInt8*. */
    void *						bufferPtr;
    
    /*! the expected USB MBus Frame number where we are looking for. Initially this is at frameOffset from the current frame number */
    UInt64						usbFrameToQueueAt;
    /*! an array of size [frameListnum] holding usbFrameToQueueAt for each frame when it was requested for read */
    UInt64 *					frameQueuedForList;
    
};


#endif
