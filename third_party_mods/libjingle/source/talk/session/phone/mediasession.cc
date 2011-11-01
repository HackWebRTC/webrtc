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
#include "talk/base/scoped_ptr.h"
#include "talk/p2p/base/constants.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/session/phone/cryptoparams.h"
#include "talk/session/phone/srtpfilter.h"
#include "talk/xmpp/constants.h"

namespace {
const char kInline[] = "inline:";
}

namespace cricket {

using talk_base::scoped_ptr;

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

static const StreamParams* FindStreamParamsByName(
    const StreamParamsVec& params_vec,
    const std::string& name) {
  for (StreamParamsVec::const_iterator it = params_vec.begin();
       it != params_vec.end(); ++it) {
    if (it->name == name)
      return &*it;
  }
  return NULL;
}

static const StreamParams* FindFirstStreamParamsByCname(
    const StreamParamsVec& params_vec,
    const std::string& cname) {
  for (StreamParamsVec::const_iterator it = params_vec.begin();
       it != params_vec.end(); ++it) {
    if (cname == it->cname)
      return &*it;
  }
  return NULL;
}

static const StreamParams* FindStreamParamsBySsrc(
    const StreamParamsVec& params_vec,
    uint32 ssrc) {
  for (StreamParamsVec::const_iterator stream_it = params_vec.begin();
       stream_it != params_vec.end(); ++stream_it) {
    const std::vector<uint32>& ssrcs = stream_it->ssrcs;
    for (std::vector<uint32>::const_iterator ssrc_it = ssrcs.begin();
         ssrc_it !=  ssrcs.end(); ++ssrc_it) {
      if (ssrc == *ssrc_it)
        return &*stream_it;
    }
  }
  return NULL;
}

// Generates a new CNAME or the CNAME of an already existing StreamParams
// if a StreamParams exist for another Stream in streams with sync_label
// sync_label.
static bool GenerateCname(const StreamParamsVec& params_vec,
                          const MediaSessionOptions::Streams& streams,
                          const std::string& synch_label,
                          std::string* cname) {
  ASSERT(cname);
  if (!cname)
    return false;

  // Check if a CNAME exist for any of the other synched streams.
  for (MediaSessionOptions::Streams::const_iterator stream_it = streams.begin();
       stream_it != streams.end() ; ++stream_it) {
    if (synch_label != stream_it->sync_label)
      continue;
    const StreamParams* param = FindStreamParamsByName(params_vec,
                                                       stream_it->name);
    if (param) {
      *cname = param->cname;
      return true;
    }
  }
  // No other stream seems to exist that we should sync with.
  // Generate a random string for the RTCP CNAME, as stated in RFC 6222.
  // This string is only used for synchronization, and therefore is opaque.
  do {
    if (!talk_base::CreateRandomString(16, cname)) {
      ASSERT(false);
      return false;
    }
  } while (FindFirstStreamParamsByCname(params_vec, *cname));

  return true;
}

// Generate a new SSRC and make sure it does not exist in params_vec.
static uint32 GenerateSsrc(const StreamParamsVec& params_vec) {
  uint32 ssrc = 0;
  do {
    ssrc = talk_base::CreateRandomNonZeroId();
  } while (FindStreamParamsBySsrc(params_vec, ssrc));
  return ssrc;
}

// Finds all StreamParams of all media types and attach them to stream_params.
static void GetCurrentStreamParams(const SessionDescription* sdesc,
                                   StreamParamsVec* stream_params) {
  if (!sdesc)
    return;

  const ContentInfos& contents = sdesc->contents();
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); content++) {
    if (!IsAudioContent(&*content) && !IsVideoContent(&*content))
      continue;
    const MediaContentDescription* media =
        static_cast<const MediaContentDescription*>(
            content->description);
    const StreamParamsVec& streams = media->streams();
    for (StreamParamsVec::const_iterator it = streams.begin();
         it != streams.end(); ++it) {
      stream_params->push_back(*it);
    }
  }
}

// Adds a StreamParams for each Stream in Streams with media type
// media_type to content_description.
// current_parms - All currently known StreamParams of any media type.
static bool AddStreamParams(
    MediaType media_type,
    const MediaSessionOptions::Streams& streams,
    StreamParamsVec* current_params,
    MediaContentDescription* content_description) {
  for (MediaSessionOptions::Streams::const_iterator stream_it = streams.begin();
       stream_it != streams.end(); ++stream_it) {
    if (stream_it->type != media_type)
      continue;  // Wrong media type.
    const StreamParams* params = FindStreamParamsByName(*current_params,
                                                        stream_it->name);
    if (!params) {
      // This is a new stream.
      // Get a CNAME. Either new or same as one of the other synched streams.
      std::string cname;
      if (!GenerateCname(*current_params, streams, stream_it->sync_label,
                         &cname)) {
        return false;
      }
      uint32 ssrc = GenerateSsrc(*current_params);
      // TODO(perkj): Generate the more complex types of stream_params.
      StreamParams stream_param(stream_it->name, ssrc, cname,
                                stream_it->sync_label);
      content_description->AddStream(stream_param);

      // Store the new StreamParams in current_params.
      // This is necessary so that we can use the CNAME for other media types.
      current_params->push_back(stream_param);
    } else {
      content_description->AddStream(*params);
    }
  }
  return true;
}

