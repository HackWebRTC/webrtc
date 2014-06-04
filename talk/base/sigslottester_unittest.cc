#include "talk/base/sigslottester.h"

#include "talk/base/gunit.h"
#include "talk/base/sigslot.h"

namespace talk_base {

TEST(SigslotTester, TestSignal1Arg) {
  sigslot::signal1<int> source1;
  int capture1;
  SigslotTester1<int, int> slot1(&source1, &capture1);
  EXPECT_EQ(0, slot1.callback_count());

  source1.emit(10);
  EXPECT_EQ(1, slot1.callback_count());
  EXPECT_EQ(10, capture1);

  source1.emit(20);
  EXPECT_EQ(2, slot1.callback_count());
  EXPECT_EQ(20, capture1);
}

TEST(SigslotTester, TestSignal2Args) {
  sigslot::signal2<int, char> source2;
  int capture1;
  char capture2;
  SigslotTester2<int, char, int, char> slot2(&source2, &capture1, &capture2);
  EXPECT_EQ(0, slot2.callback_count());

  source2.emit(10, 'x');
  EXPECT_EQ(1, slot2.callback_count());
  EXPECT_EQ(10, capture1);
  EXPECT_EQ('x', capture2);

  source2.emit(20, 'y');
  EXPECT_EQ(2, slot2.callback_count());
  EXPECT_EQ(20, capture1);
  EXPECT_EQ('y', capture2);
}

// Since it applies for 1 and 2 args, we assume it will work for up to 5 args.

TEST(SigslotTester, TestSignalWithConstReferenceArgs) {
  sigslot::signal1<const std::string&> source1;
  std::string capture1;
  SigslotTester1<const std::string&, std::string> slot1(&source1, &capture1);
  EXPECT_EQ(0, slot1.callback_count());
  source1.emit("hello");
  EXPECT_EQ(1, slot1.callback_count());
  EXPECT_EQ("hello", capture1);
}

TEST(SigslotTester, TestSignalWithPointerToConstArgs) {
  sigslot::signal1<const std::string*> source1;
  const std::string* capture1;
  SigslotTester1<const std::string*, const std::string*> slot1(&source1,
                                                               &capture1);
  EXPECT_EQ(0, slot1.callback_count());
  source1.emit(NULL);
  EXPECT_EQ(1, slot1.callback_count());
  EXPECT_EQ(NULL, capture1);
}

TEST(SigslotTester, TestSignalWithConstPointerArgs) {
  sigslot::signal1<std::string* const> source1;
  std::string* capture1;
  SigslotTester1<std::string* const, std::string*> slot1(&source1, &capture1);
  EXPECT_EQ(0, slot1.callback_count());
  source1.emit(NULL);
  EXPECT_EQ(1, slot1.callback_count());
  EXPECT_EQ(NULL, capture1);
}

}  // namespace talk_base
