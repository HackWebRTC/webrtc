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
#include "talk/media/base/voiceprocessor.h"
#include "talk/media/webrtc/fakewebrtccommon.h"
#include "talk/media/webrtc/webrtcvoe.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/config.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace cricket {

// Function returning stats will return these values
// for all values based on type.
const int kIntStatValue = 123;
const float kFractionLostStatValue = 0.5;

static const char kFakeDefaultDeviceName[] = "Fake Default";
static const int kFakeDefaultDeviceId = -1;
static const char kFakeDeviceName[] = "Fake Device";
#ifdef WIN32
static const int kFakeDeviceId = 0;
#else
static const int kFakeDeviceId = 1;
#endif

static const int kOpusBandwidthNb = 4000;
static const int kOpusBandwidthMb = 6000;
static const int kOpusBandwidthWb = 8000;
static const int kOpusBandwidthSwb = 12000;
static const int kOpusBandwidthFb = 20000;

static const webrtc::NetworkStatistics kNetStats = {
    1,  // uint16_t currentBufferSize;
    2,  // uint16_t preferredBufferSize;
    true,  // bool jitterPeaksFound;
    1234,  // uint16_t currentPacketLossRate;
    567,   // uint16_t currentDiscardRate;
    8901,  // uint16_t currentExpandRate;
    234,  // uint16_t currentSpeechExpandRate;
    5678, // uint16_t currentPreemptiveRate;
    9012, // uint16_t currentAccelerateRate;
    3456, // uint16_t currentSecondaryDecodedRate;
    7890, // int32_t clockDriftPPM;
    54,  // meanWaitingTimeMs;
    32,  // int medianWaitingTimeMs;
    1,  // int minWaitingTimeMs;
    98, // int maxWaitingTimeMs;
    7654,  // int addedSamples;
};  // These random but non-trivial numbers are used for testing.

#define WEBRTC_CHECK_CHANNEL(channel) \
  if (channels_.find(channel) == channels_.end()) return -1;

#define WEBRTC_ASSERT_CHANNEL(channel) \
  DCHECK(channels_.find(channel) != channels_.end());

