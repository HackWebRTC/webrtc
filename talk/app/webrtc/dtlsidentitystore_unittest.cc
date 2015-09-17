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

using webrtc::DtlsIdentityStoreImpl;

static const int kTimeoutMs = 10000;

class MockDtlsIdentityRequestObserver :
    public webrtc::DtlsIdentityRequestObserver {
 public:
  MockDtlsIdentityRequestObserver()
      : call_back_called_(false), last_request_success_(false) {}
  void OnFailure(int error) override {
    EXPECT_FALSE(call_back_called_);
    call_back_called_ = true;
    last_request_success_ = false;
  }
  void OnSuccess(const std::string& der_cert,
                 const std::string& der_private_key) override {
    LOG(LS_WARNING) << "The string version of OnSuccess is called unexpectedly";
    EXPECT_TRUE(false);
  }
  void OnSuccess(rtc::scoped_ptr<rtc::SSLIdentity> identity) override {
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
      : worker_thread_(new rtc::Thread()),
        store_(new DtlsIdentityStoreImpl(rtc::Thread::Current(),
                                         worker_thread_.get())),
        observer_(
            new rtc::RefCountedObject<MockDtlsIdentityRequestObserver>()) {
    RTC_CHECK(worker_thread_->Start());
  }
  ~DtlsIdentityStoreTest() {}

  static void SetUpTestCase() {
    rtc::InitializeSSL();
  }
  static void TearDownTestCase() {
    rtc::CleanupSSL();
  }

  rtc::scoped_ptr<rtc::Thread> worker_thread_;
  rtc::scoped_ptr<DtlsIdentityStoreImpl> store_;
  rtc::scoped_refptr<MockDtlsIdentityRequestObserver> observer_;
};

TEST_F(DtlsIdentityStoreTest, RequestIdentitySuccessRSA) {
  EXPECT_TRUE_WAIT(store_->HasFreeIdentityForTesting(rtc::KT_RSA), kTimeoutMs);

  store_->RequestIdentity(rtc::KT_RSA, observer_.get());
  EXPECT_TRUE_WAIT(observer_->LastRequestSucceeded(), kTimeoutMs);

  EXPECT_TRUE_WAIT(store_->HasFreeIdentityForTesting(rtc::KT_RSA), kTimeoutMs);

  observer_->Reset();

  // Verifies that the callback is async when a free identity is ready.
  store_->RequestIdentity(rtc::KT_RSA, observer_.get());
  EXPECT_FALSE(observer_->call_back_called());
  EXPECT_TRUE_WAIT(observer_->LastRequestSucceeded(), kTimeoutMs);
}

TEST_F(DtlsIdentityStoreTest, RequestIdentitySuccessECDSA) {
  // Since store currently does not preemptively generate free ECDSA identities
  // we do not invoke HasFreeIdentityForTesting between requests.

  store_->RequestIdentity(rtc::KT_ECDSA, observer_.get());
  EXPECT_TRUE_WAIT(observer_->LastRequestSucceeded(), kTimeoutMs);

  observer_->Reset();

  // Verifies that the callback is async when a free identity is ready.
  store_->RequestIdentity(rtc::KT_ECDSA, observer_.get());
  EXPECT_FALSE(observer_->call_back_called());
  EXPECT_TRUE_WAIT(observer_->LastRequestSucceeded(), kTimeoutMs);
}

TEST_F(DtlsIdentityStoreTest, DeleteStoreEarlyNoCrashRSA) {
  EXPECT_FALSE(store_->HasFreeIdentityForTesting(rtc::KT_RSA));

  store_->RequestIdentity(rtc::KT_RSA, observer_.get());
  store_.reset();

  worker_thread_->Stop();
  EXPECT_FALSE(observer_->call_back_called());
}

TEST_F(DtlsIdentityStoreTest, DeleteStoreEarlyNoCrashECDSA) {
  EXPECT_FALSE(store_->HasFreeIdentityForTesting(rtc::KT_ECDSA));

  store_->RequestIdentity(rtc::KT_ECDSA, observer_.get());
  store_.reset();

  worker_thread_->Stop();
  EXPECT_FALSE(observer_->call_back_called());
}

