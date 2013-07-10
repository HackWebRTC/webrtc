/*
 * libjingle
 * Copyright 2010 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_BASE_CPUMONITOR_H_
#define TALK_BASE_CPUMONITOR_H_

#include "talk/base/basictypes.h"
#include "talk/base/messagehandler.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#if defined(LINUX) || defined(ANDROID)
#include "talk/base/stream.h"
#endif // defined(LINUX) || defined(ANDROID)

namespace talk_base {
class Thread;
class SystemInfo;

struct CpuStats {
  CpuStats()
      : prev_total_times_(0),
        prev_cpu_times_(0),
        prev_load_(0.f),
        prev_load_time_(0u) {
  }

  uint64 prev_total_times_;
  uint64 prev_cpu_times_;
  float prev_load_;  // Previous load value.
  uint32 prev_load_time_;  // Time previous load value was taken.
};

// CpuSampler samples the process and system load.
class CpuSampler {
 public:
  CpuSampler();
  ~CpuSampler();

  // Initialize CpuSampler.  Returns true if successful.
  bool Init();

  // Set minimum interval in ms between computing new load values.
  // Default 950 ms.  Set to 0 to disable interval.
  void set_load_interval(int min_load_interval);

  // Return CPU load of current process as a float from 0 to 1.
  float GetProcessLoad();

  // Return CPU load of current process as a float from 0 to 1.
  float GetSystemLoad();

  // Return number of cpus. Includes hyperthreads.
  int GetMaxCpus() const;

  // Return current number of cpus available to this process.
  int GetCurrentCpus();

  // For testing. Allows forcing of fallback to using NTDLL functions.
  void set_force_fallback(bool fallback) {
#ifdef WIN32
    force_fallback_ = fallback;
#endif
  }

 private:
  float UpdateCpuLoad(uint64 current_total_times,
                      uint64 current_cpu_times,
                      uint64 *prev_total_times,
                      uint64 *prev_cpu_times);
  CpuStats process_;
  CpuStats system_;
  int cpus_;
  int min_load_interval_;  // Minimum time between computing new load.
  scoped_ptr<SystemInfo> sysinfo_;
#ifdef WIN32
  void* get_system_times_;
  void* nt_query_system_information_;
  bool force_fallback_;
#endif
#if defined(LINUX) || defined(ANDROID)
  // File for reading /proc/stat
  scoped_ptr<FileStream> sfile_;
#endif // defined(LINUX) || defined(ANDROID)
};

// CpuMonitor samples and signals the CPU load periodically.
class CpuMonitor
    : public talk_base::MessageHandler, public sigslot::has_slots<> {
 public:
  explicit CpuMonitor(Thread* thread);
  virtual ~CpuMonitor();
  void set_thread(Thread* thread);

  bool Start(int period_ms);
  void Stop();
  // Signal parameters are current cpus, max cpus, process load and system load.
  sigslot::signal4<int, int, float, float> SignalUpdate;

 protected:
  // Override virtual method of parent MessageHandler.
  virtual void OnMessage(talk_base::Message* msg);
  // Clear the monitor thread and stop sending it messages if the thread goes
  // away before our lifetime.
  void OnMessageQueueDestroyed() { monitor_thread_ = NULL; }

 private:
  Thread* monitor_thread_;
  CpuSampler sampler_;
  int period_ms_;

  DISALLOW_COPY_AND_ASSIGN(CpuMonitor);
};

}  // namespace talk_base

#endif  // TALK_BASE_CPUMONITOR_H_
