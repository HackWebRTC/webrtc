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

#include "talk/app/webrtc/webrtc_json.h"

#include <stdio.h>

#include <locale>
#include <string>

#include "talk/base/json.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/session/phone/mediasessionclient.h"
#include "talk/session/phone/codec.h"

namespace {

// Before [de]serializing, we need to work around a bug in jsoncpp where
// locale sensitive string conversion functions are used (e.g. sprintf,
// sscanf and stl).  The problem is that depending on what the current
// locale is, numbers might be formatted differently than the jsoncpp code
// otherwise expects.  E.g. sprintf might format a number as "1,234" and
// the parser assumes that it would be "1.234".
class AutoSwitchToClassicLocale {
 public:
  AutoSwitchToClassicLocale() {
    const char* locale_name = setlocale(LC_NUMERIC, NULL);
    if (locale_name)
      saved_locale_ = locale_name;

    // Switch the CRT to "C".
    setlocale(LC_NUMERIC, "C");

    // Switch STL to classic.
    cxx_locale_ = std::locale::global(std::locale::classic());
  }

  ~AutoSwitchToClassicLocale() {
    // Switch the locale back to what it was before.
    std::locale::global(cxx_locale_);
    setlocale(LC_NUMERIC, saved_locale_.c_str());
  }

 private:
  std::string saved_locale_;
  std::locale cxx_locale_;
};

}

