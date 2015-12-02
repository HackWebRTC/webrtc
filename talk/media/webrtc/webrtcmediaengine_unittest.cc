/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#include "testing/gtest/include/gtest/gtest.h"

#include "talk/media/webrtc/webrtcmediaengine.h"

namespace cricket {
namespace {

std::vector<RtpHeaderExtension> MakeUniqueExtensions() {
  std::vector<RtpHeaderExtension> result;
  char name[] = "a";
  for (int i = 0; i < 7; ++i) {
    result.push_back(RtpHeaderExtension(name, 1 + i));
    name[0]++;
    result.push_back(RtpHeaderExtension(name, 14 - i));
    name[0]++;
  }
  return result;
}

std::vector<RtpHeaderExtension> MakeRedundantExtensions() {
  std::vector<RtpHeaderExtension> result;
  char name[] = "a";
  for (int i = 0; i < 7; ++i) {
    result.push_back(RtpHeaderExtension(name, 1 + i));
    result.push_back(RtpHeaderExtension(name, 14 - i));
    name[0]++;
  }
  return result;
}

bool SupportedExtensions1(const std::string& name) {
  return name == "c" || name == "i";
}

bool SupportedExtensions2(const std::string& name) {
  return name != "a" && name != "n";
}

bool IsSorted(const std::vector<webrtc::RtpExtension>& extensions) {
  const std::string* last = nullptr;
  for (const auto& extension : extensions) {
    if (last && *last > extension.name) {
      return false;
    }
    last = &extension.name;
  }
  return true;
}
}  // namespace

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_EmptyList) {
  std::vector<RtpHeaderExtension> extensions;
  EXPECT_TRUE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_AllGood) {
  std::vector<RtpHeaderExtension> extensions = MakeUniqueExtensions();
  EXPECT_TRUE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OutOfRangeId_Low) {
  std::vector<RtpHeaderExtension> extensions = MakeUniqueExtensions();
  extensions.push_back(RtpHeaderExtension("foo", 0));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OutOfRangeId_High) {
  std::vector<RtpHeaderExtension> extensions = MakeUniqueExtensions();
  extensions.push_back(RtpHeaderExtension("foo", 15));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OverlappingIds_StartOfSet) {
  std::vector<RtpHeaderExtension> extensions = MakeUniqueExtensions();
  extensions.push_back(RtpHeaderExtension("foo", 1));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OverlappingIds_EndOfSet) {
  std::vector<RtpHeaderExtension> extensions = MakeUniqueExtensions();
  extensions.push_back(RtpHeaderExtension("foo", 14));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_EmptyList) {
  std::vector<RtpHeaderExtension> extensions;
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions1, true);
  EXPECT_EQ(0, filtered.size());
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_IncludeOnlySupported) {
  std::vector<RtpHeaderExtension> extensions = MakeUniqueExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions1, false);
  EXPECT_EQ(2, filtered.size());
  EXPECT_EQ("c", filtered[0].name);
  EXPECT_EQ("i", filtered[1].name);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_SortedByName_1) {
  std::vector<RtpHeaderExtension> extensions = MakeUniqueExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, false);
  EXPECT_EQ(12, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_SortedByName_2) {
  std::vector<RtpHeaderExtension> extensions = MakeUniqueExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(12, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_DontRemoveRedundant) {
  std::vector<RtpHeaderExtension> extensions = MakeRedundantExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, false);
  EXPECT_EQ(12, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
  EXPECT_EQ(filtered[0].name, filtered[1].name);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundant) {
  std::vector<RtpHeaderExtension> extensions = MakeRedundantExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(6, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
  EXPECT_NE(filtered[0].name, filtered[1].name);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBwe_1) {
  std::vector<RtpHeaderExtension> extensions;
  extensions.push_back(
      RtpHeaderExtension(kRtpTransportSequenceNumberHeaderExtension, 3));
  extensions.push_back(
      RtpHeaderExtension(kRtpTimestampOffsetHeaderExtension, 9));
  extensions.push_back(
      RtpHeaderExtension(kRtpAbsoluteSenderTimeHeaderExtension, 6));
  extensions.push_back(
      RtpHeaderExtension(kRtpTransportSequenceNumberHeaderExtension, 1));
  extensions.push_back(
      RtpHeaderExtension(kRtpTimestampOffsetHeaderExtension, 14));
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(1, filtered.size());
  EXPECT_EQ(kRtpTransportSequenceNumberHeaderExtension, filtered[0].name);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBwe_2) {
  std::vector<RtpHeaderExtension> extensions;
  extensions.push_back(
      RtpHeaderExtension(kRtpTimestampOffsetHeaderExtension, 1));
  extensions.push_back(
      RtpHeaderExtension(kRtpAbsoluteSenderTimeHeaderExtension, 14));
  extensions.push_back(
      RtpHeaderExtension(kRtpTimestampOffsetHeaderExtension, 7));
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(1, filtered.size());
  EXPECT_EQ(kRtpAbsoluteSenderTimeHeaderExtension, filtered[0].name);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBwe_3) {
  std::vector<RtpHeaderExtension> extensions;
  extensions.push_back(
      RtpHeaderExtension(kRtpTimestampOffsetHeaderExtension, 2));
  extensions.push_back(
      RtpHeaderExtension(kRtpTimestampOffsetHeaderExtension, 14));
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(1, filtered.size());
  EXPECT_EQ(kRtpTimestampOffsetHeaderExtension, filtered[0].name);
}
}  // namespace cricket
