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
 * vie_capture_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CAPTURE_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CAPTURE_IMPL_H_

#include "vie_defines.h"

#include "typedefs.h"
#include "vie_capture.h"
#include "vie_ref_count.h"
#include "vie_shared_data.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
//	ViECaptureImpl
// ----------------------------------------------------------------------------

class ViECaptureImpl: public virtual ViESharedData,
                      public ViECapture,
                      public ViERefCount
{
public:
    virtual int Release();

    // Available devices
    virtual int NumberOfCaptureDevices();

    virtual int GetCaptureDevice(unsigned int listNumber, char* deviceNameUTF8,
                                 const unsigned int deviceNameUTF8Length,
                                 char* uniqueIdUTF8,
                                 const unsigned int uniqueIdUTF8Length);

    // Allocate capture device
    virtual int AllocateCaptureDevice(const char* uniqueIdUTF8,
                                      const unsigned int uniqueIdUTF8Length,
                                      int& captureId);

    // Allocate capture device
    virtual int AllocateCaptureDevice(VideoCaptureModule& captureModule,
                                      int& captureId);
    // Allocate external capture device
    virtual int AllocateExternalCaptureDevice(
        int& captureId, ViEExternalCapture *&externalCapture);

    virtual int ReleaseCaptureDevice(const int captureId);

    // Pair capture device and channel
    virtual int ConnectCaptureDevice(const int captureId,
                                     const int videoChannel);

    virtual int DisconnectCaptureDevice(const int videoChannel);

    // Start/stop
    virtual int StartCapture(const int captureId,
                             const CaptureCapability captureCapability =
                                 CaptureCapability());

    virtual int StopCapture(const int captureId);

    virtual int SetRotateCapturedFrames(const int captureId,
                                        const RotateCapturedFrame rotation);

    virtual int SetCaptureDelay(const int captureId,
                                const unsigned int captureDelayMs);

    // Capture capabilities
    virtual int NumberOfCapabilities(const char* uniqueIdUTF8,
                                     const unsigned int uniqueIdUTF8Length);

    virtual int GetCaptureCapability(const char* uniqueIdUTF8,
                                     const unsigned int uniqueIdUTF8Length,
                                     const unsigned int capabilityNumber,
                                     CaptureCapability& capability);

    virtual int ShowCaptureSettingsDialogBox(
        const char* uniqueIdUTF8, const unsigned int uniqueIdUTF8Length,
        const char* dialogTitle, void* parentWindow = NULL,
        const unsigned int x = 200, const unsigned int y = 200);

    virtual int GetOrientation(const char* uniqueIdUTF8,
                               RotateCapturedFrame &orientation);

    // Callbacks
    virtual int EnableBrightnessAlarm(const int captureId, const bool enable);

    virtual int RegisterObserver(const int captureId,
                                 ViECaptureObserver& observer);

    virtual int DeregisterObserver(const int captureId);

protected:
    ViECaptureImpl();
    virtual ~ViECaptureImpl();
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CAPTURE_IMPL_H_
