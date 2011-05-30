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

#include <iostream>
#include <string>

#include "talk/base/gunit.h"
#include "talk/app/webrtc_json.h"

namespace webrtc {

Json::Value JsonValueFromString(const std::string &json) {
  Json::Reader reader;
  Json::Value value;

  EXPECT_TRUE(reader.parse(json, value, false));

  return value;
}

class WebRTCJsonTest : public testing::Test {
 public:
  WebRTCJsonTest() {}
  ~WebRTCJsonTest() {}
};

TEST_F(WebRTCJsonTest, TestParseConfig) {
  Json::Value value(JsonValueFromString(
      "\{"
      "   \"connectionmediator\": \"https://somewhere.example.com/conneg\","
      "   \"stun_service\": { "
      "                       \"host\" : \"stun.service.example.com\","
      "                       \"service\" : \"stun\","
      "                       \"protocol\" : \"udp\""
      "                     }"
      " }"));

  std::string c;
  EXPECT_TRUE(GetConnectionMediator(value, c));
  std::cout << " --- connectionmediator --- : " << c << std::endl;

  StunServiceDetails stun;
  EXPECT_TRUE(GetStunServer(value, stun));
  std::cout << " --- stun host --- : " << stun.host << std::endl;
  std::cout << " --- stun service --- : " << stun.service << std::endl;
  std::cout << " --- stun protocol --- : " << stun.protocol << std::endl;
}

TEST_F(WebRTCJsonTest, TestLocalBlob) {
  EXPECT_TRUE(FromSessionDescriptionToJson());
}

}
