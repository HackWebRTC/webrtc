/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/interface/cpu_wrapper.h"

#include "gtest/gtest.h"
#include "system_wrappers/interface/trace.h"

using webrtc::CpuWrapper;
using webrtc::Trace;

// Only utilizes some of the cpu_wrapper.h code. Does not very anything except
// that it doesn't crash.
// TODO(kjellander): Improve this test so it verifies the implementation
// executes as expected.
TEST(CpuWrapperTest, Usage) {
  Trace::CreateTrace();
  Trace::SetTraceFile("cpu_wrapper_unittest.txt");
  Trace::SetLevelFilter(webrtc::kTraceAll);
  printf("Number of cores detected:%u\n", CpuWrapper::DetectNumberOfCores());
  CpuWrapper* cpu = CpuWrapper::CreateCpu();
  WebRtc_UWord32 numCores;
  WebRtc_UWord32* cores;
  for (int i = 0; i < 10; i++) {
    WebRtc_Word32 total = cpu->CpuUsageMultiCore(numCores, cores);

    printf("\nNumCores:%d\n", numCores);
    printf("Total cpu:%d\n", total);

    for (WebRtc_UWord32 i = 0; i < numCores; i++) {
      printf("Core:%u CPU:%u \n", i, cores[i]);
    }
  }
  delete cpu;
  Trace::ReturnTrace();
};
