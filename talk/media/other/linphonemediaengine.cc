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

#ifndef MSILBC_LIBRARY
#define MSILBC_LIBRARY "/usr/lib/mediastreamer/plugins/libmsilbc.so"
#endif

// LinphoneMediaEngine is a Linphone implementation of MediaEngine
extern "C" {
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/msfilter.h>
}

#include "talk/media/other/linphonemediaengine.h"

#include "talk/base/buffer.h"
#include "talk/base/event.h"
#include "talk/base/logging.h"
#include "talk/base/pathutils.h"
#include "talk/base/stream.h"
#include "talk/media/base/rtpdump.h"

#ifndef WIN32
#include <libgen.h>
#endif

namespace cricket {

///////////////////////////////////////////////////////////////////////////
// Implementation of LinphoneMediaEngine.
///////////////////////////////////////////////////////////////////////////
LinphoneMediaEngine::LinphoneMediaEngine(const std::string& ringWav,  const std::string& callWav) : ring_wav_(ringWav), call_wav_(callWav) { }

bool LinphoneMediaEngine::Init() {
  ortp_init();
  ms_init();

#ifdef HAVE_ILBC
#ifndef WIN32
  char * path = strdup(MSILBC_LIBRARY);
  char * dirc = dirname(path);
  ms_load_plugins(dirc);
#endif
  if (ms_filter_codec_supported("iLBC"))
    have_ilbc = 1;
  else
    have_ilbc = 0;
#else
  have_ilbc = 0;
#endif

#ifdef HAVE_SPEEX
  voice_codecs_.push_back(AudioCodec(110, payload_type_speex_wb.mime_type, payload_type_speex_wb.clock_rate, 0, 1, 8));
  voice_codecs_.push_back(AudioCodec(111, payload_type_speex_nb.mime_type, payload_type_speex_nb.clock_rate, 0, 1, 7));
#endif

#ifdef HAVE_ILBC
  if (have_ilbc)
    voice_codecs_.push_back(AudioCodec(102, payload_type_ilbc.mime_type, payload_type_ilbc.clock_rate, 0, 1, 4));
#endif

  voice_codecs_.push_back(AudioCodec(0, payload_type_pcmu8000.mime_type, payload_type_pcmu8000.clock_rate, 0, 1, 2));
  voice_codecs_.push_back(AudioCodec(101, payload_type_telephone_event.mime_type, payload_type_telephone_event.clock_rate, 0, 1, 1));
  return true;
}

void LinphoneMediaEngine::Terminate() {
  fflush(stdout);
}


int LinphoneMediaEngine::GetCapabilities() {
  int capabilities = 0;
  capabilities |= AUDIO_SEND;
  capabilities |= AUDIO_RECV;
  return capabilities;
}

VoiceMediaChannel* LinphoneMediaEngine::CreateChannel() {
  return new LinphoneVoiceChannel(this);
}

VideoMediaChannel* LinphoneMediaEngine::CreateVideoChannel(VoiceMediaChannel* voice_ch) {
  return NULL;
}

bool LinphoneMediaEngine::FindAudioCodec(const AudioCodec &c) {
  if (c.id == 0)
    return true;
  if (c.name == payload_type_telephone_event.mime_type)
    return true;
#ifdef HAVE_SPEEX
  if (c.name == payload_type_speex_wb.mime_type && c.clockrate == payload_type_speex_wb.clock_rate)
    return true;
  if (c.name == payload_type_speex_nb.mime_type && c.clockrate == payload_type_speex_nb.clock_rate)
    return true;
#endif
#ifdef HAVE_ILBC
  if (have_ilbc && c.name == payload_type_ilbc.mime_type)
    return true;
#endif
  return false;
}

///////////////////////////////////////////////////////////////////////////
// Implementation of LinphoneVoiceChannel.
///////////////////////////////////////////////////////////////////////////
LinphoneVoiceChannel::LinphoneVoiceChannel(LinphoneMediaEngine*eng)
    : pt_(-1),
      audio_stream_(0),
      engine_(eng),
      ring_stream_(0)
{

  talk_base::Thread *thread = talk_base::ThreadManager::CurrentThread();
  talk_base::SocketServer *ss = thread->socketserver();
  socket_.reset(ss->CreateAsyncSocket(SOCK_DGRAM));

  socket_->Bind(talk_base::SocketAddress("localhost",3000));
  socket_->SignalReadEvent.connect(this, &LinphoneVoiceChannel::OnIncomingData);

}

LinphoneVoiceChannel::~LinphoneVoiceChannel()
{
  fflush(stdout);
  StopRing();

  if (audio_stream_)
    audio_stream_stop(audio_stream_);
}

bool LinphoneVoiceChannel::SetPlayout(bool playout) {
  play_ = playout;
  return true;
}

bool LinphoneVoiceChannel::SetSendCodecs(const std::vector<AudioCodec>& codecs) {

  bool first = true;
  std::vector<AudioCodec>::const_iterator i;

  ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);

