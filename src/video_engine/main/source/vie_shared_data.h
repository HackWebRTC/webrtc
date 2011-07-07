/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// vie_shared_data.h

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_SHARED_DATA_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_SHARED_DATA_H_

#include "vie_defines.h"
#include "vie_performance_monitor.h"

namespace webrtc {
class CriticalSectionWrapper;
class ViERenderManager;
class ViEChannelManager;
class ViEInputManager;
class ProcessThread;

class ViESharedData
{
protected:
    ViESharedData();
    ~ViESharedData();

    bool IsInitialized() const;
    int SetInitialized();
    int SetUnInitialized();
    void SetLastError(const int error) const;
    int LastErrorInternal() const;
protected:
    int NumberOfCores() const;

    static int _instanceCounter;
    const int _instanceId;
    CriticalSectionWrapper& _apiCritsect;
    bool _isInitialized;
    const int _numberOfCores;

    ViEPerformanceMonitor _viePerformanceMonitor;
    ViEChannelManager& _channelManager;
    ViEInputManager& _inputManager;
    ViERenderManager& _renderManager;
    ProcessThread* _moduleProcessThreadPtr;
private:
    mutable int _lastError;
};
} // namespace webrtc

#endif // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_SHARED_DATA_H_
