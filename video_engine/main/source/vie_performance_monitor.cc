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
 * vie_performance_monitor.cc
 */

#include "vie_performance_monitor.h"

#include "cpu_wrapper.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "tick_util.h"
#include "trace.h"
#include "vie_base.h"
#include "vie_defines.h"

namespace webrtc {

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEPerformanceMonitor::ViEPerformanceMonitor(int engineId)
    :   _engineId(engineId),
        _pointerCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
        _ptrViEMonitorThread(NULL),
        _monitorkEvent(*EventWrapper::Create()),
        _averageApplicationCPU(kViECpuStartValue),
        _averageSystemCPU(kViECpuStartValue), _cpu(NULL), _vieBaseObserver(NULL)
{
    _cpu = CpuWrapper::CreateCpu();
    if (_cpu)
    {
        _cpu->CpuUsage(); // to initialize
    } else
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not create CpuWrapper", __FUNCTION__);
    }
}

ViEPerformanceMonitor::~ViEPerformanceMonitor()
{
    Terminate();
    delete &_pointerCritsect;
    delete &_monitorkEvent;
    if (_cpu)
    {
        delete _cpu;
        _cpu = NULL;
    }
}

int ViEPerformanceMonitor::Init()
{
    if (_cpu == NULL)
    {
        // Performance monitoring not supported
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Not supported", __FUNCTION__);
        return 0;
    }

    CriticalSectionScoped cs(_pointerCritsect);
    if (_ptrViEMonitorThread == NULL)
    {
        _monitorkEvent.StartTimer(true, kViEMonitorPeriodMs);
        _ptrViEMonitorThread
            = ThreadWrapper::CreateThread(ViEMonitorThreadFunction, this,
                                       kNormalPriority,
                                       "ViEPerformanceMonitor");
        unsigned tId = 0;
        if (_ptrViEMonitorThread->Start(tId))
        {
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s: Performance monitor thread started %u",
                       __FUNCTION__, tId);
        } else
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s: Could not start performance monitor", __FUNCTION__);
            _monitorkEvent.StopTimer();
            return -1;
        }
    }
    return 0;
}

int ViEPerformanceMonitor::Terminate()
{
    {
        _pointerCritsect.Enter();
        _vieBaseObserver = NULL;
        _pointerCritsect.Leave();

        _monitorkEvent.StopTimer();
        if (_ptrViEMonitorThread)
        {
            ThreadWrapper* tmpThread = _ptrViEMonitorThread;
            _ptrViEMonitorThread = NULL;
            _monitorkEvent.Set();
            if (tmpThread->Stop())
            {
                delete tmpThread;
                tmpThread = NULL;
            }
        }
    }
    return 0;
}

int ViEPerformanceMonitor::RegisterViEBaseObserver(
    ViEBaseObserver* vieBaseObserver)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId), "%s",
               __FUNCTION__);
    CriticalSectionScoped cs(_pointerCritsect);
    if (vieBaseObserver)
    {
        if (_vieBaseObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s: Observer already started", __FUNCTION__);
            return -1;
        }
        _vieBaseObserver = vieBaseObserver;
    } else
    {
        _vieBaseObserver = NULL;
    }
    return 0;
}
bool ViEPerformanceMonitor::ViEBaseObserverRegistered()
{
    CriticalSectionScoped cs(_pointerCritsect);
    return _vieBaseObserver != NULL;
}

int ViEPerformanceMonitor::GetAverageApplicationCPU(int& applicationCPU)
{
    // Not supported
    return -1;
}

int ViEPerformanceMonitor::GetAverageSystemCPU(int& systemCPU)
{
    if (_cpu)
    {
        return _cpu->CpuUsage();
    }
    return -1;
}

bool ViEPerformanceMonitor::ViEMonitorThreadFunction(void* obj)
{
    return static_cast<ViEPerformanceMonitor*> (obj)->ViEMonitorProcess();
}

bool ViEPerformanceMonitor::ViEMonitorProcess()
{
    // Periodically triggered with time KViEMonitorPeriodMs
    _monitorkEvent.Wait(kViEMonitorPeriodMs);
    {
        if (_ptrViEMonitorThread == NULL)
        {
            // Thread removed, exit
            return false;
        }
        if (_cpu)
        {
            int cpuLoad = _cpu->CpuUsage();
            if (cpuLoad > 75)
            {
                _pointerCritsect.Enter();
                if (_vieBaseObserver)
                {
                    _vieBaseObserver->PerformanceAlarm(cpuLoad);
                }
                _pointerCritsect.Leave();
            }
        }
    }
    return true;
}
} //namespace webrtc