  for (i = codecs.begin(); i < codecs.end(); i++) {

    if (!engine_->FindAudioCodec(*i))
      continue;
#ifdef HAVE_ILBC
    if (engine_->have_ilbc && i->name == payload_type_ilbc.mime_type) {
      rtp_profile_set_payload(&av_profile, i->id, &payload_type_ilbc);
    }
#endif
#ifdef HAVE_SPEEX
    if (i->name == payload_type_speex_wb.mime_type && i->clockrate == payload_type_speex_wb.clock_rate) {
      rtp_profile_set_payload(&av_profile, i->id, &payload_type_speex_wb);
    } else if (i->name == payload_type_speex_nb.mime_type && i->clockrate == payload_type_speex_nb.clock_rate) {
      rtp_profile_set_payload(&av_profile, i->id, &payload_type_speex_nb);
    }
#endif

    if (i->id == 0)
      rtp_profile_set_payload(&av_profile, 0, &payload_type_pcmu8000);

    if (i->name == payload_type_telephone_event.mime_type) {
      rtp_profile_set_payload(&av_profile, i->id, &payload_type_telephone_event);
    }

    if (first) {
      StopRing();
      LOG(LS_INFO) << "Using " << i->name << "/" << i->clockrate;
      pt_ = i->id;
      audio_stream_ = audio_stream_start(&av_profile, 2000, "127.0.0.1", 3000, i->id, 250, 0);
      first = false;
    }
  }

  if (first) {
    StopRing();
    // We're being asked to set an empty list of codecs. This will only happen when
    // working with a buggy client; let's try PCMU.
    LOG(LS_WARNING) << "Received empty list of codces; using PCMU/8000";
    audio_stream_ = audio_stream_start(&av_profile, 2000, "127.0.0.1", 3000, 0, 250, 0);
  }

  return true;
}

bool LinphoneVoiceChannel::SetSend(SendFlags flag) {
  mute_ = !flag;
  return true;
}

void LinphoneVoiceChannel::OnPacketReceived(talk_base::Buffer* packet) {
  const void* data = packet->data();
  int len = packet->length();
  uint8 buf[2048];
  memcpy(buf, data, len);

  /* We may receive packets with payload type 13: comfort noise. Linphone can't
   * handle them, so let's ignore those packets.
   */
  int payloadtype = buf[1] & 0x7f;
  if (play_ && payloadtype != 13)
    socket_->SendTo(buf, len, talk_base::SocketAddress("localhost",2000));
}

void LinphoneVoiceChannel::StartRing(bool bIncomingCall)
{
  MSSndCard *sndcard = NULL;
  sndcard=ms_snd_card_manager_get_default_card(ms_snd_card_manager_get());
  if (sndcard)
  {
    if (bIncomingCall)
    {
      if (engine_->GetRingWav().size() > 0)
      {
        LOG(LS_VERBOSE) << "incoming ring. sound file: " << engine_->GetRingWav().c_str() << "\n";
        ring_stream_ = ring_start (engine_->GetRingWav().c_str(), 1, sndcard);
      }
    }
    else
    {
      if (engine_->GetCallWav().size() > 0)
      {
        LOG(LS_VERBOSE) << "outgoing ring. sound file: " << engine_->GetCallWav().c_str() << "\n";
        ring_stream_ = ring_start (engine_->GetCallWav().c_str(), 1, sndcard);
      }
    }
  }
}

void LinphoneVoiceChannel::StopRing()
{
  if (ring_stream_) {
    ring_stop(ring_stream_);
    ring_stream_ = 0;
  }
}

void LinphoneVoiceChannel::OnIncomingData(talk_base::AsyncSocket *s)
{
  char *buf[2048];
  int len;
  len = s->Recv(buf, sizeof(buf));
  talk_base::Buffer packet(buf, len);
  if (network_interface_ && !mute_)
    network_interface_->SendPacket(&packet);
}

}
