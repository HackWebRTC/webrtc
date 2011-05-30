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
 *  video_capture_mac.cc
 *
 */

// self header
#include "video_capture_mac.h"

// super class stuff
#include "../video_capture_impl.h"
#include "../device_info_impl.h"
#include "../video_capture_config.h"

#include "trace.h"

#include <QuickTime/QuickTime.h>

// 10.4 support must be decided runtime. We will just decide which framework to
// use at compile time "work" classes. One for QTKit, one for QuickTime
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
#include "QuickTime/video_capture_quick_time.h"
#include "QuickTime/video_capture_quick_time_info.h"
#else
#include "QTKit/video_capture_qtkit.h"
#include "QTKit/video_capture_qtkit_info.h"
#endif

namespace webrtc
{
namespace videocapturemodule
{

// static
bool VideoCaptureMac::CheckOSVersion()
{
    // Check OSX version
    OSErr err = noErr;

    SInt32 version;

    err = Gestalt(gestaltSystemVersion, &version);
    if (err != noErr)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Could not get OS version");
        return false;
    }

    if (version < 0x00001040) // Older version than Mac OSX 10.4
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "OS version too old: 0x%x", version);
        return false;
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, 0,
                 "OS version compatible: 0x%x", version);

    return true;
}

// static
bool VideoCaptureMac::CheckQTVersion()
{
    // Check OSX version
    OSErr err = noErr;

    SInt32 version;

    err = Gestalt(gestaltQuickTime, &version);
    if (err != noErr)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Could not get QuickTime version");
        return false;
    }

    if (version < 0x07000000) // QT v. 7.x or newer (QT 5.0.2 0x05020000)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "QuickTime version too old: 0x%x", version);
        return false;
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, 0,
                 "QuickTime version compatible: 0x%x", version);
    return true;
}
}  // videocapturemodule

/**************************************************************************
 *
 *    Create/Destroy a VideoCaptureModule
 *
 ***************************************************************************/

/*
 *   Returns version of the module and its components
 *
 *   version                 - buffer to which the version will be written
 *   remainingBufferInBytes  - remaining number of WebRtc_Word8 in the version
 *                             buffer
 *   position                - position of the next empty WebRtc_Word8 in the
 *                             version buffer
 */

VideoCaptureModule* VideoCaptureModule::Create(
    const WebRtc_Word32 id, const WebRtc_UWord8* deviceUniqueIdUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, id,
                 "Create %s", deviceUniqueIdUTF8);

    if (webrtc::videocapturemodule::VideoCaptureMac::CheckOSVersion == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "OS version is too old. Could not create video capture "
                     "module. Returning NULL");
        return NULL;
    }

#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    if (webrtc::videocapturemodule::VideoCaptureMac::CheckQTVersion == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "QuickTime version is too old. Could not create video "
                     "capture module. Returning NULL");
        return NULL;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "%s line %d. QTKit is not supported on this machine. Using "
                 "QuickTime framework to capture video",
                 __FILE__, __LINE__);

    webrtc::videocapturemodule::VideoCaptureMacQuickTime* newCaptureModule =
        new webrtc::videocapturemodule::VideoCaptureMacQuickTime(id);
    if (!newCaptureModule)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, id,
                     "could not Create for unique device %s, !newCaptureModule",
                     deviceUniqueIdUTF8);
        Destroy(newCaptureModule);
        newCaptureModule = NULL;
    }

    if (newCaptureModule->Init(id, deviceUniqueIdUTF8) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, id,
                     "could not Create for unique device %s, "
                     "newCaptureModule->Init()!=0",
                     deviceUniqueIdUTF8);
        Destroy(newCaptureModule);
        newCaptureModule = NULL;
    }

    // Successfully created VideoCaptureMacQuicktime. Return it
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "Module created for unique device %s. Will use QuickTime "
                 "framework to capture",
                 deviceUniqueIdUTF8);
    return newCaptureModule;

