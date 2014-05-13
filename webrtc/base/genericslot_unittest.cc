/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/base/genericslot.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"

namespace rtc {

TEST(GenericSlotTest, TestSlot1) {
  sigslot::signal1<int> source1;
  GenericSlot1<int> slot1(&source1, 1);
  EXPECT_FALSE(slot1.callback_received());
  source1.emit(10);
  EXPECT_TRUE(slot1.callback_received());
  EXPECT_EQ(10, slot1.arg1());
}

TEST(GenericSlotTest, TestSlot2) {
  sigslot::signal2<int, char> source2;
  GenericSlot2<int, char> slot2(&source2, 1, '0');
  EXPECT_FALSE(slot2.callback_received());
  source2.emit(10, 'x');
  EXPECT_TRUE(slot2.callback_received());
  EXPECT_EQ(10, slot2.arg1());
  EXPECT_EQ('x', slot2.arg2());
}

// By induction we assume the rest work too...

}  // namespace rtc
