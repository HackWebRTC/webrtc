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

#ifndef TALK_MEDIA_BASE_FAKEMEDIAENGINE_H_
#define TALK_MEDIA_BASE_FAKEMEDIAENGINE_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "talk/media/base/audiorenderer.h"
#include "talk/media/base/mediaengine.h"
#include "talk/media/base/rtputils.h"
#include "talk/media/base/streamparams.h"
#include "webrtc/audio/audio_sink.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/p2p/base/sessiondescription.h"

namespace cricket {

class FakeMediaEngine;
class FakeVideoEngine;
class FakeVoiceEngine;

// A common helper class that handles sending and receiving RTP/RTCP packets.
template <class Base> class RtpHelper : public Base {
 public:
  RtpHelper()
      : sending_(false),
        playout_(false),
        fail_set_send_codecs_(false),
        fail_set_recv_codecs_(false),
        send_ssrc_(0),
        ready_to_send_(false) {}
  const std::vector<RtpHeaderExtension>& recv_extensions() {
    return recv_extensions_;
  }
  const std::vector<RtpHeaderExtension>& send_extensions() {
    return send_extensions_;
  }
  bool sending() const { return sending_; }
  bool playout() const { return playout_; }
  const std::list<std::string>& rtp_packets() const { return rtp_packets_; }
  const std::list<std::string>& rtcp_packets() const { return rtcp_packets_; }

  bool SendRtp(const void* data, int len, const rtc::PacketOptions& options) {
    if (!sending_) {
      return false;
    }
    rtc::Buffer packet(reinterpret_cast<const uint8_t*>(data), len,
                       kMaxRtpPacketLen);
    return Base::SendPacket(&packet, options);
  }
  bool SendRtcp(const void* data, int len) {
    rtc::Buffer packet(reinterpret_cast<const uint8_t*>(data), len,
                       kMaxRtpPacketLen);
    return Base::SendRtcp(&packet, rtc::PacketOptions());
  }

  bool CheckRtp(const void* data, int len) {
    bool success = !rtp_packets_.empty();
    if (success) {
      std::string packet = rtp_packets_.front();
      rtp_packets_.pop_front();
      success = (packet == std::string(static_cast<const char*>(data), len));
    }
    return success;
  }
  bool CheckRtcp(const void* data, int len) {
    bool success = !rtcp_packets_.empty();
    if (success) {
      std::string packet = rtcp_packets_.front();
      rtcp_packets_.pop_front();
      success = (packet == std::string(static_cast<const char*>(data), len));
    }
    return success;
  }
  bool CheckNoRtp() { return rtp_packets_.empty(); }
  bool CheckNoRtcp() { return rtcp_packets_.empty(); }
  void set_fail_set_send_codecs(bool fail) { fail_set_send_codecs_ = fail; }
  void set_fail_set_recv_codecs(bool fail) { fail_set_recv_codecs_ = fail; }
  virtual bool AddSendStream(const StreamParams& sp) {
    if (std::find(send_streams_.begin(), send_streams_.end(), sp) !=
        send_streams_.end()) {
      return false;
    }
    send_streams_.push_back(sp);
    return true;
  }
  virtual bool RemoveSendStream(uint32_t ssrc) {
    return RemoveStreamBySsrc(&send_streams_, ssrc);
  }
  virtual bool AddRecvStream(const StreamParams& sp) {
    if (std::find(receive_streams_.begin(), receive_streams_.end(), sp) !=
        receive_streams_.end()) {
      return false;
    }
    receive_streams_.push_back(sp);
    return true;
  }
  virtual bool RemoveRecvStream(uint32_t ssrc) {
    return RemoveStreamBySsrc(&receive_streams_, ssrc);
  }
  bool IsStreamMuted(uint32_t ssrc) const {
    bool ret = muted_streams_.find(ssrc) != muted_streams_.end();
    // If |ssrc = 0| check if the first send stream is muted.
    if (!ret && ssrc == 0 && !send_streams_.empty()) {
      return muted_streams_.find(send_streams_[0].first_ssrc()) !=
             muted_streams_.end();
    }
    return ret;
  }
  const std::vector<StreamParams>& send_streams() const {
    return send_streams_;
  }
  const std::vector<StreamParams>& recv_streams() const {
    return receive_streams_;
  }
  bool HasRecvStream(uint32_t ssrc) const {
    return GetStreamBySsrc(receive_streams_, ssrc) != nullptr;
  }
  bool HasSendStream(uint32_t ssrc) const {
    return GetStreamBySsrc(send_streams_, ssrc) != nullptr;
  }
  // TODO(perkj): This is to support legacy unit test that only check one
  // sending stream.
  uint32_t send_ssrc() const {
    if (send_streams_.empty())
      return 0;
    return send_streams_[0].first_ssrc();
  }

  // TODO(perkj): This is to support legacy unit test that only check one
  // sending stream.
  const std::string rtcp_cname() {
    if (send_streams_.empty())
      return "";
    return send_streams_[0].cname;
  }

  bool ready_to_send() const {
    return ready_to_send_;
  }

 protected:
  bool MuteStream(uint32_t ssrc, bool mute) {
    if (!HasSendStream(ssrc) && ssrc != 0) {
      return false;
    }
    if (mute) {
      muted_streams_.insert(ssrc);
    } else {
      muted_streams_.erase(ssrc);
    }
    return true;
  }
  bool set_sending(bool send) {
    sending_ = send;
    return true;
  }
  void set_playout(bool playout) { playout_ = playout; }
  bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    recv_extensions_ = extensions;
    return true;
  }
  bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    send_extensions_ = extensions;
    return true;
  }
  virtual void OnPacketReceived(rtc::Buffer* packet,
                                const rtc::PacketTime& packet_time) {
    rtp_packets_.push_back(std::string(packet->data<char>(), packet->size()));
  }
  virtual void OnRtcpReceived(rtc::Buffer* packet,
                              const rtc::PacketTime& packet_time) {
    rtcp_packets_.push_back(std::string(packet->data<char>(), packet->size()));
  }
  virtual void OnReadyToSend(bool ready) {
    ready_to_send_ = ready;
  }
  bool fail_set_send_codecs() const { return fail_set_send_codecs_; }
  bool fail_set_recv_codecs() const { return fail_set_recv_codecs_; }

 private:
  bool sending_;
  bool playout_;
  std::vector<RtpHeaderExtension> recv_extensions_;
  std::vector<RtpHeaderExtension> send_extensions_;
  std::list<std::string> rtp_packets_;
  std::list<std::string> rtcp_packets_;
  std::vector<StreamParams> send_streams_;
  std::vector<StreamParams> receive_streams_;
  std::set<uint32_t> muted_streams_;
  bool fail_set_send_codecs_;
  bool fail_set_recv_codecs_;
  uint32_t send_ssrc_;
  std::string rtcp_cname_;
  bool ready_to_send_;
};

