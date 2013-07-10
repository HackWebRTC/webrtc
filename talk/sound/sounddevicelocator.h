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

#ifndef TALK_SOUND_SOUNDDEVICELOCATOR_H_
#define TALK_SOUND_SOUNDDEVICELOCATOR_H_

#include <string>

#include "talk/base/constructormagic.h"

namespace cricket {

// A simple container for holding the name of a device and any additional id
// information needed to locate and open it. Implementations of
// SoundSystemInterface must subclass this to add any id information that they
// need.
class SoundDeviceLocator {
 public:
  virtual ~SoundDeviceLocator() {}

  // Human-readable name for the device.
  const std::string &name() const { return name_; }

  // Name sound system uses to locate this device.
  const std::string &device_name() const { return device_name_; }

  // Makes a duplicate of this locator.
  virtual SoundDeviceLocator *Copy() const = 0;

 protected:
  SoundDeviceLocator(const std::string &name,
                     const std::string &device_name)
      : name_(name), device_name_(device_name) {}

  explicit SoundDeviceLocator(const SoundDeviceLocator &that)
      : name_(that.name_), device_name_(that.device_name_) {}

  std::string name_;
  std::string device_name_;

 private:
  DISALLOW_ASSIGN(SoundDeviceLocator);
};

}  // namespace cricket

#endif  // TALK_SOUND_SOUNDDEVICELOCATOR_H_
