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

#include "talk/sound/nullsoundsystem.h"

#include "talk/base/logging.h"
#include "talk/sound/sounddevicelocator.h"
#include "talk/sound/soundinputstreaminterface.h"
#include "talk/sound/soundoutputstreaminterface.h"

namespace talk_base {

class Thread;

}

namespace cricket {

// Name used for the single device and the sound system itself.
static const char kNullName[] = "null";

class NullSoundDeviceLocator : public SoundDeviceLocator {
 public:
  NullSoundDeviceLocator() : SoundDeviceLocator(kNullName, kNullName) {}

  virtual SoundDeviceLocator *Copy() const {
    return new NullSoundDeviceLocator();
  }
};

class NullSoundInputStream : public SoundInputStreamInterface {
 public:
  virtual bool StartReading() {
    return true;
  }

  virtual bool StopReading() {
    return true;
  }

  virtual bool GetVolume(int *volume) {
    *volume = SoundSystemInterface::kMinVolume;
    return true;
  }

  virtual bool SetVolume(int volume) {
    return false;
  }

  virtual bool Close() {
    return true;
  }

  virtual int LatencyUsecs() {
    return 0;
  }
};

class NullSoundOutputStream : public SoundOutputStreamInterface {
 public:
  virtual bool EnableBufferMonitoring() {
    return true;
  }

  virtual bool DisableBufferMonitoring() {
    return true;
  }

  virtual bool WriteSamples(const void *sample_data,
                            size_t size) {
    LOG(LS_VERBOSE) << "Got " << size << " bytes of playback samples";
    return true;
  }

  virtual bool GetVolume(int *volume) {
    *volume = SoundSystemInterface::kMinVolume;
    return true;
  }

  virtual bool SetVolume(int volume) {
    return false;
  }

  virtual bool Close() {
    return true;
  }

  virtual int LatencyUsecs() {
    return 0;
  }
};

NullSoundSystem::~NullSoundSystem() {
}

bool NullSoundSystem::Init() {
  return true;
}

void NullSoundSystem::Terminate() {
  // Nothing to do.
}

bool NullSoundSystem::EnumeratePlaybackDevices(
      SoundSystemInterface::SoundDeviceLocatorList *devices) {
  ClearSoundDeviceLocatorList(devices);
  SoundDeviceLocator *device;
  GetDefaultPlaybackDevice(&device);
  devices->push_back(device);
  return true;
}

bool NullSoundSystem::EnumerateCaptureDevices(
      SoundSystemInterface::SoundDeviceLocatorList *devices) {
  ClearSoundDeviceLocatorList(devices);
  SoundDeviceLocator *device;
  GetDefaultCaptureDevice(&device);
  devices->push_back(device);
  return true;
}

bool NullSoundSystem::GetDefaultPlaybackDevice(
    SoundDeviceLocator **device) {
  *device = new NullSoundDeviceLocator();
  return true;
}

bool NullSoundSystem::GetDefaultCaptureDevice(
    SoundDeviceLocator **device) {
  *device = new NullSoundDeviceLocator();
  return true;
}

SoundOutputStreamInterface *NullSoundSystem::OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) {
  return new NullSoundOutputStream();
}

SoundInputStreamInterface *NullSoundSystem::OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) {
  return new NullSoundInputStream();
}

const char *NullSoundSystem::GetName() const {
  return kNullName;
}

}  // namespace cricket
