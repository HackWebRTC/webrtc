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

#include "talk/app/voicemediaengine.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#ifdef PLATFORM_CHROMIUM
#include "content/renderer/renderer_webrtc_audio_device_impl.h"
#else
#include "modules/audio_device/main/interface/audio_device.h"
#endif
#include "talk/base/base64.h"
#include "talk/base/byteorder.h"
#include "talk/base/common.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/stringencode.h"

namespace webrtc {

static void LogMultiline(talk_base::LoggingSeverity sev, char* text) {
  const char* delim = "\r\n";
  for (char* tok = strtok(text, delim); tok; tok = strtok(NULL, delim)) {
    LOG_V(sev) << tok;
  }
}

// RtcVoiceEngine
const RtcVoiceEngine::CodecPref RtcVoiceEngine::kCodecPrefs[] = {
  { "ISAC",   16000 },
  { "ISAC",   32000 },
  { "ISACLC", 16000 },
  { "speex",  16000 },
  { "IPCMWB", 16000 },
  { "G722",   16000 },
  { "iLBC",   8000 },
  { "speex",  8000 },
  { "GSM",    8000 },
  { "EG711U", 8000 },
  { "EG711A", 8000 },
  { "PCMU",   8000 },
  { "PCMA",   8000 },
  { "CN",     32000 },
  { "CN",     16000 },
  { "CN",     8000 },
  { "red",    8000 },
  { "telephone-event", 8000 },
};

RtcVoiceEngine::RtcVoiceEngine()
    : rtc_wrapper_(new RtcWrapper()),
      log_level_(kDefaultLogSeverity),
      adm_(NULL) {
  Construct();
}

RtcVoiceEngine::RtcVoiceEngine(RtcWrapper* rtc_wrapper)
    : rtc_wrapper_(rtc_wrapper),
      log_level_(kDefaultLogSeverity),
      adm_(NULL) {
  Construct();
}

void RtcVoiceEngine::Construct() {
  LOG(INFO) << "RtcVoiceEngine::RtcVoiceEngine";
  ApplyLogging();

  if (rtc_wrapper_->base()->RegisterVoiceEngineObserver(*this) == -1) {
    LOG_RTCERR0(RegisterVoiceEngineObserver);
  }

  // Load our audio codec list
  LOG(INFO) << "WebRTC VoiceEngine codecs:";
  int ncodecs = rtc_wrapper_->codec()->NumOfCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    CodecInst gcodec;
    if (rtc_wrapper_->codec()->GetCodec(i, gcodec) >= 0) {
      int pref = GetCodecPreference(gcodec.plname, gcodec.plfreq);
      if (pref != -1) {
        if (gcodec.rate == -1) gcodec.rate = 0;
        cricket::AudioCodec codec(gcodec.pltype, gcodec.plname, gcodec.plfreq,
                         gcodec.rate, gcodec.channels, pref);
        LOG(INFO) << gcodec.plname << "/" << gcodec.plfreq << "/" \
                     << gcodec.channels << " " << gcodec.pltype;
        codecs_.push_back(codec);
      }
    }
  }
  // Make sure they are in local preference order
  std::sort(codecs_.begin(), codecs_.end(), &cricket::AudioCodec::Preferable);
}

RtcVoiceEngine::~RtcVoiceEngine() {
  LOG(INFO) << "RtcVoiceEngine::~RtcVoiceEngine";
  if (rtc_wrapper_->base()->DeRegisterVoiceEngineObserver() == -1) {
    LOG_RTCERR0(DeRegisterVoiceEngineObserver);
  }
  rtc_wrapper_.reset();
  if (adm_) {
    AudioDeviceModule::Destroy(adm_);
    adm_ = NULL;
  }
}

bool RtcVoiceEngine::Init() {
  LOG(INFO) << "RtcVoiceEngine::Init";
  bool res = InitInternal();
  if (res) {
    LOG(INFO) << "RtcVoiceEngine::Init Done!";
  } else {
    LOG(LERROR) << "RtcVoiceEngine::Init failed";
    Terminate();
  }
  return res;
}

