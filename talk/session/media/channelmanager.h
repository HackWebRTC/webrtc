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

#include "talk/base/criticalsection.h"
#include "talk/base/sigslotrepeater.h"
#include "talk/base/thread.h"
#include "talk/media/base/capturemanager.h"
#include "talk/media/base/mediaengine.h"
#include "talk/p2p/base/session.h"
#include "talk/session/media/voicechannel.h"

namespace cricket {

class Soundclip;
class VideoProcessor;
class VoiceChannel;
class VoiceProcessor;

// ChannelManager allows the MediaEngine to run on a separate thread, and takes
// care of marshalling calls between threads. It also creates and keeps track of
// voice and video channels; by doing so, it can temporarily pause all the
// channels when a new audio or video device is chosen. The voice and video
// channels are stored in separate vectors, to easily allow operations on just
// voice or just video channels.
// ChannelManager also allows the application to discover what devices it has
// using device manager.
class ChannelManager : public talk_base::MessageHandler,
                       public sigslot::has_slots<> {
 public:
#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)
  // Creates the channel manager, and specifies the worker thread to use.
  explicit ChannelManager(talk_base::Thread* worker);
#endif

  // For testing purposes. Allows the media engine and data media
  // engine and dev manager to be mocks.  The ChannelManager takes
  // ownership of these objects.
  ChannelManager(MediaEngineInterface* me,
                 DataEngineInterface* dme,
                 DeviceManagerInterface* dm,
                 CaptureManager* cm,
                 talk_base::Thread* worker);
  // Same as above, but gives an easier default DataEngine.
  ChannelManager(MediaEngineInterface* me,
                 DeviceManagerInterface* dm,
                 talk_base::Thread* worker);
  ~ChannelManager();

  // Accessors for the worker thread, allowing it to be set after construction,
  // but before Init. set_worker_thread will return false if called after Init.
  talk_base::Thread* worker_thread() const { return worker_thread_; }
  bool set_worker_thread(talk_base::Thread* thread) {
    if (initialized_) return false;
    worker_thread_ = thread;
    return true;
  }

  // Gets capabilities. Can be called prior to starting the media engine.
  int GetCapabilities();

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
      BaseSession* session, const std::string& content_name, bool rtcp);
  // Destroys a voice channel created with the Create API.
  void DestroyVoiceChannel(VoiceChannel* voice_channel);
  // Creates a video channel, synced with the specified voice channel, and
  // associated with the specified session.
  VideoChannel* CreateVideoChannel(
      BaseSession* session, const std::string& content_name, bool rtcp,
      VoiceChannel* voice_channel);
  // Destroys a video channel created with the Create API.
  void DestroyVideoChannel(VideoChannel* video_channel);
  DataChannel* CreateDataChannel(
      BaseSession* session, const std::string& content_name,
      bool rtcp, DataChannelType data_channel_type);
  // Destroys a data channel created with the Create API.
  void DestroyDataChannel(DataChannel* data_channel);

  // Creates a soundclip.
  Soundclip* CreateSoundclip();
  // Destroys a soundclip created with the Create API.
  void DestroySoundclip(Soundclip* soundclip);

  // Indicates whether any channels exist.
  bool has_channels() const {
    return (!voice_channels_.empty() || !video_channels_.empty() ||
            !soundclips_.empty());
  }

  // Configures the audio and video devices. A null pointer can be passed to
  // GetAudioOptions() for any parameter of no interest.
  bool GetAudioOptions(std::string* wave_in_device,
                       std::string* wave_out_device, int* opts);
  bool SetAudioOptions(const std::string& wave_in_device,
                       const std::string& wave_out_device, int opts);
  bool GetOutputVolume(int* level);
  bool SetOutputVolume(int level);
  bool IsSameCapturer(const std::string& capturer_name,
                      VideoCapturer* capturer);
  // TODO(noahric): Nearly everything called "device" in this API is actually a
  // device name, so this should really be GetCaptureDeviceName, and the
  // next method should be GetCaptureDevice.
  bool GetCaptureDevice(std::string* cam_device);
  // Gets the current capture Device.
  bool GetVideoCaptureDevice(Device* device);
  // Create capturer based on what has been set in SetCaptureDevice().
  VideoCapturer* CreateVideoCapturer();
  bool SetCaptureDevice(const std::string& cam_device);
  bool SetDefaultVideoEncoderConfig(const VideoEncoderConfig& config);
  // RTX will be enabled/disabled in engines that support it. The supporting
  // engines will start offering an RTX codec. Must be called before Init().
  bool SetVideoRtxEnabled(bool enable);

  // Starts/stops the local microphone and enables polling of the input level.
  bool SetLocalMonitor(bool enable);
  bool monitoring() const { return monitoring_; }
  // Sets the local renderer where to renderer the local camera.
  bool SetLocalRenderer(VideoRenderer* renderer);
  bool capturing() const { return capturing_; }

  // Configures the logging output of the mediaengine(s).
  void SetVoiceLogging(int level, const char* filter);
  void SetVideoLogging(int level, const char* filter);