class FakeVoiceMediaChannel : public RtpHelper<VoiceMediaChannel> {
 public:
  struct DtmfInfo {
    DtmfInfo(uint32_t ssrc, int event_code, int duration)
        : ssrc(ssrc),
          event_code(event_code),
          duration(duration) {}
    uint32_t ssrc;
    int event_code;
    int duration;
  };
  explicit FakeVoiceMediaChannel(FakeVoiceEngine* engine,
                                 const AudioOptions& options)
      : engine_(engine),
        time_since_last_typing_(-1) {
    output_scalings_[0] = 1.0;  // For default channel.
    SetOptions(options);
  }
  ~FakeVoiceMediaChannel();
  const std::vector<AudioCodec>& recv_codecs() const { return recv_codecs_; }
  const std::vector<AudioCodec>& send_codecs() const { return send_codecs_; }
  const std::vector<AudioCodec>& codecs() const { return send_codecs(); }
  const std::vector<DtmfInfo>& dtmf_info_queue() const {
    return dtmf_info_queue_;
  }
  const AudioOptions& options() const { return options_; }

  virtual bool SetSendParameters(const AudioSendParameters& params) {
    return (SetSendCodecs(params.codecs) &&
            SetSendRtpHeaderExtensions(params.extensions) &&
            SetMaxSendBandwidth(params.max_bandwidth_bps) &&
            SetOptions(params.options));
  }