bool RtcVoiceEngine::InitInternal() {
  // Temporarily turn logging level up for the Init call
  int old_level = log_level_;
  log_level_ = talk_base::_min(log_level_,
                               static_cast<int>(talk_base::INFO));
  ApplyLogging();

  if (!adm_) {
#ifdef PLATFORM_CHROMIUM
    adm_ = new RendererWebRtcAudioDeviceImpl(1440, 1440, 1, 1, 48000, 48000);
#else
    adm_ = AudioDeviceModule::Create(0);
#endif

    if (rtc_wrapper_->base()->RegisterAudioDeviceModule(*adm_) == -1) {
      LOG_RTCERR0_EX(Init, rtc_wrapper_->error());
      return false;
    }
  }

  // Init WebRTC VoiceEngine, enabling AEC logging if specified in SetLogging.
  if (rtc_wrapper_->base()->Init() == -1) {
    LOG_RTCERR0_EX(Init, rtc_wrapper_->error());
    return false;
  }

  // Restore the previous log level
  log_level_ = old_level;
  ApplyLogging();

  // Log the WebRTC version info
  char buffer[1024] = "";
  rtc_wrapper_->base()->GetVersion(buffer);
  LOG(INFO) << "WebRTC VoiceEngine Version:";
  LogMultiline(talk_base::INFO, buffer);

  // Turn on AEC and AGC by default.
  if (!SetOptions(
      cricket::MediaEngine::ECHO_CANCELLATION | cricket::MediaEngine::AUTO_GAIN_CONTROL)) {
    return false;
  }

  // Print our codec list again for the call diagnostic log
  LOG(INFO) << "WebRTC VoiceEngine codecs:";
  for (std::vector<cricket::AudioCodec>::const_iterator it = codecs_.begin();
      it != codecs_.end(); ++it) {
    LOG(INFO) << it->name << "/" << it->clockrate << "/"
              << it->channels << " " << it->id;
  }
  return true;
}

bool RtcVoiceEngine::SetDevices(const cricket::Device* in_device,
                                const cricket::Device* out_device) {
  LOG(INFO) << "RtcVoiceEngine::SetDevices";
  // Currently we always use the default device, so do nothing here.
  return true;
}

void RtcVoiceEngine::Terminate() {
  LOG(INFO) << "RtcVoiceEngine::Terminate";

  rtc_wrapper_->base()->Terminate();
}

int RtcVoiceEngine::GetCapabilities() {
  return cricket::MediaEngine::AUDIO_SEND | cricket::MediaEngine::AUDIO_RECV;
}

cricket::VoiceMediaChannel *RtcVoiceEngine::CreateChannel() {
  RtcVoiceMediaChannel* ch = new RtcVoiceMediaChannel(this);
  if (!ch->valid()) {
    delete ch;
    ch = NULL;
  }
  return ch;
}

bool RtcVoiceEngine::SetOptions(int options) {

  return true;
}

bool RtcVoiceEngine::FindAudioDeviceId(
    bool is_input, const std::string& dev_name, int dev_id, int* rtc_id) {
  return false;
}

bool RtcVoiceEngine::GetOutputVolume(int* level) {
  unsigned int ulevel;
  if (rtc_wrapper_->volume()->GetSpeakerVolume(ulevel) == -1) {
    LOG_RTCERR1(GetSpeakerVolume, level);
    return false;
  }
  *level = ulevel;
  return true;
}

bool RtcVoiceEngine::SetOutputVolume(int level) {
  ASSERT(level >= 0 && level <= 255);
  if (rtc_wrapper_->volume()->SetSpeakerVolume(level) == -1) {
    LOG_RTCERR1(SetSpeakerVolume, level);
    return false;
  }
  return true;
}

int RtcVoiceEngine::GetInputLevel() {
  unsigned int ulevel;
  return (rtc_wrapper_->volume()->GetSpeechInputLevel(ulevel) != -1) ?
      static_cast<int>(ulevel) : -1;
}

bool RtcVoiceEngine::SetLocalMonitor(bool enable) {
  return true;
}

const std::vector<cricket::AudioCodec>& RtcVoiceEngine::codecs() {
  return codecs_;
}

bool RtcVoiceEngine::FindCodec(const cricket::AudioCodec& in) {
  return FindRtcCodec(in, NULL);
}