#else // QTKit version

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "Using QTKit framework to capture video", id);

    webrtc::videocapturemodule::VideoCaptureMacQTKit* newCaptureModule =
        new webrtc::videocapturemodule::VideoCaptureMacQTKit(id);
    if(!newCaptureModule)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, id,
                     "could not Create for unique device %s, !newCaptureModule",
                     deviceUniqueIdUTF8);
        Destroy(newCaptureModule);
        newCaptureModule = NULL;
    }
    if(newCaptureModule->Init(id, deviceUniqueIdUTF8) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, id,
                     "could not Create for unique device %s, "
                     "newCaptureModule->Init()!=0", deviceUniqueIdUTF8);
        Destroy(newCaptureModule);
        newCaptureModule = NULL;
    }

    // Successfully created VideoCaptureMacQuicktime. Return it
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "Module created for unique device %s, will use QTKit "
                 "framework",deviceUniqueIdUTF8);
    return newCaptureModule;
#endif
}

void Destroy(VideoCaptureModule* module)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                 "%s:%d Destroying GISPModuleVideoCapture", __FUNCTION__,
                 __LINE__);

#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    webrtc::videocapturemodule::VideoCaptureMacQuickTime* captureDevice =
        static_cast<VideoCaptureMacQuickTime*> (module);
    delete captureDevice;
    captureDevice = NULL;

#else // QTKit version
    webrtc::videocapturemodule::VideoCaptureMacQTKit* captureDevice =
        static_cast<webrtc::videocapturemodule::VideoCaptureMacQTKit*> (module);
    delete captureDevice;
    captureDevice = NULL;
#endif
}
/**************************************************************************
 *
 *    End Create/Destroy VideoCaptureModule
 *
 ***************************************************************************/

//  VideoCaptureMac class
namespace videocapturemodule
{

/**************************************************************************
 *
 *    These will just delegate to the appropriate class
 *
 ***************************************************************************/

VideoCaptureMac::VideoCaptureMac(const WebRtc_Word32 id) :
    VideoCaptureImpl(id), // super class constructor
        _isFrameworkSupported(false), _captureClass(NULL)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    _isFrameworkSupported = false;
    _captureClass = new VideoCaptureMacQuickTime(_id);

#else // QTKit version
    _isFrameworkSupported = true;
    _captureClass = new VideoCaptureMacQTKit(_id);
#endif
}

VideoCaptureMac::~VideoCaptureMac()
{
    delete _captureClass;
}

WebRtc_Word32 VideoCaptureMac::Init(const WebRtc_Word32 id,
                                    const WebRtc_UWord8* deviceUniqueIdUTF8)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTime*> (_captureClass)->Init(
        id, deviceUniqueIdUTF8);

#else // QTKit version
    return static_cast<VideoCaptureMacQTKit*>(_captureClass)->Init(id, deviceUniqueIdUTF8);
#endif
}

WebRtc_Word32 VideoCaptureMac::StartCapture(
    const VideoCaptureCapability& capability)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTime*> (_captureClass)->StartCapture(
        capability);

#else // QTKit version
    return static_cast<VideoCaptureMacQTKit*>
        (_captureClass)->StartCapture(capability);
#endif
}

WebRtc_Word32 VideoCaptureMac::StopCapture()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTime*>
        (_captureClass)->StopCapture();

#else // QTKit version
    return static_cast<VideoCaptureMacQTKit*>(_captureClass)->StopCapture();
#endif
}

bool VideoCaptureMac::CaptureStarted()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTime*>
        (_captureClass)->CaptureStarted();

#else // QTKit version
    return static_cast<VideoCaptureMacQTKit*>(_captureClass)->CaptureStarted();
#endif
}

WebRtc_Word32 VideoCaptureMac::CaptureSettings(VideoCaptureCapability& settings)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTime*>
        (_captureClass)->CaptureSettings(settings);

#else // QTKit version
    return static_cast<VideoCaptureMacQTKit*>
        (_captureClass)->CaptureSettings(settings);
#endif
}
} // namespace videocapturemodule


/**************************************************************************
 *
 *    Create/Destroy a DeviceInfo
 *
 ***************************************************************************/

VideoCaptureModule::DeviceInfo*
VideoCaptureModule::CreateDeviceInfo(const WebRtc_Word32 id)
{

    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, id,
                 "Create %d", id);

    if (webrtc::videocapturemodule::VideoCaptureMac::CheckOSVersion == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "OS version is too old. Could not create video capture "
                     "module. Returning NULL");
        return NULL;
    }

