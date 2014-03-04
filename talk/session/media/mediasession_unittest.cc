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

#include <string>
#include <vector>

#include "talk/base/gunit.h"
#include "talk/base/fakesslidentity.h"
#include "talk/base/messagedigest.h"
#include "talk/media/base/codec.h"
#include "talk/media/base/testutils.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/transportdescription.h"
#include "talk/p2p/base/transportinfo.h"
#include "talk/session/media/mediasession.h"
#include "talk/session/media/srtpfilter.h"

#ifdef HAVE_SRTP
#define ASSERT_CRYPTO(cd, s, cs) \
    ASSERT_EQ(cricket::CT_NONE, cd->crypto_required()); \
    ASSERT_EQ(s, cd->cryptos().size()); \
    ASSERT_EQ(std::string(cs), cd->cryptos()[0].cipher_suite)
#else
#define ASSERT_CRYPTO(cd, s, cs) \
  ASSERT_EQ(cricket::CT_NONE, cd->crypto_required()); \
  ASSERT_EQ(0U, cd->cryptos().size());
#endif

typedef std::vector<cricket::Candidate> Candidates;

using cricket::MediaContentDescription;
using cricket::MediaSessionDescriptionFactory;
using cricket::MediaSessionOptions;
using cricket::MediaType;
using cricket::SessionDescription;
using cricket::SsrcGroup;
using cricket::StreamParams;
using cricket::StreamParamsVec;
using cricket::TransportDescription;
using cricket::TransportDescriptionFactory;
using cricket::TransportInfo;
using cricket::ContentInfo;
using cricket::CryptoParamsVec;
using cricket::AudioContentDescription;
using cricket::VideoContentDescription;
using cricket::DataContentDescription;
using cricket::GetFirstAudioContentDescription;
using cricket::GetFirstVideoContentDescription;
using cricket::GetFirstDataContentDescription;
using cricket::kAutoBandwidth;
using cricket::AudioCodec;
using cricket::VideoCodec;
using cricket::DataCodec;
using cricket::NS_JINGLE_RTP;
using cricket::MEDIA_TYPE_AUDIO;
using cricket::MEDIA_TYPE_VIDEO;
using cricket::MEDIA_TYPE_DATA;
using cricket::RtpHeaderExtension;
using cricket::SEC_DISABLED;
using cricket::SEC_ENABLED;
using cricket::SEC_REQUIRED;
using cricket::CS_AES_CM_128_HMAC_SHA1_32;
using cricket::CS_AES_CM_128_HMAC_SHA1_80;

static const AudioCodec kAudioCodecs1[] = {
  AudioCodec(103, "ISAC",   16000, -1,    1, 6),
  AudioCodec(102, "iLBC",   8000,  13300, 1, 5),
  AudioCodec(0,   "PCMU",   8000,  64000, 1, 4),
  AudioCodec(8,   "PCMA",   8000,  64000, 1, 3),
  AudioCodec(117, "red",    8000,  0,     1, 2),
  AudioCodec(107, "CN",     48000, 0,     1, 1)
};

static const AudioCodec kAudioCodecs2[] = {
  AudioCodec(126, "speex",  16000, 22000, 1, 3),
  AudioCodec(127, "iLBC",   8000,  13300, 1, 2),
  AudioCodec(0,   "PCMU",   8000,  64000, 1, 1),
};

static const AudioCodec kAudioCodecsAnswer[] = {
  AudioCodec(102, "iLBC",   8000,  13300, 1, 2),
  AudioCodec(0,   "PCMU",   8000,  64000, 1, 1),
};

static const VideoCodec kVideoCodecs1[] = {
  VideoCodec(96, "H264-SVC", 320, 200, 30, 2),
  VideoCodec(97, "H264", 320, 200, 30, 1)
};

static const VideoCodec kVideoCodecs2[] = {
  VideoCodec(126, "H264", 320, 200, 30, 2),
  VideoCodec(127, "H263", 320, 200, 30, 1)
};

static const VideoCodec kVideoCodecsAnswer[] = {
  VideoCodec(97, "H264", 320, 200, 30, 2)
};

static const DataCodec kDataCodecs1[] = {
  DataCodec(98, "binary-data", 2),
  DataCodec(99, "utf8-text", 1)
};

static const DataCodec kDataCodecs2[] = {
  DataCodec(126, "binary-data", 2),
  DataCodec(127, "utf8-text", 1)
};

static const DataCodec kDataCodecsAnswer[] = {
  DataCodec(98, "binary-data", 2),
  DataCodec(99, "utf8-text", 1)
};

static const RtpHeaderExtension kAudioRtpExtension1[] = {
  RtpHeaderExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8),
  RtpHeaderExtension("http://google.com/testing/audio_something", 10),
};

static const RtpHeaderExtension kAudioRtpExtension2[] = {
  RtpHeaderExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 2),
  RtpHeaderExtension("http://google.com/testing/audio_something_else", 8),
};

static const RtpHeaderExtension kAudioRtpExtensionAnswer[] = {
  RtpHeaderExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8),
};

static const RtpHeaderExtension kVideoRtpExtension1[] = {
  RtpHeaderExtension("urn:ietf:params:rtp-hdrext:toffset", 14),
  RtpHeaderExtension("http://google.com/testing/video_something", 15),
};

static const RtpHeaderExtension kVideoRtpExtension2[] = {
  RtpHeaderExtension("urn:ietf:params:rtp-hdrext:toffset", 2),
  RtpHeaderExtension("http://google.com/testing/video_something_else", 14),
};

static const RtpHeaderExtension kVideoRtpExtensionAnswer[] = {
  RtpHeaderExtension("urn:ietf:params:rtp-hdrext:toffset", 14),
};

static const uint32 kSimulcastParamsSsrc[] = {10, 11, 20, 21, 30, 31};
static const uint32 kSimSsrc[] = {10, 20, 30};
static const uint32 kFec1Ssrc[] = {10, 11};
static const uint32 kFec2Ssrc[] = {20, 21};
static const uint32 kFec3Ssrc[] = {30, 31};

static const char kMediaStream1[] = "stream_1";
static const char kMediaStream2[] = "stream_2";
static const char kVideoTrack1[] = "video_1";
static const char kVideoTrack2[] = "video_2";
static const char kAudioTrack1[] = "audio_1";
static const char kAudioTrack2[] = "audio_2";
static const char kAudioTrack3[] = "audio_3";
static const char kDataTrack1[] = "data_1";
static const char kDataTrack2[] = "data_2";
static const char kDataTrack3[] = "data_3";

