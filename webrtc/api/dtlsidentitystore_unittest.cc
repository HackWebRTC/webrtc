/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/dtlsidentitystore.h"

#include "webrtc/api/webrtcsessiondescriptionfactory.h"
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

  store_->RequestIdentity(rtc::KeyParams(rtc::KT_RSA),
                          rtc::Optional<uint64_t>(),
                          observer_.get());
  EXPECT_TRUE_WAIT(observer_->LastRequestSucceeded(), kTimeoutMs);

  EXPECT_TRUE_WAIT(store_->HasFreeIdentityForTesting(rtc::KT_RSA), kTimeoutMs);

  observer_->Reset();

  // Verifies that the callback is async when a free identity is ready.
  store_->RequestIdentity(rtc::KeyParams(rtc::KT_RSA),
                          rtc::Optional<uint64_t>(),
                          observer_.get());
  EXPECT_FALSE(observer_->call_back_called());
  EXPECT_TRUE_WAIT(observer_->LastRequestSucceeded(), kTimeoutMs);
}

TEST_F(DtlsIdentityStoreTest, RequestIdentitySuccessECDSA) {
  // Since store currently does not preemptively generate free ECDSA identities
  // we do not invoke HasFreeIdentityForTesting between requests.

  store_->RequestIdentity(rtc::KeyParams(rtc::KT_ECDSA),
                          rtc::Optional<uint64_t>(),
                          observer_.get());
  EXPECT_TRUE_WAIT(observer_->LastRequestSucceeded(), kTimeoutMs);

  observer_->Reset();

  // Verifies that the callback is async when a free identity is ready.
  store_->RequestIdentity(rtc::KeyParams(rtc::KT_ECDSA),
                          rtc::Optional<uint64_t>(),
                          observer_.get());
  EXPECT_FALSE(observer_->call_back_called());
  EXPECT_TRUE_WAIT(observer_->LastRequestSucceeded(), kTimeoutMs);
}

TEST_F(DtlsIdentityStoreTest, DeleteStoreEarlyNoCrashRSA) {
  EXPECT_FALSE(store_->HasFreeIdentityForTesting(rtc::KT_RSA));

  store_->RequestIdentity(rtc::KeyParams(rtc::KT_RSA),
                          rtc::Optional<uint64_t>(),
                          observer_.get());
  store_.reset();

  worker_thread_->Stop();
  EXPECT_FALSE(observer_->call_back_called());
}

TEST_F(DtlsIdentityStoreTest, DeleteStoreEarlyNoCrashECDSA) {
  EXPECT_FALSE(store_->HasFreeIdentityForTesting(rtc::KT_ECDSA));

  store_->RequestIdentity(rtc::KeyParams(rtc::KT_ECDSA),
                          rtc::Optional<uint64_t>(),
                          observer_.get());
  store_.reset();

  worker_thread_->Stop();
  EXPECT_FALSE(observer_->call_back_called());
}