  virtual bool SetRecvParameters(const AudioRecvParameters& params) {
    return (SetRecvCodecs(params.codecs) &&
            SetRecvRtpHeaderExtensions(params.extensions));
  }
  virtual bool SetPlayout(bool playout) {
    set_playout(playout);
    return true;
  }
  virtual bool SetSend(SendFlags flag) {
    return set_sending(flag != SEND_NOTHING);
  }
  virtual bool SetAudioSend(uint32_t ssrc,
                            bool enable,
                            const AudioOptions* options,
                            AudioRenderer* renderer) {
    if (!SetLocalRenderer(ssrc, renderer)) {
      return false;
    }
    if (!RtpHelper<VoiceMediaChannel>::MuteStream(ssrc, !enable)) {
      return false;
    }
    if (enable && options) {
      return SetOptions(*options);
    }
    return true;
  }
  virtual bool AddRecvStream(const StreamParams& sp) {
    if (!RtpHelper<VoiceMediaChannel>::AddRecvStream(sp))
      return false;
    output_scalings_[sp.first_ssrc()] = 1.0;
    return true;
  }
  virtual bool RemoveRecvStream(uint32_t ssrc) {
    if (!RtpHelper<VoiceMediaChannel>::RemoveRecvStream(ssrc))
      return false;
    output_scalings_.erase(ssrc);
    return true;
  }

  virtual bool GetActiveStreams(AudioInfo::StreamList* streams) { return true; }
  virtual int GetOutputLevel() { return 0; }
  void set_time_since_last_typing(int ms) { time_since_last_typing_ = ms; }
  virtual int GetTimeSinceLastTyping() { return time_since_last_typing_; }
  virtual void SetTypingDetectionParameters(
      int time_window, int cost_per_typing, int reporting_threshold,
      int penalty_decay, int type_event_delay) {}

  virtual bool CanInsertDtmf() {
    for (std::vector<AudioCodec>::const_iterator it = send_codecs_.begin();
         it != send_codecs_.end(); ++it) {
      // Find the DTMF telephone event "codec".
      if (_stricmp(it->name.c_str(), "telephone-event") == 0) {
        return true;
      }
    }
    return false;
  }
  virtual bool InsertDtmf(uint32_t ssrc,
                          int event_code,
                          int duration) {
    dtmf_info_queue_.push_back(DtmfInfo(ssrc, event_code, duration));
    return true;
  }

  virtual bool SetOutputVolume(uint32_t ssrc, double volume) {
    if (0 == ssrc) {
      std::map<uint32_t, double>::iterator it;
      for (it = output_scalings_.begin(); it != output_scalings_.end(); ++it) {
        it->second = volume;
      }
      return true;
    } else if (output_scalings_.find(ssrc) != output_scalings_.end()) {
      output_scalings_[ssrc] = volume;
      return true;
    }
    return false;
  }
  bool GetOutputVolume(uint32_t ssrc, double* volume) {
    if (output_scalings_.find(ssrc) == output_scalings_.end())
      return false;
    *volume = output_scalings_[ssrc];
    return true;
  }

  virtual bool GetStats(VoiceMediaInfo* info) { return false; }

  virtual void SetRawAudioSink(
      uint32_t ssrc,
      rtc::scoped_ptr<webrtc::AudioSinkInterface> sink) {
    sink_ = std::move(sink);
  }

