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

#ifndef TALK_APP_WEBRTC_WEBRTC_JSON_H_
#define TALK_APP_WEBRTC_WEBRTC_JSON_H_

#include <string>

#include "json/json.h"
#include "talk/session/phone/codec.h"
#include "talk/p2p/base/candidate.h"

namespace Json {
class Value;
}

namespace cricket {
class AudioContentDescription;
class VideoContentDescription;
struct ContentInfo;
class SessionDescription;
}

struct StunServiceDetails {
  std::string host;
  std::string service;
  std::string protocol;
};

namespace webrtc {

bool GetConnectionMediator(const Json::Value& value,
                           std::string* connection_mediator);
bool GetStunServer(const Json::Value& value, StunServiceDetails* stun);
bool GetTurnServer(const Json::Value& value, std::string* turn_server);
bool FromJsonToAVCodec(const Json::Value& value,
                       cricket::AudioContentDescription* audio,
                       cricket::VideoContentDescription* video);

std::vector<Json::Value> ReadValues(const Json::Value& value,
                                    const std::string& key);

bool BuildMediaMessage(
    const cricket::ContentInfo& content_info,
    const std::vector<cricket::Candidate>& candidates,
    bool video,
    Json::Value* value);

bool GetJSONSignalingMessage(
    const cricket::SessionDescription* sdp,
    const std::vector<cricket::Candidate>& candidates,
    std::string* signaling_message);

bool BuildRtpMapParams(
    const cricket::ContentInfo& audio_offer,
    bool video,
    std::vector<Json::Value>* rtpmap);

bool BuildAttributes(const std::vector<cricket::Candidate>& candidates,
                     bool video,
                     std::vector<Json::Value>* jcandidates);

std::string Serialize(const Json::Value& value);
bool Deserialize(const std::string& message, Json::Value& value);

bool ParseJSONSignalingMessage(const std::string& signaling_message,
                               cricket::SessionDescription*& sdp,
                               std::vector<cricket::Candidate>* candidates);
bool ParseAudioCodec(const Json::Value& value,
                     cricket::AudioContentDescription* content);
bool ParseVideoCodec(const Json::Value& value,
                     cricket::VideoContentDescription* content);
bool ParseICECandidates(const Json::Value& value,
                        std::vector<cricket::Candidate>* candidates);
Json::Value ReadValue(const Json::Value& value, const std::string& key);
std::string ReadString(const Json::Value& value, const std::string& key);
double ReadDouble(const Json::Value& value, const std::string& key);
uint32 ReadUInt(const Json::Value& value, const std::string& key);

// Add values
void Append(Json::Value* object, const std::string& key, bool value);

void Append(Json::Value* object, const std::string& key, char * value);
void Append(Json::Value* object, const std::string& key, double value);
void Append(Json::Value* object, const std::string& key, float value);
void Append(Json::Value* object, const std::string& key, int value);
void Append(Json::Value* object, const std::string& key,
            const std::string& value);
void Append(Json::Value* object, const std::string& key, uint32 value);
void Append(Json::Value* object, const std::string& key,
            const Json::Value& value);
void Append(Json::Value* object,
            const std::string& key,
            const std::vector<Json::Value>& values);
}

#endif  // TALK_APP_WEBRTC_WEBRTC_JSON_H_