  // The channel manager handles the Tx side for Video processing,
  // as well as Tx and Rx side for Voice processing.
  // (The Rx Video processing will go throug the simplerenderingmanager,
  //  to be implemented).
  bool RegisterVideoProcessor(VideoCapturer* capturer,
                              VideoProcessor* processor);
  bool UnregisterVideoProcessor(VideoCapturer* capturer,
                                VideoProcessor* processor);
  bool RegisterVoiceProcessor(uint32 ssrc,
                              VoiceProcessor* processor,
                              MediaProcessorDirection direction);
  bool UnregisterVoiceProcessor(uint32 ssrc,
                                VoiceProcessor* processor,
                                MediaProcessorDirection direction);
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

  bool GetAudioInputDevices(std::vector<std::string>* names);
  bool GetAudioOutputDevices(std::vector<std::string>* names);
  bool GetVideoCaptureDevices(std::vector<std::string>* names);
  void SetVideoCaptureDeviceMaxFormat(const std::string& usb_id,
                                      const VideoFormat& max_format);

  sigslot::repeater0<> SignalDevicesChange;
  sigslot::signal2<VideoCapturer*, CaptureState> SignalVideoCaptureStateChange;

  // Returns the current selected device. Note: Subtly different from
  // GetCaptureDevice(). See member video_device_ for more details.
  // This API is mainly a hook used by unittests.
  const std::string& video_device_name() const { return video_device_name_; }

  // TODO(hellner): Remove this function once the engine capturer has been
  // removed.
  VideoFormat GetStartCaptureFormat();
 protected:
  // Adds non-transient parameters which can only be changed through the
  // options store.
  bool SetAudioOptions(const std::string& wave_in_device,
                       const std::string& wave_out_device, int opts,
                       int delay_offset);
  int audio_delay_offset() const { return audio_delay_offset_; }

 private:
  typedef std::vector<VoiceChannel*> VoiceChannels;
  typedef std::vector<VideoChannel*> VideoChannels;
  typedef std::vector<DataChannel*> DataChannels;
  typedef std::vector<Soundclip*> Soundclips;

  void Construct(MediaEngineInterface* me,
                 DataEngineInterface* dme,
                 DeviceManagerInterface* dm,
                 CaptureManager* cm,
                 talk_base::Thread* worker_thread);
  void Terminate_w();
  VoiceChannel* CreateVoiceChannel_w(
      BaseSession* session, const std::string& content_name, bool rtcp);
  void DestroyVoiceChannel_w(VoiceChannel* voice_channel);
  VideoChannel* CreateVideoChannel_w(
      BaseSession* session, const std::string& content_name, bool rtcp,
      VoiceChannel* voice_channel);
  void DestroyVideoChannel_w(VideoChannel* video_channel);
  DataChannel* CreateDataChannel_w(
      BaseSession* session, const std::string& content_name,
      bool rtcp, DataChannelType data_channel_type);
  void DestroyDataChannel_w(DataChannel* data_channel);
  Soundclip* CreateSoundclip_w();
  void DestroySoundclip_w(Soundclip* soundclip);
  bool SetAudioOptions_w(int opts, int delay_offset, const Device* in_dev,
                         const Device* out_dev);
  bool SetCaptureDevice_w(const Device* cam_device);
  void OnVideoCaptureStateChange(VideoCapturer* capturer,
                                 CaptureState result);
  bool RegisterVideoProcessor_w(VideoCapturer* capturer,
                                VideoProcessor* processor);
  bool UnregisterVideoProcessor_w(VideoCapturer* capturer,
                                  VideoProcessor* processor);
  bool IsScreencastRunning_w() const;
  virtual void OnMessage(talk_base::Message *message);

  talk_base::scoped_ptr<MediaEngineInterface> media_engine_;
  talk_base::scoped_ptr<DataEngineInterface> data_media_engine_;
  talk_base::scoped_ptr<DeviceManagerInterface> device_manager_;
  talk_base::scoped_ptr<CaptureManager> capture_manager_;
  bool initialized_;
  talk_base::Thread* main_thread_;
  talk_base::Thread* worker_thread_;

  VoiceChannels voice_channels_;
  VideoChannels video_channels_;
  DataChannels data_channels_;
  Soundclips soundclips_;

  std::string audio_in_device_;
  std::string audio_out_device_;
  int audio_options_;
  int audio_delay_offset_;
  int audio_output_volume_;
  std::string camera_device_;
  VideoEncoderConfig default_video_encoder_config_;
  VideoRenderer* local_renderer_;
  bool enable_rtx_;

  bool capturing_;
  bool monitoring_;

  // String containing currently set device. Note that this string is subtly
  // different from camera_device_. E.g. camera_device_ will list unplugged
  // but selected devices while this sting will be empty or contain current
  // selected device.
  // TODO(hellner): refactor the code such that there is no need to keep two
  // strings for video devices that have subtle differences in behavior.
  std::string video_device_name_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_CHANNELMANAGER_H_