class MediaSessionDescriptionFactoryTest : public testing::Test {
 public:
  MediaSessionDescriptionFactoryTest()
      : f1_(&tdf1_), f2_(&tdf2_), id1_("id1"), id2_("id2") {
    f1_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs1));
    f1_.set_video_codecs(MAKE_VECTOR(kVideoCodecs1));
    f1_.set_data_codecs(MAKE_VECTOR(kDataCodecs1));
    f2_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs2));
    f2_.set_video_codecs(MAKE_VECTOR(kVideoCodecs2));
    f2_.set_data_codecs(MAKE_VECTOR(kDataCodecs2));
    tdf1_.set_identity(&id1_);
    tdf2_.set_identity(&id2_);
  }

  // Create a video StreamParamsVec object with:
  // - one video stream with 3 simulcast streams and FEC,
  StreamParamsVec CreateComplexVideoStreamParamsVec() {
    SsrcGroup sim_group("SIM", MAKE_VECTOR(kSimSsrc));
    SsrcGroup fec_group1("FEC", MAKE_VECTOR(kFec1Ssrc));
    SsrcGroup fec_group2("FEC", MAKE_VECTOR(kFec2Ssrc));
    SsrcGroup fec_group3("FEC", MAKE_VECTOR(kFec3Ssrc));

    std::vector<SsrcGroup> ssrc_groups;
    ssrc_groups.push_back(sim_group);
    ssrc_groups.push_back(fec_group1);
    ssrc_groups.push_back(fec_group2);
    ssrc_groups.push_back(fec_group3);

    StreamParams simulcast_params;
    simulcast_params.id = kVideoTrack1;
    simulcast_params.ssrcs = MAKE_VECTOR(kSimulcastParamsSsrc);
    simulcast_params.ssrc_groups = ssrc_groups;
    simulcast_params.cname = "Video_SIM_FEC";
    simulcast_params.sync_label = kMediaStream1;

    StreamParamsVec video_streams;
    video_streams.push_back(simulcast_params);

    return video_streams;
  }

  bool CompareCryptoParams(const CryptoParamsVec& c1,
                           const CryptoParamsVec& c2) {
    if (c1.size() != c2.size())
      return false;
    for (size_t i = 0; i < c1.size(); ++i)
      if (c1[i].tag != c2[i].tag || c1[i].cipher_suite != c2[i].cipher_suite ||
          c1[i].key_params != c2[i].key_params ||
          c1[i].session_params != c2[i].session_params)
        return false;
    return true;
  }

  void TestTransportInfo(bool offer, const MediaSessionOptions& options,
                         bool has_current_desc) {
    const std::string current_audio_ufrag = "current_audio_ufrag";
    const std::string current_audio_pwd = "current_audio_pwd";
    const std::string current_video_ufrag = "current_video_ufrag";
    const std::string current_video_pwd = "current_video_pwd";
    const std::string current_data_ufrag = "current_data_ufrag";
    const std::string current_data_pwd = "current_data_pwd";
    talk_base::scoped_ptr<SessionDescription> current_desc;
    talk_base::scoped_ptr<SessionDescription> desc;
    if (has_current_desc) {
      current_desc.reset(new SessionDescription());
      EXPECT_TRUE(current_desc->AddTransportInfo(
          TransportInfo("audio",
                        TransportDescription("",
                                             current_audio_ufrag,
                                             current_audio_pwd))));
      EXPECT_TRUE(current_desc->AddTransportInfo(
          TransportInfo("video",
                        TransportDescription("",
                                             current_video_ufrag,
                                             current_video_pwd))));
      EXPECT_TRUE(current_desc->AddTransportInfo(
          TransportInfo("data",
                        TransportDescription("",
                                             current_data_ufrag,
                                             current_data_pwd))));
    }
    if (offer) {
      desc.reset(f1_.CreateOffer(options, current_desc.get()));
    } else {
      talk_base::scoped_ptr<SessionDescription> offer;
      offer.reset(f1_.CreateOffer(options, NULL));
      desc.reset(f1_.CreateAnswer(offer.get(), options, current_desc.get()));
    }
    ASSERT_TRUE(desc.get() != NULL);
    const TransportInfo* ti_audio = desc->GetTransportInfoByName("audio");
    if (options.has_audio) {
      EXPECT_TRUE(ti_audio != NULL);
      if (has_current_desc) {
        EXPECT_EQ(current_audio_ufrag, ti_audio->description.ice_ufrag);
        EXPECT_EQ(current_audio_pwd, ti_audio->description.ice_pwd);
      } else {
        EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
                  ti_audio->description.ice_ufrag.size());
        EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
                  ti_audio->description.ice_pwd.size());
      }

    } else {
      EXPECT_TRUE(ti_audio == NULL);
    }
    const TransportInfo* ti_video = desc->GetTransportInfoByName("video");
    if (options.has_video) {
      EXPECT_TRUE(ti_video != NULL);
      if (options.bundle_enabled) {
        EXPECT_EQ(ti_audio->description.ice_ufrag,
                  ti_video->description.ice_ufrag);
        EXPECT_EQ(ti_audio->description.ice_pwd,
                  ti_video->description.ice_pwd);
      } else {
        if (has_current_desc) {
          EXPECT_EQ(current_video_ufrag, ti_video->description.ice_ufrag);
          EXPECT_EQ(current_video_pwd, ti_video->description.ice_pwd);
        } else {
          EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
                    ti_video->description.ice_ufrag.size());
          EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
                    ti_video->description.ice_pwd.size());
        }
      }
    } else {
      EXPECT_TRUE(ti_video == NULL);
    }
    const TransportInfo* ti_data = desc->GetTransportInfoByName("data");
    if (options.has_data()) {
      EXPECT_TRUE(ti_data != NULL);
      if (options.bundle_enabled) {
        EXPECT_EQ(ti_audio->description.ice_ufrag,
                  ti_data->description.ice_ufrag);
        EXPECT_EQ(ti_audio->description.ice_pwd,
                  ti_data->description.ice_pwd);
      } else {
        if (has_current_desc) {
          EXPECT_EQ(current_data_ufrag, ti_data->description.ice_ufrag);
          EXPECT_EQ(current_data_pwd, ti_data->description.ice_pwd);
        } else {
          EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
                    ti_data->description.ice_ufrag.size());
          EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
                    ti_data->description.ice_pwd.size());
        }
      }
    } else {
      EXPECT_TRUE(ti_video == NULL);
    }
  }

  void TestCryptoWithBundle(bool offer) {
    f1_.set_secure(SEC_ENABLED);
    MediaSessionOptions options;
    options.has_audio = true;
    options.has_video = true;
    options.data_channel_type = cricket::DCT_RTP;
    talk_base::scoped_ptr<SessionDescription> ref_desc;
    talk_base::scoped_ptr<SessionDescription> desc;
    if (offer) {
      options.bundle_enabled = false;
      ref_desc.reset(f1_.CreateOffer(options, NULL));
      options.bundle_enabled = true;
      desc.reset(f1_.CreateOffer(options, ref_desc.get()));
    } else {
      options.bundle_enabled = true;
      ref_desc.reset(f1_.CreateOffer(options, NULL));
      desc.reset(f1_.CreateAnswer(ref_desc.get(), options, NULL));
    }
    ASSERT_TRUE(desc.get() != NULL);
    const cricket::MediaContentDescription* audio_media_desc =
        static_cast<const cricket::MediaContentDescription*>(
            desc.get()->GetContentDescriptionByName("audio"));
    ASSERT_TRUE(audio_media_desc != NULL);
    const cricket::MediaContentDescription* video_media_desc =
        static_cast<const cricket::MediaContentDescription*>(
            desc.get()->GetContentDescriptionByName("video"));
    ASSERT_TRUE(video_media_desc != NULL);
    EXPECT_TRUE(CompareCryptoParams(audio_media_desc->cryptos(),
                                    video_media_desc->cryptos()));
    EXPECT_EQ(1u, audio_media_desc->cryptos().size());
    EXPECT_EQ(std::string(CS_AES_CM_128_HMAC_SHA1_80),
              audio_media_desc->cryptos()[0].cipher_suite);

    // Verify the selected crypto is one from the reference audio
    // media content.
    const cricket::MediaContentDescription* ref_audio_media_desc =
        static_cast<const cricket::MediaContentDescription*>(
            ref_desc.get()->GetContentDescriptionByName("audio"));
    bool found = false;
    for (size_t i = 0; i < ref_audio_media_desc->cryptos().size(); ++i) {
      if (ref_audio_media_desc->cryptos()[i].Matches(
          audio_media_desc->cryptos()[0])) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }

  // This test that the audio and video media direction is set to
  // |expected_direction_in_answer| in an answer if the offer direction is set
  // to |direction_in_offer|.
  void TestMediaDirectionInAnswer(
      cricket::MediaContentDirection direction_in_offer,
      cricket::MediaContentDirection expected_direction_in_answer) {
    MediaSessionOptions opts;
    opts.has_video = true;
    talk_base::scoped_ptr<SessionDescription> offer(
        f1_.CreateOffer(opts, NULL));
    ASSERT_TRUE(offer.get() != NULL);
    ContentInfo* ac_offer= offer->GetContentByName("audio");
    ASSERT_TRUE(ac_offer != NULL);
    AudioContentDescription* acd_offer =
        static_cast<AudioContentDescription*>(ac_offer->description);
    acd_offer->set_direction(direction_in_offer);
    ContentInfo* vc_offer= offer->GetContentByName("video");
    ASSERT_TRUE(vc_offer != NULL);
    VideoContentDescription* vcd_offer =
        static_cast<VideoContentDescription*>(vc_offer->description);
    vcd_offer->set_direction(direction_in_offer);

    talk_base::scoped_ptr<SessionDescription> answer(
        f2_.CreateAnswer(offer.get(), opts, NULL));
    const AudioContentDescription* acd_answer =
        GetFirstAudioContentDescription(answer.get());
    EXPECT_EQ(expected_direction_in_answer, acd_answer->direction());
    const VideoContentDescription* vcd_answer =
        GetFirstVideoContentDescription(answer.get());
    EXPECT_EQ(expected_direction_in_answer, vcd_answer->direction());
  }

  bool VerifyNoCNCodecs(const cricket::ContentInfo* content) {
    const cricket::ContentDescription* description = content->description;
    ASSERT(description != NULL);
    const cricket::AudioContentDescription* audio_content_desc =
        static_cast<const cricket::AudioContentDescription*>(description);
    ASSERT(audio_content_desc != NULL);
    for (size_t i = 0; i < audio_content_desc->codecs().size(); ++i) {
      if (audio_content_desc->codecs()[i].name == "CN")
        return false;
    }
    return true;
  }

 protected:
  MediaSessionDescriptionFactory f1_;
  MediaSessionDescriptionFactory f2_;
  TransportDescriptionFactory tdf1_;
  TransportDescriptionFactory tdf2_;
  talk_base::FakeSSLIdentity id1_;
  talk_base::FakeSSLIdentity id2_;
};

// Create a typical audio offer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioOffer) {
  f1_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription> offer(
      f1_.CreateOffer(MediaSessionOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc == NULL);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), ac->type);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_codecs(), acd->codecs());
  EXPECT_NE(0U, acd->first_ssrc());             // a random nonzero ssrc
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(acd, 2U, CS_AES_CM_128_HMAC_SHA1_32);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), acd->protocol());
}

// Create a typical video offer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoOffer) {
  MediaSessionOptions opts;
  opts.has_video = true;
  f1_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription>
      offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), ac->type);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), vc->type);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const VideoContentDescription* vcd =
      static_cast<const VideoContentDescription*>(vc->description);
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_codecs(), acd->codecs());
  EXPECT_NE(0U, acd->first_ssrc());             // a random nonzero ssrc
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(acd, 2U, CS_AES_CM_128_HMAC_SHA1_32);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), acd->protocol());
  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_EQ(f1_.video_codecs(), vcd->codecs());
  EXPECT_NE(0U, vcd->first_ssrc());             // a random nonzero ssrc
  EXPECT_EQ(kAutoBandwidth, vcd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(vcd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(vcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), vcd->protocol());
}

