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

#include <string>
#include <vector>

#include "talk/media/base/codec.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/mediacommon.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/devices/devicemanager.h"
#include "webrtc/audio_state.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/sigslotrepeater.h"

#if defined(GOOGLE_CHROME_BUILD) || defined(CHROMIUM_BUILD)
#define DISABLE_MEDIA_ENGINE_FACTORY
#endif

namespace webrtc {
class Call;
}

namespace cricket {

class VideoCapturer;

struct RtpCapabilities {
  std::vector<RtpHeaderExtension> header_extensions;
};

// MediaEngineInterface is an abstraction of a media engine which can be
// subclassed to support different media componentry backends.
// It supports voice and video operations in the same class to facilitate
// proper synchronization between both media types.
class MediaEngineInterface {
 public:
  virtual ~MediaEngineInterface() {}

  // Initialization
  // Starts the engine.
  virtual bool Init(rtc::Thread* worker_thread) = 0;
  // Shuts down the engine.
  virtual void Terminate() = 0;
  // TODO(solenberg): Remove once VoE API refactoring is done.
  virtual rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const = 0;

  // MediaChannel creation
  // Creates a voice media channel. Returns NULL on failure.
  virtual VoiceMediaChannel* CreateChannel(
      webrtc::Call* call,
      const AudioOptions& options) = 0;
  // Creates a video media channel, paired with the specified voice channel.
  // Returns NULL on failure.
  virtual VideoMediaChannel* CreateVideoChannel(
      webrtc::Call* call,
      const VideoOptions& options) = 0;

  // Device configuration
  // Gets the current speaker volume, as a value between 0 and 255.
  virtual bool GetOutputVolume(int* level) = 0;
  // Sets the current speaker volume, as a value between 0 and 255.
  virtual bool SetOutputVolume(int level) = 0;

  // Gets the current microphone level, as a value between 0 and 10.
  virtual int GetInputLevel() = 0;

  virtual const std::vector<AudioCodec>& audio_codecs() = 0;
  virtual RtpCapabilities GetAudioCapabilities() = 0;
  virtual const std::vector<VideoCodec>& video_codecs() = 0;
  virtual RtpCapabilities GetVideoCapabilities() = 0;

  // Starts AEC dump using existing file.
  virtual bool StartAecDump(rtc::PlatformFile file) = 0;

  // Stops recording AEC dump.
  virtual void StopAecDump() = 0;

  // Starts RtcEventLog using existing file.
  virtual bool StartRtcEventLog(rtc::PlatformFile file) = 0;

  // Stops recording an RtcEventLog.
  virtual void StopRtcEventLog() = 0;
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
  virtual ~CompositeMediaEngine() {}
  virtual bool Init(rtc::Thread* worker_thread) {
    if (!voice_.Init(worker_thread))
      return false;
    video_.Init();
    return true;
  }
  virtual void Terminate() {
    voice_.Terminate();
  }

  virtual rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const {
    return voice_.GetAudioState();
  }
  virtual VoiceMediaChannel* CreateChannel(webrtc::Call* call,
                                           const AudioOptions& options) {
    return voice_.CreateChannel(call, options);
  }
  virtual VideoMediaChannel* CreateVideoChannel(webrtc::Call* call,
                                                const VideoOptions& options) {
    return video_.CreateChannel(call, options);
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
  virtual const std::vector<AudioCodec>& audio_codecs() {
    return voice_.codecs();
  }
  virtual RtpCapabilities GetAudioCapabilities() {
    return voice_.GetCapabilities();
  }
  virtual const std::vector<VideoCodec>& video_codecs() {
    return video_.codecs();
  }
  virtual RtpCapabilities GetVideoCapabilities() {
    return video_.GetCapabilities();
  }

  virtual bool StartAecDump(rtc::PlatformFile file) {
    return voice_.StartAecDump(file);
  }

  virtual void StopAecDump() {
    voice_.StopAecDump();
  }

  virtual bool StartRtcEventLog(rtc::PlatformFile file) {
    return voice_.StartRtcEventLog(file);
  }

  virtual void StopRtcEventLog() { voice_.StopRtcEventLog(); }

 protected:
  VOICE voice_;
  VIDEO video_;
};

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
