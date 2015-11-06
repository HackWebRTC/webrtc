/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_ARRAY_VIEW_H_
#define WEBRTC_BASE_ARRAY_VIEW_H_

#include <vector>

#include "webrtc/base/checks.h"

namespace rtc {

// Keeps track of an array (a pointer and a size) that it doesn't own.
// ArrayView objects are immutable except for assignment, and small enough to
// be cheaply passed by value.
//
// Note that ArrayView<T> and ArrayView<const T> are distinct types; this is
// how you would represent mutable and unmutable views of an array.
template <typename T>
class ArrayView final {
 public:
  // Construct an empty ArrayView.
  ArrayView() : ArrayView(static_cast<T*>(nullptr), 0) {}

  // Construct an ArrayView for a (pointer,size) pair.
  template <typename U>
  ArrayView(U* data, size_t size)
      : data_(size == 0 ? nullptr : data), size_(size) {
    CheckInvariant();
  }

  // Construct an ArrayView for an array.
  template <typename U, size_t N>
  ArrayView(U (&array)[N]) : ArrayView(&array[0], N) {}

  // Construct an ArrayView for any type U that has a size() method whose
  // return value converts implicitly to size_t, and a data() method whose
  // return value converts implicitly to T*. In particular, this means we allow
  // conversion from ArrayView<T> to ArrayView<const T>, but not the other way
  // around. Other allowed conversions include std::vector<T> to ArrayView<T>
  // or ArrayView<const T>, const std::vector<T> to ArrayView<const T>, and
  // rtc::Buffer to ArrayView<uint8_t> (with the same const behavior as
  // std::vector).
  template <typename U>
  ArrayView(U& u) : ArrayView(u.data(), u.size()) {}
  // TODO(kwiberg): Remove the special case for std::vector (and the include of
  // <vector>); it is handled by the general case in C++11, since std::vector
  // has a data() method there.
  template <typename U>
  ArrayView(std::vector<U>& u)
      : ArrayView(u.empty() ? nullptr : &u[0], u.size()) {}

  // Indexing, size, and iteration. These allow mutation even if the ArrayView
  // is const, because the ArrayView doesn't own the array. (To prevent
  // mutation, use ArrayView<const T>.)
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  T* data() const { return data_; }
  T& operator[](size_t idx) const {
    RTC_DCHECK_LT(idx, size_);
    RTC_DCHECK(data_);  // Follows from size_ > idx and the class invariant.
    return data_[idx];
  }
  T* begin() const { return data_; }
  T* end() const { return data_ + size_; }
  const T* cbegin() const { return data_; }
  const T* cend() const { return data_ + size_; }

  // Comparing two ArrayViews compares their (pointer,size) pairs; it does
  // *not* dereference the pointers.
  friend bool operator==(const ArrayView& a, const ArrayView& b) {
    return a.data_ == b.data_ && a.size_ == b.size_;
  }
  friend bool operator!=(const ArrayView& a, const ArrayView& b) {
    return !(a == b);
  }

 private:
  // Invariant: !data_ iff size_ == 0.
  void CheckInvariant() const { RTC_DCHECK_EQ(!data_, size_ == 0); }
  T* data_;
  size_t size_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_ARRAY_VIEW_H_