// Test creating an offer with bundle where the Codecs have the same dynamic
// RTP playlod type. The test verifies that the offer don't contain the
// duplicate RTP payload types.
TEST_F(MediaSessionDescriptionFactoryTest, TestBundleOfferWithSameCodecPlType) {
  const VideoCodec& offered_video_codec = f2_.video_codecs()[0];
  const AudioCodec& offered_audio_codec = f2_.audio_codecs()[0];
  const DataCodec& offered_data_codec = f2_.data_codecs()[0];
  ASSERT_EQ(offered_video_codec.id, offered_audio_codec.id);
  ASSERT_EQ(offered_video_codec.id, offered_data_codec.id);

  MediaSessionOptions opts;
  opts.has_audio = true;
  opts.has_video = true;
  opts.data_channel_type = cricket::DCT_RTP;
  opts.bundle_enabled = true;
  talk_base::scoped_ptr<SessionDescription>
  offer(f2_.CreateOffer(opts, NULL));
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(offer.get());
  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(offer.get());
  const DataContentDescription* dcd =
      GetFirstDataContentDescription(offer.get());
  ASSERT_TRUE(NULL != vcd);
  ASSERT_TRUE(NULL != acd);
  ASSERT_TRUE(NULL != dcd);
  EXPECT_NE(vcd->codecs()[0].id, acd->codecs()[0].id);
  EXPECT_NE(vcd->codecs()[0].id, dcd->codecs()[0].id);
  EXPECT_NE(acd->codecs()[0].id, dcd->codecs()[0].id);
  EXPECT_EQ(vcd->codecs()[0].name, offered_video_codec.name);
  EXPECT_EQ(acd->codecs()[0].name, offered_audio_codec.name);
  EXPECT_EQ(dcd->codecs()[0].name, offered_data_codec.name);
}

// Test creating an updated offer with with bundle, audio, video and data
// after an audio only session has been negotiated.
TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateUpdatedVideoOfferWithBundle) {
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  MediaSessionOptions opts;
  opts.has_audio = true;
  opts.has_video = false;
  opts.data_channel_type = cricket::DCT_NONE;
  opts.bundle_enabled = true;
  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  MediaSessionOptions updated_opts;
  updated_opts.has_audio = true;
  updated_opts.has_video = true;
  updated_opts.data_channel_type = cricket::DCT_RTP;
  updated_opts.bundle_enabled = true;
  talk_base::scoped_ptr<SessionDescription> updated_offer(f1_.CreateOffer(
      updated_opts, answer.get()));

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(updated_offer.get());
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  const DataContentDescription* dcd =
      GetFirstDataContentDescription(updated_offer.get());
  EXPECT_TRUE(NULL != vcd);
  EXPECT_TRUE(NULL != acd);
  EXPECT_TRUE(NULL != dcd);

  ASSERT_CRYPTO(acd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), acd->protocol());
  ASSERT_CRYPTO(vcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), vcd->protocol());
  ASSERT_CRYPTO(dcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), dcd->protocol());
}
// Create a RTP data offer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateRtpDataOffer) {
  MediaSessionOptions opts;
  opts.data_channel_type = cricket::DCT_RTP;
  f1_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription>
      offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* dc = offer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(dc != NULL);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), ac->type);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), dc->type);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const DataContentDescription* dcd =
      static_cast<const DataContentDescription*>(dc->description);
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_codecs(), acd->codecs());
  EXPECT_NE(0U, acd->first_ssrc());             // a random nonzero ssrc
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(acd, 2U, CS_AES_CM_128_HMAC_SHA1_32);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), acd->protocol());
  EXPECT_EQ(MEDIA_TYPE_DATA, dcd->type());
  EXPECT_EQ(f1_.data_codecs(), dcd->codecs());
  EXPECT_NE(0U, dcd->first_ssrc());             // a random nonzero ssrc
  EXPECT_EQ(cricket::kDataMaxBandwidth,
            dcd->bandwidth());                  // default bandwidth (auto)
  EXPECT_TRUE(dcd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(dcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), dcd->protocol());
}

// Create an SCTP data offer with bundle without error.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateSctpDataOffer) {
  MediaSessionOptions opts;
  opts.has_audio = false;
  opts.bundle_enabled = true;
  opts.data_channel_type = cricket::DCT_SCTP;
  f1_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  EXPECT_TRUE(offer.get() != NULL);
  EXPECT_TRUE(offer->GetContentByName("data") != NULL);
}

// Create an audio, video offer without legacy StreamParams.
TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateOfferWithoutLegacyStreams) {
  MediaSessionOptions opts;
  opts.has_video = true;
  f1_.set_add_legacy_streams(false);
  talk_base::scoped_ptr<SessionDescription>
      offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const VideoContentDescription* vcd =
      static_cast<const VideoContentDescription*>(vc->description);

  EXPECT_FALSE(vcd->has_ssrcs());             // No StreamParams.
  EXPECT_FALSE(acd->has_ssrcs());             // No StreamParams.
}

// Create a typical audio answer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioAnswer) {
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription> offer(
      f1_.CreateOffer(MediaSessionOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), MediaSessionOptions(), NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc == NULL);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), ac->type);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(MAKE_VECTOR(kAudioCodecsAnswer), acd->codecs());
  EXPECT_NE(0U, acd->first_ssrc());             // a random nonzero ssrc
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // negotiated auto bw
  EXPECT_TRUE(acd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(acd, 1U, CS_AES_CM_128_HMAC_SHA1_32);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), acd->protocol());
}

// Create a typical video answer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoAnswer) {
  MediaSessionOptions opts;
  opts.has_video = true;
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), ac->type);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), vc->type);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const VideoContentDescription* vcd =
      static_cast<const VideoContentDescription*>(vc->description);
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(MAKE_VECTOR(kAudioCodecsAnswer), acd->codecs());
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // negotiated auto bw
  EXPECT_NE(0U, acd->first_ssrc());             // a random nonzero ssrc
  EXPECT_TRUE(acd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(acd, 1U, CS_AES_CM_128_HMAC_SHA1_32);
  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_EQ(MAKE_VECTOR(kVideoCodecsAnswer), vcd->codecs());
  EXPECT_NE(0U, vcd->first_ssrc());             // a random nonzero ssrc
  EXPECT_TRUE(vcd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(vcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), vcd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateDataAnswer) {
  MediaSessionOptions opts;
  opts.data_channel_type = cricket::DCT_RTP;
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), ac->type);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), vc->type);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const DataContentDescription* vcd =
      static_cast<const DataContentDescription*>(vc->description);
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(MAKE_VECTOR(kAudioCodecsAnswer), acd->codecs());
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // negotiated auto bw
  EXPECT_NE(0U, acd->first_ssrc());             // a random nonzero ssrc
  EXPECT_TRUE(acd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(acd, 1U, CS_AES_CM_128_HMAC_SHA1_32);
  EXPECT_EQ(MEDIA_TYPE_DATA, vcd->type());
  EXPECT_EQ(MAKE_VECTOR(kDataCodecsAnswer), vcd->codecs());
  EXPECT_NE(0U, vcd->first_ssrc());             // a random nonzero ssrc
  EXPECT_TRUE(vcd->rtcp_mux());                 // negotiated rtcp-mux
  ASSERT_CRYPTO(vcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf), vcd->protocol());
}

// This test that the media direction is set to send/receive in an answer if
// the offer is send receive.
TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToSendReceiveOffer) {
  TestMediaDirectionInAnswer(cricket::MD_SENDRECV, cricket::MD_SENDRECV);
}

// This test that the media direction is set to receive only in an answer if
// the offer is send only.
TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToSendOnlyOffer) {
  TestMediaDirectionInAnswer(cricket::MD_SENDONLY, cricket::MD_RECVONLY);
}

// This test that the media direction is set to send only in an answer if
// the offer is recv only.
TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToRecvOnlyOffer) {
  TestMediaDirectionInAnswer(cricket::MD_RECVONLY, cricket::MD_SENDONLY);
}

// This test that the media direction is set to inactive in an answer if
// the offer is inactive.
TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToInactiveOffer) {
  TestMediaDirectionInAnswer(cricket::MD_INACTIVE, cricket::MD_INACTIVE);
}

// Test that a data content with an unknown protocol is rejected in an answer.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateDataAnswerToOfferWithUnknownProtocol) {
  MediaSessionOptions opts;
  opts.data_channel_type = cricket::DCT_RTP;
  opts.has_audio = false;
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ContentInfo* dc_offer= offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer != NULL);
  DataContentDescription* dcd_offer =
      static_cast<DataContentDescription*>(dc_offer->description);
  ASSERT_TRUE(dcd_offer != NULL);
  std::string protocol = "a weird unknown protocol";
  dcd_offer->set_protocol(protocol);

  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const ContentInfo* dc_answer = answer->GetContentByName("data");
  ASSERT_TRUE(dc_answer != NULL);
  EXPECT_TRUE(dc_answer->rejected);
  const DataContentDescription* dcd_answer =
      static_cast<const DataContentDescription*>(dc_answer->description);
  ASSERT_TRUE(dcd_answer != NULL);
  EXPECT_EQ(protocol, dcd_answer->protocol());
}