bool RtcVoiceEngine::FindRtcCodec(const cricket::AudioCodec& in, CodecInst* out) {
  int ncodecs = rtc_wrapper_->codec()->NumOfCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    CodecInst gcodec;
    if (rtc_wrapper_->codec()->GetCodec(i, gcodec) >= 0) {
      cricket::AudioCodec codec(gcodec.pltype, gcodec.plname,
                       gcodec.plfreq, gcodec.rate, gcodec.channels, 0);
      if (codec.Matches(in)) {
        if (out) {
          // If the codec is VBR and an explicit rate is specified, use it.
          if (in.bitrate != 0 && gcodec.rate == -1) {
            gcodec.rate = in.bitrate;
          }
          *out = gcodec;
        }
        return true;
      }
    }
  }
  return false;
}

void RtcVoiceEngine::SetLogging(int min_sev, const char* filter) {
  log_level_ = min_sev;

  std::vector<std::string> opts;
  talk_base::tokenize(filter, ' ', &opts);

  // voice log level
  ApplyLogging();
}

int RtcVoiceEngine::GetLastRtcError() {
  return rtc_wrapper_->error();
}

void RtcVoiceEngine::ApplyLogging() {
  int filter = 0;
  switch (log_level_) {
    case talk_base::INFO: filter |= kTraceAll;      // fall through
    case talk_base::WARNING: filter |= kTraceWarning;  // fall through
    case talk_base::LERROR: filter |= kTraceError | kTraceCritical;
  }
}

void RtcVoiceEngine::Print(const TraceLevel level,
                            const char* traceString, const int length) {
  talk_base::LoggingSeverity sev = talk_base::INFO;
  if (level == kTraceError || level == kTraceCritical)
    sev = talk_base::LERROR;
  else if (level == kTraceWarning)
    sev = talk_base::WARNING;
  else if (level == kTraceStateInfo)
    sev = talk_base::INFO;

  if (sev >= log_level_) {
    // Skip past webrtc boilerplate prefix text
    if (length <= 70) {
      std::string msg(traceString, length);
      LOG(LERROR) << "Malformed WebRTC log message: ";
      LOG_V(sev) << msg;
    } else {
      std::string msg(traceString + 70, length - 71);
      LOG_V(sev) << "VoE:" << msg;
    }
  }
}

void RtcVoiceEngine::CallbackOnError(const int err_code,
                                      const int channel_num) {
  talk_base::CritScope lock(&channels_cs_);
  RtcVoiceMediaChannel* channel = NULL;
  uint32 ssrc = 0;
  LOG(WARNING) << "WebRTC error " << err_code << " reported on channel "
               << channel_num << ".";
  if (FindChannelAndSsrc(channel_num, &channel, &ssrc)) {
    ASSERT(channel != NULL);
    channel->OnError(ssrc, err_code);
  } else {
    LOG(LERROR) << "WebRTC channel " << channel_num
        << " could not be found in the channel list when error reported.";
  }
}

int RtcVoiceEngine::GetCodecPreference(const char *name, int clockrate) {
  for (size_t i = 0; i < ARRAY_SIZE(kCodecPrefs); ++i) {
    if ((strcmp(kCodecPrefs[i].name, name) == 0) &&
        (kCodecPrefs[i].clockrate == clockrate))
      return ARRAY_SIZE(kCodecPrefs) - i;
  }
  LOG(WARNING) << "Unexpected codec \"" << name << "/" << clockrate << "\"";
  return -1;
}

bool RtcVoiceEngine::FindChannelAndSsrc(
    int channel_num, RtcVoiceMediaChannel** channel, uint32* ssrc) const {
  ASSERT(channel != NULL && ssrc != NULL);

  *channel = NULL;
  *ssrc = 0;
  // Find corresponding channel and ssrc
  for (ChannelList::const_iterator it = channels_.begin();
      it != channels_.end(); ++it) {
    ASSERT(*it != NULL);
    if ((*it)->FindSsrc(channel_num, ssrc)) {
      *channel = *it;
      return true;
    }
  }

  return false;
}

void RtcVoiceEngine::RegisterChannel(RtcVoiceMediaChannel *channel) {
  talk_base::CritScope lock(&channels_cs_);
  channels_.push_back(channel);
}

void RtcVoiceEngine::UnregisterChannel(RtcVoiceMediaChannel *channel) {
  talk_base::CritScope lock(&channels_cs_);
  ChannelList::iterator i = std::find(channels_.begin(),
                                      channels_.end(),
                                      channel);
  if (i != channels_.end()) {
    channels_.erase(i);
  }
}

