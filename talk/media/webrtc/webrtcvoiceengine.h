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

#ifndef TALK_MEDIA_WEBRTCVOICEENGINE_H_
#define TALK_MEDIA_WEBRTCVOICEENGINE_H_

#include <map>
#include <string>
#include <vector>

#include "talk/media/base/rtputils.h"
#include "talk/media/webrtc/webrtccommon.h"
#include "talk/media/webrtc/webrtcvoe.h"
#include "talk/session/media/channel.h"
#include "webrtc/audio_state.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/call.h"
#include "webrtc/common.h"
#include "webrtc/config.h"

namespace cricket {

class AudioDeviceModule;
class AudioRenderer;
class VoEWrapper;
class WebRtcVoiceMediaChannel;

// WebRtcVoiceEngine is a class to be used with CompositeMediaEngine.
// It uses the WebRtc VoiceEngine library for audio handling.
class WebRtcVoiceEngine final : public webrtc::TraceCallback  {
  friend class WebRtcVoiceMediaChannel;
 public:
  // Exposed for the WVoE/MC unit test.
  static bool ToCodecInst(const AudioCodec& in, webrtc::CodecInst* out);

  WebRtcVoiceEngine();
  // Dependency injection for testing.
  explicit WebRtcVoiceEngine(VoEWrapper* voe_wrapper);
  ~WebRtcVoiceEngine();
  bool Init(rtc::Thread* worker_thread);
  void Terminate();

  rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const;
  VoiceMediaChannel* CreateChannel(webrtc::Call* call,
                                   const AudioOptions& options);

  bool GetOutputVolume(int* level);
  bool SetOutputVolume(int level);
  int GetInputLevel();

  const std::vector<AudioCodec>& codecs();
  RtpCapabilities GetCapabilities() const;

  // For tracking WebRtc channels. Needed because we have to pause them
  // all when switching devices.
  // May only be called by WebRtcVoiceMediaChannel.
  void RegisterChannel(WebRtcVoiceMediaChannel* channel);
  void UnregisterChannel(WebRtcVoiceMediaChannel* channel);

  // Called by WebRtcVoiceMediaChannel to set a gain offset from
  // the default AGC target level.
  bool AdjustAgcLevel(int delta);

  VoEWrapper* voe() { return voe_wrapper_.get(); }
  int GetLastEngineError();

  // Set the external ADM. This can only be called before Init.
  bool SetAudioDeviceModule(webrtc::AudioDeviceModule* adm);

  // Starts AEC dump using existing file.
  bool StartAecDump(rtc::PlatformFile file);

  // Stops AEC dump.
  void StopAecDump();

  // Starts recording an RtcEventLog using an existing file until 10 minutes
  // pass or the StopRtcEventLog function is called.
  bool StartRtcEventLog(rtc::PlatformFile file);

  // Stops recording the RtcEventLog.
  void StopRtcEventLog();

 private:
  void Construct();
  bool InitInternal();
  // Every option that is "set" will be applied. Every option not "set" will be
  // ignored. This allows us to selectively turn on and off different options
  // easily at any time.
  bool ApplyOptions(const AudioOptions& options);
  void SetDefaultDevices();

  // webrtc::TraceCallback:
  void Print(webrtc::TraceLevel level, const char* trace, int length) override;

  void StartAecDump(const std::string& filename);
  int CreateVoEChannel();

  rtc::ThreadChecker signal_thread_checker_;
  rtc::ThreadChecker worker_thread_checker_;

  // The primary instance of WebRtc VoiceEngine.
  rtc::scoped_ptr<VoEWrapper> voe_wrapper_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  // The external audio device manager
  webrtc::AudioDeviceModule* adm_ = nullptr;
  std::vector<AudioCodec> codecs_;
  std::vector<WebRtcVoiceMediaChannel*> channels_;
  webrtc::Config voe_config_;
  bool initialized_ = false;
  bool is_dumping_aec_ = false;

  webrtc::AgcConfig default_agc_config_;
  // Cache received extended_filter_aec, delay_agnostic_aec and experimental_ns
  // values, and apply them in case they are missing in the audio options. We
  // need to do this because SetExtraOptions() will revert to defaults for
  // options which are not provided.
  rtc::Optional<bool> extended_filter_aec_;
  rtc::Optional<bool> delay_agnostic_aec_;
  rtc::Optional<bool> experimental_ns_;

