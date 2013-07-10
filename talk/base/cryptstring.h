/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef _TALK_BASE_CRYPTSTRING_H_
#define _TALK_BASE_CRYPTSTRING_H_

#include <cstring>
#include <string>
#include <vector>
#include "talk/base/linked_ptr.h"
#include "talk/base/scoped_ptr.h"

namespace talk_base {

class CryptStringImpl {
public:
  virtual ~CryptStringImpl() {}
  virtual size_t GetLength() const = 0;
  virtual void CopyTo(char * dest, bool nullterminate) const = 0;
  virtual std::string UrlEncode() const = 0;
  virtual CryptStringImpl * Copy() const = 0;
  virtual void CopyRawTo(std::vector<unsigned char> * dest) const = 0;
};

class EmptyCryptStringImpl : public CryptStringImpl {
public:
  virtual ~EmptyCryptStringImpl() {}
  virtual size_t GetLength() const { return 0; }
  virtual void CopyTo(char * dest, bool nullterminate) const {
    if (nullterminate) {
      *dest = '\0';
    }
  }
  virtual std::string UrlEncode() const { return ""; }
  virtual CryptStringImpl * Copy() const { return new EmptyCryptStringImpl(); }
  virtual void CopyRawTo(std::vector<unsigned char> * dest) const {
    dest->clear();
  }
};

class CryptString {
public:
  CryptString() : impl_(new EmptyCryptStringImpl()) {}
  size_t GetLength() const { return impl_->GetLength(); }
  void CopyTo(char * dest, bool nullterminate) const { impl_->CopyTo(dest, nullterminate); }
  CryptString(const CryptString & other) : impl_(other.impl_->Copy()) {}
  explicit CryptString(const CryptStringImpl & impl) : impl_(impl.Copy()) {}
  CryptString & operator=(const CryptString & other) {
    if (this != &other) {
      impl_.reset(other.impl_->Copy());
    }
    return *this;
  }
  void Clear() { impl_.reset(new EmptyCryptStringImpl()); }
  std::string UrlEncode() const { return impl_->UrlEncode(); }
  void CopyRawTo(std::vector<unsigned char> * dest) const {
    return impl_->CopyRawTo(dest);
  }
  
private:
  scoped_ptr<const CryptStringImpl> impl_;
};


// Used for constructing strings where a password is involved and we
// need to ensure that we zero memory afterwards
class FormatCryptString {
public:
  FormatCryptString() {
    storage_ = new char[32];
    capacity_ = 32;
    length_ = 0;
    storage_[0] = 0;
  }
  
  void Append(const std::string & text) {
    Append(text.data(), text.length());
  }

  void Append(const char * data, size_t length) {
    EnsureStorage(length_ + length + 1);
    memcpy(storage_ + length_, data, length);
    length_ += length;
    storage_[length_] = '\0';
  }
  
  void Append(const CryptString * password) {
    size_t len = password->GetLength();
    EnsureStorage(length_ + len + 1);
    password->CopyTo(storage_ + length_, true);
    length_ += len;
  }

  size_t GetLength() {
    return length_;
  }

  const char * GetData() {
    return storage_;
  }


  // Ensures storage of at least n bytes
  void EnsureStorage(size_t n) {
    if (capacity_ >= n) {
      return;
    }

    size_t old_capacity = capacity_;
    char * old_storage = storage_;

    for (;;) {
      capacity_ *= 2;
      if (capacity_ >= n)
        break;
    }

    storage_ = new char[capacity_];

    if (old_capacity) {
      memcpy(storage_, old_storage, length_);
    
      // zero memory in a way that an optimizer won't optimize it out
      old_storage[0] = 0;
      for (size_t i = 1; i < old_capacity; i++) {
        old_storage[i] = old_storage[i - 1];
      }
      delete[] old_storage;
    }
  }  

  ~FormatCryptString() {
    if (capacity_) {
      storage_[0] = 0;
      for (size_t i = 1; i < capacity_; i++) {
        storage_[i] = storage_[i - 1];
      }
    }
    delete[] storage_;
  }
private:
  char * storage_;
  size_t capacity_;
  size_t length_;
};

class InsecureCryptStringImpl : public CryptStringImpl {
 public:
  std::string& password() { return password_; }
  const std::string& password() const { return password_; }

  virtual ~InsecureCryptStringImpl() {}
  virtual size_t GetLength() const { return password_.size(); }
  virtual void CopyTo(char * dest, bool nullterminate) const {
    memcpy(dest, password_.data(), password_.size());
    if (nullterminate) dest[password_.size()] = 0;
  }
  virtual std::string UrlEncode() const { return password_; }
  virtual CryptStringImpl * Copy() const {
    InsecureCryptStringImpl * copy = new InsecureCryptStringImpl;
    copy->password() = password_;
    return copy;
  }
  virtual void CopyRawTo(std::vector<unsigned char> * dest) const {
    dest->resize(password_.size());
    memcpy(&dest->front(), password_.data(), password_.size());
  }
 private:
  std::string password_;
};

}

#endif  // _TALK_BASE_CRYPTSTRING_H_