// RtcVoiceMediaChannel
RtcVoiceMediaChannel::RtcVoiceMediaChannel(RtcVoiceEngine *engine)
    : RtcMediaChannel<cricket::VoiceMediaChannel, RtcVoiceEngine>(engine,
          engine->webrtc()->base()->CreateChannel()),
      channel_options_(0), playout_(false), send_(cricket::SEND_NOTHING) {
  engine->RegisterChannel(this);
  LOG(INFO) << "RtcVoiceMediaChannel::RtcVoiceMediaChannel "
            << audio_channel();

  // Register external transport
  if (engine->webrtc()->network()->RegisterExternalTransport(
      audio_channel(), *static_cast<Transport*>(this)) == -1) {
    LOG_RTCERR2(RegisterExternalTransport, audio_channel(), this);
  }

  // Enable RTCP (for quality stats and feedback messages)
  EnableRtcp(audio_channel());

  // Create a random but nonzero send SSRC
  SetSendSsrc(talk_base::CreateRandomNonZeroId());
}

RtcVoiceMediaChannel::~RtcVoiceMediaChannel() {
  LOG(INFO) << "RtcVoiceMediaChannel::~RtcVoiceMediaChannel "
            << audio_channel();

  // DeRegister external transport
  if (engine()->webrtc()->network()->DeRegisterExternalTransport(
      audio_channel()) == -1) {
    LOG_RTCERR1(DeRegisterExternalTransport, audio_channel());
  }

  // Unregister ourselves from the engine.
  engine()->UnregisterChannel(this);
  // Remove any remaining streams.
  while (!mux_channels_.empty()) {
    RemoveStream(mux_channels_.begin()->first);
  }
  // Delete the primary channel.
  if (engine()->webrtc()->base()->DeleteChannel(audio_channel()) == -1) {
    LOG_RTCERR1(DeleteChannel, audio_channel());
  }
}

bool RtcVoiceMediaChannel::SetOptions(int flags) {
  // Always accept flags that are unchanged.
  if (channel_options_ == flags) {
    return true;
  }

  // Reject new options if we're already sending.
  if (send_ != cricket::SEND_NOTHING) {
    return false;
  }
  // Save the options, to be interpreted where appropriate.
  channel_options_ = flags;
  return true;
}

bool RtcVoiceMediaChannel::SetRecvCodecs(
    const std::vector<cricket::AudioCodec>& codecs) {
  // Update our receive payload types to match what we offered. This only is
  // an issue when a different entity (i.e. a server) is generating the offer
  // for us.
  bool ret = true;
  for (std::vector<cricket::AudioCodec>::const_iterator i = codecs.begin();
       i != codecs.end() && ret; ++i) {
    CodecInst gcodec;
    if (engine()->FindRtcCodec(*i, &gcodec)) {
      if (gcodec.pltype != i->id) {
        LOG(INFO) << "Updating payload type for " << gcodec.plname
                  << " from " << gcodec.pltype << " to " << i->id;
        gcodec.pltype = i->id;
        if (engine()->webrtc()->codec()->SetRecPayloadType(
            audio_channel(), gcodec) == -1) {
          LOG_RTCERR1(SetRecPayloadType, audio_channel());
          ret = false;
        }
      }
    } else {
      LOG(WARNING) << "Unknown codec " << i->name;
      ret = false;
    }
  }

  return ret;
}

bool RtcVoiceMediaChannel::SetSendCodecs(
    const std::vector<cricket::AudioCodec>& codecs) {
  bool first = true;
  CodecInst send_codec;
  memset(&send_codec, 0, sizeof(send_codec));

  for (std::vector<cricket::AudioCodec>::const_iterator i = codecs.begin();
       i != codecs.end(); ++i) {
    CodecInst gcodec;
    if (!engine()->FindRtcCodec(*i, &gcodec))
      continue;

    // We'll use the first codec in the list to actually send audio data.
    // Be sure to use the payload type requested by the remote side.
    if (first) {
      send_codec = gcodec;
      send_codec.pltype = i->id;
      first = false;
    }
  }

  // If we're being asked to set an empty list of codecs, due to a buggy client,
  // choose the most common format: PCMU
  if (first) {
    LOG(WARNING) << "Received empty list of codecs; using PCMU/8000";
    cricket::AudioCodec codec(0, "PCMU", 8000, 0, 1, 0);
    engine()->FindRtcCodec(codec, &send_codec);
  }

  // Set the codec.
  LOG(INFO) << "Selected voice codec " << send_codec.plname
            << "/" << send_codec.plfreq;
  if (engine()->webrtc()->codec()->SetSendCodec(audio_channel(),
                                                  send_codec) == -1) {
    LOG_RTCERR1(SetSendCodec, audio_channel());
    return false;
  }

  return true;
}

