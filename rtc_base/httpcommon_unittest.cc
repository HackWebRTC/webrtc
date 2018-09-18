/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/httpcommon.h"
#include "rtc_base/gunit.h"

namespace rtc {

TEST(HttpResponseData, parseLeaderHttp1_0) {
  static const char kResponseString[] = "HTTP/1.0 200 OK";
  HttpResponseData response;
  EXPECT_EQ(HE_NONE,
            response.parseLeader(kResponseString, sizeof(kResponseString) - 1));
  EXPECT_EQ(HVER_1_0, response.version);
  EXPECT_EQ(200U, response.scode);
}

TEST(HttpResponseData, parseLeaderHttp1_1) {
  static const char kResponseString[] = "HTTP/1.1 200 OK";
  HttpResponseData response;
  EXPECT_EQ(HE_NONE,
            response.parseLeader(kResponseString, sizeof(kResponseString) - 1));
  EXPECT_EQ(HVER_1_1, response.version);
  EXPECT_EQ(200U, response.scode);
}

TEST(HttpResponseData, parseLeaderHttpUnknown) {
  static const char kResponseString[] = "HTTP 200 OK";
  HttpResponseData response;
  EXPECT_EQ(HE_NONE,
            response.parseLeader(kResponseString, sizeof(kResponseString) - 1));
  EXPECT_EQ(HVER_UNKNOWN, response.version);
  EXPECT_EQ(200U, response.scode);
}

TEST(HttpResponseData, parseLeaderHttpFailure) {
  static const char kResponseString[] = "HTTP/1.1 503 Service Unavailable";
  HttpResponseData response;
  EXPECT_EQ(HE_NONE,
            response.parseLeader(kResponseString, sizeof(kResponseString) - 1));
  EXPECT_EQ(HVER_1_1, response.version);
  EXPECT_EQ(503U, response.scode);
}

TEST(HttpResponseData, parseLeaderHttpInvalid) {
  static const char kResponseString[] = "Durrrrr, what's HTTP?";
  HttpResponseData response;
  EXPECT_EQ(HE_PROTOCOL,
            response.parseLeader(kResponseString, sizeof(kResponseString) - 1));
}

}  // namespace rtc
