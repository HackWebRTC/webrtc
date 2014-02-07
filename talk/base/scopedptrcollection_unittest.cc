/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#include "talk/base/scopedptrcollection.h"
#include "talk/base/gunit.h"

namespace talk_base {

namespace {

class InstanceCounter {
 public:
  explicit InstanceCounter(int* num_instances)
      : num_instances_(num_instances) {
    ++(*num_instances_);
  }
  ~InstanceCounter() {
    --(*num_instances_);
  }

 private:
  int* num_instances_;

  DISALLOW_COPY_AND_ASSIGN(InstanceCounter);
};

}  // namespace

class ScopedPtrCollectionTest : public testing::Test {
 protected:
  ScopedPtrCollectionTest()
      : num_instances_(0),
      collection_(new ScopedPtrCollection<InstanceCounter>()) {
  }

  int num_instances_;
  scoped_ptr<ScopedPtrCollection<InstanceCounter> > collection_;
};

TEST_F(ScopedPtrCollectionTest, PushBack) {
  EXPECT_EQ(0u, collection_->collection().size());
  EXPECT_EQ(0, num_instances_);
  const int kNum = 100;
  for (int i = 0; i < kNum; ++i) {
    collection_->PushBack(new InstanceCounter(&num_instances_));
  }
  EXPECT_EQ(static_cast<size_t>(kNum), collection_->collection().size());
  EXPECT_EQ(kNum, num_instances_);
  collection_.reset();
  EXPECT_EQ(0, num_instances_);
}

TEST_F(ScopedPtrCollectionTest, Remove) {
  InstanceCounter* ic = new InstanceCounter(&num_instances_);
  collection_->PushBack(ic);
  EXPECT_EQ(1u, collection_->collection().size());
  collection_->Remove(ic);
  EXPECT_EQ(1, num_instances_);
  collection_.reset();
  EXPECT_EQ(1, num_instances_);
  delete ic;
  EXPECT_EQ(0, num_instances_);
}


}  // namespace talk_base
