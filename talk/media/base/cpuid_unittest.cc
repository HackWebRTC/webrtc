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

#include <iostream>

#include "talk/base/basictypes.h"
#include "talk/base/gunit.h"
#include "talk/base/systeminfo.h"

TEST(CpuInfoTest, CpuId) {
  LOG(LS_INFO) << "ARM: "
      << cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasARM);
  LOG(LS_INFO) << "NEON: "
      << cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasNEON);
  LOG(LS_INFO) << "X86: "
      << cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasX86);
  LOG(LS_INFO) << "SSE2: "
      << cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasSSE2);
  LOG(LS_INFO) << "SSSE3: "
      << cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasSSSE3);
  LOG(LS_INFO) << "SSE41: "
      << cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasSSE41);
  LOG(LS_INFO) << "SSE42: "
      << cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasSSE42);
  LOG(LS_INFO) << "AVX: "
      << cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasAVX);
  bool has_arm = cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasARM);
  bool has_x86 = cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasX86);
  EXPECT_FALSE(has_arm && has_x86);
}

TEST(CpuInfoTest, IsCoreIOrBetter) {
  bool core_i_or_better = cricket::IsCoreIOrBetter();
  // Tests the function is callable.  Run on known hardware to confirm.
  LOG(LS_INFO) << "IsCoreIOrBetter: " << core_i_or_better;

  // All Core I CPUs have SSE 4.1.
  if (core_i_or_better) {
    EXPECT_TRUE(cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasSSE41));
    EXPECT_TRUE(cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasSSSE3));
  }

  // All CPUs that lack SSE 4.1 are not Core I CPUs.
  if (!cricket::CpuInfo::TestCpuFlag(cricket::CpuInfo::kCpuHasSSE41)) {
    EXPECT_FALSE(core_i_or_better);
  }
}