// Test that the media protocol is RTP/AVPF if DTLS and SDES are disabled.
TEST_F(MediaSessionDescriptionFactoryTest, AudioOfferAnswerWithCryptoDisabled) {
  MediaSessionOptions opts;
  f1_.set_secure(SEC_DISABLED);
  f2_.set_secure(SEC_DISABLED);
  tdf1_.set_secure(SEC_DISABLED);
  tdf2_.set_secure(SEC_DISABLED);

  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  const AudioContentDescription* offer_acd =
      GetFirstAudioContentDescription(offer.get());
  ASSERT_TRUE(offer_acd != NULL);
  EXPECT_EQ(std::string(cricket::kMediaProtocolAvpf), offer_acd->protocol());

  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const ContentInfo* ac_answer = answer->GetContentByName("audio");
  ASSERT_TRUE(ac_answer != NULL);
  EXPECT_FALSE(ac_answer->rejected);

  const AudioContentDescription* answer_acd =
      GetFirstAudioContentDescription(answer.get());
  ASSERT_TRUE(answer_acd != NULL);
  EXPECT_EQ(std::string(cricket::kMediaProtocolAvpf), answer_acd->protocol());
}

// Create a video offer and answer and ensure the RTP header extensions
// matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestOfferAnswerWithRtpExtensions) {
  MediaSessionOptions opts;
  opts.has_video = true;

  f1_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension1));
  f1_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension1));
  f2_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension2));
  f2_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension2));

  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  EXPECT_EQ(MAKE_VECTOR(kAudioRtpExtension1),
            GetFirstAudioContentDescription(
                offer.get())->rtp_header_extensions());
  EXPECT_EQ(MAKE_VECTOR(kVideoRtpExtension1),
            GetFirstVideoContentDescription(
                offer.get())->rtp_header_extensions());
  EXPECT_EQ(MAKE_VECTOR(kAudioRtpExtensionAnswer),
            GetFirstAudioContentDescription(
                answer.get())->rtp_header_extensions());
  EXPECT_EQ(MAKE_VECTOR(kVideoRtpExtensionAnswer),
            GetFirstVideoContentDescription(
                answer.get())->rtp_header_extensions());
}

// Create an audio, video, data answer without legacy StreamParams.
TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateAnswerWithoutLegacyStreams) {
  MediaSessionOptions opts;
  opts.has_video = true;
  opts.data_channel_type = cricket::DCT_RTP;
  f1_.set_add_legacy_streams(false);
  f2_.set_add_legacy_streams(false);
  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  const ContentInfo* dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const VideoContentDescription* vcd =
      static_cast<const VideoContentDescription*>(vc->description);
  const DataContentDescription* dcd =
      static_cast<const DataContentDescription*>(dc->description);

  EXPECT_FALSE(acd->has_ssrcs());  // No StreamParams.
  EXPECT_FALSE(vcd->has_ssrcs());  // No StreamParams.
  EXPECT_FALSE(dcd->has_ssrcs());  // No StreamParams.
}

TEST_F(MediaSessionDescriptionFactoryTest, TestPartial) {
  MediaSessionOptions opts;
  opts.has_video = true;
  opts.data_channel_type = cricket::DCT_RTP;
  f1_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription>
      offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  const ContentInfo* dc = offer->GetContentByName("data");
  AudioContentDescription* acd = const_cast<AudioContentDescription*>(
      static_cast<const AudioContentDescription*>(ac->description));
  VideoContentDescription* vcd = const_cast<VideoContentDescription*>(
      static_cast<const VideoContentDescription*>(vc->description));
  DataContentDescription* dcd = const_cast<DataContentDescription*>(
      static_cast<const DataContentDescription*>(dc->description));

  EXPECT_FALSE(acd->partial());  // default is false.
  acd->set_partial(true);
  EXPECT_TRUE(acd->partial());
  acd->set_partial(false);
  EXPECT_FALSE(acd->partial());

  EXPECT_FALSE(vcd->partial());  // default is false.
  vcd->set_partial(true);
  EXPECT_TRUE(vcd->partial());
  vcd->set_partial(false);
  EXPECT_FALSE(vcd->partial());

  EXPECT_FALSE(dcd->partial());  // default is false.
  dcd->set_partial(true);
  EXPECT_TRUE(dcd->partial());
  dcd->set_partial(false);
  EXPECT_FALSE(dcd->partial());
}

// Create a typical video answer, and ensure it matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoAnswerRtcpMux) {
  MediaSessionOptions offer_opts;
  MediaSessionOptions answer_opts;
  answer_opts.has_video = true;
  offer_opts.has_video = true;
  answer_opts.data_channel_type = cricket::DCT_RTP;
  offer_opts.data_channel_type = cricket::DCT_RTP;

  talk_base::scoped_ptr<SessionDescription> offer;
  talk_base::scoped_ptr<SessionDescription> answer;

  offer_opts.rtcp_mux_enabled = true;
  answer_opts.rtcp_mux_enabled = true;

  offer.reset(f1_.CreateOffer(offer_opts, NULL));
  answer.reset(f2_.CreateAnswer(offer.get(), answer_opts, NULL));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(answer.get()));
  EXPECT_TRUE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstDataContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstDataContentDescription(answer.get())->rtcp_mux());

  offer_opts.rtcp_mux_enabled = true;
  answer_opts.rtcp_mux_enabled = false;

  offer.reset(f1_.CreateOffer(offer_opts, NULL));
  answer.reset(f2_.CreateAnswer(offer.get(), answer_opts, NULL));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(answer.get()));
  EXPECT_TRUE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstDataContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(answer.get())->rtcp_mux());

  offer_opts.rtcp_mux_enabled = false;
  answer_opts.rtcp_mux_enabled = true;

  offer.reset(f1_.CreateOffer(offer_opts, NULL));
  answer.reset(f2_.CreateAnswer(offer.get(), answer_opts, NULL));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(answer.get()));
  EXPECT_FALSE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(answer.get())->rtcp_mux());

  offer_opts.rtcp_mux_enabled = false;
  answer_opts.rtcp_mux_enabled = false;

  offer.reset(f1_.CreateOffer(offer_opts, NULL));
  answer.reset(f2_.CreateAnswer(offer.get(), answer_opts, NULL));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(offer.get()));
  ASSERT_TRUE(NULL != GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstVideoContentDescription(answer.get()));
  ASSERT_TRUE(NULL != GetFirstDataContentDescription(answer.get()));
  EXPECT_FALSE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstDataContentDescription(answer.get())->rtcp_mux());
}

// Create an audio-only answer to a video offer.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioAnswerToVideo) {
  MediaSessionOptions opts;
  opts.has_video = true;
  talk_base::scoped_ptr<SessionDescription>
      offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), MediaSessionOptions(), NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(vc->description != NULL);
  EXPECT_TRUE(vc->rejected);
}

// Create an audio-only answer to an offer with data.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateNoDataAnswerToDataOffer) {
  MediaSessionOptions opts;
  opts.data_channel_type = cricket::DCT_RTP;
  talk_base::scoped_ptr<SessionDescription>
      offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), MediaSessionOptions(), NULL));
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(dc != NULL);
  ASSERT_TRUE(dc->description != NULL);
  EXPECT_TRUE(dc->rejected);
}

// Create an answer that rejects the contents which are rejected in the offer.
TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerToOfferWithRejectedMedia) {
  MediaSessionOptions opts;
  opts.has_video = true;
  opts.data_channel_type = cricket::DCT_RTP;
  talk_base::scoped_ptr<SessionDescription>
      offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  ContentInfo* ac = offer->GetContentByName("audio");
  ContentInfo* vc = offer->GetContentByName("video");
  ContentInfo* dc = offer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  ac->rejected = true;
  vc->rejected = true;
  dc->rejected = true;
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));
  ac = answer->GetContentByName("audio");
  vc = answer->GetContentByName("video");
  dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  EXPECT_TRUE(ac->rejected);
  EXPECT_TRUE(vc->rejected);
  EXPECT_TRUE(dc->rejected);
}