bool RtcVoiceMediaChannel::SetPlayout(bool playout) {
  if (playout_ == playout) {
    return true;
  }

  bool result = true;
  if (mux_channels_.empty()) {
    // Only toggle the default channel if we don't have any other channels.
    result = SetPlayout(audio_channel(), playout);
  }
  for (ChannelMap::iterator it = mux_channels_.begin();
       it != mux_channels_.end() && result; ++it) {
    if (!SetPlayout(it->second, playout)) {
      LOG(LERROR) << "SetPlayout " << playout << " on channel " << it->second
                  << " failed";
      result = false;
    }
  }

  if (result) {
    playout_ = playout;
  }
  return result;
}

bool RtcVoiceMediaChannel::GetPlayout() {
  return playout_;
}

bool RtcVoiceMediaChannel::SetSend(cricket::SendFlags send) {
  if (send_ == send) {
    return true;
  }

  if (send == cricket::SEND_MICROPHONE) {
    if (sequence_number() != -1) {
      if (engine()->webrtc()->sync()->SetInitSequenceNumber(
              audio_channel(), sequence_number() + 1) == -1) {
        LOG_RTCERR2(SetInitSequenceNumber, audio_channel(),
                     sequence_number() + 1);
      }
    }
    if (engine()->webrtc()->base()->StartSend(audio_channel()) == -1) {
      LOG_RTCERR1(StartSend, audio_channel());
      return false;
    }
    if (engine()->webrtc()->file()->StopPlayingFileAsMicrophone(
        audio_channel()) == -1) {
      LOG_RTCERR1(StopPlayingFileAsMicrophone, audio_channel());
      return false;
    }
  } else {  // SEND_NOTHING
    if (engine()->webrtc()->base()->StopSend(audio_channel()) == -1) {
      LOG_RTCERR1(StopSend, audio_channel());
    }
  }
  send_ = send;
  return true;
}

cricket::SendFlags RtcVoiceMediaChannel::GetSend() {
  return send_;
}

bool RtcVoiceMediaChannel::AddStream(uint32 ssrc) {
  talk_base::CritScope lock(&mux_channels_cs_);

  if (mux_channels_.find(ssrc) != mux_channels_.end()) {
    return false;
  }

  // Create a new channel for receiving audio data.
  int channel = engine()->webrtc()->base()->CreateChannel();
  if (channel == -1) {
    LOG_RTCERR0(CreateChannel);
    return false;
  }

  // Configure to use external transport, like our default channel.
  if (engine()->webrtc()->network()->RegisterExternalTransport(
          channel, *this) == -1) {
    LOG_RTCERR2(SetExternalTransport, channel, this);
    return false;
  }

  // Use the same SSRC as our default channel (so the RTCP reports are correct).
  unsigned int send_ssrc;
  VoERTP_RTCP* rtp = engine()->webrtc()->rtp();
  if (rtp->GetLocalSSRC(audio_channel(), send_ssrc) == -1) {
    LOG_RTCERR2(GetSendSSRC, channel, send_ssrc);
    return false;
  }
  if (rtp->SetLocalSSRC(channel, send_ssrc) == -1) {
    LOG_RTCERR2(SetSendSSRC, channel, send_ssrc);
    return false;
  }

  if (mux_channels_.empty() && GetPlayout()) {
    LOG(INFO) << "Disabling playback on the default voice channel";
    SetPlayout(audio_channel(), false);
  }

  mux_channels_[ssrc] = channel;

  LOG(INFO) << "New audio stream " << ssrc << " registered to WebRTC channel #"
            << channel << ".";
  return SetPlayout(channel, playout_);


}

