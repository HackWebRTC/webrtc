/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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
#include "talk/base/referencecountedsingletonfactory.h"

namespace talk_base {

class MyExistenceWatcher {
 public:
  MyExistenceWatcher() { create_called_ = true; }
  ~MyExistenceWatcher() { delete_called_ = true; }

  static bool create_called_;
  static bool delete_called_;
};

bool MyExistenceWatcher::create_called_ = false;
bool MyExistenceWatcher::delete_called_ = false;

class TestReferenceCountedSingletonFactory :
    public ReferenceCountedSingletonFactory<MyExistenceWatcher> {
 protected:
  virtual bool SetupInstance() {
    instance_.reset(new MyExistenceWatcher());
    return true;
  }

  virtual void CleanupInstance() {
    instance_.reset();
  }
};

static void DoCreateAndGoOutOfScope(
    ReferenceCountedSingletonFactory<MyExistenceWatcher> *factory) {
  rcsf_ptr<MyExistenceWatcher> ptr(factory);
  ptr.get();
  // and now ptr should go out of scope.
}

TEST(ReferenceCountedSingletonFactory, ZeroReferenceCountCausesDeletion) {
  TestReferenceCountedSingletonFactory factory;
  MyExistenceWatcher::delete_called_ = false;
  DoCreateAndGoOutOfScope(&factory);
  EXPECT_TRUE(MyExistenceWatcher::delete_called_);
}

TEST(ReferenceCountedSingletonFactory, NonZeroReferenceCountDoesNotDelete) {
  TestReferenceCountedSingletonFactory factory;
  rcsf_ptr<MyExistenceWatcher> ptr(&factory);
  ptr.get();
  MyExistenceWatcher::delete_called_ = false;
  DoCreateAndGoOutOfScope(&factory);
  EXPECT_FALSE(MyExistenceWatcher::delete_called_);
}

TEST(ReferenceCountedSingletonFactory, ReturnedPointersReferToSameThing) {
  TestReferenceCountedSingletonFactory factory;
  rcsf_ptr<MyExistenceWatcher> one(&factory), two(&factory);

  EXPECT_EQ(one.get(), two.get());
}

TEST(ReferenceCountedSingletonFactory, Release) {
  TestReferenceCountedSingletonFactory factory;

  rcsf_ptr<MyExistenceWatcher> one(&factory);
  one.get();

  MyExistenceWatcher::delete_called_ = false;
  one.release();
  EXPECT_TRUE(MyExistenceWatcher::delete_called_);
}

TEST(ReferenceCountedSingletonFactory, GetWithoutRelease) {
  TestReferenceCountedSingletonFactory factory;
  rcsf_ptr<MyExistenceWatcher> one(&factory);
  one.get();

  MyExistenceWatcher::create_called_ = false;
  one.get();
  EXPECT_FALSE(MyExistenceWatcher::create_called_);
}

TEST(ReferenceCountedSingletonFactory, GetAfterRelease) {
  TestReferenceCountedSingletonFactory factory;
  rcsf_ptr<MyExistenceWatcher> one(&factory);

  MyExistenceWatcher::create_called_ = false;
  one.release();
  one.get();
  EXPECT_TRUE(MyExistenceWatcher::create_called_);
}

TEST(ReferenceCountedSingletonFactory, MultipleReleases) {
  TestReferenceCountedSingletonFactory factory;
  rcsf_ptr<MyExistenceWatcher> one(&factory), two(&factory);

  MyExistenceWatcher::create_called_ = false;
  MyExistenceWatcher::delete_called_ = false;
  one.release();
  EXPECT_FALSE(MyExistenceWatcher::delete_called_);
  one.release();
  EXPECT_FALSE(MyExistenceWatcher::delete_called_);
  one.release();
  EXPECT_FALSE(MyExistenceWatcher::delete_called_);
  one.get();
  EXPECT_TRUE(MyExistenceWatcher::create_called_);
}

TEST(ReferenceCountedSingletonFactory, Existentialism) {
  TestReferenceCountedSingletonFactory factory;

  rcsf_ptr<MyExistenceWatcher> one(&factory);

  MyExistenceWatcher::create_called_ = false;
  MyExistenceWatcher::delete_called_ = false;

  one.get();
  EXPECT_TRUE(MyExistenceWatcher::create_called_);
  one.release();
  EXPECT_TRUE(MyExistenceWatcher::delete_called_);
}

}  // namespace talk_base
