/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ViESharedData.cpp

#include "vie_shared_data.h"
#include "vie_defines.h"

#include "cpu_wrapper.h"
#include "critical_section_wrapper.h"
#include "process_thread.h"
#include "trace.h"
#include "vie_channel_manager.h"
#include "vie_input_manager.h"
#include "vie_render_manager.h"

namespace webrtc {

// Active instance counter
int ViESharedData::_instanceCounter = 0;

ViESharedData::ViESharedData()
    : _instanceId(++_instanceCounter),
      _apiCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
      _isInitialized(false), _numberOfCores(CpuWrapper::DetectNumberOfCores()),
      _moduleProcessThreadPtr(ProcessThread::CreateProcessThread()),
      _viePerformanceMonitor(ViEPerformanceMonitor(_instanceId)),
      _channelManager(*new ViEChannelManager(_instanceId, _numberOfCores,
                                             _viePerformanceMonitor)),
      _inputManager(*new ViEInputManager(_instanceId)),
      _renderManager(*new ViERenderManager(_instanceId)), _lastError(0)
{
    Trace::CreateTrace();
    _channelManager.SetModuleProcessThread(*_moduleProcessThreadPtr);
    _inputManager.SetModuleProcessThread(*_moduleProcessThreadPtr);
    _moduleProcessThreadPtr->Start();
}

ViESharedData::~ViESharedData()
{
    delete &_inputManager;
    delete &_channelManager;
    delete &_renderManager;

    _moduleProcessThreadPtr->Stop();
    ProcessThread::DestroyProcessThread(_moduleProcessThreadPtr);
    delete &_apiCritsect;
    Trace::ReturnTrace();
}

bool ViESharedData::IsInitialized() const
{
    return _isInitialized;
}

int ViESharedData::SetInitialized()
{
    _isInitialized = true;
    return 0;
}

int ViESharedData::SetUnInitialized()
{
    _isInitialized = false;
    return 0;
}

void ViESharedData::SetLastError(const int error) const
{
    _lastError = error;
}

int ViESharedData::LastErrorInternal() const
{
    int error = _lastError;
    _lastError = 0;
    return error;
}

int ViESharedData::NumberOfCores() const
{
    return _numberOfCores;
}
} // namespace webrtc
