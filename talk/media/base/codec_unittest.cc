/*
 * libjingle
 * Copyright 2009 Google Inc.
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

#include "talk/base/gunit.h"
#include "talk/media/base/codec.h"

using cricket::AudioCodec;
using cricket::Codec;
using cricket::DataCodec;
using cricket::FeedbackParam;
using cricket::VideoCodec;
using cricket::VideoEncoderConfig;

class CodecTest : public testing::Test {
 public:
  CodecTest() {}
};

TEST_F(CodecTest, TestAudioCodecOperators) {
  AudioCodec c0(96, "A", 44100, 20000, 2, 3);
  AudioCodec c1(95, "A", 44100, 20000, 2, 3);
  AudioCodec c2(96, "x", 44100, 20000, 2, 3);
  AudioCodec c3(96, "A", 48000, 20000, 2, 3);
  AudioCodec c4(96, "A", 44100, 10000, 2, 3);
  AudioCodec c5(96, "A", 44100, 20000, 1, 3);
  AudioCodec c6(96, "A", 44100, 20000, 2, 1);
  EXPECT_TRUE(c0 != c1);
  EXPECT_TRUE(c0 != c2);
  EXPECT_TRUE(c0 != c3);
  EXPECT_TRUE(c0 != c4);
  EXPECT_TRUE(c0 != c5);
  EXPECT_TRUE(c0 != c6);

  AudioCodec c7;
  AudioCodec c8(0, "", 0, 0, 0, 0);
  AudioCodec c9 = c0;
  EXPECT_TRUE(c8 == c7);
  EXPECT_TRUE(c9 != c7);
  EXPECT_TRUE(c9 == c0);

  AudioCodec c10(c0);
  AudioCodec c11(c0);
  AudioCodec c12(c0);
  AudioCodec c13(c0);
  c10.params["x"] = "abc";
  c11.params["x"] = "def";
  c12.params["y"] = "abc";
  c13.params["x"] = "abc";
  EXPECT_TRUE(c10 != c0);
  EXPECT_TRUE(c11 != c0);
  EXPECT_TRUE(c11 != c10);
  EXPECT_TRUE(c12 != c0);
  EXPECT_TRUE(c12 != c10);
  EXPECT_TRUE(c12 != c11);
  EXPECT_TRUE(c13 == c10);
}

TEST_F(CodecTest, TestAudioCodecMatches) {
  // Test a codec with a static payload type.
  AudioCodec c0(95, "A", 44100, 20000, 1, 3);
  EXPECT_TRUE(c0.Matches(AudioCodec(95, "", 44100, 20000, 1, 0)));
  EXPECT_TRUE(c0.Matches(AudioCodec(95, "", 44100, 20000, 0, 0)));
  EXPECT_TRUE(c0.Matches(AudioCodec(95, "", 44100, 0, 0, 0)));
  EXPECT_TRUE(c0.Matches(AudioCodec(95, "", 0, 0, 0, 0)));
  EXPECT_FALSE(c0.Matches(AudioCodec(96, "", 44100, 20000, 1, 0)));
  EXPECT_FALSE(c0.Matches(AudioCodec(95, "", 55100, 20000, 1, 0)));
  EXPECT_FALSE(c0.Matches(AudioCodec(95, "", 44100, 30000, 1, 0)));
  EXPECT_FALSE(c0.Matches(AudioCodec(95, "", 44100, 20000, 2, 0)));
  EXPECT_FALSE(c0.Matches(AudioCodec(95, "", 55100, 30000, 2, 0)));

  // Test a codec with a dynamic payload type.
  AudioCodec c1(96, "A", 44100, 20000, 1, 3);
  EXPECT_TRUE(c1.Matches(AudioCodec(96, "A", 0, 0, 0, 0)));
  EXPECT_TRUE(c1.Matches(AudioCodec(97, "A", 0, 0, 0, 0)));
  EXPECT_TRUE(c1.Matches(AudioCodec(96, "a", 0, 0, 0, 0)));
  EXPECT_TRUE(c1.Matches(AudioCodec(97, "a", 0, 0, 0, 0)));
  EXPECT_FALSE(c1.Matches(AudioCodec(95, "A", 0, 0, 0, 0)));
  EXPECT_FALSE(c1.Matches(AudioCodec(96, "", 44100, 20000, 2, 0)));
  EXPECT_FALSE(c1.Matches(AudioCodec(96, "A", 55100, 30000, 1, 0)));

  // Test a codec with a dynamic payload type, and auto bitrate.
  AudioCodec c2(97, "A", 16000, 0, 1, 3);
  // Use default bitrate.
  EXPECT_TRUE(c2.Matches(AudioCodec(97, "A", 16000, 0, 1, 0)));
  EXPECT_TRUE(c2.Matches(AudioCodec(97, "A", 16000, 0, 0, 0)));
  // Use explicit bitrate.
  EXPECT_TRUE(c2.Matches(AudioCodec(97, "A", 16000, 32000, 1, 0)));
  // Backward compatibility with clients that might send "-1" (for default).
  EXPECT_TRUE(c2.Matches(AudioCodec(97, "A", 16000, -1, 1, 0)));

  // Stereo doesn't match channels = 0.
  AudioCodec c3(96, "A", 44100, 20000, 2, 3);
  EXPECT_TRUE(c3.Matches(AudioCodec(96, "A", 44100, 20000, 2, 3)));
  EXPECT_FALSE(c3.Matches(AudioCodec(96, "A", 44100, 20000, 1, 3)));
  EXPECT_FALSE(c3.Matches(AudioCodec(96, "A", 44100, 20000, 0, 3)));
}

TEST_F(CodecTest, TestVideoCodecOperators) {
  VideoCodec c0(96, "V", 320, 200, 30, 3);
  VideoCodec c1(95, "V", 320, 200, 30, 3);
  VideoCodec c2(96, "x", 320, 200, 30, 3);
  VideoCodec c3(96, "V", 120, 200, 30, 3);
  VideoCodec c4(96, "V", 320, 100, 30, 3);
  VideoCodec c5(96, "V", 320, 200, 10, 3);
  VideoCodec c6(96, "V", 320, 200, 30, 1);
  EXPECT_TRUE(c0 != c1);
  EXPECT_TRUE(c0 != c2);
  EXPECT_TRUE(c0 != c3);
  EXPECT_TRUE(c0 != c4);
  EXPECT_TRUE(c0 != c5);
  EXPECT_TRUE(c0 != c6);

  VideoCodec c7;
  VideoCodec c8(0, "", 0, 0, 0, 0);
  VideoCodec c9 = c0;
  EXPECT_TRUE(c8 == c7);
  EXPECT_TRUE(c9 != c7);
  EXPECT_TRUE(c9 == c0);

  VideoCodec c10(c0);
  VideoCodec c11(c0);
  VideoCodec c12(c0);
  VideoCodec c13(c0);
  c10.params["x"] = "abc";
  c11.params["x"] = "def";
  c12.params["y"] = "abc";
  c13.params["x"] = "abc";
  EXPECT_TRUE(c10 != c0);
  EXPECT_TRUE(c11 != c0);
  EXPECT_TRUE(c11 != c10);
  EXPECT_TRUE(c12 != c0);
  EXPECT_TRUE(c12 != c10);
  EXPECT_TRUE(c12 != c11);
  EXPECT_TRUE(c13 == c10);
}

TEST_F(CodecTest, TestVideoCodecMatches) {
  // Test a codec with a static payload type.
  VideoCodec c0(95, "V", 320, 200, 30, 3);
  EXPECT_TRUE(c0.Matches(VideoCodec(95, "", 640, 400, 15, 0)));
  EXPECT_FALSE(c0.Matches(VideoCodec(96, "", 320, 200, 30, 0)));

  // Test a codec with a dynamic payload type.
  VideoCodec c1(96, "V", 320, 200, 30, 3);
  EXPECT_TRUE(c1.Matches(VideoCodec(96, "V", 640, 400, 15, 0)));
  EXPECT_TRUE(c1.Matches(VideoCodec(97, "V", 640, 400, 15, 0)));
  EXPECT_TRUE(c1.Matches(VideoCodec(96, "v", 640, 400, 15, 0)));
  EXPECT_TRUE(c1.Matches(VideoCodec(97, "v", 640, 400, 15, 0)));
  EXPECT_FALSE(c1.Matches(VideoCodec(96, "", 320, 200, 30, 0)));
  EXPECT_FALSE(c1.Matches(VideoCodec(95, "V", 640, 400, 15, 0)));
}

TEST_F(CodecTest, TestVideoEncoderConfigOperators) {
  VideoEncoderConfig c1(VideoCodec(
      96, "SVC", 320, 200, 30, 3), 1, 2);
  VideoEncoderConfig c2(VideoCodec(
      95, "SVC", 320, 200, 30, 3), 1, 2);
  VideoEncoderConfig c3(VideoCodec(
      96, "xxx", 320, 200, 30, 3), 1, 2);
  VideoEncoderConfig c4(VideoCodec(
      96, "SVC", 120, 200, 30, 3), 1, 2);
  VideoEncoderConfig c5(VideoCodec(
      96, "SVC", 320, 100, 30, 3), 1, 2);
  VideoEncoderConfig c6(VideoCodec(
      96, "SVC", 320, 200, 10, 3), 1, 2);
  VideoEncoderConfig c7(VideoCodec(
      96, "SVC", 320, 200, 30, 1), 1, 2);
  VideoEncoderConfig c8(VideoCodec(
      96, "SVC", 320, 200, 30, 3), 0, 2);
  VideoEncoderConfig c9(VideoCodec(
      96, "SVC", 320, 200, 30, 3), 1, 1);
  EXPECT_TRUE(c1 != c2);
  EXPECT_TRUE(c1 != c2);
  EXPECT_TRUE(c1 != c3);
  EXPECT_TRUE(c1 != c4);
  EXPECT_TRUE(c1 != c5);
  EXPECT_TRUE(c1 != c6);
  EXPECT_TRUE(c1 != c7);
  EXPECT_TRUE(c1 != c8);
  EXPECT_TRUE(c1 != c9);

  VideoEncoderConfig c10;
  VideoEncoderConfig c11(VideoCodec(
      0, "", 0, 0, 0, 0));
  VideoEncoderConfig c12(VideoCodec(
      0, "", 0, 0, 0, 0),
      VideoEncoderConfig::kDefaultMaxThreads,
      VideoEncoderConfig::kDefaultCpuProfile);
  VideoEncoderConfig c13 = c1;
  VideoEncoderConfig c14(VideoCodec(
      0, "", 0, 0, 0, 0), 0, 0);

  EXPECT_TRUE(c11 == c10);
  EXPECT_TRUE(c12 == c10);
  EXPECT_TRUE(c13 != c10);
  EXPECT_TRUE(c13 == c1);
  EXPECT_TRUE(c14 != c11);
  EXPECT_TRUE(c14 != c12);
}

TEST_F(CodecTest, TestDataCodecMatches) {
  // Test a codec with a static payload type.
  DataCodec c0(95, "D", 0);
  EXPECT_TRUE(c0.Matches(DataCodec(95, "", 0)));
  EXPECT_FALSE(c0.Matches(DataCodec(96, "", 0)));

  // Test a codec with a dynamic payload type.
  DataCodec c1(96, "D", 3);
  EXPECT_TRUE(c1.Matches(DataCodec(96, "D", 0)));
  EXPECT_TRUE(c1.Matches(DataCodec(97, "D", 0)));
  EXPECT_TRUE(c1.Matches(DataCodec(96, "d", 0)));
  EXPECT_TRUE(c1.Matches(DataCodec(97, "d", 0)));
  EXPECT_FALSE(c1.Matches(DataCodec(96, "", 0)));
  EXPECT_FALSE(c1.Matches(DataCodec(95, "D", 0)));
}

TEST_F(CodecTest, TestDataCodecOperators) {
  DataCodec c0(96, "D", 3);
  DataCodec c1(95, "D", 3);
  DataCodec c2(96, "x", 3);
  DataCodec c3(96, "D", 1);
  EXPECT_TRUE(c0 != c1);
  EXPECT_TRUE(c0 != c2);
  EXPECT_TRUE(c0 != c3);

  DataCodec c4;
  DataCodec c5(0, "", 0);
  DataCodec c6 = c0;
  EXPECT_TRUE(c5 == c4);
  EXPECT_TRUE(c6 != c4);
  EXPECT_TRUE(c6 == c0);
}

TEST_F(CodecTest, TestSetParamAndGetParam) {
  AudioCodec codec;
  codec.SetParam("a", "1");
  codec.SetParam("b", "x");

  int int_value = 0;
  EXPECT_TRUE(codec.GetParam("a", &int_value));
  EXPECT_EQ(1, int_value);
  EXPECT_FALSE(codec.GetParam("b", &int_value));
  EXPECT_FALSE(codec.GetParam("c", &int_value));

  std::string str_value;
  EXPECT_TRUE(codec.GetParam("a", &str_value));
  EXPECT_EQ("1", str_value);
  EXPECT_TRUE(codec.GetParam("b", &str_value));
  EXPECT_EQ("x", str_value);
  EXPECT_FALSE(codec.GetParam("c", &str_value));
}

TEST_F(CodecTest, TestIntersectFeedbackParams) {
  const FeedbackParam a1("a", "1");
  const FeedbackParam b2("b", "2");
  const FeedbackParam b3("b", "3");
  const FeedbackParam c3("c", "3");
  Codec c1;
  c1.AddFeedbackParam(a1); // Only match with c2.
  c1.AddFeedbackParam(b2); // Same param different values.
  c1.AddFeedbackParam(c3); // Not in c2.
  Codec c2;
  c2.AddFeedbackParam(a1);
  c2.AddFeedbackParam(b3);

  c1.IntersectFeedbackParams(c2);
  EXPECT_TRUE(c1.HasFeedbackParam(a1));
  EXPECT_FALSE(c1.HasFeedbackParam(b2));
  EXPECT_FALSE(c1.HasFeedbackParam(c3));
}