 private:
  class VoiceChannelAudioSink : public AudioRenderer::Sink {
   public:
    explicit VoiceChannelAudioSink(AudioRenderer* renderer)
        : renderer_(renderer) {
      renderer_->SetSink(this);
    }
    virtual ~VoiceChannelAudioSink() {
      if (renderer_) {
        renderer_->SetSink(NULL);
      }
    }
    void OnData(const void* audio_data,
                int bits_per_sample,
                int sample_rate,
                size_t number_of_channels,
                size_t number_of_frames) override {}
    void OnClose() override { renderer_ = NULL; }
    AudioRenderer* renderer() const { return renderer_; }

   private:
    AudioRenderer* renderer_;
  };

  bool SetRecvCodecs(const std::vector<AudioCodec>& codecs) {
    if (fail_set_recv_codecs()) {
      // Fake the failure in SetRecvCodecs.
      return false;
    }
    recv_codecs_ = codecs;
    return true;
  }
  bool SetSendCodecs(const std::vector<AudioCodec>& codecs) {
    if (fail_set_send_codecs()) {
      // Fake the failure in SetSendCodecs.
      return false;
    }
    send_codecs_ = codecs;
    return true;
  }
  bool SetMaxSendBandwidth(int bps) { return true; }
  bool SetOptions(const AudioOptions& options) {
    // Does a "merge" of current options and set options.
    options_.SetAll(options);
    return true;
  }
  bool SetLocalRenderer(uint32_t ssrc, AudioRenderer* renderer) {
    auto it = local_renderers_.find(ssrc);
    if (renderer) {
      if (it != local_renderers_.end()) {
        ASSERT(it->second->renderer() == renderer);
      } else {
        local_renderers_.insert(std::make_pair(
            ssrc, new VoiceChannelAudioSink(renderer)));
      }
    } else {
      if (it != local_renderers_.end()) {
        delete it->second;
        local_renderers_.erase(it);
      }
    }
    return true;
  }

  FakeVoiceEngine* engine_;
  std::vector<AudioCodec> recv_codecs_;
  std::vector<AudioCodec> send_codecs_;
  std::map<uint32_t, double> output_scalings_;
  std::vector<DtmfInfo> dtmf_info_queue_;
  int time_since_last_typing_;
  AudioOptions options_;
  std::map<uint32_t, VoiceChannelAudioSink*> local_renderers_;
  rtc::scoped_ptr<webrtc::AudioSinkInterface> sink_;
};

// A helper function to compare the FakeVoiceMediaChannel::DtmfInfo.
inline bool CompareDtmfInfo(const FakeVoiceMediaChannel::DtmfInfo& info,
                            uint32_t ssrc,
                            int event_code,
                            int duration) {
  return (info.duration == duration && info.event_code == event_code &&
          info.ssrc == ssrc);
}

class FakeVideoMediaChannel : public RtpHelper<VideoMediaChannel> {
 public:
  explicit FakeVideoMediaChannel(FakeVideoEngine* engine,
                                 const VideoOptions& options)
      : engine_(engine),
        sent_intra_frame_(false),
        requested_intra_frame_(false),
        max_bps_(-1) {
    SetOptions(options);
  }

  ~FakeVideoMediaChannel();

  const std::vector<VideoCodec>& recv_codecs() const { return recv_codecs_; }
  const std::vector<VideoCodec>& send_codecs() const { return send_codecs_; }
  const std::vector<VideoCodec>& codecs() const { return send_codecs(); }
  bool rendering() const { return playout(); }
  const VideoOptions& options() const { return options_; }
  const std::map<uint32_t, VideoRenderer*>& renderers() const {
    return renderers_;
  }
  int max_bps() const { return max_bps_; }
  bool GetSendStreamFormat(uint32_t ssrc, VideoFormat* format) {
    if (send_formats_.find(ssrc) == send_formats_.end()) {
      return false;
    }
    *format = send_formats_[ssrc];
    return true;
  }
  virtual bool SetSendStreamFormat(uint32_t ssrc, const VideoFormat& format) {
    if (send_formats_.find(ssrc) == send_formats_.end()) {
      return false;
    }
    send_formats_[ssrc] = format;
    return true;
  }
  virtual bool SetSendParameters(const VideoSendParameters& params) {
    return (SetSendCodecs(params.codecs) &&
            SetSendRtpHeaderExtensions(params.extensions) &&
            SetMaxSendBandwidth(params.max_bandwidth_bps) &&
            SetOptions(params.options));
  }

