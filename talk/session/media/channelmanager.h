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

#ifndef TALK_SESSION_MEDIA_CHANNELMANAGER_H_
#define TALK_SESSION_MEDIA_CHANNELMANAGER_H_

#include <string>
#include <vector>

#include "talk/media/base/capturemanager.h"
#include "talk/media/base/mediaengine.h"
#include "talk/session/media/voicechannel.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/sigslotrepeater.h"
#include "webrtc/base/thread.h"

namespace webrtc {
class MediaControllerInterface;
}
namespace cricket {

class VoiceChannel;

// ChannelManager allows the MediaEngine to run on a separate thread, and takes
// care of marshalling calls between threads. It also creates and keeps track of
// voice and video channels; by doing so, it can temporarily pause all the
// channels when a new audio or video device is chosen. The voice and video
// channels are stored in separate vectors, to easily allow operations on just
// voice or just video channels.
// ChannelManager also allows the application to discover what devices it has
// using device manager.
class ChannelManager : public rtc::MessageHandler,
                       public sigslot::has_slots<> {
 public:
  // For testing purposes. Allows the media engine and data media
  // engine and dev manager to be mocks.  The ChannelManager takes
  // ownership of these objects.
  ChannelManager(MediaEngineInterface* me,
                 DataEngineInterface* dme,
                 CaptureManager* cm,
                 rtc::Thread* worker);
  // Same as above, but gives an easier default DataEngine.
  ChannelManager(MediaEngineInterface* me,
                 rtc::Thread* worker);
  ~ChannelManager();

  // Accessors for the worker thread, allowing it to be set after construction,
  // but before Init. set_worker_thread will return false if called after Init.
  rtc::Thread* worker_thread() const { return worker_thread_; }
  bool set_worker_thread(rtc::Thread* thread) {
    if (initialized_) return false;
    worker_thread_ = thread;
    return true;
  }

  MediaEngineInterface* media_engine() { return media_engine_.get(); }

  // Retrieves the list of supported audio & video codec types.
  // Can be called before starting the media engine.
  void GetSupportedAudioCodecs(std::vector<AudioCodec>* codecs) const;
  void GetSupportedAudioRtpHeaderExtensions(RtpHeaderExtensions* ext) const;
  void GetSupportedVideoCodecs(std::vector<VideoCodec>* codecs) const;
  void GetSupportedVideoRtpHeaderExtensions(RtpHeaderExtensions* ext) const;
  void GetSupportedDataCodecs(std::vector<DataCodec>* codecs) const;

  // Indicates whether the media engine is started.
  bool initialized() const { return initialized_; }
  // Starts up the media engine.
  bool Init();
  // Shuts down the media engine.
  void Terminate();

  // The operations below all occur on the worker thread.
  // Creates a voice channel, to be associated with the specified session.
  VoiceChannel* CreateVoiceChannel(
      webrtc::MediaControllerInterface* media_controller,
      TransportController* transport_controller,
      const std::string& content_name,
      bool rtcp,
      const AudioOptions& options);
  // Destroys a voice channel created with the Create API.
  void DestroyVoiceChannel(VoiceChannel* voice_channel);
  // Creates a video channel, synced with the specified voice channel, and
  // associated with the specified session.
  VideoChannel* CreateVideoChannel(
      webrtc::MediaControllerInterface* media_controller,
      TransportController* transport_controller,
      const std::string& content_name,
      bool rtcp,
      const VideoOptions& options);
  // Destroys a video channel created with the Create API.
  void DestroyVideoChannel(VideoChannel* video_channel);
  DataChannel* CreateDataChannel(TransportController* transport_controller,
                                 const std::string& content_name,
                                 bool rtcp,
                                 DataChannelType data_channel_type);
  // Destroys a data channel created with the Create API.
  void DestroyDataChannel(DataChannel* data_channel);

  // Indicates whether any channels exist.
  bool has_channels() const {
    return (!voice_channels_.empty() || !video_channels_.empty());
  }

  bool GetOutputVolume(int* level);
  bool SetOutputVolume(int level);
  bool SetDefaultVideoEncoderConfig(const VideoEncoderConfig& config);
  // RTX will be enabled/disabled in engines that support it. The supporting
  // engines will start offering an RTX codec. Must be called before Init().
  bool SetVideoRtxEnabled(bool enable);

  // Starts/stops the local microphone and enables polling of the input level.
  bool capturing() const { return capturing_; }

  // Configures the logging output of the mediaengine(s).
  void SetVoiceLogging(int level, const char* filter);
  void SetVideoLogging(int level, const char* filter);

  // Gets capturer's supported formats in a thread safe manner
  std::vector<cricket::VideoFormat> GetSupportedFormats(
      VideoCapturer* capturer) const;
  // The following are done in the new "CaptureManager" style that
  // all local video capturers, processors, and managers should move to.
  // TODO(pthatcher): Make methods nicer by having start return a handle that
  // can be used for stop and restart, rather than needing to pass around
  // formats a a pseudo-handle.
  bool StartVideoCapture(VideoCapturer* video_capturer,
                         const VideoFormat& video_format);
  // When muting, produce black frames then pause the camera.
  // When unmuting, start the camera. Camera starts unmuted.
  bool MuteToBlackThenPause(VideoCapturer* video_capturer, bool muted);
  bool StopVideoCapture(VideoCapturer* video_capturer,
                        const VideoFormat& video_format);
  bool RestartVideoCapture(VideoCapturer* video_capturer,
                           const VideoFormat& previous_format,
                           const VideoFormat& desired_format,
                           CaptureManager::RestartOptions options);

  bool AddVideoRenderer(VideoCapturer* capturer, VideoRenderer* renderer);
  bool RemoveVideoRenderer(VideoCapturer* capturer, VideoRenderer* renderer);
  bool IsScreencastRunning() const;

  // The operations below occur on the main thread.

  // Starts AEC dump using existing file.
  bool StartAecDump(rtc::PlatformFile file);

  // Stops recording AEC dump.
  void StopAecDump();

  // Starts RtcEventLog using existing file.
  bool StartRtcEventLog(rtc::PlatformFile file);

  // Stops logging RtcEventLog.
  void StopRtcEventLog();

  sigslot::signal2<VideoCapturer*, CaptureState> SignalVideoCaptureStateChange;

 protected:
  // Adds non-transient parameters which can only be changed through the
  // options store.
  bool SetAudioOptions(const AudioOptions& options);

 private:
  typedef std::vector<VoiceChannel*> VoiceChannels;
  typedef std::vector<VideoChannel*> VideoChannels;
  typedef std::vector<DataChannel*> DataChannels;

  void Construct(MediaEngineInterface* me,
                 DataEngineInterface* dme,
                 CaptureManager* cm,
                 rtc::Thread* worker_thread);
  bool InitMediaEngine_w();
  void DestructorDeletes_w();
  void Terminate_w();
  VoiceChannel* CreateVoiceChannel_w(
      webrtc::MediaControllerInterface* media_controller,
      TransportController* transport_controller,
      const std::string& content_name,
      bool rtcp,
      const AudioOptions& options);
  void DestroyVoiceChannel_w(VoiceChannel* voice_channel);
  VideoChannel* CreateVideoChannel_w(
      webrtc::MediaControllerInterface* media_controller,
      TransportController* transport_controller,
      const std::string& content_name,
      bool rtcp,
      const VideoOptions& options);
  void DestroyVideoChannel_w(VideoChannel* video_channel);
  DataChannel* CreateDataChannel_w(TransportController* transport_controller,
                                   const std::string& content_name,
                                   bool rtcp,
                                   DataChannelType data_channel_type);
  void DestroyDataChannel_w(DataChannel* data_channel);
  bool SetAudioOptions_w(const AudioOptions& options,
                         const Device* in_dev, const Device* out_dev);
  void OnVideoCaptureStateChange(VideoCapturer* capturer,
                                 CaptureState result);
  void GetSupportedFormats_w(
      VideoCapturer* capturer,
      std::vector<cricket::VideoFormat>* out_formats) const;
  bool IsScreencastRunning_w() const;
  virtual void OnMessage(rtc::Message *message);

  rtc::scoped_ptr<MediaEngineInterface> media_engine_;
  rtc::scoped_ptr<DataEngineInterface> data_media_engine_;
  rtc::scoped_ptr<CaptureManager> capture_manager_;
  bool initialized_;
  rtc::Thread* main_thread_;
  rtc::Thread* worker_thread_;

  VoiceChannels voice_channels_;
  VideoChannels video_channels_;
  DataChannels data_channels_;

  AudioOptions audio_options_;
  int audio_output_volume_;
  VideoEncoderConfig default_video_encoder_config_;
  VideoRenderer* local_renderer_;
  bool enable_rtx_;

  bool capturing_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_CHANNELMANAGER_H_
