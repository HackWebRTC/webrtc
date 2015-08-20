/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/bind.h"
#include "webrtc/base/gunit.h"

#include "webrtc/base/refcount.h"

namespace rtc {

namespace {

struct MethodBindTester {
  void NullaryVoid() { ++call_count; }
  int NullaryInt() { ++call_count; return 1; }
  int NullaryConst() const { ++call_count; return 2; }
  void UnaryVoid(int dummy) { ++call_count; }
  template <class T> T Identity(T value) { ++call_count; return value; }
  int UnaryByRef(int& value) const { ++call_count; return ++value; }  // NOLINT
  int Multiply(int a, int b) const { ++call_count; return a * b; }
  mutable int call_count;
};

struct A { int dummy; };
struct B: public RefCountInterface { int dummy; };
struct C: public A, B {};
struct D {
  int AddRef();
};
struct E: public D {
  int Release();
};
struct F {
  void AddRef();
  void Release();
};

class LifeTimeCheck : public RefCountInterface {
 public:
  LifeTimeCheck(bool* has_died) : has_died_(has_died), is_ok_to_die_(false) {}
  ~LifeTimeCheck() {
    EXPECT_TRUE(is_ok_to_die_);
    *has_died_ = true;
  }
  void PrepareToDie() { is_ok_to_die_ = true; }
  void NullaryVoid() {}

 private:
  bool* const has_died_;
  bool is_ok_to_die_;
};

int Return42() { return 42; }
int Negate(int a) { return -a; }
int Multiply(int a, int b) { return a * b; }

}  // namespace

// Try to catch any problem with scoped_refptr type deduction in rtc::Bind at
// compile time.
#define EXPECT_IS_CAPTURED_AS_PTR(T)                              \
  static_assert(is_same<detail::PointerType<T>::type, T*>::value, \
                "PointerType")
#define EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(T)                        \
  static_assert(                                                      \
      is_same<detail::PointerType<T>::type, scoped_refptr<T>>::value, \
      "PointerType")

EXPECT_IS_CAPTURED_AS_PTR(void);
EXPECT_IS_CAPTURED_AS_PTR(int);
EXPECT_IS_CAPTURED_AS_PTR(double);
EXPECT_IS_CAPTURED_AS_PTR(A);
EXPECT_IS_CAPTURED_AS_PTR(D);
EXPECT_IS_CAPTURED_AS_PTR(RefCountInterface*);

EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(RefCountInterface);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(B);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(C);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(E);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(F);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(RefCountedObject<RefCountInterface>);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(RefCountedObject<B>);
EXPECT_IS_CAPTURED_AS_SCOPED_REFPTR(RefCountedObject<C>);

TEST(BindTest, BindToMethod) {
  MethodBindTester object = {0};
  EXPECT_EQ(0, object.call_count);
  Bind(&MethodBindTester::NullaryVoid, &object)();
  EXPECT_EQ(1, object.call_count);
  EXPECT_EQ(1, Bind(&MethodBindTester::NullaryInt, &object)());
  EXPECT_EQ(2, object.call_count);
  EXPECT_EQ(2, Bind(&MethodBindTester::NullaryConst,
                    static_cast<const MethodBindTester*>(&object))());
  EXPECT_EQ(3, object.call_count);
  Bind(&MethodBindTester::UnaryVoid, &object, 5)();
  EXPECT_EQ(4, object.call_count);
  EXPECT_EQ(100, Bind(&MethodBindTester::Identity<int>, &object, 100)());
  EXPECT_EQ(5, object.call_count);
  const std::string string_value("test string");
  EXPECT_EQ(string_value, Bind(&MethodBindTester::Identity<std::string>,
                               &object, string_value)());
  EXPECT_EQ(6, object.call_count);
  int value = 11;
  EXPECT_EQ(12, Bind(&MethodBindTester::UnaryByRef, &object, value)());
  EXPECT_EQ(12, value);
  EXPECT_EQ(7, object.call_count);
  EXPECT_EQ(56, Bind(&MethodBindTester::Multiply, &object, 7, 8)());
  EXPECT_EQ(8, object.call_count);
}

TEST(BindTest, BindToFunction) {
  EXPECT_EQ(42, Bind(&Return42)());
  EXPECT_EQ(3, Bind(&Negate, -3)());
  EXPECT_EQ(56, Bind(&Multiply, 8, 7)());
}

// Test Bind where method object implements RefCountInterface and is passed as a
// pointer.
TEST(BindTest, CapturePointerAsScopedRefPtr) {
  bool object_has_died = false;
  scoped_refptr<LifeTimeCheck> object =
      new RefCountedObject<LifeTimeCheck>(&object_has_died);
  {
    auto functor = Bind(&LifeTimeCheck::PrepareToDie, object.get());
    object = nullptr;
    EXPECT_FALSE(object_has_died);
    // Run prepare to die via functor.
    functor();
  }
  EXPECT_TRUE(object_has_died);
}

// Test Bind where method object implements RefCountInterface and is passed as a
// scoped_refptr<>.
TEST(BindTest, CaptureScopedRefPtrAsScopedRefPtr) {
  bool object_has_died = false;
  scoped_refptr<LifeTimeCheck> object =
      new RefCountedObject<LifeTimeCheck>(&object_has_died);
  {
    auto functor = Bind(&LifeTimeCheck::PrepareToDie, object);
    object = nullptr;
    EXPECT_FALSE(object_has_died);
    // Run prepare to die via functor.
    functor();
  }
  EXPECT_TRUE(object_has_died);
}

// Test Bind where method object is captured as scoped_refptr<> and the functor
// dies while there are references left.
TEST(BindTest, FunctorReleasesObjectOnDestruction) {
  bool object_has_died = false;
  scoped_refptr<LifeTimeCheck> object =
      new RefCountedObject<LifeTimeCheck>(&object_has_died);
  Bind(&LifeTimeCheck::NullaryVoid, object.get())();
  EXPECT_FALSE(object_has_died);
  object->PrepareToDie();
  object = nullptr;
  EXPECT_TRUE(object_has_died);
}

}  // namespace rtc
