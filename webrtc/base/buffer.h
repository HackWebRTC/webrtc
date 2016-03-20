/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_BUFFER_H_
#define WEBRTC_BASE_BUFFER_H_

#include <cstring>
#include <memory>
#include <utility>

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"

namespace rtc {

namespace internal {

// (Internal; please don't use outside this file.) ByteType<T>::t is int if T
// is uint8_t, int8_t, or char; otherwise, it's a compilation error. Use like
// this:
//
//   template <typename T, typename ByteType<T>::t = 0>
//   void foo(T* x);
//
// to let foo<T> be defined only for byte-sized integers.
template <typename T>
struct ByteType {
 private:
  static int F(uint8_t*);
  static int F(int8_t*);
  static int F(char*);

 public:
  using t = decltype(F(static_cast<T*>(nullptr)));
};

}  // namespace internal

// Basic buffer class, can be grown and shrunk dynamically.
// Unlike std::string/vector, does not initialize data when expanding capacity.
class Buffer {
 public:
  Buffer();                   // An empty buffer.
  Buffer(Buffer&& buf);       // Move contents from an existing buffer.

  // Construct a buffer with the specified number of uninitialized bytes.
  explicit Buffer(size_t size);
  Buffer(size_t size, size_t capacity);

  // Construct a buffer and copy the specified number of bytes into it. The
  // source array may be (const) uint8_t*, int8_t*, or char*.
  template <typename T, typename internal::ByteType<T>::t = 0>
  Buffer(const T* data, size_t size)
      : Buffer(data, size, size) {}

  template <typename T, typename internal::ByteType<T>::t = 0>
  Buffer(const T* data, size_t size, size_t capacity)
      : Buffer(size, capacity) {
    std::memcpy(data_.get(), data, size);
  }

  // Construct a buffer from the contents of an array.
  template <typename T, size_t N, typename internal::ByteType<T>::t = 0>
  Buffer(const T(&array)[N])
      : Buffer(array, N) {}

  ~Buffer();

  // Get a pointer to the data. Just .data() will give you a (const) uint8_t*,
  // but you may also use .data<int8_t>() and .data<char>().
  template <typename T = uint8_t, typename internal::ByteType<T>::t = 0>
  const T* data() const {
    RTC_DCHECK(IsConsistent());
    return reinterpret_cast<T*>(data_.get());
  }

  template <typename T = uint8_t, typename internal::ByteType<T>::t = 0>
  T* data() {
    RTC_DCHECK(IsConsistent());
    return reinterpret_cast<T*>(data_.get());
  }

  size_t size() const {
    RTC_DCHECK(IsConsistent());
    return size_;
  }

  size_t capacity() const {
    RTC_DCHECK(IsConsistent());
    return capacity_;
  }

  Buffer& operator=(Buffer&& buf) {
    RTC_DCHECK(IsConsistent());
    RTC_DCHECK(buf.IsConsistent());
    size_ = buf.size_;
    capacity_ = buf.capacity_;
    data_ = std::move(buf.data_);
    buf.OnMovedFrom();
    return *this;
  }

  bool operator==(const Buffer& buf) const {
    RTC_DCHECK(IsConsistent());
    return size_ == buf.size() && memcmp(data_.get(), buf.data(), size_) == 0;
  }

  bool operator!=(const Buffer& buf) const { return !(*this == buf); }

  uint8_t& operator[](size_t index) {
    RTC_DCHECK_LT(index, size_);
    return data()[index];
  }

  uint8_t operator[](size_t index) const {
    RTC_DCHECK_LT(index, size_);
    return data()[index];
  }

  // The SetData functions replace the contents of the buffer. They accept the
  // same input types as the constructors.
  template <typename T, typename internal::ByteType<T>::t = 0>
  void SetData(const T* data, size_t size) {
    RTC_DCHECK(IsConsistent());
    size_ = 0;
    AppendData(data, size);
  }

  template <typename T, size_t N, typename internal::ByteType<T>::t = 0>
  void SetData(const T(&array)[N]) {
    SetData(array, N);
  }

  void SetData(const Buffer& buf) { SetData(buf.data(), buf.size()); }

