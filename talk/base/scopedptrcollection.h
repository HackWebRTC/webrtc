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

// Stores a collection of pointers that are deleted when the container is
// destructed.

#ifndef TALK_BASE_SCOPEDPTRCOLLECTION_H_
#define TALK_BASE_SCOPEDPTRCOLLECTION_H_

#include <algorithm>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/constructormagic.h"

namespace talk_base {

template<class T>
class ScopedPtrCollection {
 public:
  typedef std::vector<T*> VectorT;

  ScopedPtrCollection() { }
  ~ScopedPtrCollection() {
    for (typename VectorT::iterator it = collection_.begin();
         it != collection_.end(); ++it) {
      delete *it;
    }
  }

  const VectorT& collection() const { return collection_; }
  void Reserve(size_t size) {
    collection_.reserve(size);
  }
  void PushBack(T* t) {
    collection_.push_back(t);
  }

  // Remove |t| from the collection without deleting it.
  void Remove(T* t) {
    collection_.erase(std::remove(collection_.begin(), collection_.end(), t),
                      collection_.end());
  }

 private:
  VectorT collection_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPtrCollection);
};

}  // namespace talk_base

#endif  // TALK_BASE_SCOPEDPTRCOLLECTION_H_
