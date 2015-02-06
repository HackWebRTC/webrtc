/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/utility/source/process_thread_impl.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {
namespace {
int64_t GetNextCallbackTime(Module* module, int64_t time_now) {
  int64_t interval = module->TimeUntilNextProcess();
  // Currently some implementations erroneously return error codes from
  // TimeUntilNextProcess(). So, as is, we correct that and log an error.
  if (interval < 0) {
    LOG(LS_ERROR) << "TimeUntilNextProcess returned an invalid value "
                  << interval;
    interval = 0;
  }
  return time_now + interval;
}
}

ProcessThread::~ProcessThread() {}

// static
rtc::scoped_ptr<ProcessThread> ProcessThread::Create() {
  return rtc::scoped_ptr<ProcessThread>(new ProcessThreadImpl()).Pass();
}

ProcessThreadImpl::ProcessThreadImpl()
    : wake_up_(EventWrapper::Create()), stop_(false) {
}

ProcessThreadImpl::~ProcessThreadImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!thread_.get());
  DCHECK(!stop_);
}

int32_t ProcessThreadImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (thread_.get())
    return -1;

  DCHECK(!stop_);

  thread_.reset(ThreadWrapper::CreateThread(
      &ProcessThreadImpl::Run, this, kNormalPriority, "ProcessThread"));
  unsigned int id;
  if (!thread_->Start(id)) {
    thread_.reset();
    return -1;
  }

  return 0;
}

int32_t ProcessThreadImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if(!thread_.get())
    return 0;

  {
    rtc::CritScope lock(&lock_);
    stop_ = true;
  }

  wake_up_->Set();

  thread_->Stop();
  thread_.reset();
  stop_ = false;

  return 0;
}

void ProcessThreadImpl::WakeUp(Module* module) {
  // Allowed to be called on any thread.
  {
    rtc::CritScope lock(&lock_);
    ModuleCallback cb(module);
    const auto& found = std::find(modules_.begin(), modules_.end(), cb);
    DCHECK(found != modules_.end()) << "programmer error?";
    (*found).next_callback = 0;
  }
  wake_up_->Set();
}

int32_t ProcessThreadImpl::RegisterModule(Module* module) {
  // Allowed to be called on any thread.
  {
    rtc::CritScope lock(&lock_);

    // Only allow module to be registered once.
    ModuleCallback cb(module);
    if (std::find(modules_.begin(), modules_.end(), cb) != modules_.end())
      return -1;
    modules_.push_front(cb);
  }

  // Wake the thread calling ProcessThreadImpl::Process() to update the
  // waiting time. The waiting time for the just registered module may be
  // shorter than all other registered modules.
  wake_up_->Set();

  return 0;
}

int32_t ProcessThreadImpl::DeRegisterModule(const Module* module) {
  // Allowed to be called on any thread.
  rtc::CritScope lock(&lock_);
  modules_.remove_if([&module](const ModuleCallback& m) {
      return m.module == module;
    });
  return 0;
}

// static
bool ProcessThreadImpl::Run(void* obj) {
  return static_cast<ProcessThreadImpl*>(obj)->Process();
}

bool ProcessThreadImpl::Process() {
  int64_t now = TickTime::MillisecondTimestamp();
  int64_t next_checkpoint = now + (1000 * 60);
  {
    rtc::CritScope lock(&lock_);
    if (stop_)
      return false;
    for (auto& m : modules_) {
      // TODO(tommi): Would be good to measure the time TimeUntilNextProcess
      // takes and dcheck if it takes too long (e.g. >=10ms).  Ideally this
      // operation should not require taking a lock, so querying all modules
      // should run in a matter of nanoseconds.
      if (m.next_callback == 0)
        m.next_callback = GetNextCallbackTime(m.module, now);

      if (m.next_callback <= now) {
        m.module->Process();
        // Use a new 'now' reference to calculate when the next callback
        // should occur.  We'll continue to use 'now' above for the baseline
        // of calculating how long we should wait, to reduce variance.
        auto new_now = TickTime::MillisecondTimestamp();
        m.next_callback = GetNextCallbackTime(m.module, new_now);
      }

      if (m.next_callback < next_checkpoint)
        next_checkpoint = m.next_callback;
    }
  }

  auto time_to_wait = next_checkpoint - TickTime::MillisecondTimestamp();
  if (time_to_wait > 0)
    wake_up_->Wait(static_cast<unsigned long>(time_to_wait));

  return true;
}
}  // namespace webrtc