void MediaSessionOptions::AddStream(MediaType type,
                                    const std::string& name,
                                    const std::string& sync_label) {
  streams.push_back(Stream(type, name, sync_label));

  if (type == MEDIA_TYPE_VIDEO)
    has_video = true;
  else if (type == MEDIA_TYPE_AUDIO)
    has_audio = true;
}

void MediaSessionOptions::RemoveStream(MediaType type,
                                       const std::string& name) {
  Streams::iterator stream_it = streams.begin();
  for (; stream_it != streams.end(); ++stream_it) {
    if (stream_it->type == type && stream_it->name == name) {
      streams.erase(stream_it);
      break;
    }
  }
  ASSERT(stream_it != streams.end());
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
  return CreateOffer(options, NULL);
}

SessionDescription* MediaSessionDescriptionFactory::CreateOffer(
    const MediaSessionOptions& options,
    const SessionDescription* current_description) {
  scoped_ptr<SessionDescription> offer(new SessionDescription());

  StreamParamsVec current_params;
  GetCurrentStreamParams(current_description, &current_params);

  if (options.has_audio) {
    scoped_ptr<AudioContentDescription> audio(new AudioContentDescription());
    for (AudioCodecs::const_iterator codec = audio_codecs_.begin();
         codec != audio_codecs_.end(); ++codec) {
      audio->AddCodec(*codec);
    }
    audio->SortCodecs();
    if (!AddStreamParams(MEDIA_TYPE_AUDIO, options.streams, &current_params,
                         audio.get())) {
      return NULL;  // Abort, something went seriously wrong.
    }

    if (options.streams.empty()) {
      // TODO(perkj): Remove this legacy ssrc when all apps use StreamParams.
      audio->set_ssrc(talk_base::CreateRandomNonZeroId());
    }
    audio->set_rtcp_mux(true);
    audio->set_lang(lang_);

    if (secure() != SEC_DISABLED) {
      CryptoParamsVec audio_cryptos;
      if (current_description) {
        // Copy crypto parameters from the previous offer.
        const ContentInfo* info =
            GetFirstAudioContent(current_description);
        if (info) {
          const AudioContentDescription* desc =
              static_cast<const AudioContentDescription*>(info->description);
          audio_cryptos = desc->cryptos();
        }
      }
      if (audio_cryptos.empty())
        GetSupportedAudioCryptos(&audio_cryptos);  // Generate new cryptos.

      for (CryptoParamsVec::const_iterator crypto = audio_cryptos.begin();
           crypto != audio_cryptos.end(); ++crypto) {
        audio->AddCrypto(*crypto);
      }

      if (secure() == SEC_REQUIRED) {
        if (audio->cryptos().empty()) {
          return NULL;  // Abort, crypto required but none found.
        }
        audio->set_crypto_required(true);
      }
    }

    offer->AddContent(CN_AUDIO, NS_JINGLE_RTP, audio.release());
  }

  // add video codecs, if this is a video call
  if (options.has_video) {
    scoped_ptr<VideoContentDescription> video(new VideoContentDescription());
    for (VideoCodecs::const_iterator codec = video_codecs_.begin();
         codec != video_codecs_.end(); ++codec) {
      video->AddCodec(*codec);
    }

    video->SortCodecs();
    if (!AddStreamParams(MEDIA_TYPE_VIDEO, options.streams, &current_params,
                         video.get())) {
      return NULL;  // Abort, something went seriously wrong.
    }

    if (options.streams.empty()) {
      // TODO(perkj): Remove this legacy ssrc when all apps use StreamParams.
      video->set_ssrc(talk_base::CreateRandomNonZeroId());
    }
    video->set_bandwidth(options.video_bandwidth);
    video->set_rtcp_mux(true);

    if (secure() != SEC_DISABLED) {
      CryptoParamsVec video_cryptos;
      if (current_description) {
        // Copy crypto parameters from the previous offer.
        const ContentInfo* info =
            GetFirstVideoContent(current_description);
        if (info) {
          const VideoContentDescription* desc =
              static_cast<const VideoContentDescription*>(info->description);
          video_cryptos = desc->cryptos();
        }
      }
      if (video_cryptos.empty())
        GetSupportedVideoCryptos(&video_cryptos);  // Generate new crypto.
      for (CryptoParamsVec::const_iterator crypto = video_cryptos.begin();
           crypto != video_cryptos.end(); ++crypto) {
        video->AddCrypto(*crypto);
      }
      if (secure() == SEC_REQUIRED) {
        if (video->cryptos().empty()) {
          return NULL;  // Abort, crypto required but none found.
        }
        video->set_crypto_required(true);
      }
    }

    offer->AddContent(CN_VIDEO, NS_JINGLE_RTP, video.release());
  }

  return offer.release();
}

