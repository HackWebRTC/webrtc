/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_SESSION_PHONE_WEBRTCVOICEENGINE_H_
#define TALK_SESSION_PHONE_WEBRTCVOICEENGINE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "talk/base/buffer.h"
#include "talk/base/byteorder.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/stream.h"
#include "talk/session/phone/channel.h"
#include "talk/session/phone/mediaengine.h"
#include "talk/session/phone/rtputils.h"
#include "talk/session/phone/webrtccommon.h"

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
class VoETraceWrapper;
class VoEWrapper;
class WebRtcSoundclipMedia;
class WebRtcVoiceMediaChannel;

// WebRtcVoiceEngine is a class to be used with CompositeMediaEngine.
// It uses the WebRtc VoiceEngine library for audio handling.
class WebRtcVoiceEngine
    : public webrtc::VoiceEngineObserver,
      public webrtc::TraceCallback {
 public:
  WebRtcVoiceEngine();
  WebRtcVoiceEngine(webrtc::AudioDeviceModule* adm,
                    webrtc::AudioDeviceModule* adm_sc);
  // Dependency injection for testing.
  WebRtcVoiceEngine(VoEWrapper* voe_wrapper,
                    VoEWrapper* voe_wrapper_sc,
                    VoETraceWrapper* tracing);
  ~WebRtcVoiceEngine();
  bool Init();
  void Terminate();

  int GetCapabilities();
  VoiceMediaChannel* CreateChannel();

  SoundclipMedia* CreateSoundclip();

  bool SetOptions(int options);
  bool SetDevices(const Device* in_device, const Device* out_device);
  bool GetOutputVolume(int* level);
  bool SetOutputVolume(int level);
  int GetInputLevel();
  bool SetLocalMonitor(bool enable);

  const std::vector<AudioCodec>& codecs();
  bool FindCodec(const AudioCodec& codec);
  bool FindWebRtcCodec(const AudioCodec& codec, webrtc::CodecInst* gcodec);

  void SetLogging(int min_sev, const char* filter);

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

  // Called by WebRtcVoiceMediaChannel to configure echo cancellation
  // and noise suppression modes.
  bool SetConferenceMode(bool enable);

  VoEWrapper* voe() { return voe_wrapper_.get(); }
  VoEWrapper* voe_sc() { return voe_wrapper_sc_.get(); }
  int GetLastEngineError();

 private:
  typedef std::vector<WebRtcSoundclipMedia *> SoundclipList;
  typedef std::vector<WebRtcVoiceMediaChannel *> ChannelList;

  struct CodecPref {
    const char* name;
    int clockrate;
  };

  void Construct();
  bool InitInternal();
  void ApplyLogging();
  virtual void Print(const webrtc::TraceLevel level,
                     const char* trace_string, const int length);
  virtual void CallbackOnError(const int channel, const int errCode);
  static int GetCodecPreference(const char *name, int clockrate);
  // Given the device type, name, and id, find device id. Return true and
  // set the output parameter rtc_id if successful.
  bool FindWebRtcAudioDeviceId(
      bool is_input, const std::string& dev_name, int dev_id, int* rtc_id);
  bool FindChannelAndSsrc(int channel_num,
                          WebRtcVoiceMediaChannel** channel,
                          uint32* ssrc) const;
  bool ChangeLocalMonitor(bool enable);
  bool PauseLocalMonitor();
  bool ResumeLocalMonitor();

  static const int kDefaultLogSeverity = talk_base::LS_WARNING;
  static const CodecPref kCodecPrefs[];

  // The primary instance of WebRtc VoiceEngine.
  talk_base::scoped_ptr<VoEWrapper> voe_wrapper_;
  // A secondary instance, for playing out soundclips (on the 'ring' device).
  talk_base::scoped_ptr<VoEWrapper> voe_wrapper_sc_;
  talk_base::scoped_ptr<VoETraceWrapper> tracing_;
  // The external audio device manager
  webrtc::AudioDeviceModule* adm_;
  webrtc::AudioDeviceModule* adm_sc_;
  int log_level_;
  bool is_dumping_aec_;
  std::vector<AudioCodec> codecs_;
  bool desired_local_monitor_enable_;
  talk_base::scoped_ptr<WebRtcMonitorStream> monitor_;
  SoundclipList soundclips_;
  ChannelList channels_;
  // channels_ can be read from WebRtc callback thread. We need a lock on that
  // callback as well as the RegisterChannel/UnregisterChannel.
  talk_base::CriticalSection channels_cs_;
  webrtc::AgcConfig default_agc_config_;
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
    if (!T::network_interface_) {
      return -1;
    }

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
    sequence_number_ = seq_num;

    talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
    return T::network_interface_->SendPacket(&packet) ? len : -1;
  }
  virtual int SendRTCPPacket(int channel, const void *data, int len) {
    if (!T::network_interface_) {
      return -1;
    }

    talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
    return T::network_interface_->SendRtcp(&packet) ? len : -1;
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
    : public WebRtcMediaChannel<VoiceMediaChannel,
                                WebRtcVoiceEngine> {
 public:
  explicit WebRtcVoiceMediaChannel(WebRtcVoiceEngine *engine);
  virtual ~WebRtcVoiceMediaChannel();
  virtual bool SetOptions(int options);
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
  virtual bool AddStream(uint32 ssrc);
  virtual bool RemoveStream(uint32 ssrc);
  virtual bool GetActiveStreams(AudioInfo::StreamList* actives);
  virtual int GetOutputLevel();

  virtual bool SetRingbackTone(const char *buf, int len);
  virtual bool PlayRingbackTone(uint32 ssrc, bool play, bool loop);
  virtual bool PressDTMF(int event, bool playout);

  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet);
  virtual void SetSendSsrc(uint32 id);
  virtual bool SetRtcpCName(const std::string& cname);
  virtual bool Mute(bool mute);
  virtual bool SetSendBandwidth(bool autobw, int bps) { return false; }
  virtual bool GetStats(VoiceMediaInfo* info);
  // Gets last reported error from WebRtc voice engine.  This should be only
  // called in response a failure.
  virtual void GetLastMediaError(uint32* ssrc,
                                 VoiceMediaChannel::Error* error);
  bool FindSsrc(int channel_num, uint32* ssrc);
  void OnError(uint32 ssrc, int error);

 protected:
  int GetLastEngineError() { return engine()->GetLastEngineError(); }
  int GetChannel(uint32 ssrc);
  int GetOutputLevel(int channel);
  bool GetRedSendCodec(const AudioCodec& red_codec,
                       const std::vector<AudioCodec>& all_codecs,
                       webrtc::CodecInst* send_codec);
  bool EnableRtcp(int channel);
  bool SetPlayout(int channel, bool playout);
  static uint32 ParseSsrc(const void* data, size_t len, bool rtcp);
  static Error WebRtcErrorToChannelError(int err_code);

 private:
  // Tandberg-bridged conferences require a -10dB gain adjustment,
  // which is actually +10 in AgcConfig.targetLeveldBOv
  static const int kTandbergDbAdjustment = 10;

  bool ChangePlayout(bool playout);
  bool ChangeSend(SendFlags send);

  typedef std::map<uint32, int> ChannelMap;
  talk_base::scoped_ptr<WebRtcSoundclipStream> ringback_tone_;
  std::set<int> ringback_channels_;  // channels playing ringback
  int channel_options_;
  bool agc_adjusted_;
  bool dtmf_allowed_;
  bool desired_playout_;
  bool playout_;
  SendFlags desired_send_;
  SendFlags send_;
  ChannelMap mux_channels_;  // for multiple sources
  // mux_channels_ can be read from WebRtc callback thread.  Accesses off the
  // WebRtc thread must be synchronized with edits on the worker thread.  Reads
  // on the worker thread are ok.
  mutable talk_base::CriticalSection mux_channels_cs_;
};
}

#endif  // TALK_SESSION_PHONE_WEBRTCVOICEENGINE_H_