// Verify the header extension ID, if enabled, is within the bounds specified in
// [RFC5285]: 1-14 inclusive.
#define WEBRTC_CHECK_HEADER_EXTENSION_ID(enable, id) \
  do { \
    if (enable && (id < 1 || id > 14)) { \
      return -1; \
    } \
  } while (0);

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

  WEBRTC_STUB(set_sample_rate_hz, (int rate));
  WEBRTC_STUB_CONST(input_sample_rate_hz, ());
  WEBRTC_STUB_CONST(sample_rate_hz, ());
  WEBRTC_STUB_CONST(proc_sample_rate_hz, ());
  WEBRTC_STUB_CONST(proc_split_sample_rate_hz, ());
  WEBRTC_STUB_CONST(num_input_channels, ());
  WEBRTC_STUB_CONST(num_output_channels, ());
  WEBRTC_STUB_CONST(num_reverse_channels, ());
  WEBRTC_VOID_STUB(set_output_will_be_muted, (bool muted));
  WEBRTC_BOOL_STUB_CONST(output_will_be_muted, ());
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
  WEBRTC_BOOL_STUB_CONST(stream_key_pressed, ());
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
      public webrtc::VoEBase, public webrtc::VoECodec, public webrtc::VoEDtmf,
      public webrtc::VoEFile, public webrtc::VoEHardware,
      public webrtc::VoEExternalMedia, public webrtc::VoENetEqStats,
      public webrtc::VoENetwork, public webrtc::VoERTP_RTCP,
      public webrtc::VoEVideoSync, public webrtc::VoEVolumeControl {
 public:
  struct DtmfInfo {
    DtmfInfo()
      : dtmf_event_code(-1),
        dtmf_out_of_band(false),
        dtmf_length_ms(-1) {}
    int dtmf_event_code;
    bool dtmf_out_of_band;
    int dtmf_length_ms;
  };
  struct Channel {
    explicit Channel()
        : external_transport(false),
          send(false),
          playout(false),
          volume_scale(1.0),
          volume_pan_left(1.0),
          volume_pan_right(1.0),
          file(false),
          vad(false),
          codec_fec(false),
          max_encoding_bandwidth(0),
          opus_dtx(false),
          red(false),
          nack(false),
          media_processor_registered(false),
          rx_agc_enabled(false),
          rx_agc_mode(webrtc::kAgcDefault),
          cn8_type(13),
          cn16_type(105),
          dtmf_type(106),
          red_type(117),
          nack_max_packets(0),
          send_ssrc(0),
          send_audio_level_ext_(-1),
          receive_audio_level_ext_(-1),
          send_absolute_sender_time_ext_(-1),
          receive_absolute_sender_time_ext_(-1),
          associate_send_channel(-1),
          neteq_capacity(-1),
          neteq_fast_accelerate(false) {
      memset(&send_codec, 0, sizeof(send_codec));
      memset(&rx_agc_config, 0, sizeof(rx_agc_config));
    }
    bool external_transport;
    bool send;
    bool playout;
    float volume_scale;
    float volume_pan_left;
    float volume_pan_right;
    bool file;
    bool vad;
    bool codec_fec;
    int max_encoding_bandwidth;
    bool opus_dtx;
    bool red;
    bool nack;
    bool media_processor_registered;
    bool rx_agc_enabled;
    webrtc::AgcModes rx_agc_mode;
    webrtc::AgcConfig rx_agc_config;
    int cn8_type;
    int cn16_type;
    int dtmf_type;
    int red_type;
    int nack_max_packets;
    uint32 send_ssrc;
    int send_audio_level_ext_;
    int receive_audio_level_ext_;
    int send_absolute_sender_time_ext_;
    int receive_absolute_sender_time_ext_;
    int associate_send_channel;
    DtmfInfo dtmf_info;
    std::vector<webrtc::CodecInst> recv_codecs;
    webrtc::CodecInst send_codec;
    webrtc::PacketTime last_rtp_packet_time;
    std::list<std::string> packets;
    int neteq_capacity;
    bool neteq_fast_accelerate;
  };

  FakeWebRtcVoiceEngine(const cricket::AudioCodec* const* codecs,
                        int num_codecs)
      : inited_(false),
        last_channel_(-1),
        fail_create_channel_(false),
        codecs_(codecs),
        num_codecs_(num_codecs),
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
        fail_start_recording_microphone_(false),
        recording_microphone_(false),
        recording_sample_rate_(-1),
        playout_sample_rate_(-1),
        media_processor_(NULL) {
    memset(&agc_config_, 0, sizeof(agc_config_));
  }
  ~FakeWebRtcVoiceEngine() {
    // Ought to have all been deleted by the WebRtcVoiceMediaChannel
    // destructors, but just in case ...
    for (std::map<int, Channel*>::const_iterator i = channels_.begin();
         i != channels_.end(); ++i) {
      delete i->second;
    }
  }

  bool IsExternalMediaProcessorRegistered() const {
    return media_processor_ != NULL;
  }
  bool IsInited() const { return inited_; }
  int GetLastChannel() const { return last_channel_; }
  int GetChannelFromLocalSsrc(uint32 local_ssrc) const {
    for (std::map<int, Channel*>::const_iterator iter = channels_.begin();
         iter != channels_.end(); ++iter) {
      if (local_ssrc == iter->second->send_ssrc)
        return iter->first;
    }
    return -1;
  }
  int GetNumChannels() const { return static_cast<int>(channels_.size()); }
  bool GetPlayout(int channel) {
    return channels_[channel]->playout;
  }
  bool GetSend(int channel) {
    return channels_[channel]->send;
  }
  bool GetRecordingMicrophone() {
    return recording_microphone_;
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
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_[channel]->last_rtp_packet_time;
  }
  int GetSendCNPayloadType(int channel, bool wideband) {
    return (wideband) ?
        channels_[channel]->cn16_type :
        channels_[channel]->cn8_type;
  }
  int GetSendTelephoneEventPayloadType(int channel) {
    return channels_[channel]->dtmf_type;
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
    DCHECK(observer_ != NULL);
    observer_->CallbackOnError(channel_num, err_code);
  }
  void set_playout_fail_channel(int channel) {
    playout_fail_channel_ = channel;
  }
  void set_send_fail_channel(int channel) {
    send_fail_channel_ = channel;
  }
  void set_fail_start_recording_microphone(
      bool fail_start_recording_microphone) {
    fail_start_recording_microphone_ = fail_start_recording_microphone;
  }
  void set_fail_create_channel(bool fail_create_channel) {
    fail_create_channel_ = fail_create_channel;
  }
  void TriggerProcessPacket(MediaProcessorDirection direction) {
    webrtc::ProcessingTypes pt =
        (direction == cricket::MPD_TX) ?
            webrtc::kRecordingPerChannel : webrtc::kPlaybackAllChannelsMixed;
    if (media_processor_ != NULL) {
      media_processor_->Process(0,
                                pt,
                                NULL,
                                0,
                                0,
                                true);
    }
  }
  int AddChannel(const webrtc::Config& config) {
    if (fail_create_channel_) {
      return -1;
    }
    Channel* ch = new Channel();
    for (int i = 0; i < NumOfCodecs(); ++i) {
      webrtc::CodecInst codec;
      GetCodec(i, codec);
      ch->recv_codecs.push_back(codec);
    }
    if (config.Get<webrtc::NetEqCapacityConfig>().enabled) {
      ch->neteq_capacity = config.Get<webrtc::NetEqCapacityConfig>().capacity;
    }
    ch->neteq_fast_accelerate =
        config.Get<webrtc::NetEqFastAccelerate>().enabled;
    channels_[++last_channel_] = ch;
    return last_channel_;
  }
  int GetSendRtpExtensionId(int channel, const std::string& extension) {
    WEBRTC_ASSERT_CHANNEL(channel);
    if (extension == kRtpAudioLevelHeaderExtension) {
      return channels_[channel]->send_audio_level_ext_;
    } else if (extension == kRtpAbsoluteSenderTimeHeaderExtension) {
      return channels_[channel]->send_absolute_sender_time_ext_;
    }
    return -1;
  }
  int GetReceiveRtpExtensionId(int channel, const std::string& extension) {
    WEBRTC_ASSERT_CHANNEL(channel);
    if (extension == kRtpAudioLevelHeaderExtension) {
      return channels_[channel]->receive_audio_level_ext_;
    } else if (extension == kRtpAbsoluteSenderTimeHeaderExtension) {
      return channels_[channel]->receive_absolute_sender_time_ext_;
    }
    return -1;
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

  // webrtc::VoECodec
  WEBRTC_FUNC(NumOfCodecs, ()) {
    return num_codecs_;
  }
  WEBRTC_FUNC(GetCodec, (int index, webrtc::CodecInst& codec)) {
    if (index < 0 || index >= NumOfCodecs()) {
      return -1;
    }
    const cricket::AudioCodec& c(*codecs_[index]);
    codec.pltype = c.id;
    rtc::strcpyn(codec.plname, sizeof(codec.plname), c.name.c_str());
    codec.plfreq = c.clockrate;
    codec.pacsize = 0;
    codec.channels = c.channels;
    codec.rate = c.bitrate;
    return 0;
  }
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
  WEBRTC_FUNC(GetRecCodec, (int channel, webrtc::CodecInst& codec)) {
    WEBRTC_CHECK_CHANNEL(channel);
    const Channel* c = channels_[channel];
    for (std::list<std::string>::const_iterator it_packet = c->packets.begin();
        it_packet != c->packets.end(); ++it_packet) {
      int pltype;
      if (!GetRtpPayloadType(it_packet->data(), it_packet->length(), &pltype)) {
        continue;
      }
      for (std::vector<webrtc::CodecInst>::const_iterator it_codec =
          c->recv_codecs.begin(); it_codec != c->recv_codecs.end();
          ++it_codec) {
        if (it_codec->pltype == pltype) {
          codec = *it_codec;
          return 0;
        }
      }
    }
    return -1;
  }
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
    for (std::vector<webrtc::CodecInst>::iterator it = ch->recv_codecs.begin();
         it != ch->recv_codecs.end(); ++it) {
      if (strcmp(it->plname, codec.plname) == 0 &&
          it->plfreq == codec.plfreq) {
        it->pltype = codec.pltype;
        it->channels = codec.channels;
        return 0;
      }
    }
    return -1;  // not found
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

  // webrtc::VoEDtmf
  WEBRTC_FUNC(SendTelephoneEvent, (int channel, int event_code,
      bool out_of_band = true, int length_ms = 160, int attenuation_db = 10)) {
    channels_[channel]->dtmf_info.dtmf_event_code = event_code;
    channels_[channel]->dtmf_info.dtmf_out_of_band = out_of_band;
    channels_[channel]->dtmf_info.dtmf_length_ms = length_ms;
    return 0;
  }

  WEBRTC_FUNC(SetSendTelephoneEventPayloadType,
      (int channel, unsigned char type)) {
    channels_[channel]->dtmf_type = type;
    return 0;
  };
  WEBRTC_STUB(GetSendTelephoneEventPayloadType,
      (int channel, unsigned char& type));

  WEBRTC_STUB(SetDtmfFeedbackStatus, (bool enable, bool directFeedback));
  WEBRTC_STUB(GetDtmfFeedbackStatus, (bool& enabled, bool& directFeedback));

  WEBRTC_FUNC(PlayDtmfTone,
      (int event_code, int length_ms = 200, int attenuation_db = 10)) {
    dtmf_info_.dtmf_event_code = event_code;
    dtmf_info_.dtmf_length_ms = length_ms;
    return 0;
  }

  // webrtc::VoEFile
  WEBRTC_FUNC(StartPlayingFileLocally, (int channel, const char* fileNameUTF8,
                                        bool loop, webrtc::FileFormats format,
                                        float volumeScaling, int startPointMs,
                                        int stopPointMs)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->file = true;
    return 0;
  }
  WEBRTC_FUNC(StartPlayingFileLocally, (int channel, webrtc::InStream* stream,
                                        webrtc::FileFormats format,
                                        float volumeScaling, int startPointMs,
                                        int stopPointMs)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->file = true;
    return 0;
  }
  WEBRTC_FUNC(StopPlayingFileLocally, (int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->file = false;
    return 0;
  }
  WEBRTC_FUNC(IsPlayingFileLocally, (int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    return (channels_[channel]->file) ? 1 : 0;
  }
  WEBRTC_STUB(StartPlayingFileAsMicrophone, (int channel,
                                             const char* fileNameUTF8,
                                             bool loop,
                                             bool mixWithMicrophone,
                                             webrtc::FileFormats format,
                                             float volumeScaling));
  WEBRTC_STUB(StartPlayingFileAsMicrophone, (int channel,
                                             webrtc::InStream* stream,
                                             bool mixWithMicrophone,
                                             webrtc::FileFormats format,
                                             float volumeScaling));
  WEBRTC_STUB(StopPlayingFileAsMicrophone, (int channel));
  WEBRTC_STUB(IsPlayingFileAsMicrophone, (int channel));
  WEBRTC_STUB(StartRecordingPlayout, (int channel, const char* fileNameUTF8,
                                      webrtc::CodecInst* compression,
                                      int maxSizeBytes));
  WEBRTC_STUB(StartRecordingPlayout, (int channel, webrtc::OutStream* stream,
                                      webrtc::CodecInst* compression));
  WEBRTC_STUB(StopRecordingPlayout, (int channel));
  WEBRTC_FUNC(StartRecordingMicrophone, (const char* fileNameUTF8,
                                         webrtc::CodecInst* compression,
                                         int maxSizeBytes)) {
    if (fail_start_recording_microphone_) {
      return -1;
    }
    recording_microphone_ = true;
    return 0;
  }
  WEBRTC_FUNC(StartRecordingMicrophone, (webrtc::OutStream* stream,
                                         webrtc::CodecInst* compression)) {
    if (fail_start_recording_microphone_) {
      return -1;
    }
    recording_microphone_ = true;
    return 0;
  }
  WEBRTC_FUNC(StopRecordingMicrophone, ()) {
    if (!recording_microphone_) {
      return -1;
    }
    recording_microphone_ = false;
    return 0;
  }

  // webrtc::VoEHardware
  WEBRTC_FUNC(GetNumOfRecordingDevices, (int& num)) {
    return GetNumDevices(num);
  }
  WEBRTC_FUNC(GetNumOfPlayoutDevices, (int& num)) {
    return GetNumDevices(num);
  }
  WEBRTC_FUNC(GetRecordingDeviceName, (int i, char* name, char* guid)) {
    return GetDeviceName(i, name, guid);
  }
  WEBRTC_FUNC(GetPlayoutDeviceName, (int i, char* name, char* guid)) {
    return GetDeviceName(i, name, guid);
  }
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

  // webrtc::VoENetEqStats
  WEBRTC_FUNC(GetNetworkStatistics, (int channel,
                                     webrtc::NetworkStatistics& ns)) {
    WEBRTC_CHECK_CHANNEL(channel);
    memcpy(&ns, &kNetStats, sizeof(webrtc::NetworkStatistics));
    return 0;
  }

  WEBRTC_FUNC_CONST(GetDecodingCallStatistics, (int channel,
      webrtc::AudioDecodingCallStats*)) {
    WEBRTC_CHECK_CHANNEL(channel);
    return 0;
  }

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
  WEBRTC_FUNC(GetLocalSSRC, (int channel, unsigned int& ssrc)) {
    WEBRTC_CHECK_CHANNEL(channel);
    ssrc = channels_[channel]->send_ssrc;
    return 0;
  }
  WEBRTC_STUB(GetRemoteSSRC, (int channel, unsigned int& ssrc));
  WEBRTC_FUNC(SetSendAudioLevelIndicationStatus, (int channel, bool enable,
      unsigned char id)) {
    WEBRTC_CHECK_CHANNEL(channel);
    WEBRTC_CHECK_HEADER_EXTENSION_ID(enable, id);
    channels_[channel]->send_audio_level_ext_ = (enable) ? id : -1;
    return 0;
  }
  WEBRTC_FUNC(SetReceiveAudioLevelIndicationStatus, (int channel, bool enable,
      unsigned char id)) {
    WEBRTC_CHECK_CHANNEL(channel);
    WEBRTC_CHECK_HEADER_EXTENSION_ID(enable, id);
    channels_[channel]->receive_audio_level_ext_ = (enable) ? id : -1;
   return 0;
  }
  WEBRTC_FUNC(SetSendAbsoluteSenderTimeStatus, (int channel, bool enable,
      unsigned char id)) {
    WEBRTC_CHECK_CHANNEL(channel);
    WEBRTC_CHECK_HEADER_EXTENSION_ID(enable, id);
    channels_[channel]->send_absolute_sender_time_ext_ = (enable) ? id : -1;
    return 0;
  }
  WEBRTC_FUNC(SetReceiveAbsoluteSenderTimeStatus, (int channel, bool enable,
      unsigned char id)) {
    WEBRTC_CHECK_CHANNEL(channel);
    WEBRTC_CHECK_HEADER_EXTENSION_ID(enable, id);
    channels_[channel]->receive_absolute_sender_time_ext_ = (enable) ? id : -1;
    return 0;
  }

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
  WEBRTC_FUNC(GetRemoteRTCPReportBlocks,
              (int channel, std::vector<webrtc::ReportBlock>* receive_blocks)) {
    WEBRTC_CHECK_CHANNEL(channel);
    webrtc::ReportBlock block;
    block.source_SSRC = channels_[channel]->send_ssrc;
    webrtc::CodecInst send_codec = channels_[channel]->send_codec;
    if (send_codec.pltype >= 0) {
      block.fraction_lost = (unsigned char)(kFractionLostStatValue * 256);
      if (send_codec.plfreq / 1000 > 0) {
        block.interarrival_jitter = kIntStatValue * (send_codec.plfreq / 1000);
      }
      block.cumulative_num_packets_lost = kIntStatValue;
      block.extended_highest_sequence_number = kIntStatValue;
      receive_blocks->push_back(block);
    }
    return 0;
  }
  WEBRTC_STUB(GetRTPStatistics, (int channel, unsigned int& averageJitterMs,
                                 unsigned int& maxJitterMs,
                                 unsigned int& discardedPackets));
  WEBRTC_FUNC(GetRTCPStatistics, (int channel, webrtc::CallStatistics& stats)) {
    WEBRTC_CHECK_CHANNEL(channel);
    stats.fractionLost = static_cast<int16>(kIntStatValue);
    stats.cumulativeLost = kIntStatValue;
    stats.extendedMax = kIntStatValue;
    stats.jitterSamples = kIntStatValue;
    stats.rttMs = kIntStatValue;
    stats.bytesSent = kIntStatValue;
    stats.packetsSent = kIntStatValue;
    stats.bytesReceived = kIntStatValue;
    stats.packetsReceived = kIntStatValue;
    return 0;
  }
  WEBRTC_FUNC(SetREDStatus, (int channel, bool enable, int redPayloadtype)) {
    return SetFECStatus(channel, enable, redPayloadtype);
  }
  // TODO(minyue): remove the below function when transition to SetREDStatus
  //               is finished.
  WEBRTC_FUNC(SetFECStatus, (int channel, bool enable, int redPayloadtype)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->red = enable;
    channels_[channel]->red_type = redPayloadtype;
    return 0;
  }
  WEBRTC_FUNC(GetREDStatus, (int channel, bool& enable, int& redPayloadtype)) {
    return GetFECStatus(channel, enable, redPayloadtype);
  }
  // TODO(minyue): remove the below function when transition to GetREDStatus
  //               is finished.
  WEBRTC_FUNC(GetFECStatus, (int channel, bool& enable, int& redPayloadtype)) {
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

  // webrtc::VoEVideoSync
  WEBRTC_STUB(GetPlayoutBufferSize, (int& bufferMs));
  WEBRTC_STUB(GetPlayoutTimestamp, (int channel, unsigned int& timestamp));
  WEBRTC_STUB(GetRtpRtcp, (int, webrtc::RtpRtcp**, webrtc::RtpReceiver**));
  WEBRTC_STUB(SetInitTimestamp, (int channel, unsigned int timestamp));
  WEBRTC_STUB(SetInitSequenceNumber, (int channel, short sequenceNumber));
  WEBRTC_STUB(SetMinimumPlayoutDelay, (int channel, int delayMs));
  WEBRTC_STUB(SetInitialPlayoutDelay, (int channel, int delay_ms));
  WEBRTC_STUB(GetDelayEstimate, (int channel, int* jitter_buffer_delay_ms,
                                 int* playout_buffer_delay_ms));
  WEBRTC_STUB_CONST(GetLeastRequiredDelayMs, (int channel));

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
  WEBRTC_FUNC(SetOutputVolumePan, (int channel, float left, float right)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->volume_pan_left = left;
    channels_[channel]->volume_pan_right = right;
    return 0;
  }
  WEBRTC_FUNC(GetOutputVolumePan, (int channel, float& left, float& right)) {
    WEBRTC_CHECK_CHANNEL(channel);
    left = channels_[channel]->volume_pan_left;
    right = channels_[channel]->volume_pan_right;
    return 0;
  }

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
  WEBRTC_FUNC(SetRxAgcStatus, (int channel, bool enable,
                               webrtc::AgcModes mode)) {
    channels_[channel]->rx_agc_enabled = enable;
    channels_[channel]->rx_agc_mode = mode;
    return 0;
  }
  WEBRTC_FUNC(GetRxAgcStatus, (int channel, bool& enabled,
                               webrtc::AgcModes& mode)) {
    enabled = channels_[channel]->rx_agc_enabled;
    mode = channels_[channel]->rx_agc_mode;
    return 0;
  }

  WEBRTC_FUNC(SetRxAgcConfig, (int channel, webrtc::AgcConfig config)) {
    channels_[channel]->rx_agc_config = config;
    return 0;
  }
  WEBRTC_FUNC(GetRxAgcConfig, (int channel, webrtc::AgcConfig& config)) {
    config = channels_[channel]->rx_agc_config;
    return 0;
  }

  WEBRTC_STUB(RegisterRxVadObserver, (int, webrtc::VoERxVadCallback&));
  WEBRTC_STUB(DeRegisterRxVadObserver, (int channel));
  WEBRTC_STUB(VoiceActivityIndicator, (int channel));
  WEBRTC_FUNC(SetEcMetricsStatus, (bool enable)) {
    ec_metrics_enabled_ = enable;
    return 0;
  }
  WEBRTC_FUNC(GetEcMetricsStatus, (bool& enabled)) {
    enabled = ec_metrics_enabled_;
    return 0;
  }
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
  bool WasSendTelephoneEventCalled(int channel, int event_code, int length_ms) {
    return (channels_[channel]->dtmf_info.dtmf_event_code == event_code &&
            channels_[channel]->dtmf_info.dtmf_out_of_band == true &&
            channels_[channel]->dtmf_info.dtmf_length_ms == length_ms);
  }
  bool WasPlayDtmfToneCalled(int event_code, int length_ms) {
    return (dtmf_info_.dtmf_event_code == event_code &&
            dtmf_info_.dtmf_length_ms == length_ms);
  }
  // webrtc::VoEExternalMedia
  WEBRTC_FUNC(RegisterExternalMediaProcessing,
              (int channel, webrtc::ProcessingTypes type,
               webrtc::VoEMediaProcess& processObject)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (channels_[channel]->media_processor_registered) {
      return -1;
    }
    channels_[channel]->media_processor_registered = true;
    media_processor_ = &processObject;
    return 0;
  }
  WEBRTC_FUNC(DeRegisterExternalMediaProcessing,
              (int channel, webrtc::ProcessingTypes type)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (!channels_[channel]->media_processor_registered) {
      return -1;
    }
    channels_[channel]->media_processor_registered = false;
    media_processor_ = NULL;
    return 0;
  }
  WEBRTC_STUB(GetAudioFrame, (int channel, int desired_sample_rate_hz,
                              webrtc::AudioFrame* frame));
  WEBRTC_STUB(SetExternalMixing, (int channel, bool enable));
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
  int GetNumDevices(int& num) {
#ifdef WIN32
    num = 1;
#else
    // On non-Windows platforms VE adds a special entry for the default device,
    // so if there is one physical device then there are two entries in the
    // list.
    num = 2;
#endif
    return 0;
  }

  int GetDeviceName(int i, char* name, char* guid) {
    const char *s;
#ifdef WIN32
    if (0 == i) {
      s = kFakeDeviceName;
    } else {
      return -1;
    }
#else
    // See comment above.
    if (0 == i) {
      s = kFakeDefaultDeviceName;
    } else if (1 == i) {
      s = kFakeDeviceName;
    } else {
      return -1;
    }
#endif
    strcpy(name, s);
    guid[0] = '\0';
    return 0;
  }

  bool inited_;
  int last_channel_;
  std::map<int, Channel*> channels_;
  bool fail_create_channel_;
  const cricket::AudioCodec* const* codecs_;
  int num_codecs_;
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
  bool fail_start_recording_microphone_;
  bool recording_microphone_;
  int recording_sample_rate_;
  int playout_sample_rate_;
  DtmfInfo dtmf_info_;
  webrtc::VoEMediaProcess* media_processor_;
  FakeAudioProcessing audio_processing_;
};

#undef WEBRTC_CHECK_HEADER_EXTENSION_ID

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_FAKEWEBRTCVOICEENGINE_H_