// Create an audio and video offer with:
// - one video track
// - two audio tracks
// - two data tracks
// and ensure it matches what we expect. Also updates the initial offer by
// adding a new video track and replaces one of the audio tracks.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateMultiStreamVideoOffer) {
  MediaSessionOptions opts;
  opts.AddStream(MEDIA_TYPE_VIDEO, kVideoTrack1, kMediaStream1);
  opts.AddStream(MEDIA_TYPE_AUDIO, kAudioTrack1, kMediaStream1);
  opts.AddStream(MEDIA_TYPE_AUDIO, kAudioTrack2, kMediaStream1);
  opts.data_channel_type = cricket::DCT_RTP;
  opts.AddStream(MEDIA_TYPE_DATA, kDataTrack1, kMediaStream1);
  opts.AddStream(MEDIA_TYPE_DATA, kDataTrack2, kMediaStream1);

  f1_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));

  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  const ContentInfo* dc = offer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const VideoContentDescription* vcd =
      static_cast<const VideoContentDescription*>(vc->description);
  const DataContentDescription* dcd =
      static_cast<const DataContentDescription*>(dc->description);
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_codecs(), acd->codecs());

  const StreamParamsVec& audio_streams = acd->streams();
  ASSERT_EQ(2U, audio_streams.size());
  EXPECT_EQ(audio_streams[0].cname , audio_streams[1].cname);
  EXPECT_EQ(kAudioTrack1, audio_streams[0].id);
  ASSERT_EQ(1U, audio_streams[0].ssrcs.size());
  EXPECT_NE(0U, audio_streams[0].ssrcs[0]);
  EXPECT_EQ(kAudioTrack2, audio_streams[1].id);
  ASSERT_EQ(1U, audio_streams[1].ssrcs.size());
  EXPECT_NE(0U, audio_streams[1].ssrcs[0]);

  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(acd, 2U, CS_AES_CM_128_HMAC_SHA1_32);

  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_EQ(f1_.video_codecs(), vcd->codecs());
  ASSERT_CRYPTO(vcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);

  const StreamParamsVec& video_streams = vcd->streams();
  ASSERT_EQ(1U, video_streams.size());
  EXPECT_EQ(video_streams[0].cname, audio_streams[0].cname);
  EXPECT_EQ(kVideoTrack1, video_streams[0].id);
  EXPECT_EQ(kAutoBandwidth, vcd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(vcd->rtcp_mux());                 // rtcp-mux defaults on

  EXPECT_EQ(MEDIA_TYPE_DATA, dcd->type());
  EXPECT_EQ(f1_.data_codecs(), dcd->codecs());
  ASSERT_CRYPTO(dcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);

  const StreamParamsVec& data_streams = dcd->streams();
  ASSERT_EQ(2U, data_streams.size());
  EXPECT_EQ(data_streams[0].cname , data_streams[1].cname);
  EXPECT_EQ(kDataTrack1, data_streams[0].id);
  ASSERT_EQ(1U, data_streams[0].ssrcs.size());
  EXPECT_NE(0U, data_streams[0].ssrcs[0]);
  EXPECT_EQ(kDataTrack2, data_streams[1].id);
  ASSERT_EQ(1U, data_streams[1].ssrcs.size());
  EXPECT_NE(0U, data_streams[1].ssrcs[0]);

  EXPECT_EQ(cricket::kDataMaxBandwidth,
            dcd->bandwidth());                  // default bandwidth (auto)
  EXPECT_TRUE(dcd->rtcp_mux());                 // rtcp-mux defaults on
  ASSERT_CRYPTO(dcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);


  // Update the offer. Add a new video track that is not synched to the
  // other tracks and replace audio track 2 with audio track 3.
  opts.AddStream(MEDIA_TYPE_VIDEO, kVideoTrack2, kMediaStream2);
  opts.RemoveStream(MEDIA_TYPE_AUDIO, kAudioTrack2);
  opts.AddStream(MEDIA_TYPE_AUDIO, kAudioTrack3, kMediaStream1);
  opts.RemoveStream(MEDIA_TYPE_DATA, kDataTrack2);
  opts.AddStream(MEDIA_TYPE_DATA, kDataTrack3, kMediaStream1);
  talk_base::scoped_ptr<SessionDescription>
      updated_offer(f1_.CreateOffer(opts, offer.get()));

  ASSERT_TRUE(updated_offer.get() != NULL);
  ac = updated_offer->GetContentByName("audio");
  vc = updated_offer->GetContentByName("video");
  dc = updated_offer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  const AudioContentDescription* updated_acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const VideoContentDescription* updated_vcd =
      static_cast<const VideoContentDescription*>(vc->description);
  const DataContentDescription* updated_dcd =
      static_cast<const DataContentDescription*>(dc->description);

  EXPECT_EQ(acd->type(), updated_acd->type());
  EXPECT_EQ(acd->codecs(), updated_acd->codecs());
  EXPECT_EQ(vcd->type(), updated_vcd->type());
  EXPECT_EQ(vcd->codecs(), updated_vcd->codecs());
  EXPECT_EQ(dcd->type(), updated_dcd->type());
  EXPECT_EQ(dcd->codecs(), updated_dcd->codecs());
  ASSERT_CRYPTO(updated_acd, 2U, CS_AES_CM_128_HMAC_SHA1_32);
  EXPECT_TRUE(CompareCryptoParams(acd->cryptos(), updated_acd->cryptos()));
  ASSERT_CRYPTO(updated_vcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_TRUE(CompareCryptoParams(vcd->cryptos(), updated_vcd->cryptos()));
  ASSERT_CRYPTO(updated_dcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_TRUE(CompareCryptoParams(dcd->cryptos(), updated_dcd->cryptos()));

  const StreamParamsVec& updated_audio_streams = updated_acd->streams();
  ASSERT_EQ(2U, updated_audio_streams.size());
  EXPECT_EQ(audio_streams[0], updated_audio_streams[0]);
  EXPECT_EQ(kAudioTrack3, updated_audio_streams[1].id);  // New audio track.
  ASSERT_EQ(1U, updated_audio_streams[1].ssrcs.size());
  EXPECT_NE(0U, updated_audio_streams[1].ssrcs[0]);
  EXPECT_EQ(updated_audio_streams[0].cname, updated_audio_streams[1].cname);

  const StreamParamsVec& updated_video_streams = updated_vcd->streams();
  ASSERT_EQ(2U, updated_video_streams.size());
  EXPECT_EQ(video_streams[0], updated_video_streams[0]);
  EXPECT_EQ(kVideoTrack2, updated_video_streams[1].id);
  EXPECT_NE(updated_video_streams[1].cname, updated_video_streams[0].cname);

  const StreamParamsVec& updated_data_streams = updated_dcd->streams();
  ASSERT_EQ(2U, updated_data_streams.size());
  EXPECT_EQ(data_streams[0], updated_data_streams[0]);
  EXPECT_EQ(kDataTrack3, updated_data_streams[1].id);  // New data track.
  ASSERT_EQ(1U, updated_data_streams[1].ssrcs.size());
  EXPECT_NE(0U, updated_data_streams[1].ssrcs[0]);
  EXPECT_EQ(updated_data_streams[0].cname, updated_data_streams[1].cname);
}

// Create an offer with simulcast video stream.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateSimulcastVideoOffer) {
  MediaSessionOptions opts;
  const int num_sim_layers = 3;
  opts.AddVideoStream(kVideoTrack1, kMediaStream1, num_sim_layers);
  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));

  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(vc != NULL);
  const VideoContentDescription* vcd =
      static_cast<const VideoContentDescription*>(vc->description);

  const StreamParamsVec& video_streams = vcd->streams();
  ASSERT_EQ(1U, video_streams.size());
  EXPECT_EQ(kVideoTrack1, video_streams[0].id);
  const SsrcGroup* sim_ssrc_group =
      video_streams[0].get_ssrc_group(cricket::kSimSsrcGroupSemantics);
  ASSERT_TRUE(sim_ssrc_group != NULL);
  EXPECT_EQ(static_cast<size_t>(num_sim_layers), sim_ssrc_group->ssrcs.size());
}

