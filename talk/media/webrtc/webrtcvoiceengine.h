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
#include <set>
#include <string>
#include <vector>

#include "talk/media/base/rtputils.h"
#include "talk/media/webrtc/webrtccommon.h"
#include "talk/media/webrtc/webrtcvoe.h"
#include "talk/session/media/channel.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/byteorder.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/call.h"
#include "webrtc/common.h"
#include "webrtc/config.h"

namespace webrtc {
class VideoEngine;
}

namespace cricket {

// WebRtcSoundclipStream is an adapter object that allows a memory stream to be
// passed into WebRtc, and support looping.
class WebRtcSoundclipStream : public webrtc::InStream {
 public:
  WebRtcSoundclipStream(const char* buf, size_t len)
      : mem_(buf, len), loop_(true) {
  }
  void set_loop(bool loop) { loop_ = loop; }

  int Read(void* buf, size_t len) override;
  int Rewind() override;

 private:
  rtc::MemoryStream mem_;
  bool loop_;
};

// WebRtcMonitorStream is used to monitor a stream coming from WebRtc.
// For now we just dump the data.
class WebRtcMonitorStream : public webrtc::OutStream {
  bool Write(const void* buf, size_t len) override { return true; }
};

class AudioDeviceModule;
class AudioRenderer;
class VoETraceWrapper;
class VoEWrapper;
class VoiceProcessor;
class WebRtcVoiceMediaChannel;

// WebRtcVoiceEngine is a class to be used with CompositeMediaEngine.
// It uses the WebRtc VoiceEngine library for audio handling.
class WebRtcVoiceEngine
    : public webrtc::VoiceEngineObserver,
      public webrtc::TraceCallback,
      public webrtc::VoEMediaProcess  {
  friend class WebRtcVoiceMediaChannel;

 public:
  WebRtcVoiceEngine();
  // Dependency injection for testing.
  WebRtcVoiceEngine(VoEWrapper* voe_wrapper, VoETraceWrapper* tracing);
  ~WebRtcVoiceEngine();
  bool Init(rtc::Thread* worker_thread);
  void Terminate();

  int GetCapabilities();
  VoiceMediaChannel* CreateChannel(const AudioOptions& options);

  AudioOptions GetOptions() const { return options_; }
  bool SetOptions(const AudioOptions& options);
  bool SetDelayOffset(int offset);
  bool SetDevices(const Device* in_device, const Device* out_device);
  bool GetOutputVolume(int* level);
  bool SetOutputVolume(int level);
  int GetInputLevel();
  bool SetLocalMonitor(bool enable);

  const std::vector<AudioCodec>& codecs();
  bool FindCodec(const AudioCodec& codec);
  bool FindWebRtcCodec(const AudioCodec& codec, webrtc::CodecInst* gcodec);

  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const;

  void SetLogging(int min_sev, const char* filter);

  bool RegisterProcessor(uint32 ssrc,
                         VoiceProcessor* voice_processor,
                         MediaProcessorDirection direction);
  bool UnregisterProcessor(uint32 ssrc,
                           VoiceProcessor* voice_processor,
                           MediaProcessorDirection direction);

  // Method from webrtc::VoEMediaProcess
  void Process(int channel,
               webrtc::ProcessingTypes type,
               int16_t audio10ms[],
               size_t length,
               int sampling_freq,
               bool is_stereo) override;

  // For tracking WebRtc channels. Needed because we have to pause them
  // all when switching devices.
  // May only be called by WebRtcVoiceMediaChannel.
  void RegisterChannel(WebRtcVoiceMediaChannel *channel);
  void UnregisterChannel(WebRtcVoiceMediaChannel *channel);

  // Called by WebRtcVoiceMediaChannel to set a gain offset from
  // the default AGC target level.
  bool AdjustAgcLevel(int delta);

  VoEWrapper* voe() { return voe_wrapper_.get(); }
  int GetLastEngineError();

  // Set the external ADM. This can only be called before Init.
  bool SetAudioDeviceModule(webrtc::AudioDeviceModule* adm);

  // Starts AEC dump using existing file.
  bool StartAecDump(rtc::PlatformFile file);

  // Check whether the supplied trace should be ignored.
  bool ShouldIgnoreTrace(const std::string& trace);

  // Create a VoiceEngine Channel.
  int CreateMediaVoiceChannel();

 private:
  typedef std::vector<WebRtcVoiceMediaChannel*> ChannelList;
  typedef sigslot::
      signal3<uint32, MediaProcessorDirection, AudioFrame*> FrameSignal;

  void Construct();
  void ConstructCodecs();
  bool GetVoeCodec(int index, webrtc::CodecInst* codec);
  bool InitInternal();
  void SetTraceFilter(int filter);
  void SetTraceOptions(const std::string& options);
  // Applies either options or overrides.  Every option that is "set"
  // will be applied.  Every option not "set" will be ignored.  This
  // allows us to selectively turn on and off different options easily
  // at any time.
  bool ApplyOptions(const AudioOptions& options);
  // Overrides, when set, take precedence over the options on a
  // per-option basis.  For example, if AGC is set in options and AEC
  // is set in overrides, AGC and AEC will be both be set.  Overrides
  // can also turn off options.  For example, if AGC is set to "on" in
  // options and AGC is set to "off" in overrides, the result is that
  // AGC will be off until different overrides are applied or until
  // the overrides are cleared.  Only one set of overrides is present
  // at a time (they do not "stack").  And when the overrides are
  // cleared, the media engine's state reverts back to the options set
  // via SetOptions.  This allows us to have both "persistent options"
  // (the normal options) and "temporary options" (overrides).
  bool SetOptionOverrides(const AudioOptions& options);
  bool ClearOptionOverrides();

  // webrtc::TraceCallback:
  void Print(webrtc::TraceLevel level, const char* trace, int length) override;

  // webrtc::VoiceEngineObserver:
  void CallbackOnError(int channel, int errCode) override;

  // Given the device type, name, and id, find device id. Return true and
  // set the output parameter rtc_id if successful.
  bool FindWebRtcAudioDeviceId(
      bool is_input, const std::string& dev_name, int dev_id, int* rtc_id);
  bool FindChannelAndSsrc(int channel_num,
                          WebRtcVoiceMediaChannel** channel,
                          uint32* ssrc) const;
  bool FindChannelNumFromSsrc(uint32 ssrc,
                              MediaProcessorDirection direction,
                              int* channel_num);
  bool ChangeLocalMonitor(bool enable);
  bool PauseLocalMonitor();
  bool ResumeLocalMonitor();

  bool UnregisterProcessorChannel(MediaProcessorDirection channel_direction,
                                  uint32 ssrc,
                                  VoiceProcessor* voice_processor,
                                  MediaProcessorDirection processor_direction);

  void StartAecDump(const std::string& filename);
  void StopAecDump();
  int CreateVoiceChannel(VoEWrapper* voe);

  // When a voice processor registers with the engine, it is connected
  // to either the Rx or Tx signals, based on the direction parameter.
  // SignalXXMediaFrame will be invoked for every audio packet.
  FrameSignal SignalRxMediaFrame;
  FrameSignal SignalTxMediaFrame;

  static const int kDefaultLogSeverity = rtc::LS_WARNING;

  // The primary instance of WebRtc VoiceEngine.
  rtc::scoped_ptr<VoEWrapper> voe_wrapper_;
  rtc::scoped_ptr<VoETraceWrapper> tracing_;
  // The external audio device manager
  webrtc::AudioDeviceModule* adm_;
  int log_filter_;
  std::string log_options_;
  bool is_dumping_aec_;
  std::vector<AudioCodec> codecs_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
  bool desired_local_monitor_enable_;
  rtc::scoped_ptr<WebRtcMonitorStream> monitor_;
  ChannelList channels_;
  // channels_ can be read from WebRtc callback thread. We need a lock on that
  // callback as well as the RegisterChannel/UnregisterChannel.
  rtc::CriticalSection channels_cs_;
  webrtc::AgcConfig default_agc_config_;

  webrtc::Config voe_config_;

  bool initialized_;
  // See SetOptions and SetOptionOverrides for a description of the
  // difference between options and overrides.
  // options_ are the base options, which combined with the
  // option_overrides_, create the current options being used.
  // options_ is stored so that when option_overrides_ is cleared, we
  // can restore the options_ without the option_overrides.
  AudioOptions options_;
  AudioOptions option_overrides_;

  // When the media processor registers with the engine, the ssrc is cached
  // here so that a look up need not be made when the callback is invoked.
  // This is necessary because the lookup results in mux_channels_cs lock being
  // held and if a remote participant leaves the hangout at the same time
  // we hit a deadlock.
  uint32 tx_processor_ssrc_;
  uint32 rx_processor_ssrc_;

  rtc::CriticalSection signal_media_critical_;

  // Cache received extended_filter_aec, delay_agnostic_aec and experimental_ns
  // values, and apply them in case they are missing in the audio options. We
  // need to do this because SetExtraOptions() will revert to defaults for
  // options which are not provided.
  Settable<bool> extended_filter_aec_;
  Settable<bool> delay_agnostic_aec_;
  Settable<bool> experimental_ns_;
};

// WebRtcVoiceMediaChannel is an implementation of VoiceMediaChannel that uses
// WebRtc Voice Engine.
class WebRtcVoiceMediaChannel : public VoiceMediaChannel,
                                public webrtc::Transport {
 public:
  explicit WebRtcVoiceMediaChannel(WebRtcVoiceEngine *engine);
  ~WebRtcVoiceMediaChannel() override;

  int voe_channel() const { return voe_channel_; }
  bool valid() const { return voe_channel_ != -1; }

  bool SetSendParameters(const AudioSendParameters& params) override;
  bool SetRecvParameters(const AudioRecvParameters& params) override;
  bool SetOptions(const AudioOptions& options) override;
  bool GetOptions(AudioOptions* options) const override {
    *options = options_;
    return true;
  }
  bool SetRecvCodecs(const std::vector<AudioCodec>& codecs) override;
  bool SetSendCodecs(const std::vector<AudioCodec>& codecs) override;
  bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) override;
  bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) override;
  bool SetPlayout(bool playout) override;
  bool PausePlayout();
  bool ResumePlayout();
  bool SetSend(SendFlags send) override;
  bool PauseSend();
  bool ResumeSend();
  bool AddSendStream(const StreamParams& sp) override;
  bool RemoveSendStream(uint32 ssrc) override;
  bool AddRecvStream(const StreamParams& sp) override;
  bool RemoveRecvStream(uint32 ssrc) override;
  bool SetRemoteRenderer(uint32 ssrc, AudioRenderer* renderer) override;
  bool SetLocalRenderer(uint32 ssrc, AudioRenderer* renderer) override;
  bool GetActiveStreams(AudioInfo::StreamList* actives) override;
  int GetOutputLevel() override;
  int GetTimeSinceLastTyping() override;
  void SetTypingDetectionParameters(int time_window,
                                    int cost_per_typing,
                                    int reporting_threshold,
                                    int penalty_decay,
                                    int type_event_delay) override;
  bool SetOutputScaling(uint32 ssrc, double left, double right) override;
  bool GetOutputScaling(uint32 ssrc, double* left, double* right) override;

  bool SetRingbackTone(const char* buf, int len) override;
  bool PlayRingbackTone(uint32 ssrc, bool play, bool loop) override;
  bool CanInsertDtmf() override;
  bool InsertDtmf(uint32 ssrc, int event, int duration, int flags) override;

  void OnPacketReceived(rtc::Buffer* packet,
                        const rtc::PacketTime& packet_time) override;
  void OnRtcpReceived(rtc::Buffer* packet,
                      const rtc::PacketTime& packet_time) override;
  void OnReadyToSend(bool ready) override {}
  bool MuteStream(uint32 ssrc, bool on) override;
  bool SetMaxSendBandwidth(int bps) override;
  bool GetStats(VoiceMediaInfo* info) override;
  // Gets last reported error from WebRtc voice engine.  This should be only
  // called in response a failure.
  void GetLastMediaError(uint32* ssrc,
                         VoiceMediaChannel::Error* error) override;

  // implements Transport interface
  int SendPacket(int channel, const void* data, size_t len) override {
    rtc::Buffer packet(reinterpret_cast<const uint8_t*>(data), len,
                       kMaxRtpPacketLen);
    return VoiceMediaChannel::SendPacket(&packet) ? static_cast<int>(len) : -1;
  }

  int SendRTCPPacket(int channel, const void* data, size_t len) override {
    rtc::Buffer packet(reinterpret_cast<const uint8_t*>(data), len,
                       kMaxRtpPacketLen);
    return VoiceMediaChannel::SendRtcp(&packet) ? static_cast<int>(len) : -1;
  }

  bool FindSsrc(int channel_num, uint32* ssrc);
  void OnError(uint32 ssrc, int error);

  bool sending() const { return send_ != SEND_NOTHING; }
  int GetReceiveChannelNum(uint32 ssrc) const;
  int GetSendChannelNum(uint32 ssrc) const;

  void SetCall(webrtc::Call* call);

 private:
  WebRtcVoiceEngine* engine() { return engine_; }
  int GetLastEngineError() { return engine()->GetLastEngineError(); }
  int GetOutputLevel(int channel);
  bool GetRedSendCodec(const AudioCodec& red_codec,
                       const std::vector<AudioCodec>& all_codecs,
                       webrtc::CodecInst* send_codec);
  bool EnableRtcp(int channel);
  bool ResetRecvCodecs(int channel);
  bool SetPlayout(int channel, bool playout);
  static uint32 ParseSsrc(const void* data, size_t len, bool rtcp);
  static Error WebRtcErrorToChannelError(int err_code);

  class WebRtcVoiceChannelRenderer;
  // Map of ssrc to WebRtcVoiceChannelRenderer object.  A new object of
  // WebRtcVoiceChannelRenderer will be created for every new stream and
  // will be destroyed when the stream goes away.
  typedef std::map<uint32, WebRtcVoiceChannelRenderer*> ChannelMap;
  typedef int (webrtc::VoERTP_RTCP::* ExtensionSetterFunction)(int, bool,
      unsigned char);

  void SetNack(int channel, bool nack_enabled);
  void SetNack(const ChannelMap& channels, bool nack_enabled);
  bool SetSendCodec(const webrtc::CodecInst& send_codec);
  bool SetSendCodec(int channel, const webrtc::CodecInst& send_codec);
  bool ChangePlayout(bool playout);
  bool ChangeSend(SendFlags send);
  bool ChangeSend(int channel, SendFlags send);
  void ConfigureSendChannel(int channel);
  bool ConfigureRecvChannel(int channel);
  bool DeleteChannel(int channel);
  bool InConferenceMode() const {
    return options_.conference_mode.GetWithDefaultIfUnset(false);
  }
  bool IsDefaultChannel(int channel_id) const {
    return channel_id == voe_channel();
  }
  bool SetSendCodecs(int channel, const std::vector<AudioCodec>& codecs);
  bool SetSendBitrateInternal(int bps);

  bool SetHeaderExtension(ExtensionSetterFunction setter, int channel_id,
                          const RtpHeaderExtension* extension);
  void TryAddAudioRecvStream(uint32 ssrc);
  void TryRemoveAudioRecvStream(uint32 ssrc);
  bool SetRecvCodecsInternal(const std::vector<AudioCodec>& new_codecs);

  bool SetChannelRecvRtpHeaderExtensions(
    int channel_id,
    const std::vector<RtpHeaderExtension>& extensions);
  bool SetChannelSendRtpHeaderExtensions(
    int channel_id,
    const std::vector<RtpHeaderExtension>& extensions);

  rtc::ThreadChecker thread_checker_;

  WebRtcVoiceEngine* engine_;
  const int voe_channel_;
  rtc::scoped_ptr<WebRtcSoundclipStream> ringback_tone_;
  std::set<int> ringback_channels_;  // channels playing ringback
  std::vector<AudioCodec> recv_codecs_;
  std::vector<AudioCodec> send_codecs_;
  rtc::scoped_ptr<webrtc::CodecInst> send_codec_;
  bool send_bitrate_setting_;
  int send_bitrate_bps_;
  AudioOptions options_;
  bool dtmf_allowed_;
  bool desired_playout_;
  bool nack_enabled_;
  bool playout_;
  bool typing_noise_detected_;
  SendFlags desired_send_;
  SendFlags send_;
  webrtc::Call* call_;

  // send_channels_ contains the channels which are being used for sending.
  // When the default channel (voe_channel) is used for sending, it is
  // contained in send_channels_, otherwise not.
  ChannelMap send_channels_;
  std::vector<RtpHeaderExtension> send_extensions_;
  uint32 default_receive_ssrc_;
  // Note the default channel (voe_channel()) can reside in both
  // receive_channels_ and send_channels_ in non-conference mode and in that
  // case it will only be there if a non-zero default_receive_ssrc_ is set.
  ChannelMap receive_channels_;  // for multiple sources
  std::map<uint32, webrtc::AudioReceiveStream*> receive_streams_;
  std::map<uint32, StreamParams> receive_stream_params_;
  // receive_channels_ can be read from WebRtc callback thread.  Access from
  // the WebRtc thread must be synchronized with edits on the worker thread.
  // Reads on the worker thread are ok.
  std::vector<RtpHeaderExtension> receive_extensions_;
  std::vector<webrtc::RtpExtension> recv_rtp_extensions_;

  // Do not lock this on the VoE media processor thread; potential for deadlock
  // exists.
  mutable rtc::CriticalSection receive_channels_cs_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCVOICEENGINE_H_
