/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_BASE_MEDIAENGINE_H_
#define TALK_MEDIA_BASE_MEDIAENGINE_H_

#ifdef OSX
#include <CoreAudio/CoreAudio.h>
#endif

#include <climits>
#include <string>
#include <vector>

#include "talk/base/fileutils.h"
#include "talk/base/sigslotrepeater.h"
#include "talk/media/base/codec.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/mediacommon.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/base/videoprocessor.h"
#include "talk/media/base/voiceprocessor.h"
#include "talk/media/devices/devicemanager.h"

#if defined(GOOGLE_CHROME_BUILD) || defined(CHROMIUM_BUILD)
#define DISABLE_MEDIA_ENGINE_FACTORY
#endif

namespace cricket {

class VideoCapturer;

// MediaEngineInterface is an abstraction of a media engine which can be
// subclassed to support different media componentry backends.
// It supports voice and video operations in the same class to facilitate
// proper synchronization between both media types.
class MediaEngineInterface {
 public:
  // Default value to be used for SetAudioDelayOffset().
  static const int kDefaultAudioDelayOffset;

  virtual ~MediaEngineInterface() {}

  // Initialization
  // Starts the engine.
  virtual bool Init(talk_base::Thread* worker_thread) = 0;
  // Shuts down the engine.
  virtual void Terminate() = 0;
  // Returns what the engine is capable of, as a set of Capabilities, above.
  virtual int GetCapabilities() = 0;

  // MediaChannel creation
  // Creates a voice media channel. Returns NULL on failure.
  virtual VoiceMediaChannel *CreateChannel() = 0;
  // Creates a video media channel, paired with the specified voice channel.
  // Returns NULL on failure.
  virtual VideoMediaChannel *CreateVideoChannel(
      VoiceMediaChannel* voice_media_channel) = 0;

  // Creates a soundclip object for playing sounds on. Returns NULL on failure.
  virtual SoundclipMedia *CreateSoundclip() = 0;

  // Configuration
  // Gets global audio options.
  virtual AudioOptions GetAudioOptions() const = 0;
  // Sets global audio options. "options" are from AudioOptions, above.
  virtual bool SetAudioOptions(const AudioOptions& options) = 0;
  // Sets global video options. "options" are from VideoOptions, above.
  virtual bool SetVideoOptions(const VideoOptions& options) = 0;
  // Sets the value used by the echo canceller to offset delay values obtained
  // from the OS.
  virtual bool SetAudioDelayOffset(int offset) = 0;
  // Sets the default (maximum) codec/resolution and encoder option to capture
  // and encode video.
  virtual bool SetDefaultVideoEncoderConfig(const VideoEncoderConfig& config)
      = 0;
  // Gets the default (maximum) codec/resolution and encoder option used to
  // capture and encode video, as set by SetDefaultVideoEncoderConfig or the
  // default from the video engine if not previously set.
  virtual VideoEncoderConfig GetDefaultVideoEncoderConfig() const = 0;

  // Device selection
  // TODO(tschmelcher): Add method for selecting the soundclip device.
  virtual bool SetSoundDevices(const Device* in_device,
                               const Device* out_device) = 0;

  // Device configuration
  // Gets the current speaker volume, as a value between 0 and 255.
  virtual bool GetOutputVolume(int* level) = 0;
  // Sets the current speaker volume, as a value between 0 and 255.
  virtual bool SetOutputVolume(int level) = 0;

  // Local monitoring
  // Gets the current microphone level, as a value between 0 and 10.
  virtual int GetInputLevel() = 0;
  // Starts or stops the local microphone. Useful if local mic info is needed
  // prior to a call being connected; the mic will be started automatically
  // when a VoiceMediaChannel starts sending.
  virtual bool SetLocalMonitor(bool enable) = 0;
  // Installs a callback for raw frames from the local camera.
  virtual bool SetLocalRenderer(VideoRenderer* renderer) = 0;

  virtual const std::vector<AudioCodec>& audio_codecs() = 0;
  virtual const std::vector<RtpHeaderExtension>&
      audio_rtp_header_extensions() = 0;
  virtual const std::vector<VideoCodec>& video_codecs() = 0;
  virtual const std::vector<RtpHeaderExtension>&
      video_rtp_header_extensions() = 0;

  // Logging control
  virtual void SetVoiceLogging(int min_sev, const char* filter) = 0;
  virtual void SetVideoLogging(int min_sev, const char* filter) = 0;

  // Starts AEC dump using existing file.
  virtual bool StartAecDump(talk_base::PlatformFile file) = 0;

