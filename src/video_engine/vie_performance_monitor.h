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
 * vie_performance_monitor.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_PERFORMANCE_MONITOR_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_PERFORMANCE_MONITOR_H_

// Defines
#include "vie_defines.h"
#include "typedefs.h"

namespace webrtc
{
class CriticalSectionWrapper;
class CpuWrapper;
class EventWrapper;
class ThreadWrapper;
class ViEBaseObserver;

class ViEPerformanceMonitor
{
public:
    ViEPerformanceMonitor(int engineId);
    ~ViEPerformanceMonitor();

    int Init(ViEBaseObserver* vieBaseObserver);
    void Terminate();
    bool ViEBaseObserverRegistered();

protected:
    static bool ViEMonitorThreadFunction(void* obj);
    bool ViEMonitorProcess();

private:
    enum { kViEMonitorPeriodMs = 975 };
    enum { kViECpuStartValue = 75 };

    const int _engineId;
    CriticalSectionWrapper& _pointerCritsect;
    ThreadWrapper* _ptrViEMonitorThread;
    EventWrapper& _monitorkEvent;
    int _averageApplicationCPU;
    int _averageSystemCPU;
    CpuWrapper* _cpu;
    ViEBaseObserver* _vieBaseObserver;
};
} // namespace webrtc
#endif    // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_PERFORMANCE_MONITOR_H_