bool RtcVoiceMediaChannel::RemoveStream(uint32 ssrc) {
  talk_base::CritScope lock(&mux_channels_cs_);
  ChannelMap::iterator it = mux_channels_.find(ssrc);

  if (it != mux_channels_.end()) {
    if (engine()->webrtc()->network()->DeRegisterExternalTransport(
        it->second) == -1) {
      LOG_RTCERR1(DeRegisterExternalTransport, it->second);
    }

    LOG(INFO) << "Removing audio stream " << ssrc << " with WebRTC channel #"
              << it->second << ".";
    if (engine()->webrtc()->base()->DeleteChannel(it->second) == -1) {
      LOG_RTCERR1(DeleteChannel, audio_channel());
      return false;
    }

    mux_channels_.erase(it);
    if (mux_channels_.empty() && GetPlayout()) {
      // The last stream was removed. We can now enable the default
      // channel for new channels to be played out immediately without
      // waiting for AddStream messages.
      // TODO(oja): Does the default channel still have it's CN state?
      LOG(INFO) << "Enabling playback on the default voice channel";
      SetPlayout(audio_channel(), true);
    }
  }
  return true;
}

bool RtcVoiceMediaChannel::GetActiveStreams(cricket::AudioInfo::StreamList* actives) {
  actives->clear();
  for (ChannelMap::iterator it = mux_channels_.begin();
       it != mux_channels_.end(); ++it) {
    int level = GetOutputLevel(it->second);
    if (level > 0) {
      actives->push_back(std::make_pair(it->first, level));
    }
  }
  return true;
}

int RtcVoiceMediaChannel::GetOutputLevel() {
  // return the highest output level of all streams
  int highest = GetOutputLevel(audio_channel());
  for (ChannelMap::iterator it = mux_channels_.begin();
       it != mux_channels_.end(); ++it) {
    int level = GetOutputLevel(it->second);
    highest = talk_base::_max(level, highest);
  }
  return highest;
}

bool RtcVoiceMediaChannel::SetRingbackTone(const char *buf, int len) {
  return true;
}

bool RtcVoiceMediaChannel::PlayRingbackTone(uint32 ssrc, bool play, bool loop) {
  return true;
}

bool RtcVoiceMediaChannel::PlayRingbackTone(bool play, bool loop) {
  return true;
}

bool RtcVoiceMediaChannel::PressDTMF(int event, bool playout) {
  return true;
}

void RtcVoiceMediaChannel::OnPacketReceived(talk_base::Buffer* packet) {
  // Pick which channel to send this packet to. If this packet doesn't match
  // any multiplexed streams, just send it to the default channel. Otherwise,
  // send it to the specific decoder instance for that stream.
  int which_channel = GetChannel(
      ParseSsrc(packet->data(), packet->length(), false));
  if (which_channel == -1) {
    which_channel = audio_channel();
  }

  engine()->webrtc()->network()->ReceivedRTPPacket(which_channel,
                                                 packet->data(),
                                                 packet->length());
}

void RtcVoiceMediaChannel::OnRtcpReceived(talk_base::Buffer* packet) {
  // See above.
  int which_channel = GetChannel(
      ParseSsrc(packet->data(), packet->length(), true));
  if (which_channel == -1) {
    which_channel = audio_channel();
  }

  engine()->webrtc()->network()->ReceivedRTCPPacket(which_channel,
                                                  packet->data(),
                                                  packet->length());
}

void RtcVoiceMediaChannel::SetSendSsrc(uint32 ssrc) {
  if (engine()->webrtc()->rtp()->SetLocalSSRC(audio_channel(), ssrc)
      == -1) {
     LOG_RTCERR2(SetSendSSRC, audio_channel(), ssrc);
  }
}

bool RtcVoiceMediaChannel::SetRtcpCName(const std::string& cname) {
  if (engine()->webrtc()->rtp()->SetRTCP_CNAME(audio_channel(),
                                                    cname.c_str()) == -1) {
     LOG_RTCERR2(SetRTCP_CNAME, audio_channel(), cname);
     return false;
  }
  return true;
}

bool RtcVoiceMediaChannel::Mute(bool muted) {
  if (engine()->webrtc()->volume()->SetInputMute(audio_channel(),
      muted) == -1) {
    LOG_RTCERR2(SetInputMute, audio_channel(), muted);
    return false;
  }
  return true;
}

