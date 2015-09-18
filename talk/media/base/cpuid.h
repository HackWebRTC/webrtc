/*
 * libjingle
 * Copyright 2011 Google Inc.
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

#ifndef TALK_MEDIA_BASE_CPUID_H_
#define TALK_MEDIA_BASE_CPUID_H_

#include "webrtc/base/constructormagic.h"

namespace cricket {

class CpuInfo {
 public:
  // The following flags must match libyuv/cpu_id.h values.
  // Internal flag to indicate cpuid requires initialization.
  static const int kCpuInit = 0x1;

  // These flags are only valid on ARM processors.
  static const int kCpuHasARM = 0x2;
  static const int kCpuHasNEON = 0x4;
  // 0x8 reserved for future ARM flag.

  // These flags are only valid on x86 processors.
  static const int kCpuHasX86 = 0x10;
  static const int kCpuHasSSE2 = 0x20;
  static const int kCpuHasSSSE3 = 0x40;
  static const int kCpuHasSSE41 = 0x80;
  static const int kCpuHasSSE42 = 0x100;
  static const int kCpuHasAVX = 0x200;
  static const int kCpuHasAVX2 = 0x400;
  static const int kCpuHasERMS = 0x800;

  // These flags are only valid on MIPS processors.
  static const int kCpuHasMIPS = 0x1000;
  static const int kCpuHasMIPS_DSP = 0x2000;
  static const int kCpuHasMIPS_DSPR2 = 0x4000;

  // Detect CPU has SSE2 etc.
  static bool TestCpuFlag(int flag);

  // For testing, allow CPU flags to be disabled.
  static void MaskCpuFlagsForTest(int enable_flags);

 private:
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(CpuInfo);
};

// Detect an Intel Core I5 or better such as 4th generation Macbook Air.
bool IsCoreIOrBetter();

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_CPUID_H_
