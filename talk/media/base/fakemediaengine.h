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

#include "talk/base/buffer.h"
#include "talk/base/stringutils.h"
#include "talk/media/base/audiorenderer.h"
#include "talk/media/base/mediaengine.h"
#include "talk/media/base/rtputils.h"
#include "talk/media/base/streamparams.h"
#include "talk/p2p/base/sessiondescription.h"

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

  bool SendRtp(const void* data, int len) {
    if (!sending_) {
      return false;
    }
    talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
    return Base::SendPacket(&packet);
  }
  bool SendRtcp(const void* data, int len) {
    talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
    return Base::SendRtcp(&packet);
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
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    recv_extensions_ = extensions;
    return true;
  }
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    send_extensions_ = extensions;
    return true;
  }
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
  virtual bool RemoveSendStream(uint32 ssrc) {
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
  virtual bool RemoveRecvStream(uint32 ssrc) {
    return RemoveStreamBySsrc(&receive_streams_, ssrc);
  }
  virtual bool MuteStream(uint32 ssrc, bool on) {
    if (!HasSendStream(ssrc) && ssrc != 0)
      return false;
    if (on)
      muted_streams_.insert(ssrc);
    else
      muted_streams_.erase(ssrc);
    return true;
  }
  bool IsStreamMuted(uint32 ssrc) const {
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
  bool HasRecvStream(uint32 ssrc) const {
    return GetStreamBySsrc(receive_streams_, ssrc, NULL);
  }
  bool HasSendStream(uint32 ssrc) const {
    return GetStreamBySsrc(send_streams_, ssrc, NULL);
  }
  // TODO(perkj): This is to support legacy unit test that only check one
  // sending stream.
  const uint32 send_ssrc() {
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
  bool set_sending(bool send) {
    sending_ = send;
    return true;
  }
  void set_playout(bool playout) { playout_ = playout; }
  virtual void OnPacketReceived(talk_base::Buffer* packet,
                                const talk_base::PacketTime& packet_time) {
    rtp_packets_.push_back(std::string(packet->data(), packet->length()));
  }
  virtual void OnRtcpReceived(talk_base::Buffer* packet,
                              const talk_base::PacketTime& packet_time) {
    rtcp_packets_.push_back(std::string(packet->data(), packet->length()));
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
  std::set<uint32> muted_streams_;
  bool fail_set_send_codecs_;
  bool fail_set_recv_codecs_;
  uint32 send_ssrc_;
  std::string rtcp_cname_;
  bool ready_to_send_;
};

class FakeVoiceMediaChannel : public RtpHelper<VoiceMediaChannel> {
 public:
  struct DtmfInfo {
    DtmfInfo(uint32 ssrc, int event_code, int duration, int flags)
        : ssrc(ssrc), event_code(event_code), duration(duration), flags(flags) {
    }
    uint32 ssrc;
    int event_code;
    int duration;
    int flags;
  };
  explicit FakeVoiceMediaChannel(FakeVoiceEngine* engine)
      : engine_(engine),
        fail_set_send_(false),
        ringback_tone_ssrc_(0),
        ringback_tone_play_(false),
        ringback_tone_loop_(false),
        time_since_last_typing_(-1) {
    output_scalings_[0] = OutputScaling();  // For default channel.
  }
  ~FakeVoiceMediaChannel();
  const std::vector<AudioCodec>& recv_codecs() const { return recv_codecs_; }
  const std::vector<AudioCodec>& send_codecs() const { return send_codecs_; }
  const std::vector<AudioCodec>& codecs() const { return send_codecs(); }
  const std::vector<DtmfInfo>& dtmf_info_queue() const {
    return dtmf_info_queue_;
  }
  const AudioOptions& options() const { return options_; }

  uint32 ringback_tone_ssrc() const { return ringback_tone_ssrc_; }
  bool ringback_tone_play() const { return ringback_tone_play_; }
  bool ringback_tone_loop() const { return ringback_tone_loop_; }

  virtual bool SetRecvCodecs(const std::vector<AudioCodec>& codecs) {
    if (fail_set_recv_codecs()) {
      // Fake the failure in SetRecvCodecs.
      return false;
    }
    recv_codecs_ = codecs;
    return true;
  }
  virtual bool SetSendCodecs(const std::vector<AudioCodec>& codecs) {
    if (fail_set_send_codecs()) {
      // Fake the failure in SetSendCodecs.
      return false;
    }
    send_codecs_ = codecs;
    return true;
  }
  virtual bool SetPlayout(bool playout) {
    set_playout(playout);
    return true;
  }
  virtual bool SetSend(SendFlags flag) {
    if (fail_set_send_) {
      return false;
    }
    return set_sending(flag != SEND_NOTHING);
  }
  virtual bool SetStartSendBandwidth(int bps) { return true; }
  virtual bool SetMaxSendBandwidth(int bps) { return true; }
  virtual bool AddRecvStream(const StreamParams& sp) {
    if (!RtpHelper<VoiceMediaChannel>::AddRecvStream(sp))
      return false;
    output_scalings_[sp.first_ssrc()] = OutputScaling();
    return true;
  }
  virtual bool RemoveRecvStream(uint32 ssrc) {
    if (!RtpHelper<VoiceMediaChannel>::RemoveRecvStream(ssrc))
      return false;
    output_scalings_.erase(ssrc);
    return true;
  }
  virtual bool SetRemoteRenderer(uint32 ssrc, AudioRenderer* renderer) {
    std::map<uint32, AudioRenderer*>::iterator it =
        remote_renderers_.find(ssrc);
    if (renderer) {
      if (it != remote_renderers_.end()) {
        ASSERT(it->second == renderer);
      } else {
        remote_renderers_.insert(std::make_pair(ssrc, renderer));
        renderer->AddChannel(0);
      }
    } else {
      if (it != remote_renderers_.end()) {
        it->second->RemoveChannel(0);
        remote_renderers_.erase(it);
      } else {
        return false;
      }
    }
    return true;
  }
  virtual bool SetLocalRenderer(uint32 ssrc, AudioRenderer* renderer) {
    std::map<uint32, AudioRenderer*>::iterator it = local_renderers_.find(ssrc);
    if (renderer) {
      if (it != local_renderers_.end()) {
        ASSERT(it->second == renderer);
      } else {
        local_renderers_.insert(std::make_pair(ssrc, renderer));
        renderer->AddChannel(0);
      }
    } else {
      if (it != local_renderers_.end()) {
        it->second->RemoveChannel(0);
        local_renderers_.erase(it);
      } else {
        return false;
      }
    }
    return true;
  }

  virtual bool GetActiveStreams(AudioInfo::StreamList* streams) { return true; }
  virtual int GetOutputLevel() { return 0; }
  void set_time_since_last_typing(int ms) { time_since_last_typing_ = ms; }
  virtual int GetTimeSinceLastTyping() { return time_since_last_typing_; }
  virtual void SetTypingDetectionParameters(
      int time_window, int cost_per_typing, int reporting_threshold,
      int penalty_decay, int type_event_delay) {}

  virtual bool SetRingbackTone(const char* buf, int len) { return true; }
  virtual bool PlayRingbackTone(uint32 ssrc, bool play, bool loop) {
    ringback_tone_ssrc_ = ssrc;
    ringback_tone_play_ = play;
    ringback_tone_loop_ = loop;
    return true;
  }

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
  virtual bool InsertDtmf(uint32 ssrc, int event_code, int duration,
                          int flags) {
    dtmf_info_queue_.push_back(DtmfInfo(ssrc, event_code, duration, flags));
    return true;
  }

  virtual bool SetOutputScaling(uint32 ssrc, double left, double right) {
    if (0 == ssrc) {
      std::map<uint32, OutputScaling>::iterator it;
      for (it = output_scalings_.begin(); it != output_scalings_.end(); ++it) {
        it->second.left = left;
        it->second.right = right;
      }
      return true;
    } else if (output_scalings_.find(ssrc) != output_scalings_.end()) {
      output_scalings_[ssrc].left = left;
      output_scalings_[ssrc].right = right;
      return true;
    }
    return false;
  }
  virtual bool GetOutputScaling(uint32 ssrc, double* left, double* right) {
    if (output_scalings_.find(ssrc) == output_scalings_.end())
      return false;
    *left = output_scalings_[ssrc].left;
    *right = output_scalings_[ssrc].right;
    return true;
  }

  virtual bool GetStats(VoiceMediaInfo* info) { return false; }
  virtual void GetLastMediaError(uint32* ssrc,
                                 VoiceMediaChannel::Error* error) {
    *ssrc = 0;
    *error = fail_set_send_ ? VoiceMediaChannel::ERROR_REC_DEVICE_OPEN_FAILED
                            : VoiceMediaChannel::ERROR_NONE;
  }

  void set_fail_set_send(bool fail) { fail_set_send_ = fail; }
  void TriggerError(uint32 ssrc, VoiceMediaChannel::Error error) {
    VoiceMediaChannel::SignalMediaError(ssrc, error);
  }

  virtual bool SetOptions(const AudioOptions& options) {
    // Does a "merge" of current options and set options.
    options_.SetAll(options);
    return true;
  }
  virtual bool GetOptions(AudioOptions* options) const {
    *options = options_;
    return true;
  }

 private:
  struct OutputScaling {
    OutputScaling() : left(1.0), right(1.0) {}
    double left, right;
  };

  FakeVoiceEngine* engine_;
  std::vector<AudioCodec> recv_codecs_;
  std::vector<AudioCodec> send_codecs_;
  std::map<uint32, OutputScaling> output_scalings_;
  std::vector<DtmfInfo> dtmf_info_queue_;
  bool fail_set_send_;
  uint32 ringback_tone_ssrc_;
  bool ringback_tone_play_;
  bool ringback_tone_loop_;
  int time_since_last_typing_;
  AudioOptions options_;
  std::map<uint32, AudioRenderer*> local_renderers_;
  std::map<uint32, AudioRenderer*> remote_renderers_;
};

// A helper function to compare the FakeVoiceMediaChannel::DtmfInfo.
inline bool CompareDtmfInfo(const FakeVoiceMediaChannel::DtmfInfo& info,
                            uint32 ssrc, int event_code, int duration,
                            int flags) {
  return (info.duration == duration && info.event_code == event_code &&
          info.flags == flags && info.ssrc == ssrc);
}

class FakeVideoMediaChannel : public RtpHelper<VideoMediaChannel> {
 public:
  explicit FakeVideoMediaChannel(FakeVideoEngine* engine)
      : engine_(engine),
        sent_intra_frame_(false),
        requested_intra_frame_(false),
        start_bps_(-1),
        max_bps_(-1) {}
  ~FakeVideoMediaChannel();

  const std::vector<VideoCodec>& recv_codecs() const { return recv_codecs_; }
  const std::vector<VideoCodec>& send_codecs() const { return send_codecs_; }
  const std::vector<VideoCodec>& codecs() const { return send_codecs(); }
  bool rendering() const { return playout(); }
  const VideoOptions& options() const { return options_; }
  const std::map<uint32, VideoRenderer*>& renderers() const {
    return renderers_;
  }
  int start_bps() const { return start_bps_; }
  int max_bps() const { return max_bps_; }
  bool GetSendStreamFormat(uint32 ssrc, VideoFormat* format) {
    if (send_formats_.find(ssrc) == send_formats_.end()) {
      return false;
    }
    *format = send_formats_[ssrc];
    return true;
  }
  virtual bool SetSendStreamFormat(uint32 ssrc, const VideoFormat& format) {
    if (send_formats_.find(ssrc) == send_formats_.end()) {
      return false;
    }
    send_formats_[ssrc] = format;
    return true;
  }

  virtual bool AddSendStream(const StreamParams& sp) {
    if (!RtpHelper<VideoMediaChannel>::AddSendStream(sp)) {
      return false;
    }
    SetSendStreamDefaultFormat(sp.first_ssrc());
    return true;
  }
  virtual bool RemoveSendStream(uint32 ssrc) {
    send_formats_.erase(ssrc);
    return RtpHelper<VideoMediaChannel>::RemoveSendStream(ssrc);
  }

  virtual bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) {
    if (fail_set_recv_codecs()) {
      // Fake the failure in SetRecvCodecs.
      return false;
    }
    recv_codecs_ = codecs;
    return true;
  }
  virtual bool SetSendCodecs(const std::vector<VideoCodec>& codecs) {
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
  virtual bool GetSendCodec(VideoCodec* send_codec) {
    if (send_codecs_.empty()) {
      return false;
    }
    *send_codec = send_codecs_[0];
    return true;
  }
  virtual bool SetRender(bool render) {
    set_playout(render);
    return true;
  }
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* r) {
    if (ssrc != 0 && renderers_.find(ssrc) == renderers_.end()) {
      return false;
    }
    if (ssrc != 0) {
      renderers_[ssrc] = r;
    }
    return true;
  }

  virtual bool SetSend(bool send) { return set_sending(send); }
  virtual bool SetCapturer(uint32 ssrc, VideoCapturer* capturer) {
    capturers_[ssrc] = capturer;
    return true;
  }
  bool HasCapturer(uint32 ssrc) const {
    return capturers_.find(ssrc) != capturers_.end();
  }
  virtual bool SetStartSendBandwidth(int bps) {
    start_bps_ = bps;
    return true;
  }
  virtual bool SetMaxSendBandwidth(int bps) {
    max_bps_ = bps;
    return true;
  }
  virtual bool AddRecvStream(const StreamParams& sp) {
    if (!RtpHelper<VideoMediaChannel>::AddRecvStream(sp))
      return false;
    renderers_[sp.first_ssrc()] = NULL;
    return true;
  }
  virtual bool RemoveRecvStream(uint32 ssrc) {
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
  virtual bool SetOptions(const VideoOptions& options) {
    options_ = options;
    return true;
  }
  virtual bool GetOptions(VideoOptions* options) const {
    *options = options_;
    return true;
  }
  virtual void UpdateAspectRatio(int ratio_w, int ratio_h) {}
  void set_sent_intra_frame(bool v) { sent_intra_frame_ = v; }
  bool sent_intra_frame() const { return sent_intra_frame_; }
  void set_requested_intra_frame(bool v) { requested_intra_frame_ = v; }
  bool requested_intra_frame() const { return requested_intra_frame_; }

 private:
  // Be default, each send stream uses the first send codec format.
  void SetSendStreamDefaultFormat(uint32 ssrc) {
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
  std::map<uint32, VideoRenderer*> renderers_;
  std::map<uint32, VideoFormat> send_formats_;
  std::map<uint32, VideoCapturer*> capturers_;
  bool sent_intra_frame_;
  bool requested_intra_frame_;
  VideoOptions options_;
  int start_bps_;
  int max_bps_;
};

class FakeSoundclipMedia : public SoundclipMedia {
 public:
  virtual bool PlaySound(const char* buf, int len, int flags) { return true; }
};

class FakeDataMediaChannel : public RtpHelper<DataMediaChannel> {
 public:
  explicit FakeDataMediaChannel(void* unused)
      : send_blocked_(false), max_bps_(-1) {}
  ~FakeDataMediaChannel() {}
  const std::vector<DataCodec>& recv_codecs() const { return recv_codecs_; }
  const std::vector<DataCodec>& send_codecs() const { return send_codecs_; }
  const std::vector<DataCodec>& codecs() const { return send_codecs(); }
  int max_bps() const { return max_bps_; }

  virtual bool SetRecvCodecs(const std::vector<DataCodec>& codecs) {
    if (fail_set_recv_codecs()) {
      // Fake the failure in SetRecvCodecs.
      return false;
    }
    recv_codecs_ = codecs;
    return true;
  }
  virtual bool SetSendCodecs(const std::vector<DataCodec>& codecs) {
    if (fail_set_send_codecs()) {
      // Fake the failure in SetSendCodecs.
      return false;
    }
    send_codecs_ = codecs;
    return true;
  }
  virtual bool SetSend(bool send) { return set_sending(send); }
  virtual bool SetReceive(bool receive) {
    set_playout(receive);
    return true;
  }
  virtual bool SetStartSendBandwidth(int bps) { return true; }
  virtual bool SetMaxSendBandwidth(int bps) {
    max_bps_ = bps;
    return true;
  }
  virtual bool AddRecvStream(const StreamParams& sp) {
    if (!RtpHelper<DataMediaChannel>::AddRecvStream(sp))
      return false;
    return true;
  }
  virtual bool RemoveRecvStream(uint32 ssrc) {
    if (!RtpHelper<DataMediaChannel>::RemoveRecvStream(ssrc))
      return false;
    return true;
  }

  virtual bool SendData(const SendDataParams& params,
                        const talk_base::Buffer& payload,
                        SendDataResult* result) {
    if (send_blocked_) {
      *result = SDR_BLOCK;
      return false;
    } else {
      last_sent_data_params_ = params;
      last_sent_data_ = std::string(payload.data(), payload.length());
      return true;
    }
  }

  SendDataParams last_sent_data_params() { return last_sent_data_params_; }
  std::string last_sent_data() { return last_sent_data_; }
  bool is_send_blocked() { return send_blocked_; }
  void set_send_blocked(bool blocked) { send_blocked_ = blocked; }

 private:
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
      : loglevel_(-1),
        options_changed_(false),
        fail_create_channel_(false) {}
  bool Init(talk_base::Thread* worker_thread) { return true; }
  void Terminate() {}

  void SetLogging(int level, const char* filter) {
    loglevel_ = level;
    logfilter_ = filter;
  }

  void set_fail_create_channel(bool fail) { fail_create_channel_ = fail; }

  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const {
    return rtp_header_extensions_;
  }

 protected:
  int loglevel_;
  std::string logfilter_;
  // Flag used by optionsmessagehandler_unittest for checking whether any
  // relevant setting has been updated.
  // TODO(thaloun): Replace with explicit checks of before & after values.
  bool options_changed_;
  bool fail_create_channel_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
};

class FakeVoiceEngine : public FakeBaseEngine {
 public:
  FakeVoiceEngine()
      : output_volume_(-1),
        delay_offset_(0),
        rx_processor_(NULL),
        tx_processor_(NULL) {
    // Add a fake audio codec. Note that the name must not be "" as there are
    // sanity checks against that.
    codecs_.push_back(AudioCodec(101, "fake_audio_codec", 0, 0, 1, 0));
  }
  int GetCapabilities() { return AUDIO_SEND | AUDIO_RECV; }
  AudioOptions GetAudioOptions() const {
    return options_;
  }
  AudioOptions GetOptions() const {
    return options_;
  }
  bool SetOptions(const AudioOptions& options) {
    options_ = options;
    options_changed_ = true;
    return true;
  }

  VoiceMediaChannel* CreateChannel() {
    if (fail_create_channel_) {
      return NULL;
    }

    FakeVoiceMediaChannel* ch = new FakeVoiceMediaChannel(this);
    channels_.push_back(ch);
    return ch;
  }
  FakeVoiceMediaChannel* GetChannel(size_t index) {
    return (channels_.size() > index) ? channels_[index] : NULL;
  }
  void UnregisterChannel(VoiceMediaChannel* channel) {
    channels_.erase(std::find(channels_.begin(), channels_.end(), channel));
  }
  SoundclipMedia* CreateSoundclip() { return new FakeSoundclipMedia(); }

  const std::vector<AudioCodec>& codecs() { return codecs_; }
  void SetCodecs(const std::vector<AudioCodec> codecs) { codecs_ = codecs; }

  bool SetDelayOffset(int offset) {
    delay_offset_ = offset;
    return true;
  }

  bool SetDevices(const Device* in_device, const Device* out_device) {
    in_device_ = (in_device) ? in_device->name : "";
    out_device_ = (out_device) ? out_device->name : "";
    options_changed_ = true;
    return true;
  }

  bool GetOutputVolume(int* level) {
    *level = output_volume_;
    return true;
  }

  bool SetOutputVolume(int level) {
    output_volume_ = level;
    options_changed_ = true;
    return true;
  }

  int GetInputLevel() { return 0; }

  bool SetLocalMonitor(bool enable) { return true; }

  bool StartAecDump(talk_base::PlatformFile file) { return false; }

  bool RegisterProcessor(uint32 ssrc, VoiceProcessor* voice_processor,
                         MediaProcessorDirection direction) {
    if (direction == MPD_RX) {
      rx_processor_ = voice_processor;
      return true;
    } else if (direction == MPD_TX) {
      tx_processor_ = voice_processor;
      return true;
    }
    return false;
  }

  bool UnregisterProcessor(uint32 ssrc, VoiceProcessor* voice_processor,
                           MediaProcessorDirection direction) {
    bool unregistered = false;
    if (direction & MPD_RX) {
      rx_processor_ = NULL;
      unregistered = true;
    }
    if (direction & MPD_TX) {
      tx_processor_ = NULL;
      unregistered = true;
    }
    return unregistered;
  }

 private:
  std::vector<FakeVoiceMediaChannel*> channels_;
  std::vector<AudioCodec> codecs_;
  int output_volume_;
  int delay_offset_;
  std::string in_device_;
  std::string out_device_;
  VoiceProcessor* rx_processor_;
  VoiceProcessor* tx_processor_;
  AudioOptions options_;

  friend class FakeMediaEngine;
};

class FakeVideoEngine : public FakeBaseEngine {
 public:
  FakeVideoEngine() : renderer_(NULL), capture_(false), processor_(NULL) {
    // Add a fake video codec. Note that the name must not be "" as there are
    // sanity checks against that.
    codecs_.push_back(VideoCodec(0, "fake_video_codec", 0, 0, 0, 0));
  }
  bool GetOptions(VideoOptions* options) const {
    *options = options_;
    return true;
  }
  bool SetOptions(const VideoOptions& options) {
    options_ = options;
    options_changed_ = true;
    return true;
  }
  int GetCapabilities() { return VIDEO_SEND | VIDEO_RECV; }
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config) {
    default_encoder_config_ = config;
    return true;
  }
  VideoEncoderConfig GetDefaultEncoderConfig() const {
    return default_encoder_config_;
  }
  const VideoEncoderConfig& default_encoder_config() const {
    return default_encoder_config_;
  }

  VideoMediaChannel* CreateChannel(VoiceMediaChannel* channel) {
    if (fail_create_channel_) {
      return NULL;
    }

    FakeVideoMediaChannel* ch = new FakeVideoMediaChannel(this);
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
  bool SetLocalRenderer(VideoRenderer* r) {
    renderer_ = r;
    return true;
  }
  bool SetCapture(bool capture) {
    capture_ = capture;
    return true;
  }
  VideoFormat GetStartCaptureFormat() const {
    return VideoFormat(640, 480, cricket::VideoFormat::FpsToInterval(30),
                       FOURCC_I420);
  }

  sigslot::repeater2<VideoCapturer*, CaptureState> SignalCaptureStateChange;

 private:
  std::vector<FakeVideoMediaChannel*> channels_;
  std::vector<VideoCodec> codecs_;
  VideoEncoderConfig default_encoder_config_;
  std::string in_device_;
  VideoRenderer* renderer_;
  bool capture_;
  VideoProcessor* processor_;
  VideoOptions options_;

  friend class FakeMediaEngine;
};

class FakeMediaEngine :
    public CompositeMediaEngine<FakeVoiceEngine, FakeVideoEngine> {
 public:
  FakeMediaEngine() {
    voice_ = FakeVoiceEngine();
    video_ = FakeVideoEngine();
  }
  virtual ~FakeMediaEngine() {}

  virtual void SetAudioCodecs(const std::vector<AudioCodec> codecs) {
    voice_.SetCodecs(codecs);
  }

  virtual void SetVideoCodecs(const std::vector<VideoCodec> codecs) {
    video_.SetCodecs(codecs);
  }

  FakeVoiceMediaChannel* GetVoiceChannel(size_t index) {
    return voice_.GetChannel(index);
  }

  FakeVideoMediaChannel* GetVideoChannel(size_t index) {
    return video_.GetChannel(index);
  }

  AudioOptions audio_options() const { return voice_.options_; }
  int audio_delay_offset() const { return voice_.delay_offset_; }
  int output_volume() const { return voice_.output_volume_; }
  const VideoEncoderConfig& default_video_encoder_config() const {
    return video_.default_encoder_config_;
  }
  const std::string& audio_in_device() const { return voice_.in_device_; }
  const std::string& audio_out_device() const { return voice_.out_device_; }
  VideoRenderer* local_renderer() { return video_.renderer_; }
  int voice_loglevel() const { return voice_.loglevel_; }
  const std::string& voice_logfilter() const { return voice_.logfilter_; }
  int video_loglevel() const { return video_.loglevel_; }
  const std::string& video_logfilter() const { return video_.logfilter_; }
  bool capture() const { return video_.capture_; }
  bool options_changed() const {
    return voice_.options_changed_ || video_.options_changed_;
  }
  void clear_options_changed() {
    video_.options_changed_ = false;
    voice_.options_changed_ = false;
  }
  void set_fail_create_channel(bool fail) {
    voice_.set_fail_create_channel(fail);
    video_.set_fail_create_channel(fail);
  }
  bool voice_processor_registered(MediaProcessorDirection direction) const {
    if (direction == MPD_RX) {
      return voice_.rx_processor_ != NULL;
    } else if (direction == MPD_TX) {
      return voice_.tx_processor_ != NULL;
    }
    return false;
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
    FakeDataMediaChannel* ch = new FakeDataMediaChannel(this);
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