#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    if (webrtc::videocapturemodule::VideoCaptureMac::CheckQTVersion == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "QuickTime version is too old. Could not create video "
                     "capture module. Returning NULL");
        return NULL;
    }

    webrtc::videocapturemodule::VideoCaptureMacQuickTimeInfo* newCaptureInfoModule =
        new webrtc::videocapturemodule::VideoCaptureMacQuickTimeInfo(id);

    if (!newCaptureInfoModule || newCaptureInfoModule->Init() != 0)
    {
        Destroy(newCaptureInfoModule);
        newCaptureInfoModule = NULL;
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                     "Failed to Init newCaptureInfoModule created with id %d "
                     "and device \"\" ", id);
        return NULL;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "VideoCaptureModule created for id", id);
    return newCaptureInfoModule;

#else // QTKit version
    webrtc::videocapturemodule::VideoCaptureMacQTKitInfo* newCaptureInfoModule =
        new webrtc::videocapturemodule::VideoCaptureMacQTKitInfo(id);

    if(!newCaptureInfoModule || newCaptureInfoModule->Init() != 0)
    {
        //Destroy(newCaptureInfoModule);
        delete newCaptureInfoModule;
        newCaptureInfoModule = NULL;
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                     "Failed to Init newCaptureInfoModule created with id %d "
                     "and device \"\" ", id);
        return NULL;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "VideoCaptureModule created for id", id);
    return newCaptureInfoModule;

#endif

}

void VideoCaptureModule::DestroyDeviceInfo(DeviceInfo* deviceInfo)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0,
                 "%s:%d", __FUNCTION__, __LINE__);

#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    webrtc::videocapturemodule::VideoCaptureMacQuickTimeInfo* captureDeviceInfo =
        static_cast<webrtc::videocapturemodule::VideoCaptureMacQuickTimeInfo*> (deviceInfo);
    delete captureDeviceInfo;
    captureDeviceInfo = NULL;

#else // QTKit version
    webrtc::videocapturemodule::VideoCaptureMacQTKitInfo* captureDeviceInfo =
        static_cast<webrtc::videocapturemodule::VideoCaptureMacQTKitInfo*> (deviceInfo);
    delete captureDeviceInfo;
    captureDeviceInfo = NULL;
#endif

}

/**************************************************************************
 *
 *    End Create/Destroy VideoCaptureModule
 *
 ***************************************************************************/

// VideoCaptureMacInfo class

namespace videocapturemodule {

/**************************************************************************
 *
 *    These will just delegate to the appropriate class
 *
 ***************************************************************************/

VideoCaptureMacInfo::VideoCaptureMacInfo(const WebRtc_Word32 id) :
    DeviceInfoImpl(id), _isFrameworkSupported(false)//,
//_captureInfoClass(        NULL) // special init below
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    _isFrameworkSupported = false;
    _captureInfoClass = new VideoCaptureMacQuickTimeInfo(_id);

#else // QTKit version
    _isFrameworkSupported = true;
    _captureInfoClass = new VideoCaptureMacQTKitInfo(_id);
#endif

}

VideoCaptureMacInfo::~VideoCaptureMacInfo()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    delete _captureInfoClass;
#else // QTKit version
    delete _captureInfoClass;
#endif
}

WebRtc_Word32 VideoCaptureMacInfo::Init()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTimeInfo*>
        (_captureInfoClass)->Init();
#else // QTKit version
    return static_cast<VideoCaptureMacQTKitInfo*>(_captureInfoClass)->Init();
#endif
}

WebRtc_UWord32 VideoCaptureMacInfo::NumberOfDevices()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTimeInfo*>
        (_captureInfoClass)->NumberOfDevices();

#else // QTKit version
    return static_cast<VideoCaptureMacQTKitInfo*>
        (_captureInfoClass)->NumberOfDevices();
#endif
}