// Create an audio and video answer to a standard video offer with:
// - one video track
// - two audio tracks
// - two data tracks
// and ensure it matches what we expect. Also updates the initial answer by
// adding a new video track and removes one of the audio tracks.
TEST_F(MediaSessionDescriptionFactoryTest, TestCreateMultiStreamVideoAnswer) {
  MediaSessionOptions offer_opts;
  offer_opts.has_video = true;
  offer_opts.data_channel_type = cricket::DCT_RTP;
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(offer_opts,
                                                                  NULL));

  MediaSessionOptions opts;
  opts.AddStream(MEDIA_TYPE_VIDEO, kVideoTrack1, kMediaStream1);
  opts.AddStream(MEDIA_TYPE_AUDIO, kAudioTrack1, kMediaStream1);
  opts.AddStream(MEDIA_TYPE_AUDIO, kAudioTrack2, kMediaStream1);
  opts.data_channel_type = cricket::DCT_RTP;
  opts.AddStream(MEDIA_TYPE_DATA, kDataTrack1, kMediaStream1);
  opts.AddStream(MEDIA_TYPE_DATA, kDataTrack2, kMediaStream1);

  talk_base::scoped_ptr<SessionDescription>
      answer(f2_.CreateAnswer(offer.get(), opts, NULL));

  ASSERT_TRUE(answer.get() != NULL);
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  const ContentInfo* dc = answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  const AudioContentDescription* acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const VideoContentDescription* vcd =
      static_cast<const VideoContentDescription*>(vc->description);
  const DataContentDescription* dcd =
      static_cast<const DataContentDescription*>(dc->description);
  ASSERT_CRYPTO(acd, 1U, CS_AES_CM_128_HMAC_SHA1_32);
  ASSERT_CRYPTO(vcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  ASSERT_CRYPTO(dcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);

  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(MAKE_VECTOR(kAudioCodecsAnswer), acd->codecs());

  const StreamParamsVec& audio_streams = acd->streams();
  ASSERT_EQ(2U, audio_streams.size());
  EXPECT_TRUE(audio_streams[0].cname ==  audio_streams[1].cname);
  EXPECT_EQ(kAudioTrack1, audio_streams[0].id);
  ASSERT_EQ(1U, audio_streams[0].ssrcs.size());
  EXPECT_NE(0U, audio_streams[0].ssrcs[0]);
  EXPECT_EQ(kAudioTrack2, audio_streams[1].id);
  ASSERT_EQ(1U, audio_streams[1].ssrcs.size());
  EXPECT_NE(0U, audio_streams[1].ssrcs[0]);

  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(acd->rtcp_mux());                 // rtcp-mux defaults on

  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_EQ(MAKE_VECTOR(kVideoCodecsAnswer), vcd->codecs());

  const StreamParamsVec& video_streams = vcd->streams();
  ASSERT_EQ(1U, video_streams.size());
  EXPECT_EQ(video_streams[0].cname, audio_streams[0].cname);
  EXPECT_EQ(kVideoTrack1, video_streams[0].id);
  EXPECT_EQ(kAutoBandwidth, vcd->bandwidth());  // default bandwidth (auto)
  EXPECT_TRUE(vcd->rtcp_mux());                 // rtcp-mux defaults on

  EXPECT_EQ(MEDIA_TYPE_DATA, dcd->type());
  EXPECT_EQ(MAKE_VECTOR(kDataCodecsAnswer), dcd->codecs());

  const StreamParamsVec& data_streams = dcd->streams();
  ASSERT_EQ(2U, data_streams.size());
  EXPECT_TRUE(data_streams[0].cname ==  data_streams[1].cname);
  EXPECT_EQ(kDataTrack1, data_streams[0].id);
  ASSERT_EQ(1U, data_streams[0].ssrcs.size());
  EXPECT_NE(0U, data_streams[0].ssrcs[0]);
  EXPECT_EQ(kDataTrack2, data_streams[1].id);
  ASSERT_EQ(1U, data_streams[1].ssrcs.size());
  EXPECT_NE(0U, data_streams[1].ssrcs[0]);

  EXPECT_EQ(cricket::kDataMaxBandwidth,
            dcd->bandwidth());                  // default bandwidth (auto)
  EXPECT_TRUE(dcd->rtcp_mux());                 // rtcp-mux defaults on

  // Update the answer. Add a new video track that is not synched to the
  // other traacks and remove 1 audio track.
  opts.AddStream(MEDIA_TYPE_VIDEO, kVideoTrack2, kMediaStream2);
  opts.RemoveStream(MEDIA_TYPE_AUDIO, kAudioTrack2);
  opts.RemoveStream(MEDIA_TYPE_DATA, kDataTrack2);
  talk_base::scoped_ptr<SessionDescription>
      updated_answer(f2_.CreateAnswer(offer.get(), opts, answer.get()));

  ASSERT_TRUE(updated_answer.get() != NULL);
  ac = updated_answer->GetContentByName("audio");
  vc = updated_answer->GetContentByName("video");
  dc = updated_answer->GetContentByName("data");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  ASSERT_TRUE(dc != NULL);
  const AudioContentDescription* updated_acd =
      static_cast<const AudioContentDescription*>(ac->description);
  const VideoContentDescription* updated_vcd =
      static_cast<const VideoContentDescription*>(vc->description);
  const DataContentDescription* updated_dcd =
      static_cast<const DataContentDescription*>(dc->description);

  ASSERT_CRYPTO(updated_acd, 1U, CS_AES_CM_128_HMAC_SHA1_32);
  EXPECT_TRUE(CompareCryptoParams(acd->cryptos(), updated_acd->cryptos()));
  ASSERT_CRYPTO(updated_vcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_TRUE(CompareCryptoParams(vcd->cryptos(), updated_vcd->cryptos()));
  ASSERT_CRYPTO(updated_dcd, 1U, CS_AES_CM_128_HMAC_SHA1_80);
  EXPECT_TRUE(CompareCryptoParams(dcd->cryptos(), updated_dcd->cryptos()));

  EXPECT_EQ(acd->type(), updated_acd->type());
  EXPECT_EQ(acd->codecs(), updated_acd->codecs());
  EXPECT_EQ(vcd->type(), updated_vcd->type());
  EXPECT_EQ(vcd->codecs(), updated_vcd->codecs());
  EXPECT_EQ(dcd->type(), updated_dcd->type());
  EXPECT_EQ(dcd->codecs(), updated_dcd->codecs());

  const StreamParamsVec& updated_audio_streams = updated_acd->streams();
  ASSERT_EQ(1U, updated_audio_streams.size());
  EXPECT_TRUE(audio_streams[0] ==  updated_audio_streams[0]);

  const StreamParamsVec& updated_video_streams = updated_vcd->streams();
  ASSERT_EQ(2U, updated_video_streams.size());
  EXPECT_EQ(video_streams[0], updated_video_streams[0]);
  EXPECT_EQ(kVideoTrack2, updated_video_streams[1].id);
  EXPECT_NE(updated_video_streams[1].cname, updated_video_streams[0].cname);

  const StreamParamsVec& updated_data_streams = updated_dcd->streams();
  ASSERT_EQ(1U, updated_data_streams.size());
  EXPECT_TRUE(data_streams[0] == updated_data_streams[0]);
}


// Create an updated offer after creating an answer to the original offer and
// verify that the codecs that were part of the original answer are not changed
// in the updated offer.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswer) {
  MediaSessionOptions opts;
  opts.has_audio = true;
  opts.has_video = true;

  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(answer.get());
  EXPECT_EQ(MAKE_VECTOR(kAudioCodecsAnswer), acd->codecs());

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());
  EXPECT_EQ(MAKE_VECTOR(kVideoCodecsAnswer), vcd->codecs());

  talk_base::scoped_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));

  // The expected audio codecs are the common audio codecs from the first
  // offer/answer exchange plus the audio codecs only |f2_| offer, sorted in
  // preference order.
  const AudioCodec kUpdatedAudioCodecOffer[] = {
    kAudioCodecs2[0],
    kAudioCodecsAnswer[0],
    kAudioCodecsAnswer[1],
  };

  // The expected video codecs are the common video codecs from the first
  // offer/answer exchange plus the video codecs only |f2_| offer, sorted in
  // preference order.
  const VideoCodec kUpdatedVideoCodecOffer[] = {
    kVideoCodecsAnswer[0],
    kVideoCodecs2[1],
  };

  const AudioContentDescription* updated_acd =
      GetFirstAudioContentDescription(updated_offer.get());
  EXPECT_EQ(MAKE_VECTOR(kUpdatedAudioCodecOffer), updated_acd->codecs());

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  EXPECT_EQ(MAKE_VECTOR(kUpdatedVideoCodecOffer), updated_vcd->codecs());
}

// Create an updated offer after creating an answer to the original offer and
// verify that the codecs that were part of the original answer are not changed
// in the updated offer. In this test Rtx is enabled.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswerWithRtx) {
  MediaSessionOptions opts;
  opts.has_video = true;
  opts.has_audio = false;
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);
  VideoCodec rtx_f1;
  rtx_f1.id = 126;
  rtx_f1.name = cricket::kRtxCodecName;

  // This creates rtx for H264 with the payload type |f1_| uses.
  rtx_f1.params[cricket::kCodecParamAssociatedPayloadType] =
      talk_base::ToString<int>(kVideoCodecs1[1].id);
  f1_codecs.push_back(rtx_f1);
  f1_.set_video_codecs(f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  VideoCodec rtx_f2;
  rtx_f2.id = 127;
  rtx_f2.name = cricket::kRtxCodecName;

  // This creates rtx for H264 with the payload type |f2_| uses.
  rtx_f2.params[cricket::kCodecParamAssociatedPayloadType] =
      talk_base::ToString<int>(kVideoCodecs2[0].id);
  f2_codecs.push_back(rtx_f2);
  f2_.set_video_codecs(f2_codecs);

  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());

  std::vector<VideoCodec> expected_codecs = MAKE_VECTOR(kVideoCodecsAnswer);
  expected_codecs.push_back(rtx_f1);

  EXPECT_EQ(expected_codecs, vcd->codecs());

  // Now, make sure we get same result, except for the preference order,
  // if |f2_| creates an updated offer even though the default payload types
  // are different from |f1_|.
  expected_codecs[0].preference = f1_codecs[1].preference;

  talk_base::scoped_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));
  ASSERT_TRUE(updated_offer);
  talk_base::scoped_ptr<SessionDescription> updated_answer(
      f1_.CreateAnswer(updated_offer.get(), opts, answer.get()));

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_answer.get());

  EXPECT_EQ(expected_codecs, updated_vcd->codecs());
}