  // Voice processors for effects.
  virtual bool RegisterVoiceProcessor(uint32 ssrc,
                                      VoiceProcessor* video_processor,
                                      MediaProcessorDirection direction) = 0;
  virtual bool UnregisterVoiceProcessor(uint32 ssrc,
                                        VoiceProcessor* video_processor,
                                        MediaProcessorDirection direction) = 0;

  virtual VideoFormat GetStartCaptureFormat() const = 0;

  virtual sigslot::repeater2<VideoCapturer*, CaptureState>&
      SignalVideoCaptureStateChange() = 0;
};


#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)
class MediaEngineFactory {
 public:
  typedef cricket::MediaEngineInterface* (*MediaEngineCreateFunction)();
  // Creates a media engine, using either the compiled system default or the
  // creation function specified in SetCreateFunction, if specified.
  static MediaEngineInterface* Create();
  // Sets the function used when calling Create. If unset, the compiled system
  // default will be used. Returns the old create function, or NULL if one
  // wasn't set. Likewise, NULL can be used as the |function| parameter to
  // reset to the default behavior.
  static MediaEngineCreateFunction SetCreateFunction(
      MediaEngineCreateFunction function);
 private:
  static MediaEngineCreateFunction create_function_;
};
#endif

// CompositeMediaEngine constructs a MediaEngine from separate
// voice and video engine classes.
template<class VOICE, class VIDEO>
class CompositeMediaEngine : public MediaEngineInterface {
 public:
  CompositeMediaEngine() {}
  virtual ~CompositeMediaEngine() {}
  virtual bool Init(talk_base::Thread* worker_thread) {
    if (!voice_.Init(worker_thread))
      return false;
    if (!video_.Init(worker_thread)) {
      voice_.Terminate();
      return false;
    }
    SignalVideoCaptureStateChange().repeat(video_.SignalCaptureStateChange);
    return true;
  }
  virtual void Terminate() {
    video_.Terminate();
    voice_.Terminate();
  }

  virtual int GetCapabilities() {
    return (voice_.GetCapabilities() | video_.GetCapabilities());
  }
  virtual VoiceMediaChannel *CreateChannel() {
    return voice_.CreateChannel();
  }
  virtual VideoMediaChannel *CreateVideoChannel(VoiceMediaChannel* channel) {
    return video_.CreateChannel(channel);
  }
  virtual SoundclipMedia *CreateSoundclip() {
    return voice_.CreateSoundclip();
  }

  virtual AudioOptions GetAudioOptions() const {
    return voice_.GetOptions();
  }
  virtual bool SetAudioOptions(const AudioOptions& options) {
    return voice_.SetOptions(options);
  }
  virtual bool SetVideoOptions(const VideoOptions& options) {
    return video_.SetOptions(options);
  }
  virtual bool SetAudioDelayOffset(int offset) {
    return voice_.SetDelayOffset(offset);
  }
  virtual bool SetDefaultVideoEncoderConfig(const VideoEncoderConfig& config) {
    return video_.SetDefaultEncoderConfig(config);
  }
  virtual VideoEncoderConfig GetDefaultVideoEncoderConfig() const {
    return video_.GetDefaultEncoderConfig();
  }

  virtual bool SetSoundDevices(const Device* in_device,
                               const Device* out_device) {
    return voice_.SetDevices(in_device, out_device);
  }

  virtual bool GetOutputVolume(int* level) {
    return voice_.GetOutputVolume(level);
  }
  virtual bool SetOutputVolume(int level) {
    return voice_.SetOutputVolume(level);
  }

  virtual int GetInputLevel() {
    return voice_.GetInputLevel();
  }
  virtual bool SetLocalMonitor(bool enable) {
    return voice_.SetLocalMonitor(enable);
  }
  virtual bool SetLocalRenderer(VideoRenderer* renderer) {
    return video_.SetLocalRenderer(renderer);
  }

  virtual const std::vector<AudioCodec>& audio_codecs() {
    return voice_.codecs();
  }
  virtual const std::vector<RtpHeaderExtension>& audio_rtp_header_extensions() {
    return voice_.rtp_header_extensions();
  }
  virtual const std::vector<VideoCodec>& video_codecs() {
    return video_.codecs();
  }
  virtual const std::vector<RtpHeaderExtension>& video_rtp_header_extensions() {
    return video_.rtp_header_extensions();
  }

  virtual void SetVoiceLogging(int min_sev, const char* filter) {
    voice_.SetLogging(min_sev, filter);
  }
  virtual void SetVideoLogging(int min_sev, const char* filter) {
    video_.SetLogging(min_sev, filter);
  }

