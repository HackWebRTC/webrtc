/*
 * libjingle
 * Copyright 2004-2010, Google Inc.
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

#ifndef TALK_BASE_BUFFER_H_
#define TALK_BASE_BUFFER_H_

#include <cstring>

#include "talk/base/scoped_ptr.h"

namespace talk_base {

// Basic buffer class, can be grown and shrunk dynamically.
// Unlike std::string/vector, does not initialize data when expanding capacity.
class Buffer {
 public:
  Buffer() {
    Construct(NULL, 0, 0);
  }
  Buffer(const void* data, size_t length) {
    Construct(data, length, length);
  }
  Buffer(const void* data, size_t length, size_t capacity) {
    Construct(data, length, capacity);
  }
  Buffer(const Buffer& buf) {
    Construct(buf.data(), buf.length(), buf.length());
  }

  const char* data() const { return data_.get(); }
  char* data() { return data_.get(); }
  // TODO: should this be size(), like STL?
  size_t length() const { return length_; }
  size_t capacity() const { return capacity_; }

  Buffer& operator=(const Buffer& buf) {
    if (&buf != this) {
      Construct(buf.data(), buf.length(), buf.length());
    }
    return *this;
  }
  bool operator==(const Buffer& buf) const {
    return (length_ == buf.length() &&
            memcmp(data_.get(), buf.data(), length_) == 0);
  }
  bool operator!=(const Buffer& buf) const {
    return !operator==(buf);
  }

  void SetData(const void* data, size_t length) {
    ASSERT(data != NULL || length == 0);
    SetLength(length);
    memcpy(data_.get(), data, length);
  }
  void AppendData(const void* data, size_t length) {
    ASSERT(data != NULL || length == 0);
    size_t old_length = length_;
    SetLength(length_ + length);
    memcpy(data_.get() + old_length, data, length);
  }
  void SetLength(size_t length) {
    SetCapacity(length);
    length_ = length;
  }
  void SetCapacity(size_t capacity) {
    if (capacity > capacity_) {
      talk_base::scoped_array<char> data(new char[capacity]);
      memcpy(data.get(), data_.get(), length_);
      data_.swap(data);
      capacity_ = capacity;
    }
  }

  void TransferTo(Buffer* buf) {
    ASSERT(buf != NULL);
    buf->data_.reset(data_.release());
    buf->length_ = length_;
    buf->capacity_ = capacity_;
    Construct(NULL, 0, 0);
  }

 protected:
  void Construct(const void* data, size_t length, size_t capacity) {
    data_.reset(new char[capacity_ = capacity]);
    SetData(data, length);
  }

  scoped_array<char> data_;
  size_t length_;
  size_t capacity_;
};

}  // namespace talk_base

#endif  // TALK_BASE_BUFFER_H_
