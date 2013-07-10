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

#ifndef TALK_SOUND_NULLSOUNDSYSTEM_H_
#define TALK_SOUND_NULLSOUNDSYSTEM_H_

#include "talk/sound/soundsysteminterface.h"

namespace cricket {

class SoundDeviceLocator;
class SoundInputStreamInterface;
class SoundOutputStreamInterface;

// A simple reference sound system that drops output samples and generates
// no input samples.
class NullSoundSystem : public SoundSystemInterface {
 public:
  static SoundSystemInterface *Create() {
    return new NullSoundSystem();
  }

  virtual ~NullSoundSystem();

  virtual bool Init();
  virtual void Terminate();

  virtual bool EnumeratePlaybackDevices(SoundDeviceLocatorList *devices);
  virtual bool EnumerateCaptureDevices(SoundDeviceLocatorList *devices);

  virtual SoundOutputStreamInterface *OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);
  virtual SoundInputStreamInterface *OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);

  virtual bool GetDefaultPlaybackDevice(SoundDeviceLocator **device);
  virtual bool GetDefaultCaptureDevice(SoundDeviceLocator **device);

  virtual const char *GetName() const;
};

}  // namespace cricket

#endif  // TALK_SOUND_NULLSOUNDSYSTEM_H_
