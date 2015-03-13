/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#include "talk/app/webrtc/dtlsidentitystore.h"

#include "talk/app/webrtc/webrtcsessiondescriptionfactory.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/ssladapter.h"

using webrtc::DtlsIdentityStore;
using webrtc::WebRtcSessionDescriptionFactory;

static const int kTimeoutMs = 10000;

class MockDtlsIdentityRequestObserver :
    public webrtc::DTLSIdentityRequestObserver {
 public:
  MockDtlsIdentityRequestObserver()
      : call_back_called_(false), last_request_success_(false) {}
  void OnFailure(int error) override {
    EXPECT_FALSE(call_back_called_);
    call_back_called_ = true;
    last_request_success_ = false;
  }
  void OnSuccess(const std::string& der_cert,
                 const std::string& der_private_key) {
    LOG(LS_WARNING) << "The string version of OnSuccess is called unexpectedly";
    EXPECT_TRUE(false);
  }
  void OnSuccessWithIdentityObj(
      rtc::scoped_ptr<rtc::SSLIdentity> identity) override {
    EXPECT_FALSE(call_back_called_);
    call_back_called_ = true;
    last_request_success_ = true;
  }

  void Reset() {
    call_back_called_ = false;
    last_request_success_ = false;
  }

  bool LastRequestSucceeded() const {
    return call_back_called_ && last_request_success_;
  }

  bool call_back_called() const {
    return call_back_called_;
  }

 private:
  bool call_back_called_;
  bool last_request_success_;
};

class DtlsIdentityStoreTest : public testing::Test {
 protected:
  DtlsIdentityStoreTest()
      : store_(new DtlsIdentityStore(rtc::Thread::Current(),
                                     rtc::Thread::Current())),
        observer_(
            new rtc::RefCountedObject<MockDtlsIdentityRequestObserver>()) {
    store_->Initialize();
  }
  ~DtlsIdentityStoreTest() {}

  static void SetUpTestCase() {
    rtc::InitializeSSL();
  }
  static void TearDownTestCase() {
    rtc::CleanupSSL();
  }

  rtc::scoped_ptr<DtlsIdentityStore> store_;
  rtc::scoped_refptr<MockDtlsIdentityRequestObserver> observer_;
};

TEST_F(DtlsIdentityStoreTest, RequestIdentitySuccess) {
  EXPECT_TRUE_WAIT(store_->HasFreeIdentityForTesting(), kTimeoutMs);

  store_->RequestIdentity(observer_.get());
  EXPECT_TRUE_WAIT(observer_->LastRequestSucceeded(), kTimeoutMs);

  EXPECT_TRUE_WAIT(store_->HasFreeIdentityForTesting(), kTimeoutMs);
}