bool RtcVoiceMediaChannel::GetStats(cricket::VoiceMediaInfo* info) {
  CallStatistics cs;
  unsigned int ssrc;
  CodecInst codec;
  unsigned int level;

  // Fill in the sender info, based on what we know, and what the
  // remote side told us it got from its RTCP report.
  cricket::VoiceSenderInfo sinfo;
  memset(&sinfo, 0, sizeof(sinfo));

  // Data we obtain locally.
  memset(&cs, 0, sizeof(cs));
  if (engine()->webrtc()->rtp()->GetRTCPStatistics(
      audio_channel(), cs) == -1 ||
      engine()->webrtc()->rtp()->GetLocalSSRC(audio_channel(), ssrc) == -1)
      {
    return false;
  }

  sinfo.ssrc = ssrc;
  sinfo.bytes_sent = cs.bytesSent;
  sinfo.packets_sent = cs.packetsSent;
  // RTT isn't known until a RTCP report is received. Until then, WebRTC
  // returns 0 to indicate an error value.
  sinfo.rtt_ms = (cs.rttMs > 0) ? cs.rttMs : -1;

  // Data from the last remote RTCP report.
  unsigned int ntp_high, ntp_low, timestamp, ptimestamp, jitter;
  unsigned short loss;  // NOLINT
  if (engine()->webrtc()->rtp()->GetRemoteRTCPData(audio_channel(),
          ntp_high, ntp_low, timestamp, ptimestamp, &jitter, &loss) != -1 &&
      engine()->webrtc()->codec()->GetSendCodec(audio_channel(),
          codec) != -1) {
    // Convert Q8 to floating point.
    sinfo.fraction_lost = static_cast<float>(loss) / (1 << 8);
    // Convert samples to milliseconds.
    if (codec.plfreq / 1000 > 0) {
      sinfo.jitter_ms = jitter / (codec.plfreq / 1000);
    }
  } else {
    sinfo.fraction_lost = -1;
    sinfo.jitter_ms = -1;
  }

  sinfo.packets_lost = -1;
  sinfo.ext_seqnum = -1;

  // Local speech level.
  sinfo.audio_level = (engine()->webrtc()->volume()->
      GetSpeechInputLevelFullRange(level) != -1) ? level : -1;
  info->senders.push_back(sinfo);

  // Build the list of receivers, one for each mux channel, or 1 in a 1:1 call.
  std::vector<int> channels;
  for (ChannelMap::const_iterator it = mux_channels_.begin();
       it != mux_channels_.end(); ++it) {
    channels.push_back(it->second);
  }
  if (channels.empty()) {
    channels.push_back(audio_channel());
  }

  // Get the SSRC and stats for each receiver, based on our own calculations.
  for (std::vector<int>::const_iterator it = channels.begin();
       it != channels.end(); ++it) {
    memset(&cs, 0, sizeof(cs));
    if (engine()->webrtc()->rtp()->GetRemoteSSRC(*it, ssrc) != -1 &&
        engine()->webrtc()->rtp()->GetRTCPStatistics(*it, cs) != -1 &&
        engine()->webrtc()->codec()->GetRecCodec(*it, codec) != -1) {
      cricket::VoiceReceiverInfo rinfo;
      memset(&rinfo, 0, sizeof(rinfo));
      rinfo.ssrc = ssrc;
      rinfo.bytes_rcvd = cs.bytesReceived;
      rinfo.packets_rcvd = cs.packetsReceived;
      // The next four fields are from the most recently sent RTCP report.
      // Convert Q8 to floating point.
      rinfo.fraction_lost = static_cast<float>(cs.fractionLost) / (1 << 8);
      rinfo.packets_lost = cs.cumulativeLost;
      rinfo.ext_seqnum = cs.extendedMax;
      // Convert samples to milliseconds.
      if (codec.plfreq / 1000 > 0) {
        rinfo.jitter_ms = cs.jitterSamples / (codec.plfreq / 1000);
      }
      // Get speech level.
      rinfo.audio_level = (engine()->webrtc()->volume()->
          GetSpeechOutputLevelFullRange(*it, level) != -1) ? level : -1;
      info->receivers.push_back(rinfo);
    }
  }

  return true;
}

void RtcVoiceMediaChannel::GetLastMediaError(
    uint32* ssrc, VoiceMediaChannel::Error* error) {
  ASSERT(ssrc != NULL);
  ASSERT(error != NULL);
  FindSsrc(audio_channel(), ssrc);
  *error = WebRTCErrorToChannelError(GetLastRtcError());
}

