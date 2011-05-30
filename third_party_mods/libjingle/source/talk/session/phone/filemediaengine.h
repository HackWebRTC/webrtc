// libjingle
// Copyright 2004--2005, Google Inc.
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

#ifndef TALK_SESSION_PHONE_FILEMEDIAENGINE_H_
#define TALK_SESSION_PHONE_FILEMEDIAENGINE_H_

#include <string>
#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/session/phone/codec.h"
#include "talk/session/phone/mediachannel.h"
#include "talk/session/phone/mediaengine.h"

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
// only the RTP dump packets. TODO: Enable RTCP packets.
class FileMediaEngine : public MediaEngine {
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
  virtual bool Init() { return true; }
  virtual void Terminate() {}
  virtual int GetCapabilities();
  virtual VoiceMediaChannel* CreateChannel();
  virtual VideoMediaChannel* CreateVideoChannel(VoiceMediaChannel* voice_ch);
  virtual SoundclipMedia* CreateSoundclip() { return NULL; }
  virtual bool SetAudioOptions(int options) { return true; }
  virtual bool SetVideoOptions(int options) { return true; }
  virtual bool SetDefaultVideoEncoderConfig(const VideoEncoderConfig& config) {
    return true;
  }
  virtual bool SetSoundDevices(const Device* in_dev, const Device* out_dev) {
    return true;
  }
  virtual bool SetVideoCaptureDevice(const Device* cam_device) { return true; }
  virtual bool GetOutputVolume(int* level) { *level = 0; return true; }
  virtual bool SetOutputVolume(int level) { return true; }
  virtual int GetInputLevel() { return 0; }
  virtual bool SetLocalMonitor(bool enable) { return true; }
  virtual bool SetLocalRenderer(VideoRenderer* renderer) { return true; }
  // TODO: control channel send?
  virtual CaptureResult SetVideoCapture(bool capture) { return CR_SUCCESS; }
  virtual const std::vector<AudioCodec>& audio_codecs() {
    return voice_codecs_;
  }
  virtual const std::vector<VideoCodec>& video_codecs() {
    return video_codecs_;
  }
  virtual bool FindAudioCodec(const AudioCodec& codec) { return true; }
  virtual bool FindVideoCodec(const VideoCodec& codec) { return true; }
  virtual void SetVoiceLogging(int min_sev, const char* filter) {}
  virtual void SetVideoLogging(int min_sev, const char* filter) {}

 private:
  std::string voice_input_filename_;
  std::string voice_output_filename_;
  std::string video_input_filename_;
  std::string video_output_filename_;
  std::vector<AudioCodec> voice_codecs_;
  std::vector<VideoCodec> video_codecs_;

  DISALLOW_COPY_AND_ASSIGN(FileMediaEngine);
};

class RtpSenderReceiver;  // Forward declaration. Defined in the .cc file.

class FileVoiceChannel : public VoiceMediaChannel {
 public:
  FileVoiceChannel(const std::string& in_file, const std::string& out_file);
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
  virtual bool AddStream(uint32 ssrc) { return true; }
  virtual bool RemoveStream(uint32 ssrc) { return true; }
  virtual bool GetActiveStreams(AudioInfo::StreamList* actives) { return true; }
  virtual int GetOutputLevel() { return 0; }
  virtual bool SetRingbackTone(const char* buf, int len) { return true; }
  virtual bool PlayRingbackTone(uint32 ssrc, bool play, bool loop) {
    return true;
  }
  virtual bool PressDTMF(int event, bool playout) { return true; }
  virtual bool GetStats(VoiceMediaInfo* info) { return true; }

  // Implement pure virtual methods of MediaChannel.
  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet) {}
  virtual void SetSendSsrc(uint32 id) {}  // TODO: change RTP packet?
  virtual bool SetRtcpCName(const std::string& cname) { return true; }
  virtual bool Mute(bool on) { return false; }
  virtual bool SetSendBandwidth(bool autobw, int bps) { return true; }
  virtual bool SetOptions(int options) { return true; }
  virtual int GetMediaChannelId() { return -1; }

 private:
  talk_base::scoped_ptr<RtpSenderReceiver> rtp_sender_receiver_;
  DISALLOW_COPY_AND_ASSIGN(FileVoiceChannel);
};

class FileVideoChannel : public VideoMediaChannel {
 public:
  FileVideoChannel(const std::string& in_file, const std::string& out_file);
  virtual ~FileVideoChannel();

  // Implement pure virtual methods of VideoMediaChannel.
  virtual bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) {
    return true;
  }
  virtual bool SetSendCodecs(const std::vector<VideoCodec>& codecs);
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
  virtual bool AddStream(uint32 ssrc, uint32 voice_ssrc) { return true; }
  virtual bool RemoveStream(uint32 ssrc) { return true; }
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* renderer) {
    return true;
  }
  virtual bool SetExternalRenderer(uint32 ssrc, void* renderer) {
    return true;
  }
  virtual bool GetStats(VideoMediaInfo* info) { return true; }
  virtual bool SendIntraFrame() { return false; }
  virtual bool RequestIntraFrame() { return false; }

  // Implement pure virtual methods of MediaChannel.
  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet) {}
  virtual void SetSendSsrc(uint32 id) {}  // TODO: change RTP packet?
  virtual bool SetRtcpCName(const std::string& cname) { return true; }
  virtual bool Mute(bool on) { return false; }
  virtual bool SetSendBandwidth(bool autobw, int bps) { return true; }
  virtual bool SetOptions(int options) { return true; }
  virtual int GetMediaChannelId() { return -1; }

 private:
  talk_base::scoped_ptr<RtpSenderReceiver> rtp_sender_receiver_;
  DISALLOW_COPY_AND_ASSIGN(FileVideoChannel);
};

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_FILEMEDIAENGINE_H_
