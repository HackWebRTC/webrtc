/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fake_voice_engine.h"

namespace {

webrtc::AudioDecodingCallStats MakeAudioDecodingCallStats() {
  webrtc::AudioDecodingCallStats stats;
  stats.calls_to_silence_generator = 234;
  stats.calls_to_neteq = 567;
  stats.decoded_normal = 890;
  stats.decoded_plc = 123;
  stats.decoded_cng = 456;
  stats.decoded_plc_cng = 789;
  return stats;
}
}  // namespace

namespace webrtc {
namespace test {

const int FakeVoiceEngine::kSendChannelId = 1;
const int FakeVoiceEngine::kRecvChannelId = 2;
const uint32_t FakeVoiceEngine::kSendSsrc = 665;
const uint32_t FakeVoiceEngine::kRecvSsrc = 667;
const int FakeVoiceEngine::kSendEchoDelayMedian = 254;
const int FakeVoiceEngine::kSendEchoDelayStdDev = -3;
const int FakeVoiceEngine::kSendEchoReturnLoss = -65;
const int FakeVoiceEngine::kSendEchoReturnLossEnhancement = 101;
const int FakeVoiceEngine::kRecvJitterBufferDelay = -7;
const int FakeVoiceEngine::kRecvPlayoutBufferDelay = 302;
const unsigned int FakeVoiceEngine::kSendSpeechInputLevel = 96;
const unsigned int FakeVoiceEngine::kRecvSpeechOutputLevel = 99;

const CallStatistics FakeVoiceEngine::kSendCallStats = {
  1345, 1678, 1901, 1234, 112, 13456, 17890, 1567, -1890, -1123
};

const CodecInst FakeVoiceEngine::kSendCodecInst = {
  -121, "codec_name_send", 48000, -231, -451, -671
};

const ReportBlock FakeVoiceEngine::kSendReportBlock = {
  456, 780, 123, 567, 890, 132, 143, 13354
};

const CallStatistics FakeVoiceEngine::kRecvCallStats = {
  345, 678, 901, 234, -12, 3456, 7890, 567, 890, 123
};

const CodecInst FakeVoiceEngine::kRecvCodecInst = {
  123, "codec_name_recv", 96000, -187, -198, -103
};

const NetworkStatistics FakeVoiceEngine::kRecvNetworkStats = {
  123, 456, false, 0, 0, 789, 12, 345, 678, 901, -1, -1, -1, -1, -1, 0
};

const AudioDecodingCallStats FakeVoiceEngine::kRecvAudioDecodingCallStats =
    MakeAudioDecodingCallStats();
}  // namespace test
}  // namespace webrtc
