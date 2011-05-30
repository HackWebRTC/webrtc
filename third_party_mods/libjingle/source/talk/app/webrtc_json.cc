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

//this file contains all the json helper methods
#include "talk/app/webrtc_json.h"

#include <stdio.h>
#include <string>

#include "talk/base/json.h"
#include "talk/base/logging.h"
#include "talk/session/phone/mediasessionclient.h"
#include "talk/session/phone/codec.h"
#include "json/json.h"

namespace webrtc {

static const int kIceComponent = 1;
static const int kIceFoundation = 1;

bool GetConnectionMediator(const Json::Value& value, std::string& connectionMediator) {
  if (value.type() != Json::objectValue && value.type() != Json::nullValue) {
    LOG(LS_WARNING) << "Failed to parse stun values" ;
    return false;
  }

  if (!GetStringFromJsonObject(value, "connectionmediator", &connectionMediator)) {
    LOG(LS_WARNING) << "Failed to parse JSON for value: "
                    << value.toStyledString();
    return false;
  }
  return true;
}

bool GetStunServer(const Json::Value& value, StunServiceDetails& stunServer) {
  if (value.type() != Json::objectValue && value.type() != Json::nullValue) {
    LOG(LS_WARNING) << "Failed to parse stun values" ;
    return false;
  }

  Json::Value stun;
  if (GetValueFromJsonObject(value, "stun_service", &stun)) {
    if (stun.type() == Json::objectValue) {
      if (!GetStringFromJsonObject(stun, "host", &stunServer.host) ||
          !GetStringFromJsonObject(stun, "service", &stunServer.service) ||
          !GetStringFromJsonObject(stun, "protocol", &stunServer.protocol)) {
        LOG(LS_WARNING) << "Failed to parse JSON value: "
                        << value.toStyledString();
        return false;
      }
    } else {
      return false;
    }
  }
  return true;

}
bool GetTurnServer(const Json::Value& value, std::string& turnServer) {
  if (value.type() != Json::objectValue && value.type() != Json::nullValue) {
    LOG(LS_WARNING) << "Failed to parse stun values" ;
    return false;
  }

  Json::Value turn;
  if (GetValueFromJsonObject(value, "turn_service", &turn)) {
    if (turn.type() == Json::objectValue) {
      if (!GetStringFromJsonObject(turn, "host", &turnServer)) {
        LOG(LS_WARNING) << "Failed to parse JSON value: "
                        << value.toStyledString();
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

bool GetJSONSignalingMessage(
    const cricket::SessionDescription* sdp,
    const std::vector<cricket::Candidate>& candidates,
    std::string* signaling_message) {
  const cricket::ContentInfo* audio_content = GetFirstAudioContent(sdp);
  const cricket::ContentInfo* video_content = GetFirstVideoContent(sdp);

  std::vector<Json::Value> media;
  if (audio_content) {
    Json::Value value;
    BuildMediaMessage(audio_content, candidates, false, value);
    media.push_back(value);
  }

  if (video_content) {
    Json::Value value;
    BuildMediaMessage(video_content, candidates, true, value);
    media.push_back(value);
  }

  Json::Value signal;
  Append(signal, "media", media);

  // now serialize
  *signaling_message = Serialize(signal);
  return true;
}

bool BuildMediaMessage(
    const cricket::ContentInfo* content_info,
    const std::vector<cricket::Candidate>& candidates,
    bool video,
    Json::Value& params) {

  if (!content_info) {
    return false;
  }

  if (video) {
    Append(params, "label", 2); //always video 2
  } else {
    Append(params, "label", 1); //always audio 1
  }
  std::vector<Json::Value> rtpmap;

  if (!BuildRtpMapParams(content_info, video, rtpmap)) {
    return false;
  }

  Append(params, "rtpmap", rtpmap);

  Json::Value attributes;
//  Append(attributes, "ice-pwd", candidates.front().password());
//  Append(attributes, "ice-ufrag", candidates.front().username());
  std::vector<Json::Value> jcandidates;

  if (!BuildAttributes(candidates, video, jcandidates)) {
    return false;
  }
  Append(attributes, "candidate", jcandidates);
  Append(params, "attributes", attributes);
  return true;
}

bool BuildRtpMapParams(const cricket::ContentInfo* content_info,
                       bool video,
                       std::vector<Json::Value>& rtpmap) {

  if (!video) {
    const cricket::AudioContentDescription* audio_offer =
        static_cast<const cricket::AudioContentDescription*>(
            content_info->description);


    for (std::vector<cricket::AudioCodec>::const_iterator iter =
        audio_offer->codecs().begin();
        iter != audio_offer->codecs().end(); ++iter) {

      Json::Value codec;
      std::string codec_str = std::string("audio/").append(iter->name);
      Append(codec, "codec", codec_str);
      Json::Value codec_id;
      Append(codec_id, talk_base::ToString(iter->id), codec);
      rtpmap.push_back(codec_id);
    }
  } else {
    const cricket::VideoContentDescription* video_offer =
        static_cast<const cricket::VideoContentDescription*>(
            content_info->description);


    for (std::vector<cricket::VideoCodec>::const_iterator iter =
        video_offer->codecs().begin();
        iter != video_offer->codecs().end(); ++iter) {

      Json::Value codec;
      std::string codec_str = std::string("video/").append(iter->name);
      Append(codec, "codec", codec_str);
      Json::Value codec_id;
      Append(codec_id, talk_base::ToString(iter->id), codec);
      rtpmap.push_back(codec_id);
    }
  }
  return true;
}

bool BuildAttributes(const std::vector<cricket::Candidate>& candidates,
                     bool video,
                     std::vector<Json::Value>& jcandidates) {

  for (std::vector<cricket::Candidate>::const_iterator iter =
      candidates.begin(); iter != candidates.end(); ++iter) {
    if ((video && !iter->name().compare("video_rtp") ||
        (!video && !iter->name().compare("rtp")))) {
      Json::Value candidate;
      Append(candidate, "component", kIceComponent);
      Append(candidate, "foundation", kIceFoundation);
      Append(candidate, "generation", iter->generation());
      Append(candidate, "proto", iter->protocol());
      Append(candidate, "priority", iter->preference());
      Append(candidate, "ip", iter->address().IPAsString());
      Append(candidate, "port", iter->address().PortAsString());
      Append(candidate, "type", iter->type());
      Append(candidate, "name", iter->name());
      Append(candidate, "network_name", iter->network_name());
      Append(candidate, "username", iter->username());
      Append(candidate, "password", iter->password());
      jcandidates.push_back(candidate);
    }
  }
  return true;
}

std::string Serialize(const Json::Value& value) {
  Json::StyledWriter writer;
  return writer.write(value);
}

bool Deserialize(const std::string& message, Json::Value& value) {
  Json::Reader reader;
  return reader.parse(message, value);
}


bool ParseJSONSignalingMessage(const std::string& signaling_message,
                               cricket::SessionDescription*& sdp,
                               std::vector<cricket::Candidate>& candidates) {
  ASSERT(!sdp); // expect this to NULL
  // first deserialize message
  Json::Value value;
  if (!Deserialize(signaling_message, value)) {
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
      audio_content->SortCodecs();
      sdp->AddContent(cricket::CN_AUDIO, cricket::NS_JINGLE_RTP, audio_content);
      ParseICECandidates(mlines[i], candidates);

    } else {
      cricket::VideoContentDescription* video_content =
                new cricket::VideoContentDescription();
      ParseVideoCodec(mlines[i], video_content);
      video_content->SortCodecs();
      sdp->AddContent(cricket::CN_VIDEO, cricket::NS_JINGLE_RTP, video_content);
      ParseICECandidates(mlines[i], candidates);
    }
  }
  return true;
}

bool ParseAudioCodec(Json::Value value,
                     cricket::AudioContentDescription* content) {
  std::vector<Json::Value> rtpmap(ReadValues(value, "rtpmap"));
  if (rtpmap.empty())
    return false;

  for (size_t i = 0; i < rtpmap.size(); ++i) {
    cricket::AudioCodec codec;
    std::string pltype = rtpmap[i].begin().memberName();
    talk_base::FromString(pltype, &codec.id);
    Json::Value codec_info = rtpmap[i][pltype];
    std::vector<std::string> tokens;
    talk_base::split(codec_info["codec"].asString(), '/', &tokens);
    codec.name = tokens[1];
    content->AddCodec(codec);
  }

  return true;
}

bool ParseVideoCodec(Json::Value value,
                     cricket::VideoContentDescription* content) {
  std::vector<Json::Value> rtpmap(ReadValues(value, "rtpmap"));
   if (rtpmap.empty())
     return false;

   for (size_t i = 0; i < rtpmap.size(); ++i) {
     cricket::VideoCodec codec;
     std::string pltype = rtpmap[i].begin().memberName();
     talk_base::FromString(pltype, &codec.id);
     Json::Value codec_info = rtpmap[i][pltype];
     std::vector<std::string> tokens;
     talk_base::split(codec_info["codec"].asString(), '/', &tokens);
     codec.name = tokens[1];
     content->AddCodec(codec);
   }
   return true;
}

bool ParseICECandidates(Json::Value& value,
                        std::vector<cricket::Candidate>& candidates) {
  Json::Value attributes = ReadValue(value, "attributes");
  std::string ice_pwd = ReadString(attributes, "ice-pwd");
  std::string ice_ufrag = ReadString(attributes, "ice-ufrag");

  std::vector<Json::Value> jcandidates = ReadValues(attributes, "candidate");
  char buffer[64];
  for (size_t i = 0; i < jcandidates.size(); ++i) {
    cricket::Candidate cand;
    std::string str;
    str = ReadUInt(jcandidates[i], "generation");
    cand.set_generation_str(str);
    str = ReadString(jcandidates[i], "proto");
    cand.set_protocol(str);
    double priority = ReadDouble(jcandidates[i], "priority");
#ifdef _DEBUG
    double as_int = static_cast<int>(priority);
    ASSERT(as_int == priority);
#endif
    sprintf(buffer, "%i", static_cast<int>(priority));
    str = buffer;
    cand.set_preference_str(str);
    talk_base::SocketAddress addr;
    str = ReadString(jcandidates[i], "ip");
    addr.SetIP(str);
    str = ReadString(jcandidates[i], "port");
    int port; talk_base::FromString(str, &port);
    addr.SetPort(port);
    cand.set_address(addr);
    str = ReadString(jcandidates[i], "type");
    cand.set_type(str);
    str = ReadString(jcandidates[i], "name");
    cand.set_name(str);
    str = ReadString(jcandidates[i], "network_name");
    cand.set_network_name(str);
    str = ReadString(jcandidates[i], "username");
    cand.set_username(str);
    str = ReadString(jcandidates[i], "password");
    cand.set_password(str);
    candidates.push_back(cand);
  }
  return true;
}

std::vector<Json::Value> ReadValues(
    Json::Value& value, const std::string& key) {
  std::vector<Json::Value> objects;
  for (size_t i = 0; i < value[key].size(); ++i) {
    objects.push_back(value[key][i]);
  }
  return objects;
}

Json::Value ReadValue(Json::Value& value, const std::string& key) {
  return value[key];
}

std::string ReadString(Json::Value& value, const std::string& key) {
  return value[key].asString();
}

uint32 ReadUInt(Json::Value& value, const std::string& key) {
  return value[key].asUInt();
}

double ReadDouble(Json::Value& value, const std::string& key) {
  return value[key].asDouble();
}

// Add values
void Append(Json::Value& object, const std::string& key, bool value) {
  object[key] = Json::Value(value);
}

void Append(Json::Value& object, const std::string& key, char * value) {
  object[key] = Json::Value(value);
}
void Append(Json::Value& object, const std::string& key, double value) {
  object[key] = Json::Value(value);
}
void Append(Json::Value& object, const std::string& key, float value) {
  object[key] = Json::Value(value);
}
void Append(Json::Value& object, const std::string& key, int value) {
  object[key] = Json::Value(value);
}
void Append(Json::Value& object, const std::string& key, std::string value) {
  object[key] = Json::Value(value);
}
void Append(Json::Value& object, const std::string& key, uint32 value) {
  object[key] = Json::Value(value);
}

void Append(Json::Value& object, const std::string& key, Json::Value value) {
  object[key] = value;
}

void Append(Json::Value & object,
            const std::string & key,
            std::vector<Json::Value>& values){
  for (std::vector<Json::Value>::const_iterator iter = values.begin();
      iter != values.end(); ++iter) {
    object[key].append(*iter);
  }
}

} //namespace webrtc
