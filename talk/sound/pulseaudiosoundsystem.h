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

#ifndef TALK_SOUND_PULSEAUDIOSOUNDSYSTEM_H_
#define TALK_SOUND_PULSEAUDIOSOUNDSYSTEM_H_

#ifdef HAVE_LIBPULSE

#include "talk/base/constructormagic.h"
#include "talk/sound/pulseaudiosymboltable.h"
#include "talk/sound/soundsysteminterface.h"

namespace cricket {

class PulseAudioInputStream;
class PulseAudioOutputStream;
class PulseAudioStream;

// Sound system implementation for PulseAudio, a cross-platform sound server
// (but commonly used only on Linux, which is the only platform we support
// it on).
// Init(), Terminate(), and the destructor should never be invoked concurrently,
// but all other methods are thread-safe.
class PulseAudioSoundSystem : public SoundSystemInterface {
  friend class PulseAudioInputStream;
  friend class PulseAudioOutputStream;
  friend class PulseAudioStream;
 public:
  static SoundSystemInterface *Create() {
    return new PulseAudioSoundSystem();
  }

  PulseAudioSoundSystem();

  virtual ~PulseAudioSoundSystem();

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
  bool IsInitialized();

  static void ConnectToPulseCallbackThunk(pa_context *context, void *userdata);

  void OnConnectToPulseCallback(pa_context *context, bool *connect_done);

  bool ConnectToPulse(pa_context *context);

  pa_context *CreateNewConnection();

  template <typename InfoStruct>
  bool EnumerateDevices(SoundDeviceLocatorList *devices,
                        pa_operation *(*enumerate_fn)(
                            pa_context *c,
                            void (*callback_fn)(
                                pa_context *c,
                                const InfoStruct *i,
                                int eol,
                                void *userdata),
                            void *userdata),
                        void (*callback_fn)(
                            pa_context *c,
                            const InfoStruct *i,
                            int eol,
                            void *userdata));

  static void EnumeratePlaybackDevicesCallbackThunk(pa_context *unused,
                                                    const pa_sink_info *info,
                                                    int eol,
                                                    void *userdata);

  static void EnumerateCaptureDevicesCallbackThunk(pa_context *unused,
                                                   const pa_source_info *info,
                                                   int eol,
                                                   void *userdata);

  void OnEnumeratePlaybackDevicesCallback(
      SoundDeviceLocatorList *devices,
      const pa_sink_info *info,
      int eol);

  void OnEnumerateCaptureDevicesCallback(
      SoundDeviceLocatorList *devices,
      const pa_source_info *info,
      int eol);

  template <const char *(pa_server_info::*field)>
  static void GetDefaultDeviceCallbackThunk(
      pa_context *unused,
      const pa_server_info *info,
      void *userdata);

  template <const char *(pa_server_info::*field)>
  void OnGetDefaultDeviceCallback(
      const pa_server_info *info,
      SoundDeviceLocator **device);

  template <const char *(pa_server_info::*field)>
  bool GetDefaultDevice(SoundDeviceLocator **device);

  static void StreamStateChangedCallbackThunk(pa_stream *stream,
                                              void *userdata);

  void OnStreamStateChangedCallback(pa_stream *stream);

  template <typename StreamInterface>
  StreamInterface *OpenDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params,
      const char *stream_name,
      StreamInterface *(PulseAudioSoundSystem::*connect_fn)(
          pa_stream *stream,
          const char *dev,
          int flags,
          pa_stream_flags_t pa_flags,
          int latency,
          const pa_sample_spec &spec));

  SoundOutputStreamInterface *ConnectOutputStream(
      pa_stream *stream,
      const char *dev,
      int flags,
      pa_stream_flags_t pa_flags,
      int latency,
      const pa_sample_spec &spec);

  SoundInputStreamInterface *ConnectInputStream(
      pa_stream *stream,
      const char *dev,
      int flags,
      pa_stream_flags_t pa_flags,
      int latency,
      const pa_sample_spec &spec);

  bool FinishOperation(pa_operation *op);

  void Lock();
  void Unlock();
  void Wait();
  void Signal();

  const char *LastError();

  pa_threaded_mainloop *mainloop_;
  pa_context *context_;
  PulseAudioSymbolTable symbol_table_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioSoundSystem);
};

}  // namespace cricket

#endif  // HAVE_LIBPULSE

#endif  // TALK_SOUND_PULSEAUDIOSOUNDSYSTEM_H_
