/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/test_suite.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "test/testsupport/fileutils.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
namespace test {

const int kLevelFilter = kTraceError | kTraceWarning | kTraceTerseInfo;

class TraceCallbackImpl : public TraceCallback {
 public:
  TraceCallbackImpl() { }
  virtual ~TraceCallbackImpl() { }

  virtual void Print(TraceLevel level, const char* msg_array, int length) {
    if (level & kLevelFilter) {
      ASSERT_GT(length, Trace::kBoilerplateLength);
      std::string msg = msg_array;
      std::string msg_time = msg.substr(Trace::kTimestampPosition,
                                        Trace::kTimestampLength);
      std::string msg_log = msg.substr(Trace::kBoilerplateLength);
      fprintf(stderr, "%s %s\n", msg_time.c_str(), msg_log.c_str());
      fflush(stderr);
    }
  }
};

TestSuite::TestSuite(int argc, char** argv)
    : trace_callback_(new TraceCallbackImpl) {
  SetExecutablePath(argv[0]);
  testing::InitGoogleMock(&argc, argv);  // Runs InitGoogleTest() internally.
}

TestSuite::~TestSuite() {
}

int TestSuite::Run() {
  Initialize();
  int result = RUN_ALL_TESTS();
  Shutdown();
  return result;
}

void TestSuite::Initialize() {
  Trace::CreateTrace();
  Trace::SetTraceCallback(trace_callback_.get());
  Trace::SetLevelFilter(kLevelFilter);
}

void TestSuite::Shutdown() {
  Trace::SetTraceCallback(NULL);
  Trace::ReturnTrace();
}

}  // namespace test
}  // namespace webrtc