  virtual bool SetRecvParameters(const VideoRecvParameters& params) {
    return (SetRecvCodecs(params.codecs) &&
            SetRecvRtpHeaderExtensions(params.extensions));
  }
  virtual bool AddSendStream(const StreamParams& sp) {
    if (!RtpHelper<VideoMediaChannel>::AddSendStream(sp)) {
      return false;
    }
    SetSendStreamDefaultFormat(sp.first_ssrc());
    return true;
  }
  virtual bool RemoveSendStream(uint32_t ssrc) {
    send_formats_.erase(ssrc);
    return RtpHelper<VideoMediaChannel>::RemoveSendStream(ssrc);
  }

  virtual bool GetSendCodec(VideoCodec* send_codec) {
    if (send_codecs_.empty()) {
      return false;
    }
    *send_codec = send_codecs_[0];
    return true;
  }
  virtual bool SetRenderer(uint32_t ssrc, VideoRenderer* r) {
    if (ssrc != 0 && renderers_.find(ssrc) == renderers_.end()) {
      return false;
    }
    if (ssrc != 0) {
      renderers_[ssrc] = r;
    }
    return true;
  }

  virtual bool SetSend(bool send) { return set_sending(send); }
  virtual bool SetVideoSend(uint32_t ssrc, bool enable,
                            const VideoOptions* options) {
    if (!RtpHelper<VideoMediaChannel>::MuteStream(ssrc, !enable)) {
      return false;
    }
    if (enable && options) {
      return SetOptions(*options);
    }
    return true;
  }
  virtual bool SetCapturer(uint32_t ssrc, VideoCapturer* capturer) {
    capturers_[ssrc] = capturer;
    return true;
  }
  bool HasCapturer(uint32_t ssrc) const {
    return capturers_.find(ssrc) != capturers_.end();
  }
  virtual bool AddRecvStream(const StreamParams& sp) {
    if (!RtpHelper<VideoMediaChannel>::AddRecvStream(sp))
      return false;
    renderers_[sp.first_ssrc()] = NULL;
    return true;
  }
  virtual bool RemoveRecvStream(uint32_t ssrc) {
    if (!RtpHelper<VideoMediaChannel>::RemoveRecvStream(ssrc))
      return false;
    renderers_.erase(ssrc);
    return true;
  }

  virtual bool GetStats(VideoMediaInfo* info) { return false; }
  virtual bool SendIntraFrame() {
    sent_intra_frame_ = true;
    return true;
  }
  virtual bool RequestIntraFrame() {
    requested_intra_frame_ = true;
    return true;
  }
  virtual void UpdateAspectRatio(int ratio_w, int ratio_h) {}
  void set_sent_intra_frame(bool v) { sent_intra_frame_ = v; }
  bool sent_intra_frame() const { return sent_intra_frame_; }
  void set_requested_intra_frame(bool v) { requested_intra_frame_ = v; }
  bool requested_intra_frame() const { return requested_intra_frame_; }

