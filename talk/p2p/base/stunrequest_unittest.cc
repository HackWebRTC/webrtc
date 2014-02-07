/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/ssladapter.h"
#include "talk/base/timeutils.h"
#include "talk/p2p/base/stunrequest.h"

using namespace cricket;

class StunRequestTest : public testing::Test,
                        public sigslot::has_slots<> {
 public:
  static void SetUpTestCase() {
    talk_base::InitializeSSL();
  }

  static void TearDownTestCase() {
    talk_base::CleanupSSL();
  }

  StunRequestTest()
      : manager_(talk_base::Thread::Current()),
        request_count_(0), response_(NULL),
        success_(false), failure_(false), timeout_(false) {
    manager_.SignalSendPacket.connect(this, &StunRequestTest::OnSendPacket);
  }

  void OnSendPacket(const void* data, size_t size, StunRequest* req) {
    request_count_++;
  }

  void OnResponse(StunMessage* res) {
    response_ = res;
    success_ = true;
  }
  void OnErrorResponse(StunMessage* res) {
    response_ = res;
    failure_ = true;
  }
  void OnTimeout() {
    timeout_ = true;
  }

 protected:
  static StunMessage* CreateStunMessage(StunMessageType type,
                                        StunMessage* req) {
    StunMessage* msg = new StunMessage();
    msg->SetType(type);
    if (req) {
      msg->SetTransactionID(req->transaction_id());
    }
    return msg;
  }
  static int TotalDelay(int sends) {
    int total = 0;
    for (int i = 0; i < sends; i++) {
      if (i < 4)
        total += 100 << i;
      else
        total += 1600;
    }
    return total;
  }

  StunRequestManager manager_;
  int request_count_;
  StunMessage* response_;
  bool success_;
  bool failure_;
  bool timeout_;
};

// Forwards results to the test class.
class StunRequestThunker : public StunRequest {
 public:
  StunRequestThunker(StunMessage* msg, StunRequestTest* test)
      : StunRequest(msg), test_(test) {}
  explicit StunRequestThunker(StunRequestTest* test) : test_(test) {}
 private:
  virtual void OnResponse(StunMessage* res) {
    test_->OnResponse(res);
  }
  virtual void OnErrorResponse(StunMessage* res) {
    test_->OnErrorResponse(res);
  }
  virtual void OnTimeout() {
    test_->OnTimeout();
  }

  virtual void Prepare(StunMessage* request) {
    request->SetType(STUN_BINDING_REQUEST);
  }

  StunRequestTest* test_;
};

// Test handling of a normal binding response.
TEST_F(StunRequestTest, TestSuccess) {
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);

  manager_.Send(new StunRequestThunker(req, this));
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, req);
  EXPECT_TRUE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == res);
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}

// Test handling of an error binding response.
TEST_F(StunRequestTest, TestError) {
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);

  manager_.Send(new StunRequestThunker(req, this));
  StunMessage* res = CreateStunMessage(STUN_BINDING_ERROR_RESPONSE, req);
  EXPECT_TRUE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == res);
  EXPECT_FALSE(success_);
  EXPECT_TRUE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}

// Test handling of a binding response with the wrong transaction id.
TEST_F(StunRequestTest, TestUnexpected) {
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);

  manager_.Send(new StunRequestThunker(req, this));
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, NULL);
  EXPECT_FALSE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == NULL);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}

// Test that requests are sent at the right times, and that the 9th request
// (sent at 7900 ms) can be properly replied to.
TEST_F(StunRequestTest, TestBackoff) {
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);

  uint32 start = talk_base::Time();
  manager_.Send(new StunRequestThunker(req, this));
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, req);
  for (int i = 0; i < 9; ++i) {
    while (request_count_ == i)
      talk_base::Thread::Current()->ProcessMessages(1);
    int32 elapsed = talk_base::TimeSince(start);
    LOG(LS_INFO) << "STUN request #" << (i + 1)
                 << " sent at " << elapsed << " ms";
    EXPECT_GE(TotalDelay(i + 1), elapsed);
  }
  EXPECT_TRUE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == res);
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}

// Test that we timeout properly if no response is received in 9500 ms.
TEST_F(StunRequestTest, TestTimeout) {
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, req);

  manager_.Send(new StunRequestThunker(req, this));
  talk_base::Thread::Current()->ProcessMessages(10000);  // > STUN timeout
  EXPECT_FALSE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == NULL);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_TRUE(timeout_);
  delete res;
}

// Regression test for specific crash where we receive a response with the
// same id as a request that doesn't have an underlying StunMessage yet.
TEST_F(StunRequestTest, TestNoEmptyRequest) {
  StunRequestThunker* request = new StunRequestThunker(this);

  manager_.SendDelayed(request, 100);

  StunMessage dummy_req;
  dummy_req.SetTransactionID(request->id());
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, &dummy_req);

  EXPECT_TRUE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == res);
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}
