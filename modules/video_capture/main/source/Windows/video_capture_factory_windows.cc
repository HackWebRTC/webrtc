/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_capture_windows.h"
#include "trace.h"

namespace webrtc
{
VideoCaptureModule* VideoCaptureModule::Create(
                                       const WebRtc_Word32 id,
                                       const WebRtc_UWord8* deviceUniqueIdUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, id, "Create %s",
                 deviceUniqueIdUTF8);

    if (deviceUniqueIdUTF8 == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, id,
                     "Create Invalid deviceUniqueIdUTF8");
        return NULL;
    }

    WebRtc_UWord8 productId[kVideoCaptureProductIdLength];
    videocapturemodule::DeviceInfoWindows::GetProductId(deviceUniqueIdUTF8,
                                                        productId,
                                                        sizeof(productId));

    videocapturemodule::VideoCaptureDS* newCaptureModule = NULL;
    newCaptureModule = new videocapturemodule::VideoCaptureDS(id);

    if (!newCaptureModule
        || newCaptureModule->Init(id, deviceUniqueIdUTF8) != 0)
    {
        Destroy(newCaptureModule);
        newCaptureModule = NULL;
    }
    return newCaptureModule;
}
} //namespace webrtc
