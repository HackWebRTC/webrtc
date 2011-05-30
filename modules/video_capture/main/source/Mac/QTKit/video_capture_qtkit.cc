/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_capture_qtkit.h"
#include "video_capture_qtkit_objc.h"
#include "video_capture_qtkit_info_objc.h"
#include "trace.h"
#include "critical_section_wrapper.h"
#include "../../video_capture_config.h"

namespace webrtc
{

/*
 *   Returns version of the module and its components
 *
 *   version                 - buffer to which the version will be written
 *   remainingBufferInBytes  - remaining number of WebRtc_Word8 in the version
 *                             buffer
 *   position                - position of the next empty WebRtc_Word8 in the
 *                             version buffer
 */

WebRtc_Word32 VideoCaptureModule::GetVersion(
    WebRtc_Word8* version, WebRtc_UWord32& remainingBufferInBytes,
    WebRtc_UWord32& position)
{
    return webrtc::videocapturemodule::VideoCaptureMacQTKit::GetVersion(
        version, remainingBufferInBytes, position);
}

namespace videocapturemodule
{

VideoCaptureMacQTKit::VideoCaptureMacQTKit(const WebRtc_Word32 id) :
    VideoCaptureImpl(id),
    _id(id),
    _captureWidth(QTKIT_DEFAULT_WIDTH),
    _captureHeight(QTKIT_DEFAULT_HEIGHT),
    _captureFrameRate(QTKIT_DEFAULT_FRAME_RATE),
    _isCapturing(false),
    _frameCount(0)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, id,
                 "VideoCaptureMacQTKit::VideoCaptureMacQTKit() called");

    memset(_currentDeviceNameUTF8, 0, MAX_NAME_LENGTH);
    memset(_currentDeviceUniqueIdUTF8, 0, MAX_NAME_LENGTH);
    memset(_currentDeviceProductUniqueIDUTF8, 0, MAX_NAME_LENGTH);
}

VideoCaptureMacQTKit::~VideoCaptureMacQTKit()
{

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "~VideoCaptureMacQTKit() called");
    if(_captureDevice)
    {
        [_captureDevice stopCapture];
        [_captureDevice release];
    }

    if(_captureInfo)
    {
        [_captureInfo release];
    }
}

WebRtc_Word32 VideoCaptureMacQTKit::Init(
    const WebRtc_Word32 id, const WebRtc_UWord8* iDeviceUniqueIdUTF8)
{
    CriticalSectionScoped cs(_apiCs);

    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, id,
                 "VideoCaptureMacQTKit::Init() called with id %d and unique "
                 "device %s", id, iDeviceUniqueIdUTF8);

    WebRtc_Word32 result=0;
    const WebRtc_Word32 nameLength =
        (WebRtc_Word32) strlen((char*)iDeviceUniqueIdUTF8);
    if(nameLength>kVideoCaptureUniqueNameLength)
        return -1;

    // Store the device name
    _deviceUniqueId = new WebRtc_UWord8[nameLength+1];
    memcpy(_deviceUniqueId, iDeviceUniqueIdUTF8,nameLength+1);

    _captureDevice = [[VideoCaptureMacQTKitObjC alloc] init];
    if(NULL == _captureDevice)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "Failed to create an instance of "
                     "VideoCaptureMacQTKitObjC");
        return -1;
    }

    if(-1 == [[_captureDevice registerOwner:this]intValue])
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "Failed to register owner for _captureDevice");
        return -1;
    }

    if(0 == strcmp((char*)iDeviceUniqueIdUTF8, ""))
    {
        // the user doesn't want to set a capture device at this time
        return 0;
    }

    _captureInfo = [[VideoCaptureMacQTKitInfoObjC alloc]init];
    if(nil == _captureInfo)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id, "Failed to create an instance of VideoCaptureMacQTKitInfoObjC");
        return -1;
    }

    int captureDeviceCount = [[_captureInfo getCaptureDeviceCount]intValue];
    if(captureDeviceCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "No Capture Devices Present");
        return -1;
    }

    const int NAME_LENGTH = 1024;
    WebRtc_UWord8 deviceNameUTF8[1024] = "";
    WebRtc_UWord8 deviceUniqueIdUTF8[1024] = "";
    WebRtc_UWord8 deviceProductUniqueIDUTF8[1024] = "";

    bool captureDeviceFound = false;
    for(int index = 0; index < captureDeviceCount; index++){

        memset(deviceNameUTF8, 0, NAME_LENGTH);
        memset(deviceUniqueIdUTF8, 0, NAME_LENGTH);
        memset(deviceProductUniqueIDUTF8, 0, NAME_LENGTH);
        if(-1 == [[_captureInfo getDeviceNamesFromIndex:index
                   DefaultName:deviceNameUTF8 WithLength:NAME_LENGTH
                   AndUniqueID:deviceUniqueIdUTF8 WithLength:NAME_LENGTH
                   AndProductID:deviceProductUniqueIDUTF8
                   WithLength:NAME_LENGTH]intValue])
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                         "GetDeviceName returned -1 for index %d", index);
            return -1;
        }
        if(0 == strcmp((const char*)iDeviceUniqueIdUTF8,
                       (char*)deviceUniqueIdUTF8))
        {
            // we have a match
            captureDeviceFound = true;
            break;
        }
    }

    if(false == captureDeviceFound)
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                     "Failed to find capture device unique ID %s",
                     iDeviceUniqueIdUTF8);
        return -1;
    }

    // at this point we know that the user has passed in a valid camera. Let's
    // set it as the current.
    if(-1 == [[_captureDevice
               setCaptureDeviceByName:(char*)deviceNameUTF8]intValue])
    {
        strcpy((char*)_deviceUniqueId, (char*)deviceNameUTF8);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "Failed to set capture device %s (unique ID %s) even "
                     "though it was a valid return from "
                     "VideoCaptureMacQTKitInfo");
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "successfully Init VideoCaptureMacQTKit" );
    return 0;
}

WebRtc_Word32 VideoCaptureMacQTKit::StartCapture(
    const VideoCaptureCapability& capability)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, _id,
                 "StartCapture width %d, height %d, frameRate %d",
                 capability.width, capability.height, capability.maxFPS);

    _captureWidth = capability.width;
    _captureHeight = capability.height;
    _captureFrameRate = capability.maxFPS;

    if(-1 == [[_captureDevice setCaptureHeight:_captureHeight
               AndWidth:_captureWidth AndFrameRate:_captureFrameRate]intValue])
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                     "Could not set width=%d height=%d frameRate=%d",
                     _captureWidth, _captureHeight, _captureFrameRate);
        return -1;
    }

    if(-1 == [[_captureDevice startCapture]intValue])
    {
        return -1;
    }
    _isCapturing = true;
    return 0;
}

WebRtc_Word32 VideoCaptureMacQTKit::StopCapture()
{
    [_captureDevice stopCapture];

    _isCapturing = false;
    return 0;
}

bool VideoCaptureMacQTKit::CaptureStarted()
{
    return _isCapturing;
}

WebRtc_Word32 VideoCaptureMacQTKit::CaptureSettings(VideoCaptureCapability& settings)
{
    settings.width = _captureWidth;
    settings.height = _captureHeight;
    settings.maxFPS = _captureFrameRate;
    return 0;
}


// ********** begin functions inherited from DeviceInfoImpl **********

struct VideoCaptureCapabilityMacQTKit:public VideoCaptureCapability
{
    VideoCaptureCapabilityMacQTKit()
    {
    }
};
}  // namespace videocapturemodule
}  // namespace webrtc

