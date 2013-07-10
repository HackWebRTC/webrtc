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

#ifndef TALK_SOUND_ALSASOUNDSYSTEM_H_
#define TALK_SOUND_ALSASOUNDSYSTEM_H_

#include "talk/base/constructormagic.h"
#include "talk/sound/alsasymboltable.h"
#include "talk/sound/soundsysteminterface.h"

namespace cricket {

class AlsaStream;
class AlsaInputStream;
class AlsaOutputStream;

// Sound system implementation for ALSA, the predominant sound device API on
// Linux (but typically not used directly by applications anymore).
class AlsaSoundSystem : public SoundSystemInterface {
  friend class AlsaStream;
  friend class AlsaInputStream;
  friend class AlsaOutputStream;
 public:
  static SoundSystemInterface *Create() {
    return new AlsaSoundSystem();
  }

  AlsaSoundSystem();

  virtual ~AlsaSoundSystem();

  virtual bool Init();
  virtual void Terminate();

  virtual bool EnumeratePlaybackDevices(SoundDeviceLocatorList *devices);
  virtual bool EnumerateCaptureDevices(SoundDeviceLocatorList *devices);

  virtual bool GetDefaultPlaybackDevice(SoundDeviceLocator **device);
  virtual bool GetDefaultCaptureDevice(SoundDeviceLocator **device);

  virtual SoundOutputStreamInterface *OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);
  virtual SoundInputStreamInterface *OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);

  virtual const char *GetName() const;

 private:
  bool IsInitialized() { return initialized_; }

  bool EnumerateDevices(SoundDeviceLocatorList *devices,
                        bool capture_not_playback);

  bool GetDefaultDevice(SoundDeviceLocator **device);

  static size_t FrameSize(const OpenParams &params);

  template <typename StreamInterface>
  StreamInterface *OpenDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params,
      snd_pcm_stream_t type,
      StreamInterface *(AlsaSoundSystem::*start_fn)(
          snd_pcm_t *handle,
          size_t frame_size,
          int wait_timeout_ms,
          int flags,
          int freq));

  SoundOutputStreamInterface *StartOutputStream(
      snd_pcm_t *handle,
      size_t frame_size,
      int wait_timeout_ms,
      int flags,
      int freq);

  SoundInputStreamInterface *StartInputStream(
      snd_pcm_t *handle,
      size_t frame_size,
      int wait_timeout_ms,
      int flags,
      int freq);

  const char *GetError(int err);

  bool initialized_;
  AlsaSymbolTable symbol_table_;

  DISALLOW_COPY_AND_ASSIGN(AlsaSoundSystem);
};

}  // namespace cricket

#endif  // TALK_SOUND_ALSASOUNDSYSTEM_H_