  virtual bool StartAecDump(talk_base::PlatformFile file) {
    return voice_.StartAecDump(file);
  }

  virtual bool RegisterVoiceProcessor(uint32 ssrc,
                                      VoiceProcessor* processor,
                                      MediaProcessorDirection direction) {
    return voice_.RegisterProcessor(ssrc, processor, direction);
  }
  virtual bool UnregisterVoiceProcessor(uint32 ssrc,
                                        VoiceProcessor* processor,
                                        MediaProcessorDirection direction) {
    return voice_.UnregisterProcessor(ssrc, processor, direction);
  }
  virtual VideoFormat GetStartCaptureFormat() const {
    return video_.GetStartCaptureFormat();
  }
  virtual sigslot::repeater2<VideoCapturer*, CaptureState>&
      SignalVideoCaptureStateChange() {
    return signal_state_change_;
  }

 protected:
  VOICE voice_;
  VIDEO video_;
  sigslot::repeater2<VideoCapturer*, CaptureState> signal_state_change_;
};

// NullVoiceEngine can be used with CompositeMediaEngine in the case where only
// a video engine is desired.
class NullVoiceEngine {
 public:
  bool Init(talk_base::Thread* worker_thread) { return true; }
  void Terminate() {}
  int GetCapabilities() { return 0; }
  // If you need this to return an actual channel, use FakeMediaEngine instead.
  VoiceMediaChannel* CreateChannel() {
    return NULL;
  }
  SoundclipMedia* CreateSoundclip() {
    return NULL;
  }
  bool SetDelayOffset(int offset) { return true; }
  AudioOptions GetOptions() const { return AudioOptions(); }
  bool SetOptions(const AudioOptions& options) { return true; }
  bool SetDevices(const Device* in_device, const Device* out_device) {
    return true;
  }
  bool GetOutputVolume(int* level) {
    *level = 0;
    return true;
  }
  bool SetOutputVolume(int level) { return true; }
  int GetInputLevel() { return 0; }
  bool SetLocalMonitor(bool enable) { return true; }
  const std::vector<AudioCodec>& codecs() { return codecs_; }
  const std::vector<RtpHeaderExtension>& rtp_header_extensions() {
    return rtp_header_extensions_;
  }
  void SetLogging(int min_sev, const char* filter) {}
  bool StartAecDump(talk_base::PlatformFile file) { return false; }
  bool RegisterProcessor(uint32 ssrc,
                         VoiceProcessor* voice_processor,
                         MediaProcessorDirection direction) { return true; }
  bool UnregisterProcessor(uint32 ssrc,
                           VoiceProcessor* voice_processor,
                           MediaProcessorDirection direction) { return true; }

 private:
  std::vector<AudioCodec> codecs_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
};

// NullVideoEngine can be used with CompositeMediaEngine in the case where only
// a voice engine is desired.
class NullVideoEngine {
 public:
  bool Init(talk_base::Thread* worker_thread) { return true; }
  void Terminate() {}
  int GetCapabilities() { return 0; }
  // If you need this to return an actual channel, use FakeMediaEngine instead.
  VideoMediaChannel* CreateChannel(
      VoiceMediaChannel* voice_media_channel) {
    return NULL;
  }
  bool SetOptions(const VideoOptions& options) { return true; }
  VideoEncoderConfig GetDefaultEncoderConfig() const {
    return VideoEncoderConfig();
  }
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config) {
    return true;
  }
  bool SetLocalRenderer(VideoRenderer* renderer) { return true; }
  const std::vector<VideoCodec>& codecs() { return codecs_; }
  const std::vector<RtpHeaderExtension>& rtp_header_extensions() {
    return rtp_header_extensions_;
  }
  void SetLogging(int min_sev, const char* filter) {}
  VideoFormat GetStartCaptureFormat() const { return VideoFormat(); }

  sigslot::signal2<VideoCapturer*, CaptureState> SignalCaptureStateChange;
 private:
  std::vector<VideoCodec> codecs_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
};

typedef CompositeMediaEngine<NullVoiceEngine, NullVideoEngine> NullMediaEngine;

enum DataChannelType {
  DCT_NONE = 0,
  DCT_RTP = 1,
  DCT_SCTP = 2
};

class DataEngineInterface {
 public:
  virtual ~DataEngineInterface() {}
  virtual DataMediaChannel* CreateChannel(DataChannelType type) = 0;
  virtual const std::vector<DataCodec>& data_codecs() = 0;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_MEDIAENGINE_H_
