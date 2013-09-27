// libjingle
// Copyright 2004 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef TALK_MEDIA_BASE_FILEMEDIAENGINE_H_
#define TALK_MEDIA_BASE_FILEMEDIAENGINE_H_

#include <string>
#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/base/stream.h"
#include "talk/media/base/codec.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/mediaengine.h"

namespace talk_base {
class StreamInterface;
}

namespace cricket {

// A media engine contains a capturer, an encoder, and a sender in the sender
// side and a receiver, a decoder, and a renderer in the receiver side.
// FileMediaEngine simulates the capturer and the encoder via an input RTP dump
// stream and simulates the decoder and the renderer via an output RTP dump
// stream. Depending on the parameters of the constructor, FileMediaEngine can
// act as file voice engine, file video engine, or both. Currently, we use
// only the RTP dump packets. TODO(whyuan): Enable RTCP packets.
class FileMediaEngine : public MediaEngineInterface {
 public:
  FileMediaEngine() {}
  virtual ~FileMediaEngine() {}

  // Set the file name of the input or output RTP dump for voice or video.
  // Should be called before the channel is created.
  void set_voice_input_filename(const std::string& filename) {
    voice_input_filename_ = filename;
  }
  void set_voice_output_filename(const std::string& filename) {
    voice_output_filename_ = filename;
  }
  void set_video_input_filename(const std::string& filename) {
    video_input_filename_ = filename;
  }
  void set_video_output_filename(const std::string& filename) {
    video_output_filename_ = filename;
  }

  // Should be called before codecs() and video_codecs() are called. We need to
  // set the voice and video codecs; otherwise, Jingle initiation will fail.
  void set_voice_codecs(const std::vector<AudioCodec>& codecs) {
    voice_codecs_ = codecs;
  }
  void set_video_codecs(const std::vector<VideoCodec>& codecs) {
    video_codecs_ = codecs;
  }

  // Implement pure virtual methods of MediaEngine.
  virtual bool Init(talk_base::Thread* worker_thread) {
    return true;
  }
  virtual void Terminate() {}
  virtual int GetCapabilities();
  virtual VoiceMediaChannel* CreateChannel();
  virtual VideoMediaChannel* CreateVideoChannel(VoiceMediaChannel* voice_ch);
  virtual SoundclipMedia* CreateSoundclip() { return NULL; }
  virtual AudioOptions GetAudioOptions() const { return AudioOptions(); }
  virtual bool SetAudioOptions(const AudioOptions& options) { return true; }
  virtual bool SetVideoOptions(const VideoOptions& options) { return true; }
  virtual bool SetAudioDelayOffset(int offset) { return true; }
  virtual bool SetDefaultVideoEncoderConfig(const VideoEncoderConfig& config) {
    return true;
  }
  virtual bool SetSoundDevices(const Device* in_dev, const Device* out_dev) {
    return true;
  }
  virtual bool SetVideoCaptureDevice(const Device* cam_device) { return true; }
  virtual bool SetVideoCapturer(VideoCapturer* /*capturer*/) {
    return true;
  }
  virtual VideoCapturer* GetVideoCapturer() const {
    return NULL;
  }
  virtual bool GetOutputVolume(int* level) {
    *level = 0;
    return true;
  }
  virtual bool SetOutputVolume(int level) { return true; }
  virtual int GetInputLevel() { return 0; }
  virtual bool SetLocalMonitor(bool enable) { return true; }
  virtual bool SetLocalRenderer(VideoRenderer* renderer) { return true; }
  // TODO(whyuan): control channel send?
  virtual bool SetVideoCapture(bool capture) { return true; }
  virtual const std::vector<AudioCodec>& audio_codecs() {
    return voice_codecs_;
  }
  virtual const std::vector<VideoCodec>& video_codecs() {
    return video_codecs_;
  }
  virtual const std::vector<RtpHeaderExtension>& audio_rtp_header_extensions() {
    return audio_rtp_header_extensions_;
  }
  virtual const std::vector<RtpHeaderExtension>& video_rtp_header_extensions() {
    return video_rtp_header_extensions_;
  }

  virtual bool FindAudioCodec(const AudioCodec& codec) { return true; }
  virtual bool FindVideoCodec(const VideoCodec& codec) { return true; }
  virtual void SetVoiceLogging(int min_sev, const char* filter) {}
  virtual void SetVideoLogging(int min_sev, const char* filter) {}

  virtual bool RegisterVideoProcessor(VideoProcessor* processor) {
    return true;
  }
  virtual bool UnregisterVideoProcessor(VideoProcessor* processor) {
    return true;
  }
  virtual bool RegisterVoiceProcessor(uint32 ssrc,
                                      VoiceProcessor* processor,
                                      MediaProcessorDirection direction) {
    return true;
  }
  virtual bool UnregisterVoiceProcessor(uint32 ssrc,
                                        VoiceProcessor* processor,
                                        MediaProcessorDirection direction) {
    return true;
  }
  VideoFormat GetStartCaptureFormat() const {
    return VideoFormat();
  }

  virtual sigslot::repeater2<VideoCapturer*, CaptureState>&
      SignalVideoCaptureStateChange() {
    return signal_state_change_;
  }

 private:
  std::string voice_input_filename_;
  std::string voice_output_filename_;
  std::string video_input_filename_;
  std::string video_output_filename_;
  std::vector<AudioCodec> voice_codecs_;
  std::vector<VideoCodec> video_codecs_;
  std::vector<RtpHeaderExtension> audio_rtp_header_extensions_;
  std::vector<RtpHeaderExtension> video_rtp_header_extensions_;
  sigslot::repeater2<VideoCapturer*, CaptureState>
     signal_state_change_;

