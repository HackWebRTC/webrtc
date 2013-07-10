/*
 * libjingle
 * Copyright 2004--2010, Google Inc.
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

#ifndef TALK_BASE_REFERENCECOUNTEDSINGLETONFACTORY_H_
#define TALK_BASE_REFERENCECOUNTEDSINGLETONFACTORY_H_

#include "talk/base/common.h"
#include "talk/base/criticalsection.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"

namespace talk_base {

template <typename Interface> class rcsf_ptr;

// A ReferenceCountedSingletonFactory is an object which owns another object,
// and doles out the owned object to consumers in a reference-counted manner.
// Thus, the factory owns at most one object of the desired kind, and
// hands consumers a special pointer to it, through which they can access it.
// When the consumers delete the pointer, the reference count goes down,
// and if the reference count hits zero, the factory can throw the object
// away.  If a consumer requests the pointer and the factory has none,
// it can create one on the fly and pass it back.
template <typename Interface>
class ReferenceCountedSingletonFactory {
  friend class rcsf_ptr<Interface>;
 public:
  ReferenceCountedSingletonFactory() : ref_count_(0) {}

  virtual ~ReferenceCountedSingletonFactory() {
    ASSERT(ref_count_ == 0);
  }

 protected:
  // Must be implemented in a sub-class. The sub-class may choose whether or not
  // to cache the instance across lifetimes by either reset()'ing or not
  // reset()'ing the scoped_ptr in CleanupInstance().
  virtual bool SetupInstance() = 0;
  virtual void CleanupInstance() = 0;

  scoped_ptr<Interface> instance_;

 private:
  Interface* GetInstance() {
    talk_base::CritScope cs(&crit_);
    if (ref_count_ == 0) {
      if (!SetupInstance()) {
        LOG(LS_VERBOSE) << "Failed to setup instance";
        return NULL;
      }
      ASSERT(instance_.get() != NULL);
    }
    ++ref_count_;

    LOG(LS_VERBOSE) << "Number of references: " << ref_count_;
    return instance_.get();
  }

  void ReleaseInstance() {
    talk_base::CritScope cs(&crit_);
    ASSERT(ref_count_ > 0);
    ASSERT(instance_.get() != NULL);
    --ref_count_;
    LOG(LS_VERBOSE) << "Number of references: " << ref_count_;
    if (ref_count_ == 0) {
      CleanupInstance();
    }
  }

  CriticalSection crit_;
  int ref_count_;

  DISALLOW_COPY_AND_ASSIGN(ReferenceCountedSingletonFactory);
};

template <typename Interface>
class rcsf_ptr {
 public:
  // Create a pointer that uses the factory to get the instance.
  // This is lazy - it won't generate the instance until it is requested.
  explicit rcsf_ptr(ReferenceCountedSingletonFactory<Interface>* factory)
      : instance_(NULL),
        factory_(factory) {
  }

  ~rcsf_ptr() {
    release();
  }

  Interface& operator*() {
    EnsureAcquired();
    return *instance_;
  }

  Interface* operator->() {
    EnsureAcquired();
    return instance_;
  }

  // Gets the pointer, creating the singleton if necessary. May return NULL if
  // creation failed.
  Interface* get() {
    Acquire();
    return instance_;
  }

  // Set instance to NULL and tell the factory we aren't using the instance
  // anymore.
  void release() {
    if (instance_) {
      instance_ = NULL;
      factory_->ReleaseInstance();
    }
  }

  // Lets us know whether instance is valid or not right now.
  // Even though attempts to use the instance will automatically create it, it
  // is advisable to check this because creation can fail.
  bool valid() const {
    return instance_ != NULL;
  }

  // Returns the factory that this pointer is using.
  ReferenceCountedSingletonFactory<Interface>* factory() const {
    return factory_;
  }

 private:
  void EnsureAcquired() {
    Acquire();
    ASSERT(instance_ != NULL);
  }

  void Acquire() {
    // Since we're getting a singleton back, acquire is a noop if instance is
    // already populated.
    if (!instance_) {
      instance_ = factory_->GetInstance();
    }
  }

  Interface* instance_;
  ReferenceCountedSingletonFactory<Interface>* factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(rcsf_ptr);
};

};  // namespace talk_base

#endif  // TALK_BASE_REFERENCECOUNTEDSINGLETONFACTORY_H_
