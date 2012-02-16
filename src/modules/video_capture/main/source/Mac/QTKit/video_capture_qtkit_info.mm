/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "trace.h"
#include "../../video_capture_config.h"
#import "video_capture_qtkit_info_objc.h"

#include "video_capture.h"

namespace webrtc
{
namespace videocapturemodule
{

VideoCaptureMacQTKitInfo::VideoCaptureMacQTKitInfo(const WebRtc_Word32 id) :
    DeviceInfoImpl(id)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);
    _captureInfo = [[VideoCaptureMacQTKitInfoObjC alloc] init];
}

VideoCaptureMacQTKitInfo::~VideoCaptureMacQTKitInfo()
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);
    [_captureInfo release];

}

WebRtc_Word32 VideoCaptureMacQTKitInfo::Init()
{

    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);
    return 0;
}

WebRtc_UWord32 VideoCaptureMacQTKitInfo::NumberOfDevices()
{

    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);
    WebRtc_UWord32 captureDeviceCount =
        [[_captureInfo getCaptureDeviceCount]intValue];
    return captureDeviceCount;

}

WebRtc_Word32 VideoCaptureMacQTKitInfo::GetDeviceName(
    WebRtc_UWord32 deviceNumber, WebRtc_UWord8* deviceNameUTF8,
    WebRtc_UWord32 deviceNameLength, WebRtc_UWord8* deviceUniqueIdUTF8,
    WebRtc_UWord32 deviceUniqueIdUTF8Length, WebRtc_UWord8* productUniqueIdUTF8,
    WebRtc_UWord32 productUniqueIdUTF8Length)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);
    int errNum = [[_captureInfo getDeviceNamesFromIndex:deviceNumber
                   DefaultName:deviceNameUTF8 WithLength:deviceNameLength
                   AndUniqueID:deviceUniqueIdUTF8
                   WithLength:deviceUniqueIdUTF8Length
                   AndProductID:productUniqueIdUTF8
                   WithLength:productUniqueIdUTF8Length]intValue];
    return errNum;
}

WebRtc_Word32 VideoCaptureMacQTKitInfo::NumberOfCapabilities(
    const WebRtc_UWord8* deviceUniqueIdUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}


WebRtc_Word32 VideoCaptureMacQTKitInfo::GetCapability(
    const WebRtc_UWord8* deviceUniqueIdUTF8,
    const WebRtc_UWord32 deviceCapabilityNumber,
    VideoCaptureCapability& capability)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}


WebRtc_Word32 VideoCaptureMacQTKitInfo::GetBestMatchedCapability(
    const WebRtc_UWord8*deviceUniqueIdUTF8,
    const VideoCaptureCapability& requested, VideoCaptureCapability& resulting)
{
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}

WebRtc_Word32 VideoCaptureMacQTKitInfo::DisplayCaptureSettingsDialogBox(
    const WebRtc_UWord8* deviceUniqueIdUTF8,
    const WebRtc_UWord8* dialogTitleUTF8, void* parentWindow,
    WebRtc_UWord32 positionX, WebRtc_UWord32 positionY)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);

    return [[_captureInfo
             displayCaptureSettingsDialogBoxWithDevice:deviceUniqueIdUTF8
             AndTitle:dialogTitleUTF8
             AndParentWindow:parentWindow AtX:positionX AndY:positionY]
             intValue];
}

WebRtc_Word32 VideoCaptureMacQTKitInfo::CreateCapabilityMap(
    const WebRtc_UWord8* deviceUniqueIdUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
     return -1;
}
}  // namespace videocapturemodule
}  // namespace webrtc
