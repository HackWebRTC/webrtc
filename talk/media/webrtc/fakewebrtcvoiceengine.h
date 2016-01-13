/*
 * libjingle
 * Copyright 2010 Google Inc.
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

#ifndef TALK_SESSION_PHONE_FAKEWEBRTCVOICEENGINE_H_
#define TALK_SESSION_PHONE_FAKEWEBRTCVOICEENGINE_H_

#include <list>
#include <map>
#include <vector>

#include "talk/media/base/codec.h"
#include "talk/media/base/rtputils.h"
#include "talk/media/webrtc/fakewebrtccommon.h"
#include "talk/media/webrtc/webrtcvoe.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/config.h"
#include "webrtc/modules/audio_coding/acm2/rent_a_codec.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace cricket {

static const int kOpusBandwidthNb = 4000;
static const int kOpusBandwidthMb = 6000;
static const int kOpusBandwidthWb = 8000;
static const int kOpusBandwidthSwb = 12000;
static const int kOpusBandwidthFb = 20000;

#define WEBRTC_CHECK_CHANNEL(channel) \
  if (channels_.find(channel) == channels_.end()) return -1;

class FakeAudioProcessing : public webrtc::AudioProcessing {
 public:
  FakeAudioProcessing() : experimental_ns_enabled_(false) {}

  WEBRTC_STUB(Initialize, ())
  WEBRTC_STUB(Initialize, (
      int input_sample_rate_hz,
      int output_sample_rate_hz,
      int reverse_sample_rate_hz,
      webrtc::AudioProcessing::ChannelLayout input_layout,
      webrtc::AudioProcessing::ChannelLayout output_layout,
      webrtc::AudioProcessing::ChannelLayout reverse_layout));
  WEBRTC_STUB(Initialize, (
      const webrtc::ProcessingConfig& processing_config));

  WEBRTC_VOID_FUNC(SetExtraOptions, (const webrtc::Config& config)) {
    experimental_ns_enabled_ = config.Get<webrtc::ExperimentalNs>().enabled;
  }

  WEBRTC_STUB_CONST(input_sample_rate_hz, ());
  WEBRTC_STUB_CONST(proc_sample_rate_hz, ());
  WEBRTC_STUB_CONST(proc_split_sample_rate_hz, ());
  size_t num_input_channels() const override { return 0; }
  size_t num_proc_channels() const override { return 0; }
  size_t num_output_channels() const override { return 0; }
  size_t num_reverse_channels() const override { return 0; }
  WEBRTC_VOID_STUB(set_output_will_be_muted, (bool muted));
  WEBRTC_STUB(ProcessStream, (webrtc::AudioFrame* frame));
  WEBRTC_STUB(ProcessStream, (
      const float* const* src,
      size_t samples_per_channel,
      int input_sample_rate_hz,
      webrtc::AudioProcessing::ChannelLayout input_layout,
      int output_sample_rate_hz,
      webrtc::AudioProcessing::ChannelLayout output_layout,
      float* const* dest));
  WEBRTC_STUB(ProcessStream,
              (const float* const* src,
               const webrtc::StreamConfig& input_config,
               const webrtc::StreamConfig& output_config,
               float* const* dest));
  WEBRTC_STUB(AnalyzeReverseStream, (webrtc::AudioFrame* frame));
  WEBRTC_STUB(ProcessReverseStream, (webrtc::AudioFrame * frame));
  WEBRTC_STUB(AnalyzeReverseStream, (
      const float* const* data,
      size_t samples_per_channel,
      int sample_rate_hz,
      webrtc::AudioProcessing::ChannelLayout layout));
  WEBRTC_STUB(ProcessReverseStream,
              (const float* const* src,
               const webrtc::StreamConfig& reverse_input_config,
               const webrtc::StreamConfig& reverse_output_config,
               float* const* dest));
  WEBRTC_STUB(set_stream_delay_ms, (int delay));
  WEBRTC_STUB_CONST(stream_delay_ms, ());
  WEBRTC_BOOL_STUB_CONST(was_stream_delay_set, ());
  WEBRTC_VOID_STUB(set_stream_key_pressed, (bool key_pressed));
  WEBRTC_VOID_STUB(set_delay_offset_ms, (int offset));
  WEBRTC_STUB_CONST(delay_offset_ms, ());
  WEBRTC_STUB(StartDebugRecording, (const char filename[kMaxFilenameSize]));
  WEBRTC_STUB(StartDebugRecording, (FILE* handle));
  WEBRTC_STUB(StopDebugRecording, ());
  WEBRTC_VOID_STUB(UpdateHistogramsOnCallEnd, ());
  webrtc::EchoCancellation* echo_cancellation() const override { return NULL; }
  webrtc::EchoControlMobile* echo_control_mobile() const override {
    return NULL;
  }
  webrtc::GainControl* gain_control() const override { return NULL; }
  webrtc::HighPassFilter* high_pass_filter() const override { return NULL; }
  webrtc::LevelEstimator* level_estimator() const override { return NULL; }
  webrtc::NoiseSuppression* noise_suppression() const override { return NULL; }
  webrtc::VoiceDetection* voice_detection() const override { return NULL; }

  bool experimental_ns_enabled() {
    return experimental_ns_enabled_;
  }

 private:
  bool experimental_ns_enabled_;
};

class FakeWebRtcVoiceEngine
    : public webrtc::VoEAudioProcessing,
      public webrtc::VoEBase, public webrtc::VoECodec,
      public webrtc::VoEHardware,
      public webrtc::VoENetwork, public webrtc::VoERTP_RTCP,
      public webrtc::VoEVolumeControl {
 public:
  struct Channel {
    explicit Channel()
        : external_transport(false),
          send(false),
          playout(false),
          volume_scale(1.0),
          vad(false),
          codec_fec(false),
          max_encoding_bandwidth(0),
          opus_dtx(false),
          red(false),
          nack(false),
          cn8_type(13),
          cn16_type(105),
          red_type(117),
          nack_max_packets(0),
          send_ssrc(0),
          associate_send_channel(-1),
          recv_codecs(),
          neteq_capacity(-1),
          neteq_fast_accelerate(false) {
      memset(&send_codec, 0, sizeof(send_codec));
    }
    bool external_transport;
    bool send;
    bool playout;
    float volume_scale;
    bool vad;
    bool codec_fec;
    int max_encoding_bandwidth;
    bool opus_dtx;
    bool red;
    bool nack;
    int cn8_type;
    int cn16_type;
    int red_type;
    int nack_max_packets;
    uint32_t send_ssrc;
    int associate_send_channel;
    std::vector<webrtc::CodecInst> recv_codecs;
    webrtc::CodecInst send_codec;
    webrtc::PacketTime last_rtp_packet_time;
    std::list<std::string> packets;
    int neteq_capacity;
    bool neteq_fast_accelerate;
  };

  FakeWebRtcVoiceEngine()
      : inited_(false),
        last_channel_(-1),
        fail_create_channel_(false),
        num_set_send_codecs_(0),
        ec_enabled_(false),
        ec_metrics_enabled_(false),
        cng_enabled_(false),
        ns_enabled_(false),
        agc_enabled_(false),
        highpass_filter_enabled_(false),
        stereo_swapping_enabled_(false),
        typing_detection_enabled_(false),
        ec_mode_(webrtc::kEcDefault),
        aecm_mode_(webrtc::kAecmSpeakerphone),
        ns_mode_(webrtc::kNsDefault),
        agc_mode_(webrtc::kAgcDefault),
        observer_(NULL),
        playout_fail_channel_(-1),
        send_fail_channel_(-1),
        recording_sample_rate_(-1),
        playout_sample_rate_(-1) {
    memset(&agc_config_, 0, sizeof(agc_config_));
  }
  ~FakeWebRtcVoiceEngine() {
    RTC_CHECK(channels_.empty());
  }

  bool ec_metrics_enabled() const { return ec_metrics_enabled_; }

  bool IsInited() const { return inited_; }
  int GetLastChannel() const { return last_channel_; }
  int GetNumChannels() const { return static_cast<int>(channels_.size()); }
  uint32_t GetLocalSSRC(int channel) {
    return channels_[channel]->send_ssrc;
  }
  bool GetPlayout(int channel) {
    return channels_[channel]->playout;
  }
  bool GetSend(int channel) {
    return channels_[channel]->send;
  }
  bool GetVAD(int channel) {
    return channels_[channel]->vad;
  }
  bool GetOpusDtx(int channel) {
    return channels_[channel]->opus_dtx;
  }
  bool GetRED(int channel) {
    return channels_[channel]->red;
  }
  bool GetCodecFEC(int channel) {
    return channels_[channel]->codec_fec;
  }
  int GetMaxEncodingBandwidth(int channel) {
    return channels_[channel]->max_encoding_bandwidth;
  }
  bool GetNACK(int channel) {
    return channels_[channel]->nack;
  }
  int GetNACKMaxPackets(int channel) {
    return channels_[channel]->nack_max_packets;
  }
  const webrtc::PacketTime& GetLastRtpPacketTime(int channel) {
    RTC_DCHECK(channels_.find(channel) != channels_.end());
    return channels_[channel]->last_rtp_packet_time;
  }
  int GetSendCNPayloadType(int channel, bool wideband) {
    return (wideband) ?
        channels_[channel]->cn16_type :
        channels_[channel]->cn8_type;
  }
  int GetSendREDPayloadType(int channel) {
    return channels_[channel]->red_type;
  }
  bool CheckPacket(int channel, const void* data, size_t len) {
    bool result = !CheckNoPacket(channel);
    if (result) {
      std::string packet = channels_[channel]->packets.front();
      result = (packet == std::string(static_cast<const char*>(data), len));
      channels_[channel]->packets.pop_front();
    }
    return result;
  }
  bool CheckNoPacket(int channel) {
    return channels_[channel]->packets.empty();
  }
  void TriggerCallbackOnError(int channel_num, int err_code) {
    RTC_DCHECK(observer_ != NULL);
    observer_->CallbackOnError(channel_num, err_code);
  }
  void set_playout_fail_channel(int channel) {
    playout_fail_channel_ = channel;
  }
  void set_send_fail_channel(int channel) {
    send_fail_channel_ = channel;
  }
  void set_fail_create_channel(bool fail_create_channel) {
    fail_create_channel_ = fail_create_channel;
  }
  int AddChannel(const webrtc::Config& config) {
    if (fail_create_channel_) {
      return -1;
    }
    Channel* ch = new Channel();
    auto db = webrtc::acm2::RentACodec::Database();
    ch->recv_codecs.assign(db.begin(), db.end());
    if (config.Get<webrtc::NetEqCapacityConfig>().enabled) {
      ch->neteq_capacity = config.Get<webrtc::NetEqCapacityConfig>().capacity;
    }
    ch->neteq_fast_accelerate =
        config.Get<webrtc::NetEqFastAccelerate>().enabled;
    channels_[++last_channel_] = ch;
    return last_channel_;
  }

  int GetNumSetSendCodecs() const { return num_set_send_codecs_; }

  int GetAssociateSendChannel(int channel) {
    return channels_[channel]->associate_send_channel;
  }

  WEBRTC_STUB(Release, ());

  // webrtc::VoEBase
  WEBRTC_FUNC(RegisterVoiceEngineObserver, (
      webrtc::VoiceEngineObserver& observer)) {
    observer_ = &observer;
    return 0;
  }
  WEBRTC_STUB(DeRegisterVoiceEngineObserver, ());
  WEBRTC_FUNC(Init, (webrtc::AudioDeviceModule* adm,
                     webrtc::AudioProcessing* audioproc)) {
    inited_ = true;
    return 0;
  }
  WEBRTC_FUNC(Terminate, ()) {
    inited_ = false;
    return 0;
  }
  webrtc::AudioProcessing* audio_processing() override {
    return &audio_processing_;
  }
  WEBRTC_FUNC(CreateChannel, ()) {
    webrtc::Config empty_config;
    return AddChannel(empty_config);
  }
  WEBRTC_FUNC(CreateChannel, (const webrtc::Config& config)) {
    return AddChannel(config);
  }
  WEBRTC_FUNC(DeleteChannel, (int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    for (const auto& ch : channels_) {
      if (ch.second->associate_send_channel == channel) {
        ch.second->associate_send_channel = -1;
      }
    }
    delete channels_[channel];
    channels_.erase(channel);
    return 0;
  }
  WEBRTC_STUB(StartReceive, (int channel));
  WEBRTC_FUNC(StartPlayout, (int channel)) {
    if (playout_fail_channel_ != channel) {
      WEBRTC_CHECK_CHANNEL(channel);
      channels_[channel]->playout = true;
      return 0;
    } else {
      // When playout_fail_channel_ == channel, fail the StartPlayout on this
      // channel.
      return -1;
    }
  }
  WEBRTC_FUNC(StartSend, (int channel)) {
    if (send_fail_channel_ != channel) {
      WEBRTC_CHECK_CHANNEL(channel);
      channels_[channel]->send = true;
      return 0;
    } else {
      // When send_fail_channel_ == channel, fail the StartSend on this
      // channel.
      return -1;
    }
  }
  WEBRTC_STUB(StopReceive, (int channel));
  WEBRTC_FUNC(StopPlayout, (int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->playout = false;
    return 0;
  }
  WEBRTC_FUNC(StopSend, (int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->send = false;
    return 0;
  }
  WEBRTC_STUB(GetVersion, (char version[1024]));
  WEBRTC_STUB(LastError, ());
  WEBRTC_FUNC(AssociateSendChannel, (int channel,
                                     int accociate_send_channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->associate_send_channel = accociate_send_channel;
    return 0;
  }
  webrtc::RtcEventLog* GetEventLog() { return nullptr; }

  // webrtc::VoECodec
  WEBRTC_STUB(NumOfCodecs, ());
  WEBRTC_STUB(GetCodec, (int index, webrtc::CodecInst& codec));
  WEBRTC_FUNC(SetSendCodec, (int channel, const webrtc::CodecInst& codec)) {
    WEBRTC_CHECK_CHANNEL(channel);
    // To match the behavior of the real implementation.
    if (_stricmp(codec.plname, "telephone-event") == 0 ||
        _stricmp(codec.plname, "audio/telephone-event") == 0 ||
        _stricmp(codec.plname, "CN") == 0 ||
        _stricmp(codec.plname, "red") == 0 ) {
      return -1;
    }
    channels_[channel]->send_codec = codec;
    ++num_set_send_codecs_;
    return 0;
  }
  WEBRTC_FUNC(GetSendCodec, (int channel, webrtc::CodecInst& codec)) {
    WEBRTC_CHECK_CHANNEL(channel);
    codec = channels_[channel]->send_codec;
    return 0;
  }
  WEBRTC_STUB(SetBitRate, (int channel, int bitrate_bps));
  WEBRTC_STUB(GetRecCodec, (int channel, webrtc::CodecInst& codec));
  WEBRTC_FUNC(SetRecPayloadType, (int channel,
                                  const webrtc::CodecInst& codec)) {
    WEBRTC_CHECK_CHANNEL(channel);
    Channel* ch = channels_[channel];
    if (ch->playout)
      return -1;  // Channel is in use.
    // Check if something else already has this slot.
    if (codec.pltype != -1) {
      for (std::vector<webrtc::CodecInst>::iterator it =
          ch->recv_codecs.begin(); it != ch->recv_codecs.end(); ++it) {
        if (it->pltype == codec.pltype &&
            _stricmp(it->plname, codec.plname) != 0) {
          return -1;
        }
      }
    }
    // Otherwise try to find this codec and update its payload type.
    int result = -1;  // not found
    for (std::vector<webrtc::CodecInst>::iterator it = ch->recv_codecs.begin();
         it != ch->recv_codecs.end(); ++it) {
      if (strcmp(it->plname, codec.plname) == 0 &&
          it->plfreq == codec.plfreq &&
          it->channels == codec.channels) {
        it->pltype = codec.pltype;
        result = 0;
      }
    }
    return result;
  }
  WEBRTC_FUNC(SetSendCNPayloadType, (int channel, int type,
                                     webrtc::PayloadFrequencies frequency)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (frequency == webrtc::kFreq8000Hz) {
      channels_[channel]->cn8_type = type;
    } else if (frequency == webrtc::kFreq16000Hz) {
      channels_[channel]->cn16_type = type;
    }
    return 0;
  }
  WEBRTC_FUNC(GetRecPayloadType, (int channel, webrtc::CodecInst& codec)) {
    WEBRTC_CHECK_CHANNEL(channel);
    Channel* ch = channels_[channel];
    for (std::vector<webrtc::CodecInst>::iterator it = ch->recv_codecs.begin();
         it != ch->recv_codecs.end(); ++it) {
      if (strcmp(it->plname, codec.plname) == 0 &&
          it->plfreq == codec.plfreq &&
          it->channels == codec.channels &&
          it->pltype != -1) {
        codec.pltype = it->pltype;
        return 0;
      }
    }
    return -1;  // not found
  }
  WEBRTC_FUNC(SetVADStatus, (int channel, bool enable, webrtc::VadModes mode,
                             bool disableDTX)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (channels_[channel]->send_codec.channels == 2) {
      // Replicating VoE behavior; VAD cannot be enabled for stereo.
      return -1;
    }
    channels_[channel]->vad = enable;
    return 0;
  }
  WEBRTC_STUB(GetVADStatus, (int channel, bool& enabled,
                             webrtc::VadModes& mode, bool& disabledDTX));

  WEBRTC_FUNC(SetFECStatus, (int channel, bool enable)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (_stricmp(channels_[channel]->send_codec.plname, "opus") != 0) {
      // Return -1 if current send codec is not Opus.
      // TODO(minyue): Excludes other codecs if they support inband FEC.
      return -1;
    }
    channels_[channel]->codec_fec = enable;
    return 0;
  }
  WEBRTC_FUNC(GetFECStatus, (int channel, bool& enable)) {
    WEBRTC_CHECK_CHANNEL(channel);
    enable = channels_[channel]->codec_fec;
    return 0;
  }

  WEBRTC_FUNC(SetOpusMaxPlaybackRate, (int channel, int frequency_hz)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (_stricmp(channels_[channel]->send_codec.plname, "opus") != 0) {
      // Return -1 if current send codec is not Opus.
      return -1;
    }
    if (frequency_hz <= 8000)
      channels_[channel]->max_encoding_bandwidth = kOpusBandwidthNb;
    else if (frequency_hz <= 12000)
      channels_[channel]->max_encoding_bandwidth = kOpusBandwidthMb;
    else if (frequency_hz <= 16000)
      channels_[channel]->max_encoding_bandwidth = kOpusBandwidthWb;
    else if (frequency_hz <= 24000)
      channels_[channel]->max_encoding_bandwidth = kOpusBandwidthSwb;
    else
      channels_[channel]->max_encoding_bandwidth = kOpusBandwidthFb;
    return 0;
  }

  WEBRTC_FUNC(SetOpusDtx, (int channel, bool enable_dtx)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (_stricmp(channels_[channel]->send_codec.plname, "opus") != 0) {
      // Return -1 if current send codec is not Opus.
      return -1;
    }
    channels_[channel]->opus_dtx = enable_dtx;
    return 0;
  }

  // webrtc::VoEHardware
  WEBRTC_STUB(GetNumOfRecordingDevices, (int& num));
  WEBRTC_STUB(GetNumOfPlayoutDevices, (int& num));
  WEBRTC_STUB(GetRecordingDeviceName, (int i, char* name, char* guid));
  WEBRTC_STUB(GetPlayoutDeviceName, (int i, char* name, char* guid));
  WEBRTC_STUB(SetRecordingDevice, (int, webrtc::StereoChannel));
  WEBRTC_STUB(SetPlayoutDevice, (int));
  WEBRTC_STUB(SetAudioDeviceLayer, (webrtc::AudioLayers));
  WEBRTC_STUB(GetAudioDeviceLayer, (webrtc::AudioLayers&));
  WEBRTC_FUNC(SetRecordingSampleRate, (unsigned int samples_per_sec)) {
    recording_sample_rate_ = samples_per_sec;
    return 0;
  }
  WEBRTC_FUNC_CONST(RecordingSampleRate, (unsigned int* samples_per_sec)) {
    *samples_per_sec = recording_sample_rate_;
    return 0;
  }
  WEBRTC_FUNC(SetPlayoutSampleRate, (unsigned int samples_per_sec)) {
    playout_sample_rate_ = samples_per_sec;
    return 0;
  }
  WEBRTC_FUNC_CONST(PlayoutSampleRate, (unsigned int* samples_per_sec)) {
    *samples_per_sec = playout_sample_rate_;
    return 0;
  }
  WEBRTC_STUB(EnableBuiltInAEC, (bool enable));
  virtual bool BuiltInAECIsAvailable() const { return false; }
  WEBRTC_STUB(EnableBuiltInAGC, (bool enable));
  virtual bool BuiltInAGCIsAvailable() const { return false; }
  WEBRTC_STUB(EnableBuiltInNS, (bool enable));
  virtual bool BuiltInNSIsAvailable() const { return false; }

  // webrtc::VoENetwork
  WEBRTC_FUNC(RegisterExternalTransport, (int channel,
                                          webrtc::Transport& transport)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->external_transport = true;
    return 0;
  }
  WEBRTC_FUNC(DeRegisterExternalTransport, (int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->external_transport = false;
    return 0;
  }
  WEBRTC_FUNC(ReceivedRTPPacket, (int channel, const void* data,
                                  size_t length)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (!channels_[channel]->external_transport) return -1;
    channels_[channel]->packets.push_back(
        std::string(static_cast<const char*>(data), length));
    return 0;
  }
  WEBRTC_FUNC(ReceivedRTPPacket, (int channel, const void* data,
                                  size_t length,
                                  const webrtc::PacketTime& packet_time)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (ReceivedRTPPacket(channel, data, length) == -1) {
      return -1;
    }
    channels_[channel]->last_rtp_packet_time = packet_time;
    return 0;
  }

  WEBRTC_STUB(ReceivedRTCPPacket, (int channel, const void* data,
                                   size_t length));

  // webrtc::VoERTP_RTCP
  WEBRTC_FUNC(SetLocalSSRC, (int channel, unsigned int ssrc)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->send_ssrc = ssrc;
    return 0;
  }
  WEBRTC_STUB(GetLocalSSRC, (int channel, unsigned int& ssrc));
  WEBRTC_STUB(GetRemoteSSRC, (int channel, unsigned int& ssrc));
  WEBRTC_STUB(SetSendAudioLevelIndicationStatus, (int channel, bool enable,
      unsigned char id));
  WEBRTC_STUB(SetReceiveAudioLevelIndicationStatus, (int channel, bool enable,
      unsigned char id));
  WEBRTC_STUB(SetSendAbsoluteSenderTimeStatus, (int channel, bool enable,
      unsigned char id));
  WEBRTC_STUB(SetReceiveAbsoluteSenderTimeStatus, (int channel, bool enable,
      unsigned char id));
  WEBRTC_STUB(SetRTCPStatus, (int channel, bool enable));
  WEBRTC_STUB(GetRTCPStatus, (int channel, bool& enabled));
  WEBRTC_STUB(SetRTCP_CNAME, (int channel, const char cname[256]));
  WEBRTC_STUB(GetRTCP_CNAME, (int channel, char cname[256]));
  WEBRTC_STUB(GetRemoteRTCP_CNAME, (int channel, char* cname));
  WEBRTC_STUB(GetRemoteRTCPData, (int channel, unsigned int& NTPHigh,
                                  unsigned int& NTPLow,
                                  unsigned int& timestamp,
                                  unsigned int& playoutTimestamp,
                                  unsigned int* jitter,
                                  unsigned short* fractionLost));
  WEBRTC_STUB(GetRemoteRTCPReportBlocks,
              (int channel, std::vector<webrtc::ReportBlock>* receive_blocks));
  WEBRTC_STUB(GetRTPStatistics, (int channel, unsigned int& averageJitterMs,
                                 unsigned int& maxJitterMs,
                                 unsigned int& discardedPackets));
  WEBRTC_STUB(GetRTCPStatistics, (int channel, webrtc::CallStatistics& stats));
  WEBRTC_FUNC(SetREDStatus, (int channel, bool enable, int redPayloadtype)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->red = enable;
    channels_[channel]->red_type = redPayloadtype;
    return 0;
  }
  WEBRTC_FUNC(GetREDStatus, (int channel, bool& enable, int& redPayloadtype)) {
    WEBRTC_CHECK_CHANNEL(channel);
    enable = channels_[channel]->red;
    redPayloadtype = channels_[channel]->red_type;
    return 0;
  }
  WEBRTC_FUNC(SetNACKStatus, (int channel, bool enable, int maxNoPackets)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->nack = enable;
    channels_[channel]->nack_max_packets = maxNoPackets;
    return 0;
  }

  // webrtc::VoEVolumeControl
  WEBRTC_STUB(SetSpeakerVolume, (unsigned int));
  WEBRTC_STUB(GetSpeakerVolume, (unsigned int&));
  WEBRTC_STUB(SetMicVolume, (unsigned int));
  WEBRTC_STUB(GetMicVolume, (unsigned int&));
  WEBRTC_STUB(SetInputMute, (int, bool));
  WEBRTC_STUB(GetInputMute, (int, bool&));
  WEBRTC_STUB(GetSpeechInputLevel, (unsigned int&));
  WEBRTC_STUB(GetSpeechOutputLevel, (int, unsigned int&));
  WEBRTC_STUB(GetSpeechInputLevelFullRange, (unsigned int&));
  WEBRTC_STUB(GetSpeechOutputLevelFullRange, (int, unsigned int&));
  WEBRTC_FUNC(SetChannelOutputVolumeScaling, (int channel, float scale)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->volume_scale= scale;
    return 0;
  }
  WEBRTC_FUNC(GetChannelOutputVolumeScaling, (int channel, float& scale)) {
    WEBRTC_CHECK_CHANNEL(channel);
    scale = channels_[channel]->volume_scale;
    return 0;
  }
  WEBRTC_STUB(SetOutputVolumePan, (int channel, float left, float right));
  WEBRTC_STUB(GetOutputVolumePan, (int channel, float& left, float& right));

  // webrtc::VoEAudioProcessing
  WEBRTC_FUNC(SetNsStatus, (bool enable, webrtc::NsModes mode)) {
    ns_enabled_ = enable;
    ns_mode_ = mode;
    return 0;
  }
  WEBRTC_FUNC(GetNsStatus, (bool& enabled, webrtc::NsModes& mode)) {
    enabled = ns_enabled_;
    mode = ns_mode_;
    return 0;
  }

  WEBRTC_FUNC(SetAgcStatus, (bool enable, webrtc::AgcModes mode)) {
    agc_enabled_ = enable;
    agc_mode_ = mode;
    return 0;
  }
  WEBRTC_FUNC(GetAgcStatus, (bool& enabled, webrtc::AgcModes& mode)) {
    enabled = agc_enabled_;
    mode = agc_mode_;
    return 0;
  }

  WEBRTC_FUNC(SetAgcConfig, (webrtc::AgcConfig config)) {
    agc_config_ = config;
    return 0;
  }
  WEBRTC_FUNC(GetAgcConfig, (webrtc::AgcConfig& config)) {
    config = agc_config_;
    return 0;
  }
  WEBRTC_FUNC(SetEcStatus, (bool enable, webrtc::EcModes mode)) {
    ec_enabled_ = enable;
    ec_mode_ = mode;
    return 0;
  }
  WEBRTC_FUNC(GetEcStatus, (bool& enabled, webrtc::EcModes& mode)) {
    enabled = ec_enabled_;
    mode = ec_mode_;
    return 0;
  }
  WEBRTC_STUB(EnableDriftCompensation, (bool enable))
  WEBRTC_BOOL_STUB(DriftCompensationEnabled, ())
  WEBRTC_VOID_STUB(SetDelayOffsetMs, (int offset))
  WEBRTC_STUB(DelayOffsetMs, ());
  WEBRTC_FUNC(SetAecmMode, (webrtc::AecmModes mode, bool enableCNG)) {
    aecm_mode_ = mode;
    cng_enabled_ = enableCNG;
    return 0;
  }
  WEBRTC_FUNC(GetAecmMode, (webrtc::AecmModes& mode, bool& enabledCNG)) {
    mode = aecm_mode_;
    enabledCNG = cng_enabled_;
    return 0;
  }
  WEBRTC_STUB(SetRxNsStatus, (int channel, bool enable, webrtc::NsModes mode));
  WEBRTC_STUB(GetRxNsStatus, (int channel, bool& enabled,
                              webrtc::NsModes& mode));
  WEBRTC_STUB(SetRxAgcStatus, (int channel, bool enable,
                               webrtc::AgcModes mode));
  WEBRTC_STUB(GetRxAgcStatus, (int channel, bool& enabled,
                               webrtc::AgcModes& mode));
  WEBRTC_STUB(SetRxAgcConfig, (int channel, webrtc::AgcConfig config));
  WEBRTC_STUB(GetRxAgcConfig, (int channel, webrtc::AgcConfig& config));

  WEBRTC_STUB(RegisterRxVadObserver, (int, webrtc::VoERxVadCallback&));
  WEBRTC_STUB(DeRegisterRxVadObserver, (int channel));
  WEBRTC_STUB(VoiceActivityIndicator, (int channel));
  WEBRTC_FUNC(SetEcMetricsStatus, (bool enable)) {
    ec_metrics_enabled_ = enable;
    return 0;
  }
  WEBRTC_STUB(GetEcMetricsStatus, (bool& enabled));
  WEBRTC_STUB(GetEchoMetrics, (int& ERL, int& ERLE, int& RERL, int& A_NLP));
  WEBRTC_STUB(GetEcDelayMetrics, (int& delay_median, int& delay_std,
      float& fraction_poor_delays));

  WEBRTC_STUB(StartDebugRecording, (const char* fileNameUTF8));
  WEBRTC_STUB(StartDebugRecording, (FILE* handle));
  WEBRTC_STUB(StopDebugRecording, ());

  WEBRTC_FUNC(SetTypingDetectionStatus, (bool enable)) {
    typing_detection_enabled_ = enable;
    return 0;
  }
  WEBRTC_FUNC(GetTypingDetectionStatus, (bool& enabled)) {
    enabled = typing_detection_enabled_;
    return 0;
  }

  WEBRTC_STUB(TimeSinceLastTyping, (int& seconds));
  WEBRTC_STUB(SetTypingDetectionParameters, (int timeWindow,
                                             int costPerTyping,
                                             int reportingThreshold,
                                             int penaltyDecay,
                                             int typeEventDelay));
  int EnableHighPassFilter(bool enable) {
    highpass_filter_enabled_ = enable;
    return 0;
  }
  bool IsHighPassFilterEnabled() {
    return highpass_filter_enabled_;
  }
  bool IsStereoChannelSwappingEnabled() {
    return stereo_swapping_enabled_;
  }
  void EnableStereoChannelSwapping(bool enable) {
    stereo_swapping_enabled_ = enable;
  }
  int GetNetEqCapacity() const {
    auto ch = channels_.find(last_channel_);
    ASSERT(ch != channels_.end());
    return ch->second->neteq_capacity;
  }
  bool GetNetEqFastAccelerate() const {
    auto ch = channels_.find(last_channel_);
    ASSERT(ch != channels_.end());
    return ch->second->neteq_fast_accelerate;
  }

 private:
  bool inited_;
  int last_channel_;
  std::map<int, Channel*> channels_;
  bool fail_create_channel_;
  int num_set_send_codecs_;  // how many times we call SetSendCodec().
  bool ec_enabled_;
  bool ec_metrics_enabled_;
  bool cng_enabled_;
  bool ns_enabled_;
  bool agc_enabled_;
  bool highpass_filter_enabled_;
  bool stereo_swapping_enabled_;
  bool typing_detection_enabled_;
  webrtc::EcModes ec_mode_;
  webrtc::AecmModes aecm_mode_;
  webrtc::NsModes ns_mode_;
  webrtc::AgcModes agc_mode_;
  webrtc::AgcConfig agc_config_;
  webrtc::VoiceEngineObserver* observer_;
  int playout_fail_channel_;
  int send_fail_channel_;
  int recording_sample_rate_;
  int playout_sample_rate_;
  FakeAudioProcessing audio_processing_;
};

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_FAKEWEBRTCVOICEENGINE_H_