// Create an updated offer that adds video after creating an audio only answer
// to the original offer. This test verifies that if a video codec and the RTX
// codec have the same default payload type as an audio codec that is already in
// use, the added codecs payload types are changed.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferWithVideoAndRtxAfterCreatingAudioAnswer) {
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);
  VideoCodec rtx_f1;
  rtx_f1.id = 126;
  rtx_f1.name = cricket::kRtxCodecName;

  // This creates rtx for H264 with the payload type |f1_| uses.
  rtx_f1.params[cricket::kCodecParamAssociatedPayloadType] =
      talk_base::ToString<int>(kVideoCodecs1[1].id);
  f1_codecs.push_back(rtx_f1);
  f1_.set_video_codecs(f1_codecs);

  MediaSessionOptions opts;
  opts.has_audio = true;
  opts.has_video = false;

  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(answer.get());
  EXPECT_EQ(MAKE_VECTOR(kAudioCodecsAnswer), acd->codecs());

  // Now - let |f2_| add video with RTX and let the payload type the RTX codec
  // reference  be the same as an audio codec that was negotiated in the
  // first offer/answer exchange.
  opts.has_audio = true;
  opts.has_video = true;

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  int used_pl_type = acd->codecs()[0].id;
  f2_codecs[0].id = used_pl_type;  // Set the payload type for H264.
  VideoCodec rtx_f2;
  rtx_f2.id = 127;
  rtx_f2.name = cricket::kRtxCodecName;
  rtx_f2.params[cricket::kCodecParamAssociatedPayloadType] =
      talk_base::ToString<int>(used_pl_type);
  f2_codecs.push_back(rtx_f2);
  f2_.set_video_codecs(f2_codecs);

  talk_base::scoped_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));
  ASSERT_TRUE(updated_offer);
  talk_base::scoped_ptr<SessionDescription> updated_answer(
      f1_.CreateAnswer(updated_offer.get(), opts, answer.get()));

  const AudioContentDescription* updated_acd =
      GetFirstAudioContentDescription(answer.get());
  EXPECT_EQ(MAKE_VECTOR(kAudioCodecsAnswer), updated_acd->codecs());

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_answer.get());

  ASSERT_EQ("H264", updated_vcd->codecs()[0].name);
  ASSERT_EQ(std::string(cricket::kRtxCodecName), updated_vcd->codecs()[1].name);
  int new_h264_pl_type =  updated_vcd->codecs()[0].id;
  EXPECT_NE(used_pl_type, new_h264_pl_type);
  VideoCodec rtx = updated_vcd->codecs()[1];
  int pt_referenced_by_rtx = talk_base::FromString<int>(
      rtx.params[cricket::kCodecParamAssociatedPayloadType]);
  EXPECT_EQ(new_h264_pl_type, pt_referenced_by_rtx);
}

// Test that RTX is ignored when there is no associated payload type parameter.
TEST_F(MediaSessionDescriptionFactoryTest, RtxWithoutApt) {
  MediaSessionOptions opts;
  opts.has_video = true;
  opts.has_audio = false;
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);
  VideoCodec rtx_f1;
  rtx_f1.id = 126;
  rtx_f1.name = cricket::kRtxCodecName;

  f1_codecs.push_back(rtx_f1);
  f1_.set_video_codecs(f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  VideoCodec rtx_f2;
  rtx_f2.id = 127;
  rtx_f2.name = cricket::kRtxCodecName;

  // This creates rtx for H264 with the payload type |f2_| uses.
  rtx_f2.SetParam(cricket::kCodecParamAssociatedPayloadType,
                  talk_base::ToString<int>(kVideoCodecs2[0].id));
  f2_codecs.push_back(rtx_f2);
  f2_.set_video_codecs(f2_codecs);

  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  // kCodecParamAssociatedPayloadType will always be added to the offer when RTX
  // is selected. Manually remove kCodecParamAssociatedPayloadType so that it
  // is possible to test that that RTX is dropped when
  // kCodecParamAssociatedPayloadType is missing in the offer.
  VideoContentDescription* desc =
      static_cast<cricket::VideoContentDescription*>(
          offer->GetContentDescriptionByName(cricket::CN_VIDEO));
  ASSERT_TRUE(desc != NULL);
  std::vector<VideoCodec> codecs = desc->codecs();
  for (std::vector<VideoCodec>::iterator iter = codecs.begin();
       iter != codecs.end(); ++iter) {
    if (iter->name.find(cricket::kRtxCodecName) == 0) {
      iter->params.clear();
    }
  }
  desc->set_codecs(codecs);

  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());

  for (std::vector<VideoCodec>::const_iterator iter = vcd->codecs().begin();
       iter != vcd->codecs().end(); ++iter) {
    ASSERT_STRNE(iter->name.c_str(), cricket::kRtxCodecName);
  }
}

// Create an updated offer after creating an answer to the original offer and
// verify that the RTP header extensions that were part of the original answer
// are not changed in the updated offer.
TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswerWithRtpExtensions) {
  MediaSessionOptions opts;
  opts.has_audio = true;
  opts.has_video = true;

  f1_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension1));
  f1_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension1));
  f2_.set_audio_rtp_header_extensions(MAKE_VECTOR(kAudioRtpExtension2));
  f2_.set_video_rtp_header_extensions(MAKE_VECTOR(kVideoRtpExtension2));

  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(opts, NULL));
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), opts, NULL));

  EXPECT_EQ(MAKE_VECTOR(kAudioRtpExtensionAnswer),
            GetFirstAudioContentDescription(
                answer.get())->rtp_header_extensions());
  EXPECT_EQ(MAKE_VECTOR(kVideoRtpExtensionAnswer),
            GetFirstVideoContentDescription(
                answer.get())->rtp_header_extensions());

  talk_base::scoped_ptr<SessionDescription> updated_offer(
      f2_.CreateOffer(opts, answer.get()));

  // The expected RTP header extensions in the new offer are the resulting
  // extensions from the first offer/answer exchange plus the extensions only
  // |f2_| offer.
  // Since the default local extension id |f2_| uses has already been used by
  // |f1_| for another extensions, it is changed to 255.
  const RtpHeaderExtension kUpdatedAudioRtpExtensions[] = {
    kAudioRtpExtensionAnswer[0],
    RtpHeaderExtension(kAudioRtpExtension2[1].uri, 255),
  };

  // Since the default local extension id |f2_| uses has already been used by
  // |f1_| for another extensions, is is changed to 254.
  const RtpHeaderExtension kUpdatedVideoRtpExtensions[] = {
    kVideoRtpExtensionAnswer[0],
    RtpHeaderExtension(kVideoRtpExtension2[1].uri, 254),
  };

  const AudioContentDescription* updated_acd =
      GetFirstAudioContentDescription(updated_offer.get());
  EXPECT_EQ(MAKE_VECTOR(kUpdatedAudioRtpExtensions),
            updated_acd->rtp_header_extensions());

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  EXPECT_EQ(MAKE_VECTOR(kUpdatedVideoRtpExtensions),
            updated_vcd->rtp_header_extensions());
}

TEST(MediaSessionDescription, CopySessionDescription) {
  SessionDescription source;
  cricket::ContentGroup group(cricket::CN_AUDIO);
  source.AddGroup(group);
  AudioContentDescription* acd(new AudioContentDescription());
  acd->set_codecs(MAKE_VECTOR(kAudioCodecs1));
  acd->AddLegacyStream(1);
  source.AddContent(cricket::CN_AUDIO, cricket::NS_JINGLE_RTP, acd);
  VideoContentDescription* vcd(new VideoContentDescription());
  vcd->set_codecs(MAKE_VECTOR(kVideoCodecs1));
  vcd->AddLegacyStream(2);
  source.AddContent(cricket::CN_VIDEO, cricket::NS_JINGLE_RTP, vcd);

  talk_base::scoped_ptr<SessionDescription> copy(source.Copy());
  ASSERT_TRUE(copy.get() != NULL);
  EXPECT_TRUE(copy->HasGroup(cricket::CN_AUDIO));
  const ContentInfo* ac = copy->GetContentByName("audio");
  const ContentInfo* vc = copy->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc != NULL);
  EXPECT_EQ(std::string(NS_JINGLE_RTP), ac->type);
  const AudioContentDescription* acd_copy =
      static_cast<const AudioContentDescription*>(ac->description);
  EXPECT_EQ(acd->codecs(), acd_copy->codecs());
  EXPECT_EQ(1u, acd->first_ssrc());

  EXPECT_EQ(std::string(NS_JINGLE_RTP), vc->type);
  const VideoContentDescription* vcd_copy =
      static_cast<const VideoContentDescription*>(vc->description);
  EXPECT_EQ(vcd->codecs(), vcd_copy->codecs());
  EXPECT_EQ(2u, vcd->first_ssrc());
}