  RTC_DISALLOW_COPY_AND_ASSIGN(WebRtcVoiceEngine);
};

// WebRtcVoiceMediaChannel is an implementation of VoiceMediaChannel that uses
// WebRtc Voice Engine.
class WebRtcVoiceMediaChannel final : public VoiceMediaChannel,
                                      public webrtc::Transport {
 public:
  WebRtcVoiceMediaChannel(WebRtcVoiceEngine* engine,
                          const AudioOptions& options,
                          webrtc::Call* call);
  ~WebRtcVoiceMediaChannel() override;

  const AudioOptions& options() const { return options_; }

  bool SetSendParameters(const AudioSendParameters& params) override;
  bool SetRecvParameters(const AudioRecvParameters& params) override;
  bool SetPlayout(bool playout) override;
  bool PausePlayout();
  bool ResumePlayout();
  bool SetSend(SendFlags send) override;
  bool PauseSend();
  bool ResumeSend();
  bool SetAudioSend(uint32_t ssrc,
                    bool enable,
                    const AudioOptions* options,
                    AudioRenderer* renderer) override;
  bool AddSendStream(const StreamParams& sp) override;
  bool RemoveSendStream(uint32_t ssrc) override;
  bool AddRecvStream(const StreamParams& sp) override;
  bool RemoveRecvStream(uint32_t ssrc) override;
  bool GetActiveStreams(AudioInfo::StreamList* actives) override;
  int GetOutputLevel() override;
  int GetTimeSinceLastTyping() override;
  void SetTypingDetectionParameters(int time_window,
                                    int cost_per_typing,
                                    int reporting_threshold,
                                    int penalty_decay,
                                    int type_event_delay) override;
  bool SetOutputVolume(uint32_t ssrc, double volume) override;

  bool CanInsertDtmf() override;
  bool InsertDtmf(uint32_t ssrc, int event, int duration) override;

  void OnPacketReceived(rtc::Buffer* packet,
                        const rtc::PacketTime& packet_time) override;
  void OnRtcpReceived(rtc::Buffer* packet,
                      const rtc::PacketTime& packet_time) override;
  void OnReadyToSend(bool ready) override {}
  bool GetStats(VoiceMediaInfo* info) override;

  void SetRawAudioSink(
      uint32_t ssrc,
      rtc::scoped_ptr<webrtc::AudioSinkInterface> sink) override;

  // implements Transport interface
  bool SendRtp(const uint8_t* data,
               size_t len,
               const webrtc::PacketOptions& options) override {
    rtc::Buffer packet(reinterpret_cast<const uint8_t*>(data), len,
                       kMaxRtpPacketLen);
    rtc::PacketOptions rtc_options;
    rtc_options.packet_id = options.packet_id;
    return VoiceMediaChannel::SendPacket(&packet, rtc_options);
  }

  bool SendRtcp(const uint8_t* data, size_t len) override {
    rtc::Buffer packet(reinterpret_cast<const uint8_t*>(data), len,
                       kMaxRtpPacketLen);
    return VoiceMediaChannel::SendRtcp(&packet, rtc::PacketOptions());
  }

  int GetReceiveChannelId(uint32_t ssrc) const;
  int GetSendChannelId(uint32_t ssrc) const;

 private:
  bool SetSendCodecs(const std::vector<AudioCodec>& codecs);
  bool SetOptions(const AudioOptions& options);
  bool SetMaxSendBandwidth(int bps);
  bool SetRecvCodecs(const std::vector<AudioCodec>& codecs);
  bool SetLocalRenderer(uint32_t ssrc, AudioRenderer* renderer);
  bool MuteStream(uint32_t ssrc, bool mute);

  WebRtcVoiceEngine* engine() { return engine_; }
  int GetLastEngineError() { return engine()->GetLastEngineError(); }
  int GetOutputLevel(int channel);
  bool GetRedSendCodec(const AudioCodec& red_codec,
                       const std::vector<AudioCodec>& all_codecs,
                       webrtc::CodecInst* send_codec);
  bool SetPlayout(int channel, bool playout);
  void SetNack(int channel, bool nack_enabled);
  bool SetSendCodec(int channel, const webrtc::CodecInst& send_codec);
  bool ChangePlayout(bool playout);
  bool ChangeSend(SendFlags send);
  bool ChangeSend(int channel, SendFlags send);
  int CreateVoEChannel();
  bool DeleteVoEChannel(int channel);
  bool IsDefaultRecvStream(uint32_t ssrc) {
    return default_recv_ssrc_ == static_cast<int64_t>(ssrc);
  }
  bool SetSendCodecs(int channel, const std::vector<AudioCodec>& codecs);
  bool SetSendBitrateInternal(int bps);

  rtc::ThreadChecker worker_thread_checker_;

  WebRtcVoiceEngine* const engine_ = nullptr;
  std::vector<AudioCodec> recv_codecs_;
  std::vector<AudioCodec> send_codecs_;
  rtc::scoped_ptr<webrtc::CodecInst> send_codec_;
  bool send_bitrate_setting_ = false;
  int send_bitrate_bps_ = 0;
  AudioOptions options_;
  rtc::Optional<int> dtmf_payload_type_;
  bool desired_playout_ = false;
  bool nack_enabled_ = false;
  bool playout_ = false;
  SendFlags desired_send_ = SEND_NOTHING;
  SendFlags send_ = SEND_NOTHING;
  webrtc::Call* const call_ = nullptr;

  // SSRC of unsignalled receive stream, or -1 if there isn't one.
  int64_t default_recv_ssrc_ = -1;
  // Volume for unsignalled stream, which may be set before the stream exists.
  double default_recv_volume_ = 1.0;
  // Default SSRC to use for RTCP receiver reports in case of no signaled
  // send streams. See: https://code.google.com/p/webrtc/issues/detail?id=4740
  // and https://code.google.com/p/chromium/issues/detail?id=547661
  uint32_t receiver_reports_ssrc_ = 0xFA17FA17u;

  class WebRtcAudioSendStream;
  std::map<uint32_t, WebRtcAudioSendStream*> send_streams_;
  std::vector<webrtc::RtpExtension> send_rtp_extensions_;

  class WebRtcAudioReceiveStream;
  std::map<uint32_t, WebRtcAudioReceiveStream*> recv_streams_;
  std::vector<webrtc::RtpExtension> recv_rtp_extensions_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcVoiceMediaChannel);
};
}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCVOICEENGINE_H_
