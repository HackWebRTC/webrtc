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

#ifndef TALK_SOUND_AUTOMATICALLYCHOSENSOUNDSYSTEM_H_
#define TALK_SOUND_AUTOMATICALLYCHOSENSOUNDSYSTEM_H_

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/sound/soundsysteminterface.h"
#include "talk/sound/soundsystemproxy.h"

namespace cricket {

// A function type that creates an instance of a sound system implementation.
typedef SoundSystemInterface *(*SoundSystemCreator)();

// An AutomaticallyChosenSoundSystem is a sound system proxy that defers to
// an instance of the first sound system implementation in a list that
// successfully initializes.
template <const SoundSystemCreator kSoundSystemCreators[], int kNumSoundSystems>
class AutomaticallyChosenSoundSystem : public SoundSystemProxy {
 public:
  // Chooses and initializes the underlying sound system.
  virtual bool Init();
  // Terminates the underlying sound system implementation, but caches it for
  // future re-use.
  virtual void Terminate();

  virtual const char *GetName() const;

 private:
  talk_base::scoped_ptr<SoundSystemInterface> sound_systems_[kNumSoundSystems];
};

template <const SoundSystemCreator kSoundSystemCreators[], int kNumSoundSystems>
bool AutomaticallyChosenSoundSystem<kSoundSystemCreators,
                                    kNumSoundSystems>::Init() {
  if (wrapped_) {
    return true;
  }
  for (int i = 0; i < kNumSoundSystems; ++i) {
    if (!sound_systems_[i].get()) {
      sound_systems_[i].reset((*kSoundSystemCreators[i])());
    }
    if (sound_systems_[i]->Init()) {
      // This is the first sound system in the list to successfully
      // initialize, so we're done.
      wrapped_ = sound_systems_[i].get();
      break;
    }
    // Else it failed to initialize, so try the remaining ones.
  }
  if (!wrapped_) {
    LOG(LS_ERROR) << "Failed to find a usable sound system";
    return false;
  }
  LOG(LS_INFO) << "Selected " << wrapped_->GetName() << " sound system";
  return true;
}

template <const SoundSystemCreator kSoundSystemCreators[], int kNumSoundSystems>
void AutomaticallyChosenSoundSystem<kSoundSystemCreators,
                                    kNumSoundSystems>::Terminate() {
  if (!wrapped_) {
    return;
  }
  wrapped_->Terminate();
  wrapped_ = NULL;
  // We do not free the scoped_ptrs because we may be re-init'ed soon.
}

template <const SoundSystemCreator kSoundSystemCreators[], int kNumSoundSystems>
const char *AutomaticallyChosenSoundSystem<kSoundSystemCreators,
                                           kNumSoundSystems>::GetName() const {
  return wrapped_ ? wrapped_->GetName() : "automatic";
}

}  // namespace cricket

#endif  // TALK_SOUND_AUTOMATICALLYCHOSENSOUNDSYSTEM_H_