 private:
  bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) {
    if (fail_set_recv_codecs()) {
      // Fake the failure in SetRecvCodecs.
      return false;
    }
    recv_codecs_ = codecs;
    return true;
  }
  bool SetSendCodecs(const std::vector<VideoCodec>& codecs) {
    if (fail_set_send_codecs()) {
      // Fake the failure in SetSendCodecs.
      return false;
    }
    send_codecs_ = codecs;

    for (std::vector<StreamParams>::const_iterator it = send_streams().begin();
         it != send_streams().end(); ++it) {
      SetSendStreamDefaultFormat(it->first_ssrc());
    }
    return true;
  }
  bool SetOptions(const VideoOptions& options) {
    options_ = options;
    return true;
  }
  bool SetMaxSendBandwidth(int bps) {
    max_bps_ = bps;
    return true;
  }

  // Be default, each send stream uses the first send codec format.
  void SetSendStreamDefaultFormat(uint32_t ssrc) {
    if (!send_codecs_.empty()) {
      send_formats_[ssrc] = VideoFormat(
          send_codecs_[0].width, send_codecs_[0].height,
          cricket::VideoFormat::FpsToInterval(send_codecs_[0].framerate),
          cricket::FOURCC_I420);
    }
  }

  FakeVideoEngine* engine_;
  std::vector<VideoCodec> recv_codecs_;
  std::vector<VideoCodec> send_codecs_;
  std::map<uint32_t, VideoRenderer*> renderers_;
  std::map<uint32_t, VideoFormat> send_formats_;
  std::map<uint32_t, VideoCapturer*> capturers_;
  bool sent_intra_frame_;
  bool requested_intra_frame_;
  VideoOptions options_;
  int max_bps_;
};

class FakeDataMediaChannel : public RtpHelper<DataMediaChannel> {
 public:
  explicit FakeDataMediaChannel(void* unused, const DataOptions& options)
      : send_blocked_(false), max_bps_(-1) {}
  ~FakeDataMediaChannel() {}
  const std::vector<DataCodec>& recv_codecs() const { return recv_codecs_; }
  const std::vector<DataCodec>& send_codecs() const { return send_codecs_; }
  const std::vector<DataCodec>& codecs() const { return send_codecs(); }
  int max_bps() const { return max_bps_; }

  virtual bool SetSendParameters(const DataSendParameters& params) {
    return (SetSendCodecs(params.codecs) &&
            SetMaxSendBandwidth(params.max_bandwidth_bps));
  }
  virtual bool SetRecvParameters(const DataRecvParameters& params) {
    return SetRecvCodecs(params.codecs);
  }
  virtual bool SetSend(bool send) { return set_sending(send); }
  virtual bool SetReceive(bool receive) {
    set_playout(receive);
    return true;
  }
  virtual bool AddRecvStream(const StreamParams& sp) {
    if (!RtpHelper<DataMediaChannel>::AddRecvStream(sp))
      return false;
    return true;
  }
  virtual bool RemoveRecvStream(uint32_t ssrc) {
    if (!RtpHelper<DataMediaChannel>::RemoveRecvStream(ssrc))
      return false;
    return true;
  }

  virtual bool SendData(const SendDataParams& params,
                        const rtc::Buffer& payload,
                        SendDataResult* result) {
    if (send_blocked_) {
      *result = SDR_BLOCK;
      return false;
    } else {
      last_sent_data_params_ = params;
      last_sent_data_ = std::string(payload.data<char>(), payload.size());
      return true;
    }
  }

  SendDataParams last_sent_data_params() { return last_sent_data_params_; }
  std::string last_sent_data() { return last_sent_data_; }
  bool is_send_blocked() { return send_blocked_; }
  void set_send_blocked(bool blocked) { send_blocked_ = blocked; }

 private:
  bool SetRecvCodecs(const std::vector<DataCodec>& codecs) {
    if (fail_set_recv_codecs()) {
      // Fake the failure in SetRecvCodecs.
      return false;
    }
    recv_codecs_ = codecs;
    return true;
  }
  bool SetSendCodecs(const std::vector<DataCodec>& codecs) {
    if (fail_set_send_codecs()) {
      // Fake the failure in SetSendCodecs.
      return false;
    }
    send_codecs_ = codecs;
    return true;
  }
  bool SetMaxSendBandwidth(int bps) {
    max_bps_ = bps;
    return true;
  }

  std::vector<DataCodec> recv_codecs_;
  std::vector<DataCodec> send_codecs_;
  SendDataParams last_sent_data_params_;
  std::string last_sent_data_;
  bool send_blocked_;
  int max_bps_;
};

// A base class for all of the shared parts between FakeVoiceEngine
// and FakeVideoEngine.
class FakeBaseEngine {
 public:
  FakeBaseEngine()
      : options_changed_(false),
        fail_create_channel_(false) {}
  void set_fail_create_channel(bool fail) { fail_create_channel_ = fail; }

