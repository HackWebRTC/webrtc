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
#include "system_wrappers/interface/cpu_info.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "system_wrappers/interface/trace.h"

using webrtc::CpuInfo;
using webrtc::CpuWrapper;
using webrtc::Trace;

// Only utilizes some of the cpu_info.h and cpu_wrapper.h code. Does not verify
// anything except that it doesn't crash.
// TODO(kjellander): Improve this test so it verifies the implementation
// executes as expected.
TEST(CpuWrapperTest, Usage) {
  Trace::CreateTrace();
  Trace::SetTraceFile("cpu_wrapper_unittest.txt");
  Trace::SetLevelFilter(webrtc::kTraceAll);
  printf("Number of cores detected:%u\n", CpuInfo::DetectNumberOfCores());
  CpuWrapper* cpu = CpuWrapper::CreateCpu();
  ASSERT_TRUE(cpu != NULL);
  webrtc::EventWrapper* sleep_event = webrtc::EventWrapper::Create();
  ASSERT_TRUE(sleep_event != NULL);

  int num_iterations = 0;
  WebRtc_UWord32 num_cores = 0;
  WebRtc_UWord32* cores = NULL;
  bool cpu_usage_available = cpu->CpuUsageMultiCore(num_cores, cores) != -1;
  // Initializing the CPU measurements may take a couple of seconds on Windows.
  // Since the initialization is lazy we need to wait until it is completed.
  // Should not take more than 10000 ms.
  while (cpu_usage_available && (++num_iterations < 10000)) {
    if (cores != NULL) {
      ASSERT_GT(num_cores, 0u);
      break;
    }
    sleep_event->Wait(1);
    cpu_usage_available = cpu->CpuUsageMultiCore(num_cores, cores) != -1;
  }
  ASSERT_TRUE(cpu_usage_available);

  const WebRtc_Word32 total = cpu->CpuUsageMultiCore(num_cores, cores);
  ASSERT_TRUE(cores != NULL);
  EXPECT_GT(num_cores, 0u);
  EXPECT_GE(total, 0);

  printf("\nNumCores:%d\n", num_cores);
  printf("Total cpu:%d\n", total);
  for (WebRtc_UWord32 i = 0; i < num_cores; i++) {
    printf("Core:%u CPU:%u \n", i, cores[i]);
    EXPECT_LE(cores[i], static_cast<WebRtc_UWord32> (total));
  }

  delete cpu;
  Trace::ReturnTrace();
};
