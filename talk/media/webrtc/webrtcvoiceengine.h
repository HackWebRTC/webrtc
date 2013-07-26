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

#include "talk/base/buffer.h"
#include "talk/base/byteorder.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/stream.h"
#include "talk/media/base/rtputils.h"
#include "talk/media/webrtc/webrtccommon.h"
#include "talk/media/webrtc/webrtcexport.h"
#include "talk/media/webrtc/webrtcvoe.h"
#include "talk/session/media/channel.h"

#if !defined(LIBPEERCONNECTION_LIB) && \
    !defined(LIBPEERCONNECTION_IMPLEMENTATION)
#error "Bogus include."
#endif


namespace cricket {

// WebRtcSoundclipStream is an adapter object that allows a memory stream to be
// passed into WebRtc, and support looping.
class WebRtcSoundclipStream : public webrtc::InStream {
 public:
  WebRtcSoundclipStream(const char* buf, size_t len)
      : mem_(buf, len), loop_(true) {
  }
  void set_loop(bool loop) { loop_ = loop; }
  virtual int Read(void* buf, int len);
  virtual int Rewind();

 private:
  talk_base::MemoryStream mem_;
  bool loop_;
};

// WebRtcMonitorStream is used to monitor a stream coming from WebRtc.
// For now we just dump the data.
class WebRtcMonitorStream : public webrtc::OutStream {
  virtual bool Write(const void *buf, int len) {
    return true;
  }
};

class AudioDeviceModule;
class AudioRenderer;
class VoETraceWrapper;
class VoEWrapper;
class VoiceProcessor;
class WebRtcSoundclipMedia;
class WebRtcVoiceMediaChannel;

// WebRtcVoiceEngine is a class to be used with CompositeMediaEngine.
// It uses the WebRtc VoiceEngine library for audio handling.
class WebRtcVoiceEngine
    : public webrtc::VoiceEngineObserver,
      public webrtc::TraceCallback,
      public webrtc::VoEMediaProcess  {
 public:
  WebRtcVoiceEngine();
  // Dependency injection for testing.
  WebRtcVoiceEngine(VoEWrapper* voe_wrapper,
                    VoEWrapper* voe_wrapper_sc,
                    VoETraceWrapper* tracing);
  ~WebRtcVoiceEngine();
  bool Init(talk_base::Thread* worker_thread);
  void Terminate();

  int GetCapabilities();
  VoiceMediaChannel* CreateChannel();

  SoundclipMedia* CreateSoundclip();

  // TODO(pthatcher): Rename to SetOptions and replace the old
  // flags-based SetOptions.
  bool SetAudioOptions(const AudioOptions& options);
  // Eventually, we will replace them with AudioOptions.
  // In the meantime, we leave this here for backwards compat.
  bool SetOptions(int flags);
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
  virtual void Process(int channel,
                       webrtc::ProcessingTypes type,
                       int16_t audio10ms[],
                       int length,
                       int sampling_freq,
                       bool is_stereo);

  // For tracking WebRtc channels. Needed because we have to pause them
  // all when switching devices.
  // May only be called by WebRtcVoiceMediaChannel.
  void RegisterChannel(WebRtcVoiceMediaChannel *channel);
  void UnregisterChannel(WebRtcVoiceMediaChannel *channel);

  // May only be called by WebRtcSoundclipMedia.
  void RegisterSoundclip(WebRtcSoundclipMedia *channel);
  void UnregisterSoundclip(WebRtcSoundclipMedia *channel);

  // Called by WebRtcVoiceMediaChannel to set a gain offset from
  // the default AGC target level.
  bool AdjustAgcLevel(int delta);

  VoEWrapper* voe() { return voe_wrapper_.get(); }
  VoEWrapper* voe_sc() { return voe_wrapper_sc_.get(); }
  int GetLastEngineError();

  // Set the external ADMs. This can only be called before Init.
  bool SetAudioDeviceModule(webrtc::AudioDeviceModule* adm,
                            webrtc::AudioDeviceModule* adm_sc);

  // Check whether the supplied trace should be ignored.
  bool ShouldIgnoreTrace(const std::string& trace);

 private:
  typedef std::vector<WebRtcSoundclipMedia *> SoundclipList;
  typedef std::vector<WebRtcVoiceMediaChannel *> ChannelList;
  typedef sigslot::
      signal3<uint32, MediaProcessorDirection, AudioFrame*> FrameSignal;

  void Construct();
  void ConstructCodecs();
  bool InitInternal();
  void SetTraceFilter(int filter);
  void SetTraceOptions(const std::string& options);
  // Applies either options or overrides.  Every option that is "set"
  // will be applied.  Every option not "set" will be ignored.  This
  // allows us to selectively turn on and off different options easily
  // at any time.
  bool ApplyOptions(const AudioOptions& options);
  virtual void Print(webrtc::TraceLevel level, const char* trace, int length);
  virtual void CallbackOnError(int channel, int errCode);
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

  // When a voice processor registers with the engine, it is connected
  // to either the Rx or Tx signals, based on the direction parameter.
  // SignalXXMediaFrame will be invoked for every audio packet.
  FrameSignal SignalRxMediaFrame;
  FrameSignal SignalTxMediaFrame;

  static const int kDefaultLogSeverity = talk_base::LS_WARNING;

  // The primary instance of WebRtc VoiceEngine.
  talk_base::scoped_ptr<VoEWrapper> voe_wrapper_;
  // A secondary instance, for playing out soundclips (on the 'ring' device).
  talk_base::scoped_ptr<VoEWrapper> voe_wrapper_sc_;
  talk_base::scoped_ptr<VoETraceWrapper> tracing_;
  // The external audio device manager
  webrtc::AudioDeviceModule* adm_;
  webrtc::AudioDeviceModule* adm_sc_;
  int log_filter_;
  std::string log_options_;
  bool is_dumping_aec_;
  std::vector<AudioCodec> codecs_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
  bool desired_local_monitor_enable_;
  talk_base::scoped_ptr<WebRtcMonitorStream> monitor_;
  SoundclipList soundclips_;
  ChannelList channels_;
  // channels_ can be read from WebRtc callback thread. We need a lock on that
  // callback as well as the RegisterChannel/UnregisterChannel.
  talk_base::CriticalSection channels_cs_;
  webrtc::AgcConfig default_agc_config_;
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

  talk_base::CriticalSection signal_media_critical_;
};

// WebRtcMediaChannel is a class that implements the common WebRtc channel
// functionality.
template <class T, class E>
class WebRtcMediaChannel : public T, public webrtc::Transport {
 public:
  WebRtcMediaChannel(E *engine, int channel)
      : engine_(engine), voe_channel_(channel), sequence_number_(-1) {}
  E *engine() { return engine_; }
  int voe_channel() const { return voe_channel_; }
  bool valid() const { return voe_channel_ != -1; }