  RtpCapabilities GetCapabilities() const { return capabilities_; }
  void set_rtp_header_extensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    capabilities_.header_extensions = extensions;
  }

 protected:
  // Flag used by optionsmessagehandler_unittest for checking whether any
  // relevant setting has been updated.
  // TODO(thaloun): Replace with explicit checks of before & after values.
  bool options_changed_;
  bool fail_create_channel_;
  RtpCapabilities capabilities_;
};

class FakeVoiceEngine : public FakeBaseEngine {
 public:
  FakeVoiceEngine()
      : output_volume_(-1) {
    // Add a fake audio codec. Note that the name must not be "" as there are
    // sanity checks against that.
    codecs_.push_back(AudioCodec(101, "fake_audio_codec", 0, 0, 1, 0));
  }
  bool Init(rtc::Thread* worker_thread) { return true; }
  void Terminate() {}
  rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const {
    return rtc::scoped_refptr<webrtc::AudioState>();
  }

  VoiceMediaChannel* CreateChannel(webrtc::Call* call,
                                   const AudioOptions& options) {
    if (fail_create_channel_) {
      return nullptr;
    }

    FakeVoiceMediaChannel* ch = new FakeVoiceMediaChannel(this, options);
    channels_.push_back(ch);
    return ch;
  }
  FakeVoiceMediaChannel* GetChannel(size_t index) {
    return (channels_.size() > index) ? channels_[index] : NULL;
  }
  void UnregisterChannel(VoiceMediaChannel* channel) {
    channels_.erase(std::find(channels_.begin(), channels_.end(), channel));
  }

  const std::vector<AudioCodec>& codecs() { return codecs_; }
  void SetCodecs(const std::vector<AudioCodec> codecs) { codecs_ = codecs; }

  bool GetOutputVolume(int* level) {
    *level = output_volume_;
    return true;
  }
  bool SetOutputVolume(int level) {
    output_volume_ = level;
    return true;
  }

  int GetInputLevel() { return 0; }

  bool StartAecDump(rtc::PlatformFile file) { return false; }

  void StopAecDump() {}

  bool StartRtcEventLog(rtc::PlatformFile file) { return false; }

  void StopRtcEventLog() {}

 private:
  std::vector<FakeVoiceMediaChannel*> channels_;
  std::vector<AudioCodec> codecs_;
  int output_volume_;

  friend class FakeMediaEngine;
};

class FakeVideoEngine : public FakeBaseEngine {
 public:
  FakeVideoEngine() : capture_(false) {
    // Add a fake video codec. Note that the name must not be "" as there are
    // sanity checks against that.
    codecs_.push_back(VideoCodec(0, "fake_video_codec", 0, 0, 0, 0));
  }
  void Init() {}
  bool SetOptions(const VideoOptions& options) {
    options_ = options;
    options_changed_ = true;
    return true;
  }

  VideoMediaChannel* CreateChannel(webrtc::Call* call,
                                   const VideoOptions& options) {
    if (fail_create_channel_) {
      return NULL;
    }

    FakeVideoMediaChannel* ch = new FakeVideoMediaChannel(this, options);
    channels_.push_back(ch);
    return ch;
  }
  FakeVideoMediaChannel* GetChannel(size_t index) {
    return (channels_.size() > index) ? channels_[index] : NULL;
  }
  void UnregisterChannel(VideoMediaChannel* channel) {
    channels_.erase(std::find(channels_.begin(), channels_.end(), channel));
  }

  const std::vector<VideoCodec>& codecs() const { return codecs_; }
  bool FindCodec(const VideoCodec& in) {
    for (size_t i = 0; i < codecs_.size(); ++i) {
      if (codecs_[i].Matches(in)) {
        return true;
      }
    }
    return false;
  }
  void SetCodecs(const std::vector<VideoCodec> codecs) { codecs_ = codecs; }

