#include "talk/base/genericslot.h"
#include "talk/base/gunit.h"
#include "talk/base/sigslot.h"

namespace talk_base {

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

}  // namespace talk_base