SessionDescription* MediaSessionDescriptionFactory::CreateAnswer(
    const SessionDescription* offer,
    const MediaSessionOptions& options) {
  return CreateAnswer(offer, options, NULL);
}

SessionDescription* MediaSessionDescriptionFactory::CreateAnswer(
    const SessionDescription* offer, const MediaSessionOptions& options,
    const SessionDescription* current_description) {
  // The answer contains the intersection of the codecs in the offer with the
  // codecs we support, ordered by our local preference. As indicated by
  // XEP-0167, we retain the same payload ids from the offer in the answer.
  scoped_ptr<SessionDescription> accept(new SessionDescription());

  StreamParamsVec current_params;
  GetCurrentStreamParams(current_description, &current_params);

  const ContentInfo* audio_content = GetFirstAudioContent(offer);
  if (audio_content && options.has_audio) {
    const AudioContentDescription* audio_offer =
        static_cast<const AudioContentDescription*>(audio_content->description);
    scoped_ptr<AudioContentDescription> audio_accept(
        new AudioContentDescription());
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
    if (!AddStreamParams(MEDIA_TYPE_AUDIO, options.streams, &current_params,
                         audio_accept.get())) {
      return NULL;  // Abort, something went seriously wrong.
    }

    if (options.streams.empty()) {
      // TODO(perkj): Remove this legacy ssrc when all apps use StreamParams.
      audio_accept->set_ssrc(talk_base::CreateRandomNonZeroId());
    }
    audio_accept->set_rtcp_mux(audio_offer->rtcp_mux());

    if (secure() != SEC_DISABLED) {
      CryptoParams crypto;

      if (SelectCrypto(audio_offer, &crypto)) {
        if (current_description) {
          // Check if this crypto already exist in the previous
          // session description. Use it in that case.
          const ContentInfo* info =
              GetFirstAudioContent(current_description);
          if (info) {
            const AudioContentDescription* desc =
                static_cast<const AudioContentDescription*>(info->description);
            const CryptoParamsVec& cryptos = desc->cryptos();
            for (CryptoParamsVec::const_iterator it = cryptos.begin();
                it != cryptos.end(); ++it) {
              if (crypto.Matches(*it)) {
                crypto = *it;
                break;
              }
            }
          }
        }
        audio_accept->AddCrypto(crypto);
      }
    }

    if (audio_accept->cryptos().empty() &&
        (audio_offer->crypto_required() || secure() == SEC_REQUIRED)) {
      return NULL;  // Fails the session setup.
    }
    accept->AddContent(audio_content->name, audio_content->type,
                       audio_accept.release());
  } else {
    LOG(LS_INFO) << "Audio is not supported in answer";
  }

  const ContentInfo* video_content = GetFirstVideoContent(offer);
  if (video_content && options.has_video) {
    const VideoContentDescription* video_offer =
        static_cast<const VideoContentDescription*>(video_content->description);
    scoped_ptr<VideoContentDescription> video_accept(
        new VideoContentDescription());
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
    if (!AddStreamParams(MEDIA_TYPE_VIDEO, options.streams, &current_params,
                         video_accept.get())) {
      return NULL;  // Abort, something went seriously wrong.
    }

    if (options.streams.empty()) {
      // TODO(perkj): Remove this legacy ssrc when all apps use StreamParams.
      video_accept->set_ssrc(talk_base::CreateRandomNonZeroId());
    }
    video_accept->set_bandwidth(options.video_bandwidth);
    video_accept->set_rtcp_mux(video_offer->rtcp_mux());
    video_accept->SortCodecs();

    if (secure() != SEC_DISABLED) {
      CryptoParams crypto;

      if (SelectCrypto(video_offer, &crypto)) {
        if (current_description) {
          // Check if this crypto already exist in the previous
          // session description. Use it in that case.
          const ContentInfo* info = GetFirstVideoContent(current_description);
          if (info) {
            const VideoContentDescription* desc =
                static_cast<const VideoContentDescription*>(info->description);
            const CryptoParamsVec& cryptos = desc->cryptos();
            for (CryptoParamsVec::const_iterator it = cryptos.begin();
                 it != cryptos.end(); ++it) {
              if (crypto.Matches(*it)) {
                crypto = *it;
                break;
              }
            }
          }
        }
        video_accept->AddCrypto(crypto);
      }
    }

    if (video_accept->cryptos().empty() &&
        (video_offer->crypto_required() || secure() == SEC_REQUIRED)) {
      return NULL;  // Fails the session setup.
    }
    accept->AddContent(video_content->name, video_content->type,
                       video_accept.release());
  } else {
    LOG(LS_INFO) << "Video is not supported in answer";
  }
  return accept.release();
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