bool RtcVoiceMediaChannel::FindSsrc(int channel_num, uint32* ssrc) {
  talk_base::CritScope lock(&mux_channels_cs_);
  ASSERT(ssrc != NULL);
  if (channel_num == audio_channel()) {
    unsigned local_ssrc = 0;
    // This is a sending channel.
    if (engine()->webrtc()->rtp()->GetLocalSSRC(
        channel_num, local_ssrc) != -1) {
      *ssrc = local_ssrc;
    }
    return true;
  } else {
    // Check whether this is a receiving channel.
    for (ChannelMap::const_iterator it = mux_channels_.begin();
        it != mux_channels_.end(); ++it) {
      if (it->second == channel_num) {
        *ssrc = it->first;
        return true;
      }
    }
  }
  return false;
}

void RtcVoiceMediaChannel::OnError(uint32 ssrc, int error) {
  SignalMediaError(ssrc, WebRTCErrorToChannelError(error));
}

int RtcVoiceMediaChannel::GetChannel(uint32 ssrc) {
  ChannelMap::iterator it = mux_channels_.find(ssrc);
  return (it != mux_channels_.end()) ? it->second : -1;
}

int RtcVoiceMediaChannel::GetOutputLevel(int channel) {
  unsigned int ulevel;
  int ret =
      engine()->webrtc()->volume()->GetSpeechOutputLevel(channel, ulevel);
  return (ret == 0) ? static_cast<int>(ulevel) : -1;
}

bool RtcVoiceMediaChannel::EnableRtcp(int channel) {
  if (engine()->webrtc()->rtp()->SetRTCPStatus(channel, true) == -1) {
    LOG_RTCERR2(SetRTCPStatus, audio_channel(), 1);
    return false;
  }
  return true;
}

bool RtcVoiceMediaChannel::SetPlayout(int channel, bool playout) {
  if (playout) {
    LOG(INFO) << "Starting playout for channel #" << channel;
    if (engine()->webrtc()->base()->StartPlayout(channel) == -1) {
      LOG_RTCERR1(StartPlayout, channel);
      return false;
    }
  } else {
    LOG(INFO) << "Stopping playout for channel #" << channel;
    engine()->webrtc()->base()->StopPlayout(channel);
  }
  return true;
}

uint32 RtcVoiceMediaChannel::ParseSsrc(const void* data, size_t len,
                                        bool rtcp) {
  size_t ssrc_pos = (!rtcp) ? 8 : 4;
  uint32 ssrc = 0;
  if (len >= (ssrc_pos + sizeof(ssrc))) {
    ssrc = talk_base::GetBE32(static_cast<const char*>(data) + ssrc_pos);
  }
  return ssrc;
}

// Convert WebRTC error code into VoiceMediaChannel::Error enum.
cricket::VoiceMediaChannel::Error RtcVoiceMediaChannel::WebRTCErrorToChannelError(
    int err_code) {
  switch (err_code) {
    case 0:
      return ERROR_NONE;
    case VE_CANNOT_START_RECORDING:
    case VE_MIC_VOL_ERROR:
    case VE_GET_MIC_VOL_ERROR:
    case VE_CANNOT_ACCESS_MIC_VOL:
      return ERROR_REC_DEVICE_OPEN_FAILED;
    case VE_SATURATION_WARNING:
      return ERROR_REC_DEVICE_SATURATION;
    case VE_REC_DEVICE_REMOVED:
      return ERROR_REC_DEVICE_REMOVED;
    case VE_RUNTIME_REC_WARNING:
    case VE_RUNTIME_REC_ERROR:
      return ERROR_REC_RUNTIME_ERROR;
    case VE_CANNOT_START_PLAYOUT:
    case VE_SPEAKER_VOL_ERROR:
    case VE_GET_SPEAKER_VOL_ERROR:
    case VE_CANNOT_ACCESS_SPEAKER_VOL:
      return ERROR_PLAY_DEVICE_OPEN_FAILED;
    case VE_RUNTIME_PLAY_WARNING:
    case VE_RUNTIME_PLAY_ERROR:
      return ERROR_PLAY_RUNTIME_ERROR;
    default:
      return VoiceMediaChannel::ERROR_OTHER;
  }
}

}  // namespace webrtc

