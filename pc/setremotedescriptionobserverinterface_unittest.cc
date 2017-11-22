/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/setremotedescriptionobserverinterface.h"

#include <string>

#include "api/jsep.h"
#include "api/optional.h"
#include "api/rtcerror.h"
#include "pc/test/mockpeerconnectionobservers.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"

// TODO(hbos): This is a test for api/setremotedescriptionobserverinterface.h
// and should be in api/ instead of pc/, but the dependency on
// pc/test/mockpeerconnectionobservers.h and rtc_base/thread.h is not allowed
// from api:rtc_api_unittests.

const int kDefaultTimeoutMs = 1000;

class SetRemoteDescriptionObserverWrapperTest : public testing::Test {
 public:
  SetRemoteDescriptionObserverWrapperTest()
      : set_desc_observer_(new rtc::RefCountedObject<
                           webrtc::MockSetSessionDescriptionObserver>()),
        observer_(new webrtc::SetRemoteDescriptionObserverAdapter(
            set_desc_observer_)) {}

 protected:
  rtc::scoped_refptr<webrtc::MockSetSessionDescriptionObserver>
      set_desc_observer_;
  rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverAdapter> observer_;
};

TEST_F(SetRemoteDescriptionObserverWrapperTest, OnCompleteWithSuccess) {
  observer_->OnSetRemoteDescriptionComplete(webrtc::RTCError::OK());
  EXPECT_TRUE_WAIT(set_desc_observer_->called(), kDefaultTimeoutMs);
  EXPECT_TRUE(set_desc_observer_->result());
}

TEST_F(SetRemoteDescriptionObserverWrapperTest, OnCompleteWithFailure) {
  observer_->OnSetRemoteDescriptionComplete(webrtc::RTCError(
      webrtc::RTCErrorType::INVALID_PARAMETER, "FailureMessage"));
  EXPECT_TRUE_WAIT(set_desc_observer_->called(), kDefaultTimeoutMs);
  EXPECT_FALSE(set_desc_observer_->result());
  EXPECT_EQ(set_desc_observer_->error(), "FailureMessage");
}

TEST_F(SetRemoteDescriptionObserverWrapperTest, IsAsynchronous) {
  observer_->OnSetRemoteDescriptionComplete(webrtc::RTCError::OK());
  // Untill this thread's messages are processed by EXPECT_TRUE_WAIT,
  // |set_desc_observer_| should not have been called.
  EXPECT_FALSE(set_desc_observer_->called());
  EXPECT_TRUE_WAIT(set_desc_observer_->called(), kDefaultTimeoutMs);
  EXPECT_TRUE(set_desc_observer_->result());
}

TEST_F(SetRemoteDescriptionObserverWrapperTest, SurvivesDereferencing) {
  observer_->OnSetRemoteDescriptionComplete(webrtc::RTCError::OK());
  // Even if there are no external references to |observer_| the operation
  // should complete.
  observer_ = nullptr;
  EXPECT_TRUE_WAIT(set_desc_observer_->called(), kDefaultTimeoutMs);
  EXPECT_TRUE(set_desc_observer_->result());
}