  bool SetCaptureDevice(const Device* device) {
    in_device_ = (device) ? device->name : "";
    options_changed_ = true;
    return true;
  }
  bool SetCapture(bool capture) {
    capture_ = capture;
    return true;
  }

 private:
  std::vector<FakeVideoMediaChannel*> channels_;
  std::vector<VideoCodec> codecs_;
  std::string in_device_;
  bool capture_;
  VideoOptions options_;

  friend class FakeMediaEngine;
};

class FakeMediaEngine :
    public CompositeMediaEngine<FakeVoiceEngine, FakeVideoEngine> {
 public:
  FakeMediaEngine() {}
  virtual ~FakeMediaEngine() {}

  void SetAudioCodecs(const std::vector<AudioCodec>& codecs) {
    voice_.SetCodecs(codecs);
  }
  void SetVideoCodecs(const std::vector<VideoCodec>& codecs) {
    video_.SetCodecs(codecs);
  }

  void SetAudioRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    voice_.set_rtp_header_extensions(extensions);
  }
  void SetVideoRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    video_.set_rtp_header_extensions(extensions);
  }

  FakeVoiceMediaChannel* GetVoiceChannel(size_t index) {
    return voice_.GetChannel(index);
  }
  FakeVideoMediaChannel* GetVideoChannel(size_t index) {
    return video_.GetChannel(index);
  }

  int output_volume() const { return voice_.output_volume_; }
  bool capture() const { return video_.capture_; }
  bool options_changed() const {
    return video_.options_changed_;
  }
  void clear_options_changed() {
    video_.options_changed_ = false;
  }
  void set_fail_create_channel(bool fail) {
    voice_.set_fail_create_channel(fail);
    video_.set_fail_create_channel(fail);
  }
};

// CompositeMediaEngine with FakeVoiceEngine to expose SetAudioCodecs to
// establish a media connectionwith minimum set of audio codes required
template <class VIDEO>
class CompositeMediaEngineWithFakeVoiceEngine :
    public CompositeMediaEngine<FakeVoiceEngine, VIDEO> {
 public:
  CompositeMediaEngineWithFakeVoiceEngine() {}
  virtual ~CompositeMediaEngineWithFakeVoiceEngine() {}

  virtual void SetAudioCodecs(const std::vector<AudioCodec>& codecs) {
    CompositeMediaEngine<FakeVoiceEngine, VIDEO>::voice_.SetCodecs(codecs);
  }
};

// Have to come afterwards due to declaration order
inline FakeVoiceMediaChannel::~FakeVoiceMediaChannel() {
  if (engine_) {
    engine_->UnregisterChannel(this);
  }
}

inline FakeVideoMediaChannel::~FakeVideoMediaChannel() {
  if (engine_) {
    engine_->UnregisterChannel(this);
  }
}

class FakeDataEngine : public DataEngineInterface {
 public:
  FakeDataEngine() : last_channel_type_(DCT_NONE) {}

  virtual DataMediaChannel* CreateChannel(DataChannelType data_channel_type) {
    last_channel_type_ = data_channel_type;
    FakeDataMediaChannel* ch = new FakeDataMediaChannel(this, DataOptions());
    channels_.push_back(ch);
    return ch;
  }

  FakeDataMediaChannel* GetChannel(size_t index) {
    return (channels_.size() > index) ? channels_[index] : NULL;
  }

  void UnregisterChannel(DataMediaChannel* channel) {
    channels_.erase(std::find(channels_.begin(), channels_.end(), channel));
  }

  virtual void SetDataCodecs(const std::vector<DataCodec>& data_codecs) {
    data_codecs_ = data_codecs;
  }

  virtual const std::vector<DataCodec>& data_codecs() { return data_codecs_; }

  DataChannelType last_channel_type() const { return last_channel_type_; }

 private:
  std::vector<FakeDataMediaChannel*> channels_;
  std::vector<DataCodec> data_codecs_;
  DataChannelType last_channel_type_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_FAKEMEDIAENGINE_H_
