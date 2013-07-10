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

#ifndef TALK_SOUND_SOUNDSYSTEMINTERFACE_H_
#define TALK_SOUND_SOUNDSYSTEMINTERFACE_H_

#include <vector>

#include "talk/base/constructormagic.h"

namespace cricket {

class SoundDeviceLocator;
class SoundInputStreamInterface;
class SoundOutputStreamInterface;

// Interface for a platform's sound system.
// Implementations must guarantee thread-safety for at least the following use
// cases:
// 1) Concurrent enumeration and opening of devices from different threads.
// 2) Concurrent use of different Sound(Input|Output)StreamInterface
// instances from different threads (but concurrent use of the _same_ one from
// different threads need not be supported).
class SoundSystemInterface {
 public:
  typedef std::vector<SoundDeviceLocator *> SoundDeviceLocatorList;

  enum SampleFormat {
    // Only one supported sample format at this time.
    // The values here may be used in lookup tables, so they shouldn't change.
    FORMAT_S16LE = 0,
  };

  enum Flags {
    // Enable reporting the current stream latency in
    // Sound(Input|Output)StreamInterface. See those classes for more details.
    FLAG_REPORT_LATENCY = (1 << 0),
  };

  struct OpenParams {
    // Format for the sound stream.
    SampleFormat format;
    // Sampling frequency in hertz.
    unsigned int freq;
    // Number of channels in the PCM stream.
    unsigned int channels;
    // Misc flags. Should be taken from the Flags enum above.
    int flags;
    // Desired latency, measured as number of bytes of sample data
    int latency;
  };

  // Special values for the "latency" field of OpenParams.
  // Use this one to say you don't care what the latency is. The sound system
  // will optimize for other things instead.
  static const int kNoLatencyRequirements = -1;
  // Use this one to say that you want the sound system to pick an appropriate
  // small latency value. The sound system may pick the minimum allowed one, or
  // a slightly higher one in the event that the true minimum requires an
  // undesirable trade-off.
  static const int kLowLatency = 0;
 
  // Max value for the volume parameters for Sound(Input|Output)StreamInterface.
  static const int kMaxVolume = 255;
  // Min value for the volume parameters for Sound(Input|Output)StreamInterface.
  static const int kMinVolume = 0;

  // Helper for clearing a locator list and deleting the entries.
  static void ClearSoundDeviceLocatorList(SoundDeviceLocatorList *devices);

  virtual ~SoundSystemInterface() {}

  virtual bool Init() = 0;
  virtual void Terminate() = 0;

  // Enumerates the available devices. (Any pre-existing locators in the lists
  // are deleted.)
  virtual bool EnumeratePlaybackDevices(SoundDeviceLocatorList *devices) = 0;
  virtual bool EnumerateCaptureDevices(SoundDeviceLocatorList *devices) = 0;

  // Gets a special locator for the default device.
  virtual bool GetDefaultPlaybackDevice(SoundDeviceLocator **device) = 0;
  virtual bool GetDefaultCaptureDevice(SoundDeviceLocator **device) = 0;

  // Opens the given device, or returns NULL on error.
  virtual SoundOutputStreamInterface *OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) = 0;
  virtual SoundInputStreamInterface *OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) = 0;

  // A human-readable name for this sound system.
  virtual const char *GetName() const = 0;

 protected:
  SoundSystemInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SoundSystemInterface);
};

}  // namespace cricket

#endif  // TALK_SOUND_SOUNDSYSTEMINTERFACE_H_