// The below TestTransportInfoXXX tests create different offers/answers, and
// ensure the TransportInfo in the SessionDescription matches what we expect.
TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferAudio) {
  MediaSessionOptions options;
  options.has_audio = true;
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferAudioCurrent) {
  MediaSessionOptions options;
  options.has_audio = true;
  TestTransportInfo(true, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferMultimedia) {
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_RTP;
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
    TestTransportInfoOfferMultimediaCurrent) {
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_RTP;
  TestTransportInfo(true, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferBundle) {
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_RTP;
  options.bundle_enabled = true;
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoOfferBundleCurrent) {
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_RTP;
  options.bundle_enabled = true;
  TestTransportInfo(true, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoAnswerAudio) {
  MediaSessionOptions options;
  options.has_audio = true;
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
    TestTransportInfoAnswerAudioCurrent) {
  MediaSessionOptions options;
  options.has_audio = true;
  TestTransportInfo(false, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoAnswerMultimedia) {
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_RTP;
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
    TestTransportInfoAnswerMultimediaCurrent) {
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_RTP;
  TestTransportInfo(false, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoAnswerBundle) {
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_RTP;
  options.bundle_enabled = true;
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
    TestTransportInfoAnswerBundleCurrent) {
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_RTP;
  options.bundle_enabled = true;
  TestTransportInfo(false, options, true);
}

// Create an offer with bundle enabled and verify the crypto parameters are
// the common set of the available cryptos.
TEST_F(MediaSessionDescriptionFactoryTest, TestCryptoWithOfferBundle) {
  TestCryptoWithBundle(true);
}

// Create an answer with bundle enabled and verify the crypto parameters are
// the common set of the available cryptos.
TEST_F(MediaSessionDescriptionFactoryTest, TestCryptoWithAnswerBundle) {
  TestCryptoWithBundle(false);
}

// Test that we include both SDES and DTLS in the offer, but only include SDES
// in the answer if DTLS isn't negotiated.
TEST_F(MediaSessionDescriptionFactoryTest, TestCryptoDtls) {
  f1_.set_secure(SEC_ENABLED);
  f2_.set_secure(SEC_ENABLED);
  tdf1_.set_secure(SEC_ENABLED);
  tdf2_.set_secure(SEC_DISABLED);
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  talk_base::scoped_ptr<SessionDescription> offer, answer;
  const cricket::MediaContentDescription* audio_media_desc;
  const cricket::MediaContentDescription* video_media_desc;
  const cricket::TransportDescription* audio_trans_desc;
  const cricket::TransportDescription* video_trans_desc;

  // Generate an offer with SDES and DTLS support.
  offer.reset(f1_.CreateOffer(options, NULL));
  ASSERT_TRUE(offer.get() != NULL);

  audio_media_desc = static_cast<const cricket::MediaContentDescription*>(
      offer->GetContentDescriptionByName("audio"));
  ASSERT_TRUE(audio_media_desc != NULL);
  video_media_desc = static_cast<const cricket::MediaContentDescription*>(
      offer->GetContentDescriptionByName("video"));
  ASSERT_TRUE(video_media_desc != NULL);
  EXPECT_EQ(2u, audio_media_desc->cryptos().size());
  EXPECT_EQ(1u, video_media_desc->cryptos().size());

  audio_trans_desc = offer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_trans_desc != NULL);
  video_trans_desc = offer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_trans_desc != NULL);
  ASSERT_TRUE(audio_trans_desc->identity_fingerprint.get() != NULL);
  ASSERT_TRUE(video_trans_desc->identity_fingerprint.get() != NULL);

  // Generate an answer with only SDES support, since tdf2 has crypto disabled.
  answer.reset(f2_.CreateAnswer(offer.get(), options, NULL));
  ASSERT_TRUE(answer.get() != NULL);

  audio_media_desc = static_cast<const cricket::MediaContentDescription*>(
      answer->GetContentDescriptionByName("audio"));
  ASSERT_TRUE(audio_media_desc != NULL);
  video_media_desc = static_cast<const cricket::MediaContentDescription*>(
      answer->GetContentDescriptionByName("video"));
  ASSERT_TRUE(video_media_desc != NULL);
  EXPECT_EQ(1u, audio_media_desc->cryptos().size());
  EXPECT_EQ(1u, video_media_desc->cryptos().size());

  audio_trans_desc = answer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_trans_desc != NULL);
  video_trans_desc = answer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_trans_desc != NULL);
  ASSERT_TRUE(audio_trans_desc->identity_fingerprint.get() == NULL);
  ASSERT_TRUE(video_trans_desc->identity_fingerprint.get() == NULL);

  // Enable DTLS; the answer should now only have DTLS support.
  tdf2_.set_secure(SEC_ENABLED);
  answer.reset(f2_.CreateAnswer(offer.get(), options, NULL));
  ASSERT_TRUE(answer.get() != NULL);

  audio_media_desc = static_cast<const cricket::MediaContentDescription*>(
      answer->GetContentDescriptionByName("audio"));
  ASSERT_TRUE(audio_media_desc != NULL);
  video_media_desc = static_cast<const cricket::MediaContentDescription*>(
      answer->GetContentDescriptionByName("video"));
  ASSERT_TRUE(video_media_desc != NULL);
  EXPECT_TRUE(audio_media_desc->cryptos().empty());
  EXPECT_TRUE(video_media_desc->cryptos().empty());
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf),
            audio_media_desc->protocol());
  EXPECT_EQ(std::string(cricket::kMediaProtocolSavpf),
            video_media_desc->protocol());

  audio_trans_desc = answer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_trans_desc != NULL);
  video_trans_desc = answer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_trans_desc != NULL);
  ASSERT_TRUE(audio_trans_desc->identity_fingerprint.get() != NULL);
  ASSERT_TRUE(video_trans_desc->identity_fingerprint.get() != NULL);

  // Try creating offer again. DTLS enabled now, crypto's should be empty
  // in new offer.
  offer.reset(f1_.CreateOffer(options, offer.get()));
  ASSERT_TRUE(offer.get() != NULL);
  audio_media_desc = static_cast<const cricket::MediaContentDescription*>(
      offer->GetContentDescriptionByName("audio"));
  ASSERT_TRUE(audio_media_desc != NULL);
  video_media_desc = static_cast<const cricket::MediaContentDescription*>(
      offer->GetContentDescriptionByName("video"));
  ASSERT_TRUE(video_media_desc != NULL);
  EXPECT_TRUE(audio_media_desc->cryptos().empty());
  EXPECT_TRUE(video_media_desc->cryptos().empty());

  audio_trans_desc = offer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_trans_desc != NULL);
  video_trans_desc = offer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_trans_desc != NULL);
  ASSERT_TRUE(audio_trans_desc->identity_fingerprint.get() != NULL);
  ASSERT_TRUE(video_trans_desc->identity_fingerprint.get() != NULL);
}

// Test that an answer can't be created if cryptos are required but the offer is
// unsecure.
TEST_F(MediaSessionDescriptionFactoryTest, TestSecureAnswerToUnsecureOffer) {
  MediaSessionOptions options;
  f1_.set_secure(SEC_DISABLED);
  tdf1_.set_secure(SEC_DISABLED);
  f2_.set_secure(SEC_REQUIRED);
  tdf1_.set_secure(SEC_ENABLED);

  talk_base::scoped_ptr<SessionDescription> offer(f1_.CreateOffer(options,
                                                                  NULL));
  ASSERT_TRUE(offer.get() != NULL);
  talk_base::scoped_ptr<SessionDescription> answer(
      f2_.CreateAnswer(offer.get(), options, NULL));
  EXPECT_TRUE(answer.get() == NULL);
}

// Test that we accept a DTLS offer without SDES and create an appropriate
// answer.
TEST_F(MediaSessionDescriptionFactoryTest, TestCryptoOfferDtlsButNotSdes) {
  f1_.set_secure(SEC_DISABLED);
  f2_.set_secure(SEC_ENABLED);
  tdf1_.set_secure(SEC_ENABLED);
  tdf2_.set_secure(SEC_ENABLED);
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  options.data_channel_type = cricket::DCT_RTP;

  talk_base::scoped_ptr<SessionDescription> offer, answer;

  // Generate an offer with DTLS but without SDES.
  offer.reset(f1_.CreateOffer(options, NULL));
  ASSERT_TRUE(offer.get() != NULL);

  const AudioContentDescription* audio_offer =
      GetFirstAudioContentDescription(offer.get());
  ASSERT_TRUE(audio_offer->cryptos().empty());
  const VideoContentDescription* video_offer =
      GetFirstVideoContentDescription(offer.get());
  ASSERT_TRUE(video_offer->cryptos().empty());
  const DataContentDescription* data_offer =
      GetFirstDataContentDescription(offer.get());
  ASSERT_TRUE(data_offer->cryptos().empty());

  const cricket::TransportDescription* audio_offer_trans_desc =
      offer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_offer_trans_desc->identity_fingerprint.get() != NULL);
  const cricket::TransportDescription* video_offer_trans_desc =
      offer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_offer_trans_desc->identity_fingerprint.get() != NULL);
  const cricket::TransportDescription* data_offer_trans_desc =
      offer->GetTransportDescriptionByName("data");
  ASSERT_TRUE(data_offer_trans_desc->identity_fingerprint.get() != NULL);

  // Generate an answer with DTLS.
  answer.reset(f2_.CreateAnswer(offer.get(), options, NULL));
  ASSERT_TRUE(answer.get() != NULL);

  const cricket::TransportDescription* audio_answer_trans_desc =
      answer->GetTransportDescriptionByName("audio");
  EXPECT_TRUE(audio_answer_trans_desc->identity_fingerprint.get() != NULL);
  const cricket::TransportDescription* video_answer_trans_desc =
      answer->GetTransportDescriptionByName("video");
  EXPECT_TRUE(video_answer_trans_desc->identity_fingerprint.get() != NULL);
  const cricket::TransportDescription* data_answer_trans_desc =
      answer->GetTransportDescriptionByName("data");
  EXPECT_TRUE(data_answer_trans_desc->identity_fingerprint.get() != NULL);
}

// Verifies if vad_enabled option is set to false, CN codecs are not present in
// offer or answer.
TEST_F(MediaSessionDescriptionFactoryTest, TestVADEnableOption) {
  MediaSessionOptions options;
  options.has_audio = true;
  options.has_video = true;
  talk_base::scoped_ptr<SessionDescription> offer(
      f1_.CreateOffer(options, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  const ContentInfo* audio_content = offer->GetContentByName("audio");
  EXPECT_FALSE(VerifyNoCNCodecs(audio_content));

  options.vad_enabled = false;
  offer.reset(f1_.CreateOffer(options, NULL));
  ASSERT_TRUE(offer.get() != NULL);
  audio_content = offer->GetContentByName("audio");
  EXPECT_TRUE(VerifyNoCNCodecs(audio_content));
  talk_base::scoped_ptr<SessionDescription> answer(
      f1_.CreateAnswer(offer.get(), options, NULL));
  ASSERT_TRUE(answer.get() != NULL);
  audio_content = answer->GetContentByName("audio");
  EXPECT_TRUE(VerifyNoCNCodecs(audio_content));
}
