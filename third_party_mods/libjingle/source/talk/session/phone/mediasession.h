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

// Types and classes used in media session descriptions.

#ifndef TALK_SESSION_PHONE_MEDIASESSION_H_
#define TALK_SESSION_PHONE_MEDIASESSION_H_

#include <string>
#include <vector>
#include <algorithm>

#include "talk/session/phone/codec.h"
#include "talk/session/phone/cryptoparams.h"
#include "talk/session/phone/mediachannel.h"
#include "talk/p2p/base/sessiondescription.h"

namespace cricket {

class ChannelManager;
typedef std::vector<AudioCodec> AudioCodecs;
typedef std::vector<VideoCodec> VideoCodecs;
typedef std::vector<CryptoParams> CryptoParamsVec;

// SEC_ENABLED and SEC_REQUIRED should only be used if the session
// was negotiated over TLS, to protect the inline crypto material
// exchange.
// SEC_DISABLED: No crypto in outgoing offer and answer. Fail any
//               offer with crypto required.
// SEC_ENABLED: Crypto in outgoing offer and answer. Fail any offer
//              with unsupported required crypto. Crypto set but not
//              required in outgoing offer.
// SEC_REQUIRED: Crypto in outgoing offer and answer with
//               required='true'. Fail any offer with no or
//               unsupported crypto (implicit crypto required='true'
//               in the offer.)
enum SecureMediaPolicy {
  SEC_DISABLED,
  SEC_ENABLED,
  SEC_REQUIRED
};

// Structure to describe a sending source.
struct SourceParam {
  SourceParam(uint32 ssrc,
              const std::string description,
              const std::string& cname)
      : ssrc(ssrc), description(description), cname(cname) {}
  uint32 ssrc;
  std::string description;
  std::string cname;
};
typedef std::vector<SourceParam> Sources;

// Options to control how session descriptions are generated.
const int kAutoBandwidth = -1;
struct MediaSessionOptions {
  MediaSessionOptions() :
      is_video(false),
      is_muc(false),
      video_bandwidth(kAutoBandwidth) {
  }
  Sources audio_sources;
  Sources video_sources;
  bool is_video;
  bool is_muc;
  // bps. -1 == auto.
  int video_bandwidth;
};

enum MediaType {
  MEDIA_TYPE_AUDIO,
  MEDIA_TYPE_VIDEO
};

// "content" (as used in XEP-0166) descriptions for voice and video.
class MediaContentDescription : public ContentDescription {
 public:
  MediaContentDescription()
      : ssrc_(0),
        ssrc_set_(false),
        rtcp_mux_(false),
        bandwidth_(kAutoBandwidth),
        crypto_required_(false),
        rtp_header_extensions_set_(false) {
  }

  virtual MediaType type() const = 0;

  uint32 ssrc() const { return ssrc_; }
  bool ssrc_set() const { return ssrc_set_; }
  void set_ssrc(uint32 ssrc) {
    ssrc_ = ssrc;
    ssrc_set_ = true;
  }

  bool rtcp_mux() const { return rtcp_mux_; }
  void set_rtcp_mux(bool mux) { rtcp_mux_ = mux; }

  int bandwidth() const { return bandwidth_; }
  void set_bandwidth(int bandwidth) { bandwidth_ = bandwidth; }

  const std::vector<CryptoParams>& cryptos() const { return cryptos_; }
  void AddCrypto(const CryptoParams& params) {
    cryptos_.push_back(params);
  }
  bool crypto_required() const { return crypto_required_; }
  void set_crypto_required(bool crypto) {
    crypto_required_ = crypto;
  }

  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const {
    return rtp_header_extensions_;
  }
  void AddRtpHeaderExtension(const RtpHeaderExtension& ext) {
    rtp_header_extensions_.push_back(ext);
    rtp_header_extensions_set_ = true;
  }
  void ClearRtpHeaderExtensions() {
    rtp_header_extensions_.clear();
    rtp_header_extensions_set_ = true;
  }
  // We can't always tell if an empty list of header extensions is
  // because the other side doesn't support them, or just isn't hooked up to
  // signal them. For now we assume an empty list means no signaling, but
  // provide the ClearRtpHeaderExtensions method to allow "no support" to be
  // clearly indicated (i.e. when derived from other information).
  bool rtp_header_extensions_set() const {
    return rtp_header_extensions_set_;
  }
  const Sources& sources() const {
    return sources_;
  }
  void set_sources(const Sources& sources) {
    sources_ = sources;
  }

 protected:
  uint32 ssrc_;
  bool ssrc_set_;
  bool rtcp_mux_;
  int bandwidth_;
  std::vector<CryptoParams> cryptos_;
  bool crypto_required_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
  bool rtp_header_extensions_set_;
  std::vector<SourceParam> sources_;
};

template <class C>
class MediaContentDescriptionImpl : public MediaContentDescription {
 public:
  struct PreferenceSort {
    bool operator()(C a, C b) { return a.preference > b.preference; }
  };

  const std::vector<C>& codecs() const { return codecs_; }
  void AddCodec(const C& codec) {
    codecs_.push_back(codec);
  }
  void SortCodecs() {
    std::sort(codecs_.begin(), codecs_.end(), PreferenceSort());
  }

 private:
  std::vector<C> codecs_;
};

class AudioContentDescription : public MediaContentDescriptionImpl<AudioCodec> {
 public:
  AudioContentDescription() :
      conference_mode_(false) {}

  virtual MediaType type() const { return MEDIA_TYPE_AUDIO; }

  bool conference_mode() const { return conference_mode_; }
  void set_conference_mode(bool enable) {
    conference_mode_ = enable;
  }

  const std::string &lang() const { return lang_; }
  void set_lang(const std::string &lang) { lang_ = lang; }


 private:
  bool conference_mode_;
  std::string lang_;
};

class VideoContentDescription : public MediaContentDescriptionImpl<VideoCodec> {
 public:
  virtual MediaType type() const { return MEDIA_TYPE_VIDEO; }
};

// Creates media session descriptions according to the supplied codecs and
// other fields, as well as the supplied per-call options.
// When creating answers, performs the appropriate negotiation
// of the various fields to determine the proper result.
class MediaSessionDescriptionFactory {
 public:
  // Default ctor; use methods below to set configuration.
  MediaSessionDescriptionFactory();
  // Helper, to allow configuration to be loaded from a ChannelManager.
  explicit MediaSessionDescriptionFactory(ChannelManager* manager);

  const AudioCodecs& audio_codecs() const { return audio_codecs_; }
  void set_audio_codecs(const AudioCodecs& codecs) { audio_codecs_ = codecs; }
  const VideoCodecs& video_codecs() const { return video_codecs_; }
  void set_video_codecs(const VideoCodecs& codecs) { video_codecs_ = codecs; }
  SecureMediaPolicy secure() const { return secure_; }
  void set_secure(SecureMediaPolicy s) { secure_ = s; }

  SessionDescription* CreateOffer(const MediaSessionOptions& options);
  SessionDescription* CreateAnswer(const SessionDescription* offer,
                                   const MediaSessionOptions& options);

 private:
  AudioCodecs audio_codecs_;
  VideoCodecs video_codecs_;
  SecureMediaPolicy secure_;
  std::string lang_;
};

// Convenience functions.
bool IsAudioContent(const ContentInfo* content);
bool IsVideoContent(const ContentInfo* content);
const ContentInfo* GetFirstAudioContent(const SessionDescription* sdesc);
const ContentInfo* GetFirstVideoContent(const SessionDescription* sdesc);

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_MEDIASESSION_H_