  // Replace the data in the buffer with at most |max_bytes| of data, using the
  // function |setter|, which should have the following signature:
  //   size_t setter(ArrayView<T> view)
  // |setter| is given an appropriately typed ArrayView of the area in which to
  // write the data (i.e. starting at the beginning of the buffer) and should
  // return the number of bytes actually written. This number must be <=
  // |max_bytes|.
  template <typename T = uint8_t, typename F,
            typename internal::ByteType<T>::t = 0>
  size_t SetData(size_t max_bytes, F&& setter) {
    RTC_DCHECK(IsConsistent());
    size_ = 0;
    return AppendData<T>(max_bytes, std::forward<F>(setter));
  }

  // The AppendData functions adds data to the end of the buffer. They accept
  // the same input types as the constructors.
  template <typename T, typename internal::ByteType<T>::t = 0>
  void AppendData(const T* data, size_t size) {
    RTC_DCHECK(IsConsistent());
    const size_t new_size = size_ + size;
    EnsureCapacity(new_size);
    std::memcpy(data_.get() + size_, data, size);
    size_ = new_size;
    RTC_DCHECK(IsConsistent());
  }

  template <typename T, size_t N, typename internal::ByteType<T>::t = 0>
  void AppendData(const T(&array)[N]) {
    AppendData(array, N);
  }

  void AppendData(const Buffer& buf) { AppendData(buf.data(), buf.size()); }

  // Append at most |max_bytes| of data to the end of the buffer, using the
  // function |setter|, which should have the following signature:
  //   size_t setter(ArrayView<T> view)
  // |setter| is given an appropriately typed ArrayView of the area in which to
  // write the data (i.e. starting at the former end of the buffer) and should
  // return the number of bytes actually written. This number must be <=
  // |max_bytes|.
  template <typename T = uint8_t, typename F,
            typename internal::ByteType<T>::t = 0>
  size_t AppendData(size_t max_bytes, F&& setter) {
    RTC_DCHECK(IsConsistent());
    const size_t old_size = size_;
    SetSize(old_size + max_bytes);
    T *base_ptr = data<T>() + old_size;
    size_t written_bytes =
        setter(rtc::ArrayView<T>(base_ptr, max_bytes));

    RTC_CHECK_LE(written_bytes, max_bytes);
    size_ = old_size + written_bytes;
    RTC_DCHECK(IsConsistent());
    return written_bytes;
  }

  // Sets the size of the buffer. If the new size is smaller than the old, the
  // buffer contents will be kept but truncated; if the new size is greater,
  // the existing contents will be kept and the new space will be
  // uninitialized.
  void SetSize(size_t size) {
    EnsureCapacity(size);
    size_ = size;
  }

  // Ensure that the buffer size can be increased to at least capacity without
  // further reallocation. (Of course, this operation might need to reallocate
  // the buffer.)
  void EnsureCapacity(size_t capacity) {
    RTC_DCHECK(IsConsistent());
    if (capacity <= capacity_)
      return;
    std::unique_ptr<uint8_t[]> new_data(new uint8_t[capacity]);
    std::memcpy(new_data.get(), data_.get(), size_);
    data_ = std::move(new_data);
    capacity_ = capacity;
    RTC_DCHECK(IsConsistent());
  }

  // Resets the buffer to zero size without altering capacity. Works even if the
  // buffer has been moved from.
  void Clear() {
    size_ = 0;
    RTC_DCHECK(IsConsistent());
  }

  // Swaps two buffers. Also works for buffers that have been moved from.
  friend void swap(Buffer& a, Buffer& b) {
    using std::swap;
    swap(a.size_, b.size_);
    swap(a.capacity_, b.capacity_);
    swap(a.data_, b.data_);
  }

 private:
  // Precondition for all methods except Clear and the destructor.
  // Postcondition for all methods except move construction and move
  // assignment, which leave the moved-from object in a possibly inconsistent
  // state.
  bool IsConsistent() const {
    return (data_ || capacity_ == 0) && capacity_ >= size_;
  }

  // Called when *this has been moved from. Conceptually it's a no-op, but we
  // can mutate the state slightly to help subsequent sanity checks catch bugs.
  void OnMovedFrom() {
#ifdef NDEBUG
    // Make *this consistent and empty. Shouldn't be necessary, but better safe
    // than sorry.
    size_ = 0;
    capacity_ = 0;
#else
    // Ensure that *this is always inconsistent, to provoke bugs.
    size_ = 1;
    capacity_ = 0;
#endif
  }

  size_t size_;
  size_t capacity_;
  std::unique_ptr<uint8_t[]> data_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Buffer);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_BUFFER_H_
