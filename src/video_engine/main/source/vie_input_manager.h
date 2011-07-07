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
 * vie_input_manager.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_INPUT_MANAGER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_INPUT_MANAGER_H_

#include "vie_defines.h"
#include "typedefs.h"
#include "map_wrapper.h"
#include "video_capture.h"
#include "vie_manager_base.h"
#include "vie_frame_provider_base.h"
#include "vie_capture.h"

class ViEExternalCapture;

namespace webrtc {
class CriticalSectionWrapper;
class ProcessThread;
class RWLockWrapper;
class ViECapturer;
class ViEFilePlayer;
class VoiceEngine;

class ViEInputManager: private ViEManagerBase
{
    friend class ViEInputManagerScoped;
public:
    ViEInputManager(int engineId);
    ~ViEInputManager();

    void SetModuleProcessThread(ProcessThread& moduleProcessThread);

    // Capture device information
    int NumberOfCaptureDevices();
    int GetDeviceName(WebRtc_UWord32 deviceNumber,
                      WebRtc_UWord8* deviceNameUTF8,
                      WebRtc_UWord32 deviceNameLength,
                      WebRtc_UWord8* deviceUniqueIdUTF8,
                      WebRtc_UWord32 deviceUniqueIdUTF8Length);
    int NumberOfCaptureCapabilities(const WebRtc_UWord8* deviceUniqueIdUTF8);
    int GetCaptureCapability(const WebRtc_UWord8* deviceUniqueIdUTF8,
                             const WebRtc_UWord32 deviceCapabilityNumber,
                             CaptureCapability& capability);
    int DisplayCaptureSettingsDialogBox(const WebRtc_UWord8* deviceUniqueIdUTF8,
                                        const WebRtc_UWord8* dialogTitleUTF8,
                                        void* parentWindow,
                                        WebRtc_UWord32 positionX,
                                        WebRtc_UWord32 positionY);
    int GetOrientation(const WebRtc_UWord8* deviceUniqueIdUTF8,
                       RotateCapturedFrame &orientation);

    // Create/delete Capture device settings
    // Return zero on success. A ViEError on failure.
    int CreateCaptureDevice(const WebRtc_UWord8* deviceUniqueIdUTF8,
                            const WebRtc_UWord32 deviceUniqueIdUTF8Length,
                            int& captureId);
    int CreateCaptureDevice(VideoCaptureModule& captureModule,
                            int& captureId);
    int CreateExternalCaptureDevice(ViEExternalCapture*& externalCapture,
                                    int& captureId);
    int DestroyCaptureDevice(int captureId);

    int CreateFilePlayer(const WebRtc_Word8* fileNameUTF8, const bool loop,
                         const webrtc::FileFormats fileFormat, VoiceEngine* vePtr,
                         int& fileId);
    int DestroyFilePlayer(int fileId);

private:
    bool GetFreeCaptureId(int& freecaptureId);
    void ReturnCaptureId(int captureId);
    bool GetFreeFileId(int& freeFileId);
    void ReturnFileId(int fileId);

    ViEFrameProviderBase* ViEFrameProvider(const ViEFrameCallback*
                                                       captureObserver) const;
    ViEFrameProviderBase* ViEFrameProvider(int providerId) const;

    ViECapturer* ViECapturePtr(int captureId) const;
    void GetViECaptures(MapWrapper& vieCaptureMap);

    ViEFilePlayer* ViEFilePlayerPtr(int fileId) const;

    // Members
    int _engineId;
    CriticalSectionWrapper& _mapCritsect;
    MapWrapper _vieFrameProviderMap;

    // Capture devices
    VideoCaptureModule::DeviceInfo* _ptrCaptureDeviceInfo;
    int _freeCaptureDeviceId[kViEMaxCaptureDevices];
    //File Players
    int _freeFileId[kViEMaxFilePlayers];
    //uses
    ProcessThread* _moduleProcessThread;
};

class ViEInputManagerScoped: private ViEManagerScopedBase
{
public:
    ViEInputManagerScoped(const ViEInputManager& vieInputManager);

    ViECapturer* Capture(int captureId) const;
    ViEFilePlayer* FilePlayer(int fileId) const;
    ViEFrameProviderBase* FrameProvider(int providerId) const;
    ViEFrameProviderBase* FrameProvider(const ViEFrameCallback*
                                                        captureObserver) const;
};
} // namespace webrtc
#endif    // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_INPUT_MANAGER_H_