 protected:
  // implements Transport interface
  virtual int SendPacket(int channel, const void *data, int len) {
    // We need to store the sequence number to be able to pick up
    // the same sequence when the device is restarted.
    // TODO(oja): Remove when WebRtc has fixed the problem.
    int seq_num;
    if (!GetRtpSeqNum(data, len, &seq_num)) {
      return -1;
    }
    if (sequence_number() == -1) {
      LOG(INFO) << "WebRtcVoiceMediaChannel sends first packet seqnum="
                << seq_num;
    }

    talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
    if (!T::SendPacket(&packet)) {
      return -1;
    }
    sequence_number_ = seq_num;
    return len;
  }

  virtual int SendRTCPPacket(int channel, const void *data, int len) {
    talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
    return T::SendRtcp(&packet) ? len : -1;
  }

  int sequence_number() const {
    return sequence_number_;
  }

 private:
  E *engine_;
  int voe_channel_;
  int sequence_number_;
};

// WebRtcVoiceMediaChannel is an implementation of VoiceMediaChannel that uses
// WebRtc Voice Engine.
class WebRtcVoiceMediaChannel
    : public WebRtcMediaChannel<VoiceMediaChannel, WebRtcVoiceEngine> {
 public:
  explicit WebRtcVoiceMediaChannel(WebRtcVoiceEngine *engine);
  virtual ~WebRtcVoiceMediaChannel();
  virtual bool SetOptions(const AudioOptions& options);
  virtual bool GetOptions(AudioOptions* options) const {
    *options = options_;
    return true;
  }
  virtual bool SetRecvCodecs(const std::vector<AudioCodec> &codecs);
  virtual bool SetSendCodecs(const std::vector<AudioCodec> &codecs);
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions);
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions);
  virtual bool SetPlayout(bool playout);
  bool PausePlayout();
  bool ResumePlayout();
  virtual bool SetSend(SendFlags send);
  bool PauseSend();
  bool ResumeSend();
  virtual bool AddSendStream(const StreamParams& sp);
  virtual bool RemoveSendStream(uint32 ssrc);
  virtual bool AddRecvStream(const StreamParams& sp);
  virtual bool RemoveRecvStream(uint32 ssrc);
  virtual bool SetRemoteRenderer(uint32 ssrc, AudioRenderer* renderer);
  virtual bool SetLocalRenderer(uint32 ssrc, AudioRenderer* renderer);
  virtual bool GetActiveStreams(AudioInfo::StreamList* actives);
  virtual int GetOutputLevel();
  virtual int GetTimeSinceLastTyping();
  virtual void SetTypingDetectionParameters(int time_window,
      int cost_per_typing, int reporting_threshold, int penalty_decay,
      int type_event_delay);
  virtual bool SetOutputScaling(uint32 ssrc, double left, double right);
  virtual bool GetOutputScaling(uint32 ssrc, double* left, double* right);

  virtual bool SetRingbackTone(const char *buf, int len);
  virtual bool PlayRingbackTone(uint32 ssrc, bool play, bool loop);
  virtual bool CanInsertDtmf();
  virtual bool InsertDtmf(uint32 ssrc, int event, int duration, int flags);

  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet);
  virtual void OnReadyToSend(bool ready) {}
  virtual bool MuteStream(uint32 ssrc, bool on);
  virtual bool SetSendBandwidth(bool autobw, int bps);
  virtual bool GetStats(VoiceMediaInfo* info);
  // Gets last reported error from WebRtc voice engine.  This should be only
  // called in response a failure.
  virtual void GetLastMediaError(uint32* ssrc,
                                 VoiceMediaChannel::Error* error);
  bool FindSsrc(int channel_num, uint32* ssrc);
  void OnError(uint32 ssrc, int error);

  bool sending() const { return send_ != SEND_NOTHING; }
  int GetReceiveChannelNum(uint32 ssrc);
  int GetSendChannelNum(uint32 ssrc);

 protected:
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

 private:
  struct WebRtcVoiceChannelInfo;

  void SetNack(uint32 ssrc, int channel, bool nack_enabled);
  bool SetSendCodec(const webrtc::CodecInst& send_codec);
  bool ChangePlayout(bool playout);
  bool ChangeSend(SendFlags send);
  bool InConferenceMode() const {
    return options_.conference_mode.GetWithDefaultIfUnset(false);
  }

  typedef std::map<uint32, WebRtcVoiceChannelInfo> ChannelMap;
  talk_base::scoped_ptr<WebRtcSoundclipStream> ringback_tone_;
  std::set<int> ringback_channels_;  // channels playing ringback
  std::vector<AudioCodec> recv_codecs_;
  talk_base::scoped_ptr<webrtc::CodecInst> send_codec_;
  AudioOptions options_;
  bool dtmf_allowed_;
  bool desired_playout_;
  bool nack_enabled_;
  bool playout_;
  SendFlags desired_send_;
  SendFlags send_;

  // TODO(xians): Add support for multiple send channels.
  uint32 send_ssrc_;
  // Weak pointer to the renderer of the local audio track. It is owned by the
  // track and will set to NULL when the track is going away or channel gets
  // deleted. Used to notify the audio track that the media channel is added
  // or removed.
  AudioRenderer* local_renderer_;
  uint32 default_receive_ssrc_;
  // Note the default channel (voe_channel()) can reside in both
  // receive_channels_ and send channel in non-conference mode and in that case
  // it will only be there if a non-zero default_receive_ssrc_ is set.
  ChannelMap receive_channels_;  // for multiple sources
  // receive_channels_ can be read from WebRtc callback thread.  Access from
  // the WebRtc thread must be synchronized with edits on the worker thread.
  // Reads on the worker thread are ok.
  //
  // Do not lock this on the VoE media processor thread; potential for deadlock
  // exists.
  mutable talk_base::CriticalSection receive_channels_cs_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCVOICEENGINE_H_
