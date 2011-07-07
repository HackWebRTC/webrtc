/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  video_capture_mac.h
 *
 */

/**************************************************************************
 *
 *    This class exists so that the correct capturing framework can be called
 *    at runtime for compatiblity reasons.
 *
 *    * QTKit is the modern objective-c interface. Our capturing code was
 *    rewritten to use QTKit because QuickTime does not support 64-bit.
 *        Although QTKit exists in 10.4, it does not support all of the
 *        capture APIs needed
 *    * QuickTime is the older C++ interface. It supports
 *
 ***************************************************************************/

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_VIDEO_CAPTURE_MAC_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_VIDEO_CAPTURE_MAC_H_

#include "../video_capture_impl.h"
#include "../device_info_impl.h"
#include "../video_capture_config.h"

namespace webrtc
{
namespace videocapturemodule
{

class VideoCaptureMac: public VideoCaptureImpl
{
public:
    VideoCaptureMac(const WebRtc_Word32 id);
    virtual ~VideoCaptureMac();

    static VideoCaptureModule* Create(const WebRtc_Word32 id,
                                      const WebRtc_UWord8* deviceUniqueIdUTF8);
    static void Destroy(VideoCaptureModule* module);

    WebRtc_Word32 Init(const WebRtc_Word32 id,
                       const WebRtc_UWord8* deviceUniqueIdUTF8);
    virtual WebRtc_Word32 StartCapture(
            const VideoCaptureCapability& capability);
    virtual WebRtc_Word32 StopCapture();
    virtual bool CaptureStarted();
    virtual WebRtc_Word32 CaptureSettings(VideoCaptureCapability& settings);
    static bool CheckQTVersion();
    static bool CheckOSVersion();

private:

    bool _isFrameworkSupported;
    VideoCaptureImpl* _captureClass;
    WebRtc_Word32 _id;
};

class VideoCaptureMacInfo: public DeviceInfoImpl
{
public:
    // public methods

    static DeviceInfo* Create(const WebRtc_Word32 id);
    static void Destroy(DeviceInfo* deviceInfo);

    VideoCaptureMacInfo(const WebRtc_Word32 id);
    virtual ~VideoCaptureMacInfo();

    WebRtc_Word32 Init();

    virtual WebRtc_UWord32 NumberOfDevices();
    virtual WebRtc_Word32 GetDeviceName(
            WebRtc_UWord32 deviceNumber,
            WebRtc_UWord8* deviceNameUTF8,
            WebRtc_UWord32 deviceNameLength,
            WebRtc_UWord8* deviceUniqueIdUTF8,
            WebRtc_UWord32 deviceUniqueIdUTF8Length,
            WebRtc_UWord8* productUniqueIdUTF8 = 0,
            WebRtc_UWord32 productUniqueIdUTF8Length = 0);

    virtual WebRtc_Word32 NumberOfCapabilities(
            const WebRtc_UWord8* deviceUniqueIdUTF8);

    virtual WebRtc_Word32 GetCapability(
            const WebRtc_UWord8* deviceUniqueIdUTF8,
            const WebRtc_UWord32 deviceCapabilityNumber,
            VideoCaptureCapability& capability);

    virtual WebRtc_Word32 GetBestMatchedCapability(
            const WebRtc_UWord8*deviceUniqueIdUTF8,
            const VideoCaptureCapability requested,
            VideoCaptureCapability& resulting);

    virtual WebRtc_Word32 DisplayCaptureSettingsDialogBox(
            const WebRtc_UWord8* deviceUniqueIdUTF8,
            const WebRtc_UWord8* dialogTitleUTF8,
            void* parentWindow, WebRtc_UWord32 positionX,
            WebRtc_UWord32 positionY);

    virtual WebRtc_Word32 CreateCapabilityMap(
            const WebRtc_UWord8* deviceUniqueIdUTF8);

private:
    bool _isFrameworkSupported;
    DeviceInfoImpl* _captureInfoClass;
    WebRtc_Word32 _id;
};
}  // namespace videocapturemodule
}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_VIDEO_CAPTURE_MAC_H_