WebRtc_Word32 VideoCaptureMacInfo::GetDeviceName(
    WebRtc_UWord32 deviceNumber, WebRtc_UWord8* deviceNameUTF8,
    WebRtc_UWord32 deviceNameLength, WebRtc_UWord8* deviceUniqueIdUTF8,
    WebRtc_UWord32 deviceUniqueIdUTF8Length, WebRtc_UWord8* productUniqueIdUTF8,
    WebRtc_UWord32 productUniqueIdUTF8Length)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTimeInfo*>
    (_captureInfoClass)->GetDeviceName(deviceNumber, deviceNameUTF8,
                                       deviceNameLength, deviceUniqueIdUTF8,
                                       deviceUniqueIdUTF8Length,
                                       productUniqueIdUTF8,
                                       productUniqueIdUTF8Length);

#else // QTKit version
    return static_cast<VideoCaptureMacQTKitInfo*>
        (_captureInfoClass)->GetDeviceName(deviceNumber, deviceNameUTF8,
                                           deviceNameLength, deviceUniqueIdUTF8,
                                           deviceUniqueIdUTF8Length,
                                           productUniqueIdUTF8,
                                           productUniqueIdUTF8Length);
#endif
}

WebRtc_Word32 VideoCaptureMacInfo::NumberOfCapabilities(
    const WebRtc_UWord8* deviceUniqueIdUTF8)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTimeInfo*>
    (_captureInfoClass)->NumberOfCapabilities(deviceUniqueIdUTF8);

#else // QTKit version
    return static_cast<VideoCaptureMacQTKitInfo*>
        (_captureInfoClass)->NumberOfCapabilities(deviceUniqueIdUTF8);
#endif
}

WebRtc_Word32 VideoCaptureMacInfo::GetCapability(
    const WebRtc_UWord8* deviceUniqueIdUTF8,
    const WebRtc_UWord32 deviceCapabilityNumber,
    VideoCaptureCapability& capability)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTimeInfo*>
    (_captureInfoClass)->GetCapability(deviceUniqueIdUTF8,
                                       deviceCapabilityNumber, capability);

#else // QTKit version
    return static_cast<VideoCaptureMacQTKitInfo*>
        (_captureInfoClass)->GetCapability(deviceUniqueIdUTF8,
                                           deviceCapabilityNumber, capability);
#endif
}

WebRtc_Word32 VideoCaptureMacInfo::GetBestMatchedCapability(
    const WebRtc_UWord8*deviceUniqueIdUTF8,
    const VideoCaptureCapability requested, VideoCaptureCapability& resulting)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTimeInfo*>
        (_captureInfoClass)->GetBestMatchedCapability(deviceUniqueIdUTF8,
                                                      requested, resulting);

#else // QTKit version
    return static_cast<VideoCaptureMacQTKitInfo*>
        (_captureInfoClass)->GetBestMatchedCapability(deviceUniqueIdUTF8,
                                                      requested, resulting);
#endif
}

WebRtc_Word32 VideoCaptureMacInfo::DisplayCaptureSettingsDialogBox(
    const WebRtc_UWord8* deviceUniqueIdUTF8,
    const WebRtc_UWord8* dialogTitleUTF8, void* parentWindow,
    WebRtc_UWord32 positionX, WebRtc_UWord32 positionY)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    return static_cast<VideoCaptureMacQuickTimeInfo*>
        (_captureInfoClass)->DisplayCaptureSettingsDialogBox(deviceUniqueIdUTF8,
                                                             dialogTitleUTF8,
                                                             parentWindow,
                                                             positionX,
                                                             positionY);
#else // QTKit version
    return static_cast<VideoCaptureMacQTKitInfo*>
        (_captureInfoClass)->DisplayCaptureSettingsDialogBox(deviceUniqueIdUTF8,
                                                             dialogTitleUTF8,
                                                             parentWindow,
                                                             positionX,
                                                             positionY);
#endif
}

WebRtc_Word32 VideoCaptureMacInfo::CreateCapabilityMap(
    const WebRtc_UWord8* deviceUniqueIdUTF8)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
#else // QTKit version
#endif
    // not supported. The call stack should never make it this deep.
    // This call should be returned higher in the order
    return -1;

}
}  // namespace webrtc
}  // namespace videocapturemodule
