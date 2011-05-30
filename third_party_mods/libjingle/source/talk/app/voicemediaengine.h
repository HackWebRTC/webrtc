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

#ifndef TALK_APP_WEBRTC_AUDIOMEDIAENGINE_H_
#define TALK_APP_WEBRTC_AUDIOMEDIAENGINE_H_

#include <map>
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
#include "talk/app/voiceengine.h"

namespace cricket {
class SoundclipMedia;
class VoiceMediaChannel;
}
namespace webrtc {

// MonitorStream is used to monitor a stream coming from WebRTC.
// For now we just dump the data.
class MonitorStream : public OutStream {
  virtual bool Write(const void *buf, int len) {
    return true;
  }
};

class AudioDeviceModule;
class RtcVoiceMediaChannel;

// RtcVoiceEngine is a class to be used with CompositeMediaEngine.
// It uses the WebRTC VoiceEngine library for audio handling.
class RtcVoiceEngine
    : public VoiceEngineObserver,
      public TraceCallback {
 public:
  RtcVoiceEngine();  // NOLINT
  // Dependency injection for testing.
  explicit RtcVoiceEngine(RtcWrapper* rtc_wrapper);
  ~RtcVoiceEngine();
  bool Init();
  void Terminate();

  int GetCapabilities();
  cricket::VoiceMediaChannel* CreateChannel();
  cricket::SoundclipMedia* CreateSoundclip() { return NULL; }
  bool SetDevices(const cricket::Device* in_device,
                  const cricket::Device* out_device);
  bool SetOptions(int options);
  bool GetOutputVolume(int* level);
  bool SetOutputVolume(int level);
  int GetInputLevel();
  bool SetLocalMonitor(bool enable);

  const std::vector<cricket::AudioCodec>& codecs();
  bool FindCodec(const cricket::AudioCodec& codec);
  bool FindRtcCodec(const cricket::AudioCodec& codec, CodecInst* gcodec);

  void SetLogging(int min_sev, const char* filter);

  // For tracking WebRTC channels. Needed because we have to pause them
  // all when switching devices.
  // May only be called by RtcVoiceMediaChannel.
  void RegisterChannel(RtcVoiceMediaChannel *channel);
  void UnregisterChannel(RtcVoiceMediaChannel *channel);

  RtcWrapper* webrtc() { return rtc_wrapper_.get(); }
  int GetLastRtcError();

 private:
  typedef std::vector<RtcVoiceMediaChannel *> ChannelList;

  struct CodecPref {
    const char* name;
    int clockrate;
  };

  void Construct();
  bool InitInternal();
  void ApplyLogging();
  virtual void Print(const TraceLevel level,
                     const char* traceString, const int length);
  virtual void CallbackOnError(const int errCode, const int channel);
  static int GetCodecPreference(const char *name, int clockrate);
  // Given the device type, name, and id, find WebRTC's device id. Return true and
  // set the output parameter rtc_id if successful.
  bool FindAudioDeviceId(
      bool is_input, const std::string& dev_name, int dev_id, int* rtc_id);
  bool FindChannelAndSsrc(int channel_num,
                          RtcVoiceMediaChannel** channel,
                          uint32* ssrc) const;

  static const int kDefaultLogSeverity = talk_base::LS_WARNING;
  static const CodecPref kCodecPrefs[];

  // The primary instance of WebRTC VoiceEngine.
  talk_base::scoped_ptr<RtcWrapper> rtc_wrapper_;
  int log_level_;
  std::vector<cricket::AudioCodec> codecs_;
  talk_base::scoped_ptr<MonitorStream> monitor_;
  // TODO: Can't use scoped_ptr here since ~AudioDeviceModule is protected.
  AudioDeviceModule* adm_;
  ChannelList channels_;
  talk_base::CriticalSection channels_cs_;
};

// RtcMediaChannel is a class that implements the common WebRTC channel
// functionality.
template <class T, class E>
class RtcMediaChannel : public T, public Transport {
 public:
  RtcMediaChannel(E *engine, int channel)
      : engine_(engine), audio_channel_(channel), sequence_number_(-1) {}
  E *engine() { return engine_; }
  int audio_channel() const { return audio_channel_; }
  bool valid() const { return audio_channel_ != -1; }
 protected:
  // implements Transport interface
  virtual int SendPacket(int channel, const void *data, int len) {
    if (!T::network_interface_) {
      return -1;
    }

    const uint8* header = static_cast<const uint8*>(data);
    sequence_number_ = talk_base::GetBE16(header + 2);

    talk_base::Buffer packet(data, len, cricket::kMaxRtpPacketLen);
    return T::network_interface_->SendPacket(&packet) ? len : -1;
  }
  virtual int SendRTCPPacket(int channel, const void *data, int len) {
    if (!T::network_interface_) {
      return -1;
    }

    talk_base::Buffer packet(data, len, cricket::kMaxRtpPacketLen);
    return T::network_interface_->SendRtcp(&packet) ? len : -1;
  }
  int sequence_number() {
    return sequence_number_;
  }
 private:
  E *engine_;
  int audio_channel_;
  int sequence_number_;
};

// RtcVoiceMediaChannel is an implementation of VoiceMediaChannel that uses
// WebRTC Voice Engine.
class RtcVoiceMediaChannel
    : public RtcMediaChannel<cricket::VoiceMediaChannel, RtcVoiceEngine> {
 public:
  explicit RtcVoiceMediaChannel(RtcVoiceEngine *engine);
  virtual ~RtcVoiceMediaChannel();
  virtual bool SetOptions(int options);
  virtual bool SetRecvCodecs(const std::vector<cricket::AudioCodec> &codecs);
  virtual bool SetSendCodecs(const std::vector<cricket::AudioCodec> &codecs);
  virtual bool SetPlayout(bool playout);
  bool GetPlayout();
  virtual bool SetSend(cricket::SendFlags send);
  cricket::SendFlags GetSend();
  virtual bool AddStream(uint32 ssrc);
  virtual bool RemoveStream(uint32 ssrc);
  virtual bool GetActiveStreams(cricket::AudioInfo::StreamList* actives);
  virtual int GetOutputLevel();

  virtual bool SetRingbackTone(const char *buf, int len);
  virtual bool PlayRingbackTone(uint32 ssrc, bool play, bool loop);
  virtual bool PlayRingbackTone(bool play, bool loop);
  virtual bool PressDTMF(int event, bool playout);

  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet);
  virtual void SetSendSsrc(uint32 id);
  virtual bool SetRtcpCName(const std::string& cname);
  virtual bool Mute(bool mute);
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<cricket::RtpHeaderExtension>& extensions) { return false; }
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<cricket::RtpHeaderExtension>& extensions) { return false; }
  virtual bool SetSendBandwidth(bool autobw, int bps) { return false; }
  virtual bool GetStats(cricket::VoiceMediaInfo* info);

  virtual void GetLastMediaError(uint32* ssrc,
                                 VoiceMediaChannel::Error* error);
  bool FindSsrc(int channel_num, uint32* ssrc);
  void OnError(uint32 ssrc, int error);
  virtual int GetMediaChannelId() { return audio_channel(); }

 protected:
  int GetLastRtcError() { return engine()->GetLastRtcError(); }
  int GetChannel(uint32 ssrc);
  int GetOutputLevel(int channel);
  bool EnableRtcp(int channel);
  bool SetPlayout(int channel, bool playout);
  static uint32 ParseSsrc(const void* data, size_t len, bool rtcp);
  static Error WebRTCErrorToChannelError(int err_code);

 private:

  typedef std::map<uint32, int> ChannelMap;
  int channel_options_;
  bool playout_;
  cricket::SendFlags send_;
  ChannelMap mux_channels_;  // for multiple sources
  // mux_channels_ can be read from WebRTC callback thread.  Accesses off the
  // WebRTC thread must be synchronized with edits on the worker thread.  Reads
  // on the worker thread are ok.
  mutable talk_base::CriticalSection mux_channels_cs_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_AUDIOMEDIAENGINE_H_