namespace webrtc {
static const int kIceComponent = 1;
static const int kIceFoundation = 1;

bool GetConnectionMediator(const Json::Value& value,
                           std::string* connection_mediator) {
  if (value.type() != Json::objectValue && value.type() != Json::nullValue) {
    LOG(LS_WARNING) << "Failed to parse stun values";
    return false;
  }

  if (!GetStringFromJsonObject(value,
                               "connectionmediator",
                               connection_mediator)) {
    LOG(LS_WARNING) << "Failed to parse JSON for value: "
                    << value.toStyledString();
    return false;
  }
  return true;
}

bool GetStunServer(const Json::Value& value, StunServiceDetails* stunServer) {
  if (value.type() != Json::objectValue && value.type() != Json::nullValue) {
    LOG(LS_WARNING) << "Failed to parse stun values";
    return false;
  }

  Json::Value stun;
  if (GetValueFromJsonObject(value, "stun_service", &stun)) {
    if (stun.type() == Json::objectValue) {
      if (!GetStringFromJsonObject(stun, "host", &stunServer->host) ||
          !GetStringFromJsonObject(stun, "service", &stunServer->service) ||
          !GetStringFromJsonObject(stun, "protocol", &stunServer->protocol)) {
        LOG(LS_WARNING) << "Failed to parse JSON value: "
                        << value.toStyledString();
        return false;
      }
    } else {
      LOG(LS_WARNING) << "Failed to find the stun_service member.";
      return false;
    }
  } else {
    LOG(LS_WARNING) << "Wrong ValueType. Expect Json::objectValue).";
    return false;
  }
  return true;
}

bool GetTurnServer(const Json::Value& value, std::string* turn_server) {
  if (value.type() != Json::objectValue && value.type() != Json::nullValue) {
    LOG(LS_WARNING) << "Failed to parse stun values";
    return false;
  }

  Json::Value turn;
  if (GetValueFromJsonObject(value, "turn_service", &turn)) {
    if (turn.type() == Json::objectValue) {
      if (!GetStringFromJsonObject(turn, "host", turn_server)) {
        LOG(LS_WARNING) << "Failed to parse JSON value: "
                        << value.toStyledString();
        return false;
      }
    } else {
      LOG(LS_WARNING) << "Wrong ValueType. Expect Json::objectValue).";
      return false;
    }
  }
  return true;
}

bool GetJSONSignalingMessage(
    const cricket::SessionDescription* sdp,
    const std::vector<cricket::Candidate>& candidates,
    std::string* signaling_message) {
  // See documentation for AutoSwitchToClassicLocale.
  AutoSwitchToClassicLocale auto_switch;

  const cricket::ContentInfo* audio_content = GetFirstAudioContent(sdp);
  const cricket::ContentInfo* video_content = GetFirstVideoContent(sdp);

  std::vector<Json::Value> media;
  if (audio_content) {
    Json::Value value;
    BuildMediaMessage(*audio_content, candidates, false, &value);
    media.push_back(value);
  }

  if (video_content) {
    Json::Value value;
    BuildMediaMessage(*video_content, candidates, true, &value);
    media.push_back(value);
  }

  Json::Value signal;
  Append(&signal, "media", media);

  // Now serialize.
  *signaling_message = Serialize(signal);

  return true;
}

bool BuildMediaMessage(
    const cricket::ContentInfo& content_info,
    const std::vector<cricket::Candidate>& candidates,
    bool video,
    Json::Value* params) {
  if (video) {
    Append(params, "label", 2);  // always video 2
  } else {
    Append(params, "label", 1);  // always audio 1
  }

  const cricket::MediaContentDescription* media_info =
      static_cast<const cricket::MediaContentDescription*> (
          content_info.description);
  if (media_info->rtcp_mux()) {
    Append(params, "rtcp_mux", std::string("supported"));
  }

  std::vector<Json::Value> rtpmap;
  if (!BuildRtpMapParams(content_info, video, &rtpmap)) {
    return false;
  }

  Append(params, "rtpmap", rtpmap);

  Json::Value attributes;
  std::vector<Json::Value> jcandidates;

  if (!BuildAttributes(candidates, video, &jcandidates)) {
    return false;
  }
  Append(&attributes, "candidate", jcandidates);
  Append(params, "attributes", attributes);
  return true;
}

bool BuildRtpMapParams(const cricket::ContentInfo& content_info,
                       bool video,
                       std::vector<Json::Value>* rtpmap) {
  if (!video) {
    const cricket::AudioContentDescription* audio_offer =
        static_cast<const cricket::AudioContentDescription*>(
            content_info.description);

    std::vector<cricket::AudioCodec>::const_iterator iter =
        audio_offer->codecs().begin();
    std::vector<cricket::AudioCodec>::const_iterator iter_end =
        audio_offer->codecs().end();
    for (; iter != iter_end; ++iter) {
      Json::Value codec;
      std::string codec_str(std::string("audio/").append(iter->name));
      // adding clockrate
      Append(&codec, "clockrate", iter->clockrate);
      Append(&codec, "codec", codec_str);
      Json::Value codec_id;
      Append(&codec_id, talk_base::ToString(iter->id), codec);
      rtpmap->push_back(codec_id);
    }
  } else {
    const cricket::VideoContentDescription* video_offer =
        static_cast<const cricket::VideoContentDescription*>(
            content_info.description);

    std::vector<cricket::VideoCodec>::const_iterator iter =
        video_offer->codecs().begin();
    std::vector<cricket::VideoCodec>::const_iterator iter_end =
        video_offer->codecs().end();
    for (; iter != iter_end; ++iter) {
      Json::Value codec;
      std::string codec_str(std::string("video/").append(iter->name));
      Append(&codec, "codec", codec_str);
      Json::Value codec_id;
      Append(&codec_id, talk_base::ToString(iter->id), codec);
      rtpmap->push_back(codec_id);
    }
  }
  return true;
}

bool BuildAttributes(const std::vector<cricket::Candidate>& candidates,
                     bool video,
                     std::vector<Json::Value>* jcandidates) {
  std::vector<cricket::Candidate>::const_iterator iter =
      candidates.begin();
  std::vector<cricket::Candidate>::const_iterator iter_end =
      candidates.end();
  for (; iter != iter_end; ++iter) {
    if ((video && (!iter->name().compare("video_rtcp") ||
                  (!iter->name().compare("video_rtp")))) ||
        (!video && (!iter->name().compare("rtp") ||
                   (!iter->name().compare("rtcp"))))) {
      Json::Value candidate;
      Append(&candidate, "component", kIceComponent);
      Append(&candidate, "foundation", kIceFoundation);
      Append(&candidate, "generation", iter->generation());
      Append(&candidate, "proto", iter->protocol());
      Append(&candidate, "priority", iter->preference());
      Append(&candidate, "ip", iter->address().IPAsString());
      Append(&candidate, "port", iter->address().PortAsString());
      Append(&candidate, "type", iter->type());
      Append(&candidate, "name", iter->name());
      Append(&candidate, "network_name", iter->network_name());
      Append(&candidate, "username", iter->username());
      Append(&candidate, "password", iter->password());
      jcandidates->push_back(candidate);
    }
  }
  return true;
}

std::string Serialize(const Json::Value& value) {
  Json::StyledWriter writer;
  return writer.write(value);
}

bool Deserialize(const std::string& message, Json::Value* value) {
  Json::Reader reader;
  return reader.parse(message, *value);
}

bool ParseJSONSignalingMessage(const std::string& signaling_message,
                               cricket::SessionDescription*& sdp,
                               std::vector<cricket::Candidate>* candidates) {
  ASSERT(!sdp);  // expect this to be NULL

  // See documentation for AutoSwitchToClassicLocale.
  AutoSwitchToClassicLocale auto_switch;

  // first deserialize message
  Json::Value value;
  if (!Deserialize(signaling_message, &value)) {
    return false;
  }

  // get media objects
  std::vector<Json::Value> mlines = ReadValues(value, "media");
  if (mlines.empty()) {
    // no m-lines found
    return false;
  }

  sdp = new cricket::SessionDescription();

  // get codec information
  for (size_t i = 0; i < mlines.size(); ++i) {
    if (mlines[i]["label"].asInt() == 1) {
      cricket::AudioContentDescription* audio_content =
          new cricket::AudioContentDescription();
      ParseAudioCodec(mlines[i], audio_content);

      audio_content->set_rtcp_mux(ParseRTCPMux(mlines[i]));
      audio_content->SortCodecs();
      sdp->AddContent(cricket::CN_AUDIO, cricket::NS_JINGLE_RTP, audio_content);
      ParseICECandidates(mlines[i], candidates);
    } else {
      cricket::VideoContentDescription* video_content =
          new cricket::VideoContentDescription();
      ParseVideoCodec(mlines[i], video_content);

      video_content->set_rtcp_mux(ParseRTCPMux(mlines[i]));
      video_content->SortCodecs();
      sdp->AddContent(cricket::CN_VIDEO, cricket::NS_JINGLE_RTP, video_content);
      ParseICECandidates(mlines[i], candidates);
    }
  }
  return true;
}

bool ParseRTCPMux(const Json::Value& value) {
  Json::Value rtcp_mux(ReadValue(value, "rtcp_mux"));
  if (!rtcp_mux.empty()) {
    if (rtcp_mux.asString().compare("supported") == 0) {
      return true;
    }
  }
  return false;
}

bool ParseAudioCodec(const Json::Value& value,
                     cricket::AudioContentDescription* content) {
  std::vector<Json::Value> rtpmap(ReadValues(value, "rtpmap"));
  if (rtpmap.empty())
    return false;

  std::vector<Json::Value>::const_iterator iter =
      rtpmap.begin();
  std::vector<Json::Value>::const_iterator iter_end =
      rtpmap.end();
  for (; iter != iter_end; ++iter) {
    cricket::AudioCodec codec;
    std::string pltype(iter->begin().memberName());
    talk_base::FromString(pltype, &codec.id);
    Json::Value codec_info((*iter)[pltype]);
    std::string codec_name(ReadString(codec_info, "codec"));
    std::vector<std::string> tokens;
    talk_base::split(codec_name, '/', &tokens);
    codec.name = tokens[1];
    codec.clockrate = ReadUInt(codec_info, "clockrate");
    content->AddCodec(codec);
  }

  return true;
}

bool ParseVideoCodec(const Json::Value& value,
                     cricket::VideoContentDescription* content) {
  std::vector<Json::Value> rtpmap(ReadValues(value, "rtpmap"));
  if (rtpmap.empty())
    return false;

  std::vector<Json::Value>::const_iterator iter =
      rtpmap.begin();
  std::vector<Json::Value>::const_iterator iter_end =
      rtpmap.end();
  for (; iter != iter_end; ++iter) {
    cricket::VideoCodec codec;
    std::string pltype(iter->begin().memberName());
    talk_base::FromString(pltype, &codec.id);
    Json::Value codec_info((*iter)[pltype]);
    std::vector<std::string> tokens;
    talk_base::split(codec_info["codec"].asString(), '/', &tokens);
    codec.name = tokens[1];
    content->AddCodec(codec);
  }
  return true;
}

bool ParseICECandidates(const Json::Value& value,
                        std::vector<cricket::Candidate>* candidates) {
  Json::Value attributes(ReadValue(value, "attributes"));
  std::string ice_pwd(ReadString(attributes, "ice-pwd"));
  std::string ice_ufrag(ReadString(attributes, "ice-ufrag"));

  std::vector<Json::Value> jcandidates(ReadValues(attributes, "candidate"));

  std::vector<Json::Value>::const_iterator iter =
      jcandidates.begin();
  std::vector<Json::Value>::const_iterator iter_end =
      jcandidates.end();
  char buffer[16];
  for (; iter != iter_end; ++iter) {
    cricket::Candidate cand;
    std::string str;
    str = ReadUInt(*iter, "generation");
    cand.set_generation_str(str);
    str = ReadString(*iter, "proto");
    cand.set_protocol(str);
    double priority = ReadDouble(*iter, "priority");
    talk_base::sprintfn(buffer, ARRAY_SIZE(buffer), "%f", priority);
    cand.set_preference_str(buffer);
    talk_base::SocketAddress addr;
    str = ReadString(*iter, "ip");
    addr.SetIP(str);
    str = ReadString(*iter, "port");
    int port;
    talk_base::FromString(str, &port);
    addr.SetPort(port);
    cand.set_address(addr);
    str = ReadString(*iter, "type");
    cand.set_type(str);
    str = ReadString(*iter, "name");
    cand.set_name(str);
    str = ReadString(*iter, "network_name");
    cand.set_network_name(str);
    str = ReadString(*iter, "username");
    cand.set_username(str);
    str = ReadString(*iter, "password");
    cand.set_password(str);
    candidates->push_back(cand);
  }
  return true;
}

std::vector<Json::Value> ReadValues(
    const Json::Value& value, const std::string& key) {
  std::vector<Json::Value> objects;
  for (size_t i = 0; i < value[key].size(); ++i) {
    objects.push_back(value[key][i]);
  }
  return objects;
}

Json::Value ReadValue(const Json::Value& value, const std::string& key) {
  return value[key];
}

std::string ReadString(const Json::Value& value, const std::string& key) {
  return value[key].asString();
}

uint32 ReadUInt(const Json::Value& value, const std::string& key) {
  return value[key].asUInt();
}

double ReadDouble(const Json::Value& value, const std::string& key) {
  return value[key].asDouble();
}

// Add values
void Append(Json::Value* object, const std::string& key, bool value) {
  (*object)[key] = Json::Value(value);
}

void Append(Json::Value* object, const std::string& key, char * value) {
  (*object)[key] = Json::Value(value);
}

void Append(Json::Value* object, const std::string& key, double value) {
  (*object)[key] = Json::Value(value);
}

void Append(Json::Value* object, const std::string& key, int value) {
  (*object)[key] = Json::Value(value);
}

void Append(Json::Value* object, const std::string& key,
            const std::string& value) {
  (*object)[key] = Json::Value(value);
}

void Append(Json::Value* object, const std::string& key, uint32 value) {
  (*object)[key] = Json::Value(value);
}

void Append(Json::Value* object, const std::string& key,
            const Json::Value& value) {
  (*object)[key] = value;
}

void Append(Json::Value* object,
            const std::string & key,
            const std::vector<Json::Value>& values) {
  for (std::vector<Json::Value>::const_iterator iter = values.begin();
      iter != values.end(); ++iter) {
    (*object)[key].append(*iter);
  }
}

}  // namespace webrtc
