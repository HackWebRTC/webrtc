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

#include "talk/media/base/cpuid.h"

#if !defined(DISABLE_YUV)
#include "libyuv/cpu_id.h"
#endif

namespace cricket {

bool CpuInfo::TestCpuFlag(int flag) {
#if !defined(DISABLE_YUV)
  return libyuv::TestCpuFlag(flag) ? true : false;
#else
  return false;
#endif
}

void CpuInfo::MaskCpuFlagsForTest(int enable_flags) {
#if !defined(DISABLE_YUV)
  libyuv::MaskCpuFlags(enable_flags);
#endif
}

// Detect an Intel Core I5 or better such as 4th generation Macbook Air.
bool IsCoreIOrBetter() {
#if !defined(DISABLE_YUV) && (defined(__i386__) || defined(__x86_64__) || \
    defined(_M_IX86) || defined(_M_X64))
  int cpu_info[4];
  libyuv::CpuId(cpu_info, 0);  // Function 0: Vendor ID
  if (cpu_info[1] == 0x756e6547 && cpu_info[3] == 0x49656e69 &&
      cpu_info[2] == 0x6c65746e) {  // GenuineIntel
    // Detect CPU Family and Model
    // 3:0 - Stepping
    // 7:4 - Model
    // 11:8 - Family
    // 13:12 - Processor Type
    // 19:16 - Extended Model
    // 27:20 - Extended Family
    libyuv::CpuId(cpu_info, 1);  // Function 1: Family and Model
    int family = ((cpu_info[0] >> 8) & 0x0f) | ((cpu_info[0] >> 16) & 0xff0);
    int model = ((cpu_info[0] >> 4) & 0x0f) | ((cpu_info[0] >> 12) & 0xf0);
    // CpuFamily | CpuModel |  Name
    //         6 |       14 |  Yonah -- Core
    //         6 |       15 |  Merom -- Core 2
    //         6 |       23 |  Penryn -- Core 2 (most common)
    //         6 |       26 |  Nehalem -- Core i*
    //         6 |       28 |  Atom
    //         6 |       30 |  Lynnfield -- Core i*
    //         6 |       37 |  Westmere -- Core i*
    const int kAtom = 28;
    const int kCore2 = 23;
    if (family < 6 || family == 15 ||
        (family == 6 && (model == kAtom || model <= kCore2))) {
      return false;
    }
    return true;
  }
#endif
  return false;
}

}  // namespace cricket