  DISALLOW_COPY_AND_ASSIGN(FileMediaEngine);
};

class RtpSenderReceiver;  // Forward declaration. Defined in the .cc file.

class FileVoiceChannel : public VoiceMediaChannel {
 public:
  FileVoiceChannel(talk_base::StreamInterface* input_file_stream,
      talk_base::StreamInterface* output_file_stream);
  virtual ~FileVoiceChannel();

  // Implement pure virtual methods of VoiceMediaChannel.
  virtual bool SetRecvCodecs(const std::vector<AudioCodec>& codecs) {
    return true;
  }
  virtual bool SetSendCodecs(const std::vector<AudioCodec>& codecs);
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    return true;
  }
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    return true;
  }
  virtual bool SetPlayout(bool playout) { return true; }
  virtual bool SetSend(SendFlags flag);
  virtual bool SetRemoteRenderer(uint32 ssrc, AudioRenderer* renderer) {
    return false;
  }
  virtual bool SetLocalRenderer(uint32 ssrc, AudioRenderer* renderer) {
    return false;
  }
  virtual bool GetActiveStreams(AudioInfo::StreamList* actives) { return true; }
  virtual int GetOutputLevel() { return 0; }
  virtual int GetTimeSinceLastTyping() { return -1; }
  virtual void SetTypingDetectionParameters(int time_window,
    int cost_per_typing, int reporting_threshold, int penalty_decay,
    int type_event_delay) {}

  virtual bool SetOutputScaling(uint32 ssrc, double left, double right) {
    return false;
  }
  virtual bool GetOutputScaling(uint32 ssrc, double* left, double* right) {
    return false;
  }
  virtual bool SetRingbackTone(const char* buf, int len) { return true; }
  virtual bool PlayRingbackTone(uint32 ssrc, bool play, bool loop) {
    return true;
  }
  virtual bool InsertDtmf(uint32 ssrc, int event, int duration, int flags) {
    return false;
  }
  virtual bool GetStats(VoiceMediaInfo* info) { return true; }

  // Implement pure virtual methods of MediaChannel.
  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet) {}
  virtual void OnReadyToSend(bool ready) {}
  virtual bool AddSendStream(const StreamParams& sp);
  virtual bool RemoveSendStream(uint32 ssrc);
  virtual bool AddRecvStream(const StreamParams& sp) { return true; }
  virtual bool RemoveRecvStream(uint32 ssrc) { return true; }
  virtual bool MuteStream(uint32 ssrc, bool on) { return false; }
  virtual bool SetSendBandwidth(bool autobw, int bps) { return true; }
  virtual bool SetOptions(const AudioOptions& options) {
    options_ = options;
    return true;
  }
  virtual bool GetOptions(AudioOptions* options) const {
    *options = options_;
    return true;
  }

 private:
  uint32 send_ssrc_;
  talk_base::scoped_ptr<RtpSenderReceiver> rtp_sender_receiver_;
  AudioOptions options_;

  DISALLOW_COPY_AND_ASSIGN(FileVoiceChannel);
};

class FileVideoChannel : public VideoMediaChannel {
 public:
  FileVideoChannel(talk_base::StreamInterface* input_file_stream,
      talk_base::StreamInterface* output_file_stream);
  virtual ~FileVideoChannel();

  // Implement pure virtual methods of VideoMediaChannel.
  virtual bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) {
    return true;
  }
  virtual bool SetSendCodecs(const std::vector<VideoCodec>& codecs);
  virtual bool GetSendCodec(VideoCodec* send_codec) {
    *send_codec = VideoCodec();
    return true;
  }
  virtual bool SetSendStreamFormat(uint32 ssrc, const VideoFormat& format) {
    return true;
  }
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    return true;
  }
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    return true;
  }
  virtual bool SetRender(bool render) { return true; }
  virtual bool SetSend(bool send);
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* renderer) {
    return true;
  }
  virtual bool SetCapturer(uint32 ssrc, VideoCapturer* capturer) {
    return false;
  }
  virtual bool GetStats(VideoMediaInfo* info) { return true; }
  virtual bool SendIntraFrame() { return false; }
  virtual bool RequestIntraFrame() { return false; }

  // Implement pure virtual methods of MediaChannel.
  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet) {}
  virtual void OnReadyToSend(bool ready) {}
  virtual bool AddSendStream(const StreamParams& sp);
  virtual bool RemoveSendStream(uint32 ssrc);
  virtual bool AddRecvStream(const StreamParams& sp) { return true; }
  virtual bool RemoveRecvStream(uint32 ssrc) { return true; }
  virtual bool MuteStream(uint32 ssrc, bool on) { return false; }
  virtual bool SetSendBandwidth(bool autobw, int bps) { return true; }
  virtual bool SetOptions(const VideoOptions& options) {
    options_ = options;
    return true;
  }
  virtual bool GetOptions(VideoOptions* options) const {
    *options = options_;
    return true;
  }
  virtual void UpdateAspectRatio(int ratio_w, int ratio_h) {}

 private:
  uint32 send_ssrc_;
  talk_base::scoped_ptr<RtpSenderReceiver> rtp_sender_receiver_;
  VideoOptions options_;

  DISALLOW_COPY_AND_ASSIGN(FileVideoChannel);
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_FILEMEDIAENGINE_H_
