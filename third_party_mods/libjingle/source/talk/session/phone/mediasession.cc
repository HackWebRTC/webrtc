/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#include "talk/session/phone/mediasession.h"

#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/p2p/base/constants.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/session/phone/cryptoparams.h"
#include "talk/session/phone/srtpfilter.h"
#include "talk/xmpp/constants.h"

namespace {
const char kInline[] = "inline:";
}

namespace cricket {

static bool CreateCryptoParams(int tag, const std::string& cipher,
                               CryptoParams *out) {
  std::string key;
  key.reserve(SRTP_MASTER_KEY_BASE64_LEN);

  if (!talk_base::CreateRandomString(SRTP_MASTER_KEY_BASE64_LEN, &key)) {
    return false;
  }
  out->tag = tag;
  out->cipher_suite = cipher;
  out->key_params = kInline;
  out->key_params += key;
  return true;
}

#ifdef HAVE_SRTP
static bool AddCryptoParams(const std::string& cipher_suite,
                            CryptoParamsVec *out) {
  int size = out->size();

  out->resize(size + 1);
  return CreateCryptoParams(size, cipher_suite, &out->at(size));
}
#endif

// For audio, HMAC 32 is prefered because of the low overhead.
static bool GetSupportedAudioCryptos(CryptoParamsVec* cryptos) {
#ifdef HAVE_SRTP
  return AddCryptoParams(CS_AES_CM_128_HMAC_SHA1_32, cryptos) &&
      AddCryptoParams(CS_AES_CM_128_HMAC_SHA1_80, cryptos);
#else
  return false;
#endif
}

static bool GetSupportedVideoCryptos(CryptoParamsVec* cryptos) {
#ifdef HAVE_SRTP
  return AddCryptoParams(CS_AES_CM_128_HMAC_SHA1_80, cryptos);
#else
  return false;
#endif
}

// For video support only 80-bit SHA1 HMAC. For audio 32-bit HMAC is
// tolerated because it is low overhead. Pick the crypto in the list
// that is supported.
static bool SelectCrypto(const MediaContentDescription* offer,
                         CryptoParams *crypto) {
  bool audio = offer->type() == MEDIA_TYPE_AUDIO;
  const CryptoParamsVec& cryptos = offer->cryptos();

  for (CryptoParamsVec::const_iterator i = cryptos.begin();
       i != cryptos.end(); ++i) {
    if (CS_AES_CM_128_HMAC_SHA1_80 == i->cipher_suite ||
        (CS_AES_CM_128_HMAC_SHA1_32 == i->cipher_suite && audio)) {
      return CreateCryptoParams(i->tag, i->cipher_suite, crypto);
    }
  }
  return false;
}

MediaSessionDescriptionFactory::MediaSessionDescriptionFactory()
    : secure_(SEC_DISABLED) {
}

MediaSessionDescriptionFactory::MediaSessionDescriptionFactory(
    ChannelManager* channel_manager)
    : secure_(SEC_DISABLED) {
  channel_manager->GetSupportedAudioCodecs(&audio_codecs_);
  channel_manager->GetSupportedVideoCodecs(&video_codecs_);
}

SessionDescription* MediaSessionDescriptionFactory::CreateOffer(
    const MediaSessionOptions& options) {
  SessionDescription* offer = new SessionDescription();

  if (true) {  // TODO: Allow audio to be optional
    AudioContentDescription* audio = new AudioContentDescription();
    for (AudioCodecs::const_iterator codec = audio_codecs_.begin();
         codec != audio_codecs_.end(); ++codec) {
      audio->AddCodec(*codec);
    }
    audio->SortCodecs();
    audio->set_ssrc(talk_base::CreateRandomNonZeroId());
    audio->set_rtcp_mux(true);
    audio->set_lang(lang_);
    audio->set_sources(options.audio_sources);

    if (secure() != SEC_DISABLED) {
      CryptoParamsVec audio_cryptos;
      if (GetSupportedAudioCryptos(&audio_cryptos)) {
        for (CryptoParamsVec::const_iterator crypto = audio_cryptos.begin();
             crypto != audio_cryptos.end(); ++crypto) {
          audio->AddCrypto(*crypto);
        }
      }
      if (secure() == SEC_REQUIRED) {
        if (audio->cryptos().empty()) {
          return NULL;  // Abort, crypto required but none found.
        }
        audio->set_crypto_required(true);
      }
    }

    offer->AddContent(CN_AUDIO, NS_JINGLE_RTP, audio);
  }

  // add video codecs, if this is a video call
  if (options.is_video) {
    VideoContentDescription* video = new VideoContentDescription();
    for (VideoCodecs::const_iterator codec = video_codecs_.begin();
         codec != video_codecs_.end(); ++codec) {
      video->AddCodec(*codec);
    }

    video->SortCodecs();
    video->set_ssrc(talk_base::CreateRandomNonZeroId());
    video->set_bandwidth(options.video_bandwidth);
    video->set_rtcp_mux(true);
    video->set_sources(options.video_sources);

    if (secure() != SEC_DISABLED) {
      CryptoParamsVec video_cryptos;
      if (GetSupportedVideoCryptos(&video_cryptos)) {
        for (CryptoParamsVec::const_iterator crypto = video_cryptos.begin();
             crypto != video_cryptos.end(); ++crypto) {
          video->AddCrypto(*crypto);
        }
      }
      if (secure() == SEC_REQUIRED) {
        if (video->cryptos().empty()) {
          return NULL;  // Abort, crypto required but none found.
        }
        video->set_crypto_required(true);
      }
    }

    offer->AddContent(CN_VIDEO, NS_JINGLE_RTP, video);
  }

  return offer;
}

SessionDescription* MediaSessionDescriptionFactory::CreateAnswer(
    const SessionDescription* offer, const MediaSessionOptions& options) {
  // The answer contains the intersection of the codecs in the offer with the
  // codecs we support, ordered by our local preference. As indicated by
  // XEP-0167, we retain the same payload ids from the offer in the answer.
  SessionDescription* accept = new SessionDescription();

  const ContentInfo* audio_content = GetFirstAudioContent(offer);
  if (audio_content) {
    const AudioContentDescription* audio_offer =
        static_cast<const AudioContentDescription*>(audio_content->description);
    AudioContentDescription* audio_accept = new AudioContentDescription();
    for (AudioCodecs::const_iterator ours = audio_codecs_.begin();
        ours != audio_codecs_.end(); ++ours) {
      for (AudioCodecs::const_iterator theirs = audio_offer->codecs().begin();
          theirs != audio_offer->codecs().end(); ++theirs) {
        if (ours->Matches(*theirs)) {
          AudioCodec negotiated(*ours);
          negotiated.id = theirs->id;
          audio_accept->AddCodec(negotiated);
        }
      }
    }

    audio_accept->SortCodecs();
    audio_accept->set_ssrc(talk_base::CreateRandomNonZeroId());
    audio_accept->set_rtcp_mux(audio_offer->rtcp_mux());
    audio_accept->set_sources(options.audio_sources);

    if (secure() != SEC_DISABLED) {
      CryptoParams crypto;

      if (SelectCrypto(audio_offer, &crypto)) {
        audio_accept->AddCrypto(crypto);
      }
    }

    if (audio_accept->cryptos().empty() &&
        (audio_offer->crypto_required() || secure() == SEC_REQUIRED)) {
      return NULL;  // Fails the session setup.
    }
    accept->AddContent(audio_content->name, audio_content->type, audio_accept);
  }

  const ContentInfo* video_content = GetFirstVideoContent(offer);
  if (video_content && options.is_video) {
    const VideoContentDescription* video_offer =
        static_cast<const VideoContentDescription*>(video_content->description);
    VideoContentDescription* video_accept = new VideoContentDescription();
    for (VideoCodecs::const_iterator ours = video_codecs_.begin();
        ours != video_codecs_.end(); ++ours) {
      for (VideoCodecs::const_iterator theirs = video_offer->codecs().begin();
          theirs != video_offer->codecs().end(); ++theirs) {
        if (ours->Matches(*theirs)) {
          VideoCodec negotiated(*ours);
          negotiated.id = theirs->id;
          video_accept->AddCodec(negotiated);
        }
      }
    }

    video_accept->set_ssrc(talk_base::CreateRandomNonZeroId());
    video_accept->set_bandwidth(options.video_bandwidth);
    video_accept->set_rtcp_mux(video_offer->rtcp_mux());
    video_accept->SortCodecs();
    video_accept->set_sources(options.video_sources);

    if (secure() != SEC_DISABLED) {
      CryptoParams crypto;

      if (SelectCrypto(video_offer, &crypto)) {
        video_accept->AddCrypto(crypto);
      }
    }

    if (video_accept->cryptos().empty() &&
        (video_offer->crypto_required() || secure() == SEC_REQUIRED)) {
      return NULL;  // Fails the session setup.
    }
    accept->AddContent(video_content->name, video_content->type, video_accept);
  }
  return accept;
}

static bool IsMediaContent(const ContentInfo* content, MediaType media_type) {
  if (content == NULL || content->type != NS_JINGLE_RTP) {
    return false;
  }

  const MediaContentDescription* media =
      static_cast<const MediaContentDescription*>(content->description);
  return media->type() == media_type;
}

bool IsAudioContent(const ContentInfo* content) {
  return IsMediaContent(content, MEDIA_TYPE_AUDIO);
}

bool IsVideoContent(const ContentInfo* content) {
  return IsMediaContent(content, MEDIA_TYPE_VIDEO);
}

static const ContentInfo* GetFirstMediaContent(const SessionDescription* sdesc,
                                               MediaType media_type) {
  if (sdesc == NULL)
    return NULL;

  const ContentInfos& contents = sdesc->contents();
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); content++) {
    if (IsMediaContent(&*content, media_type)) {
      return &*content;
    }
  }
  return NULL;
}

const ContentInfo* GetFirstAudioContent(const SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_AUDIO);
}

const ContentInfo* GetFirstVideoContent(const SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_VIDEO);
}

}  // namespace cricket
