/*
 * libjingle
 * Copyright 2008 Google Inc.
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

#ifndef TALK_BASE_SYSTEMINFO_H__
#define TALK_BASE_SYSTEMINFO_H__

#include <string>

#include "talk/base/basictypes.h"

namespace talk_base {

class SystemInfo {
 public:
  enum Architecture {
    SI_ARCH_UNKNOWN = -1,
    SI_ARCH_X86 = 0,
    SI_ARCH_X64 = 1,
    SI_ARCH_ARM = 2
  };

  SystemInfo();

  // The number of CPU Cores in the system.
  int GetMaxPhysicalCpus();
  // The number of CPU Threads in the system.
  int GetMaxCpus();
  // The number of CPU Threads currently available to this process.
  int GetCurCpus();
  // Identity of the CPUs.
  Architecture GetCpuArchitecture();
  std::string GetCpuVendor();
  int GetCpuFamily();
  int GetCpuModel();
  int GetCpuStepping();
  // Return size of CPU cache in bytes.  Uses largest available cache (L3).
  int GetCpuCacheSize();
  // Estimated speed of the CPUs, in MHz.  e.g. 2400 for 2.4 GHz
  int GetMaxCpuSpeed();
  int GetCurCpuSpeed();
  // Total amount of physical memory, in bytes.
  int64 GetMemorySize();
  // The model name of the machine, e.g. "MacBookAir1,1"
  std::string GetMachineModel();

  // The gpu identifier
  struct GpuInfo {
    GpuInfo() : vendor_id(0), device_id(0) {}
    std::string device_name;
    std::string description;
    int vendor_id;
    int device_id;
    std::string driver;
    std::string driver_version;
  };
  bool GetGpuInfo(GpuInfo *info);

 private:
  int physical_cpus_;
  int logical_cpus_;
  int cache_size_;
  Architecture cpu_arch_;
  std::string cpu_vendor_;
  int cpu_family_;
  int cpu_model_;
  int cpu_stepping_;
  int cpu_speed_;
  int64 memory_;
  std::string machine_model_;
};

}  // namespace talk_base

#endif  // TALK_BASE_SYSTEMINFO_H__
