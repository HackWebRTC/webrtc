/*
 * libjingle
 * Copyright 2010, Google Inc.
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
#include "talk/base/pathutils.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/win32toolhelp.h"

namespace talk_base {

typedef struct {
  // Required to match the toolhelp api struct 'design'.
  DWORD dwSize;
  int a;
  uint32 b;
} TestData;

class Win32ToolhelpTest : public testing::Test {
 public:
  Win32ToolhelpTest() {
  }

  HANDLE AsHandle() {
    return reinterpret_cast<HANDLE>(this);
  }

  static Win32ToolhelpTest* AsFixture(HANDLE handle) {
    return reinterpret_cast<Win32ToolhelpTest*>(handle);
  }

  static bool First(HANDLE handle, TestData* d) {
    Win32ToolhelpTest* tst = Win32ToolhelpTest::AsFixture(handle);
    // This method should be called only once for every test.
    // If it is called more than once it return false which
    // should break the test.
    EXPECT_EQ(0, tst->first_called_); // Just to be safe.
    if (tst->first_called_ > 0) {
      return false;
    }

    *d = kTestData[0];
    tst->index_ = 1;
    ++(tst->first_called_);
    return true;
  }

  static bool Next(HANDLE handle, TestData* d) {
    Win32ToolhelpTest* tst = Win32ToolhelpTest::AsFixture(handle);
    ++(tst->next_called_);

    if (tst->index_ >= kTestDataSize) {
      return FALSE;
    }

    *d = kTestData[tst->index_];
    ++(tst->index_);
    return true;
  }

  static bool Fail(HANDLE handle, TestData* d) {
    Win32ToolhelpTest* tst = Win32ToolhelpTest::AsFixture(handle);
    ++(tst->fail_called_);
    return false;
  }

  static bool CloseHandle(HANDLE handle) {
    Win32ToolhelpTest* tst = Win32ToolhelpTest::AsFixture(handle);
    ++(tst->close_handle_called_);
    return true;
  }

 protected:
  virtual void SetUp() {
    fail_called_ = 0;
    first_called_ = 0;
    next_called_ = 0;
    close_handle_called_ = 0;
    index_ = 0;
  }

  static bool AllZero(const TestData& data) {
    return data.dwSize == 0 && data.a == 0 && data.b == 0;
  }

  static bool Equals(const TestData& expected, const TestData& actual) {
    return expected.dwSize == actual.dwSize
        && expected.a == actual.a
        && expected.b == actual.b;
  }

  bool CheckCallCounters(int first, int next, int fail, int close) {
    bool match = first_called_ == first && next_called_ == next
      && fail_called_ == fail && close_handle_called_ == close;

    if (!match) {
      LOG(LS_ERROR) << "Expected: ("
                    << first << ", "
                    << next << ", "
                    << fail << ", "
                    << close << ")";

      LOG(LS_ERROR) << "Actual: ("
                    << first_called_ << ", "
                    << next_called_ << ", "
                    << fail_called_ << ", "
                    << close_handle_called_ << ")";
    }
    return match;
  }

  static const int kTestDataSize = 3;
  static const TestData kTestData[];
  int index_;
  int first_called_;
  int fail_called_;
  int next_called_;
  int close_handle_called_;
};

const TestData Win32ToolhelpTest::kTestData[] = {
  {1, 1, 1}, {2, 2, 2}, {3, 3, 3}
};


class TestTraits {
 public:
  typedef TestData Type;

  static bool First(HANDLE handle, Type* t) {
    return Win32ToolhelpTest::First(handle, t);
  }

  static bool Next(HANDLE handle, Type* t) {
    return Win32ToolhelpTest::Next(handle, t);
  }

  static bool CloseHandle(HANDLE handle) {
    return Win32ToolhelpTest::CloseHandle(handle);
  }
};

class BadFirstTraits {
 public:
  typedef TestData Type;

  static bool First(HANDLE handle, Type* t) {
    return Win32ToolhelpTest::Fail(handle, t);
  }

  static bool Next(HANDLE handle, Type* t) {
    // This should never be called.
    ADD_FAILURE();
    return false;
  }

  static bool CloseHandle(HANDLE handle) {
    return Win32ToolhelpTest::CloseHandle(handle);
  }
};

class BadNextTraits {
 public:
  typedef TestData Type;

  static bool First(HANDLE handle, Type* t) {
    return Win32ToolhelpTest::First(handle, t);
  }

  static bool Next(HANDLE handle, Type* t) {
    return Win32ToolhelpTest::Fail(handle, t);
  }

  static bool CloseHandle(HANDLE handle) {
    return Win32ToolhelpTest::CloseHandle(handle);
  }
};

// The toolhelp in normally inherited but most of
// these tests only excercise the methods from the
// traits therefore I use a typedef to make the
// test code easier to read.
typedef talk_base::ToolhelpEnumeratorBase<TestTraits> EnumeratorForTest;

TEST_F(Win32ToolhelpTest, TestNextWithInvalidCtorHandle) {
  EnumeratorForTest t(INVALID_HANDLE_VALUE);

  EXPECT_FALSE(t.Next());
  EXPECT_TRUE(CheckCallCounters(0, 0, 0, 0));
}

// Tests that Next() returns false if the first-pointer
// function fails.
TEST_F(Win32ToolhelpTest, TestNextFirstFails) {
  typedef talk_base::ToolhelpEnumeratorBase<BadFirstTraits> BadEnumerator;
  talk_base::scoped_ptr<BadEnumerator> t(new BadEnumerator(AsHandle()));

  // If next ever fails it shall always fail.
  EXPECT_FALSE(t->Next());
  EXPECT_FALSE(t->Next());
  EXPECT_FALSE(t->Next());
  t.reset();
  EXPECT_TRUE(CheckCallCounters(0, 0, 1, 1));
}

// Tests that Next() returns false if the next-pointer
// function fails.
TEST_F(Win32ToolhelpTest, TestNextNextFails) {
  typedef talk_base::ToolhelpEnumeratorBase<BadNextTraits> BadEnumerator;
  talk_base::scoped_ptr<BadEnumerator> t(new BadEnumerator(AsHandle()));

  // If next ever fails it shall always fail. No more calls
  // shall be dispatched to Next(...).
  EXPECT_TRUE(t->Next());
  EXPECT_FALSE(t->Next());
  EXPECT_FALSE(t->Next());
  t.reset();
  EXPECT_TRUE(CheckCallCounters(1, 0, 1, 1));
}


// Tests that current returns an object is all zero's
// if Next() hasn't been called.
TEST_F(Win32ToolhelpTest, TestCurrentNextNotCalled) {
  talk_base::scoped_ptr<EnumeratorForTest> t(new EnumeratorForTest(AsHandle()));
  EXPECT_TRUE(AllZero(t->current()));
  t.reset();
  EXPECT_TRUE(CheckCallCounters(0, 0, 0, 1));
}

// Tests the simple everything works path through the code.
TEST_F(Win32ToolhelpTest, TestCurrentNextCalled) {
  talk_base::scoped_ptr<EnumeratorForTest> t(new EnumeratorForTest(AsHandle()));

  EXPECT_TRUE(t->Next());
  EXPECT_TRUE(Equals(t->current(), kTestData[0]));
  EXPECT_TRUE(t->Next());
  EXPECT_TRUE(Equals(t->current(), kTestData[1]));
  EXPECT_TRUE(t->Next());
  EXPECT_TRUE(Equals(t->current(), kTestData[2]));
  EXPECT_FALSE(t->Next());
  t.reset();
  EXPECT_TRUE(CheckCallCounters(1, 3, 0, 1));
}

TEST_F(Win32ToolhelpTest, TestCurrentProcess) {
  int size = MAX_PATH;
  WCHAR buf[MAX_PATH];
  GetModuleFileName(NULL, buf, ARRAY_SIZE(buf));
  std::wstring name = ToUtf16(Pathname(ToUtf8(buf)).filename());

  talk_base::ProcessEnumerator processes;
  bool found = false;
  while (processes.Next()) {
    if (!name.compare(processes.current().szExeFile)) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);

  talk_base::ModuleEnumerator modules(processes.current().th32ProcessID);
  found = false;
  while (modules.Next()) {
    if (!name.compare(modules.current().szModule)) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

}  // namespace talk_base
