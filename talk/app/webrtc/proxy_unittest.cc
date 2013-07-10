/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#include "talk/app/webrtc/proxy.h"

#include <string>

#include "talk/base/refcount.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/base/gunit.h"
#include "testing/base/public/gmock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace webrtc {

// Interface used for testing here.
class FakeInterface : public talk_base::RefCountInterface {
 public:
  virtual void VoidMethod0() = 0;
  virtual std::string Method0() = 0;
  virtual std::string ConstMethod0() const = 0;
  virtual std::string Method1(std::string s) = 0;
  virtual std::string ConstMethod1(std::string s) const = 0;
  virtual std::string Method2(std::string s1, std::string s2) = 0;

 protected:
  ~FakeInterface() {}
};

// Proxy for the test interface.
BEGIN_PROXY_MAP(Fake)
  PROXY_METHOD0(void, VoidMethod0)
  PROXY_METHOD0(std::string, Method0)
  PROXY_CONSTMETHOD0(std::string, ConstMethod0)
  PROXY_METHOD1(std::string, Method1, std::string)
  PROXY_CONSTMETHOD1(std::string, ConstMethod1, std::string)
  PROXY_METHOD2(std::string, Method2, std::string, std::string)
END_PROXY()

// Implementation of the test interface.
class Fake : public FakeInterface {
 public:
  static talk_base::scoped_refptr<Fake> Create() {
    return new talk_base::RefCountedObject<Fake>();
  }

  MOCK_METHOD0(VoidMethod0, void());
  MOCK_METHOD0(Method0, std::string());
  MOCK_CONST_METHOD0(ConstMethod0, std::string());

  MOCK_METHOD1(Method1, std::string(std::string));
  MOCK_CONST_METHOD1(ConstMethod1, std::string(std::string));

  MOCK_METHOD2(Method2, std::string(std::string, std::string));

 protected:
  Fake() {}
  ~Fake() {}
};

class ProxyTest: public testing::Test {
 public:
  // Checks that the functions is called on the |signaling_thread_|.
  void CheckThread() {
    EXPECT_EQ(talk_base::Thread::Current(), signaling_thread_.get());
  }

 protected:
  virtual void SetUp() {
    signaling_thread_.reset(new talk_base::Thread());
    ASSERT_TRUE(signaling_thread_->Start());
    fake_ = Fake::Create();
    fake_proxy_ = FakeProxy::Create(signaling_thread_.get(), fake_.get());
  }

 protected:
  talk_base::scoped_ptr<talk_base::Thread> signaling_thread_;
  talk_base::scoped_refptr<FakeInterface> fake_proxy_;
  talk_base::scoped_refptr<Fake> fake_;
};

TEST_F(ProxyTest, VoidMethod0) {
  EXPECT_CALL(*fake_, VoidMethod0())
            .Times(Exactly(1))
            .WillOnce(InvokeWithoutArgs(this, &ProxyTest::CheckThread));
  fake_proxy_->VoidMethod0();
}

TEST_F(ProxyTest, Method0) {
  EXPECT_CALL(*fake_, Method0())
            .Times(Exactly(1))
            .WillOnce(
                DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckThread),
                      Return("Method0")));
  EXPECT_EQ("Method0",
            fake_proxy_->Method0());
}

TEST_F(ProxyTest, ConstMethod0) {
  EXPECT_CALL(*fake_, ConstMethod0())
            .Times(Exactly(1))
            .WillOnce(
                DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckThread),
                      Return("ConstMethod0")));
  EXPECT_EQ("ConstMethod0",
            fake_proxy_->ConstMethod0());
}

TEST_F(ProxyTest, Method1) {
  const std::string arg1 = "arg1";
  EXPECT_CALL(*fake_, Method1(arg1))
            .Times(Exactly(1))
            .WillOnce(
                DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckThread),
                      Return("Method1")));
  EXPECT_EQ("Method1", fake_proxy_->Method1(arg1));
}

TEST_F(ProxyTest, ConstMethod1) {
  const std::string arg1 = "arg1";
  EXPECT_CALL(*fake_, ConstMethod1(arg1))
            .Times(Exactly(1))
            .WillOnce(
                DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckThread),
                      Return("ConstMethod1")));
  EXPECT_EQ("ConstMethod1", fake_proxy_->ConstMethod1(arg1));
}

TEST_F(ProxyTest, Method2) {
  const std::string arg1 = "arg1";
  const std::string arg2 = "arg2";
  EXPECT_CALL(*fake_, Method2(arg1, arg2))
            .Times(Exactly(1))
            .WillOnce(
                DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckThread),
                      Return("Method2")));
  EXPECT_EQ("Method2", fake_proxy_->Method2(arg1, arg2));
}

}  // namespace webrtc
