/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>  // For ULONG_MAX returned by strtoul.
#include <stdio.h>
#include <stdlib.h>  // For strtoul.

#include <algorithm>
#include <ios>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>

#include "gflags/gflags.h"
#include "webrtc/modules/audio_coding/neteq/include/neteq.h"
#include "webrtc/modules/audio_coding/neteq/tools/fake_decode_from_file.h"
#include "webrtc/modules/audio_coding/neteq/tools/input_audio_file.h"
#include "webrtc/modules/audio_coding/neteq/tools/neteq_delay_analyzer.h"
#include "webrtc/modules/audio_coding/neteq/tools/neteq_packet_source_input.h"
#include "webrtc/modules/audio_coding/neteq/tools/neteq_replacement_input.h"
#include "webrtc/modules/audio_coding/neteq/tools/neteq_test.h"
#include "webrtc/modules/audio_coding/neteq/tools/output_audio_file.h"
#include "webrtc/modules/audio_coding/neteq/tools/output_wav_file.h"
#include "webrtc/modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {
namespace {

// Parses the input string for a valid SSRC (at the start of the string). If a
// valid SSRC is found, it is written to the output variable |ssrc|, and true is
// returned. Otherwise, false is returned.
bool ParseSsrc(const std::string& str, uint32_t* ssrc) {
  if (str.empty())
    return true;
  int base = 10;
  // Look for "0x" or "0X" at the start and change base to 16 if found.
  if ((str.compare(0, 2, "0x") == 0) || (str.compare(0, 2, "0X") == 0))
    base = 16;
  errno = 0;
  char* end_ptr;
  unsigned long value = strtoul(str.c_str(), &end_ptr, base);
  if (value == ULONG_MAX && errno == ERANGE)
    return false;  // Value out of range for unsigned long.
  if (sizeof(unsigned long) > sizeof(uint32_t) && value > 0xFFFFFFFF)
    return false;  // Value out of range for uint32_t.
  if (end_ptr - str.c_str() < static_cast<ptrdiff_t>(str.length()))
    return false;  // Part of the string was not parsed.
  *ssrc = static_cast<uint32_t>(value);
  return true;
}

// Flag validators.
bool ValidatePayloadType(const char* flagname, int32_t value) {
  if (value >= 0 && value <= 127)  // Value is ok.
    return true;
  printf("Invalid value for --%s: %d\n", flagname, static_cast<int>(value));
  return false;
}

bool ValidateSsrcValue(const char* flagname, const std::string& str) {
  uint32_t dummy_ssrc;
  return ParseSsrc(str, &dummy_ssrc);
}

static bool ValidateExtensionId(const char* flagname, int32_t value) {
  if (value > 0 && value <= 255)  // Value is ok.
    return true;
  printf("Invalid value for --%s: %d\n", flagname, static_cast<int>(value));
  return false;
}

// Define command line flags.
DEFINE_int32(pcmu, 0, "RTP payload type for PCM-u");
const bool pcmu_dummy =
    google::RegisterFlagValidator(&FLAGS_pcmu, &ValidatePayloadType);
DEFINE_int32(pcma, 8, "RTP payload type for PCM-a");
const bool pcma_dummy =
    google::RegisterFlagValidator(&FLAGS_pcma, &ValidatePayloadType);
DEFINE_int32(ilbc, 102, "RTP payload type for iLBC");
const bool ilbc_dummy =
    google::RegisterFlagValidator(&FLAGS_ilbc, &ValidatePayloadType);
DEFINE_int32(isac, 103, "RTP payload type for iSAC");
const bool isac_dummy =
    google::RegisterFlagValidator(&FLAGS_isac, &ValidatePayloadType);
DEFINE_int32(isac_swb, 104, "RTP payload type for iSAC-swb (32 kHz)");
const bool isac_swb_dummy =
    google::RegisterFlagValidator(&FLAGS_isac_swb, &ValidatePayloadType);
DEFINE_int32(opus, 111, "RTP payload type for Opus");
const bool opus_dummy =
    google::RegisterFlagValidator(&FLAGS_opus, &ValidatePayloadType);
DEFINE_int32(pcm16b, 93, "RTP payload type for PCM16b-nb (8 kHz)");
const bool pcm16b_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b, &ValidatePayloadType);
DEFINE_int32(pcm16b_wb, 94, "RTP payload type for PCM16b-wb (16 kHz)");
const bool pcm16b_wb_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b_wb, &ValidatePayloadType);
DEFINE_int32(pcm16b_swb32, 95, "RTP payload type for PCM16b-swb32 (32 kHz)");
const bool pcm16b_swb32_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b_swb32, &ValidatePayloadType);
DEFINE_int32(pcm16b_swb48, 96, "RTP payload type for PCM16b-swb48 (48 kHz)");
const bool pcm16b_swb48_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b_swb48, &ValidatePayloadType);
DEFINE_int32(g722, 9, "RTP payload type for G.722");
const bool g722_dummy =
    google::RegisterFlagValidator(&FLAGS_g722, &ValidatePayloadType);
DEFINE_int32(avt, 106, "RTP payload type for AVT/DTMF (8 kHz)");
const bool avt_dummy =
    google::RegisterFlagValidator(&FLAGS_avt, &ValidatePayloadType);
DEFINE_int32(avt_16, 114, "RTP payload type for AVT/DTMF (16 kHz)");
const bool avt_16_dummy =
    google::RegisterFlagValidator(&FLAGS_avt_16, &ValidatePayloadType);
DEFINE_int32(avt_32, 115, "RTP payload type for AVT/DTMF (32 kHz)");
const bool avt_32_dummy =
    google::RegisterFlagValidator(&FLAGS_avt_32, &ValidatePayloadType);
DEFINE_int32(avt_48, 116, "RTP payload type for AVT/DTMF (48 kHz)");
const bool avt_48_dummy =
    google::RegisterFlagValidator(&FLAGS_avt_48, &ValidatePayloadType);
DEFINE_int32(red, 117, "RTP payload type for redundant audio (RED)");
const bool red_dummy =
    google::RegisterFlagValidator(&FLAGS_red, &ValidatePayloadType);
DEFINE_int32(cn_nb, 13, "RTP payload type for comfort noise (8 kHz)");
const bool cn_nb_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_nb, &ValidatePayloadType);
DEFINE_int32(cn_wb, 98, "RTP payload type for comfort noise (16 kHz)");
const bool cn_wb_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_wb, &ValidatePayloadType);
DEFINE_int32(cn_swb32, 99, "RTP payload type for comfort noise (32 kHz)");
const bool cn_swb32_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_swb32, &ValidatePayloadType);
DEFINE_int32(cn_swb48, 100, "RTP payload type for comfort noise (48 kHz)");
const bool cn_swb48_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_swb48, &ValidatePayloadType);
DEFINE_bool(codec_map, false, "Prints the mapping between RTP payload type and "
    "codec");
DEFINE_string(replacement_audio_file, "",
              "A PCM file that will be used to populate ""dummy"" RTP packets");
DEFINE_string(ssrc,
              "",
              "Only use packets with this SSRC (decimal or hex, the latter "
              "starting with 0x)");
const bool hex_ssrc_dummy =
    google::RegisterFlagValidator(&FLAGS_ssrc, &ValidateSsrcValue);
DEFINE_int32(audio_level, 1, "Extension ID for audio level (RFC 6464)");
const bool audio_level_dummy =
    google::RegisterFlagValidator(&FLAGS_audio_level, &ValidateExtensionId);
DEFINE_int32(abs_send_time, 3, "Extension ID for absolute sender time");
const bool abs_send_time_dummy =
    google::RegisterFlagValidator(&FLAGS_abs_send_time, &ValidateExtensionId);
DEFINE_bool(matlabplot,
            false,
            "Generates a matlab script for plotting the delay profile");

// Maps a codec type to a printable name string.
std::string CodecName(NetEqDecoder codec) {
  switch (codec) {
    case NetEqDecoder::kDecoderPCMu:
      return "PCM-u";
    case NetEqDecoder::kDecoderPCMa:
      return "PCM-a";
    case NetEqDecoder::kDecoderILBC:
      return "iLBC";
    case NetEqDecoder::kDecoderISAC:
      return "iSAC";
    case NetEqDecoder::kDecoderISACswb:
      return "iSAC-swb (32 kHz)";
    case NetEqDecoder::kDecoderOpus:
      return "Opus";
    case NetEqDecoder::kDecoderPCM16B:
      return "PCM16b-nb (8 kHz)";
    case NetEqDecoder::kDecoderPCM16Bwb:
      return "PCM16b-wb (16 kHz)";
    case NetEqDecoder::kDecoderPCM16Bswb32kHz:
      return "PCM16b-swb32 (32 kHz)";
    case NetEqDecoder::kDecoderPCM16Bswb48kHz:
      return "PCM16b-swb48 (48 kHz)";
    case NetEqDecoder::kDecoderG722:
      return "G.722";
    case NetEqDecoder::kDecoderRED:
      return "redundant audio (RED)";
    case NetEqDecoder::kDecoderAVT:
      return "AVT/DTMF (8 kHz)";
    case NetEqDecoder::kDecoderAVT16kHz:
      return "AVT/DTMF (16 kHz)";
    case NetEqDecoder::kDecoderAVT32kHz:
      return "AVT/DTMF (32 kHz)";
    case NetEqDecoder::kDecoderAVT48kHz:
      return "AVT/DTMF (48 kHz)";
    case NetEqDecoder::kDecoderCNGnb:
      return "comfort noise (8 kHz)";
    case NetEqDecoder::kDecoderCNGwb:
      return "comfort noise (16 kHz)";
    case NetEqDecoder::kDecoderCNGswb32kHz:
      return "comfort noise (32 kHz)";
    case NetEqDecoder::kDecoderCNGswb48kHz:
      return "comfort noise (48 kHz)";
    default:
      FATAL();
      return "undefined";
  }
}

void PrintCodecMappingEntry(NetEqDecoder codec, google::int32 flag) {
  std::cout << CodecName(codec) << ": " << flag << std::endl;
}

void PrintCodecMapping() {
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCMu, FLAGS_pcmu);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCMa, FLAGS_pcma);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderILBC, FLAGS_ilbc);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderISAC, FLAGS_isac);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderISACswb, FLAGS_isac_swb);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderOpus, FLAGS_opus);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCM16B, FLAGS_pcm16b);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCM16Bwb, FLAGS_pcm16b_wb);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCM16Bswb32kHz,
                         FLAGS_pcm16b_swb32);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderPCM16Bswb48kHz,
                         FLAGS_pcm16b_swb48);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderG722, FLAGS_g722);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderAVT, FLAGS_avt);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderAVT16kHz, FLAGS_avt_16);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderAVT32kHz, FLAGS_avt_32);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderAVT48kHz, FLAGS_avt_48);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderRED, FLAGS_red);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderCNGnb, FLAGS_cn_nb);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderCNGwb, FLAGS_cn_wb);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderCNGswb32kHz, FLAGS_cn_swb32);
  PrintCodecMappingEntry(NetEqDecoder::kDecoderCNGswb48kHz, FLAGS_cn_swb48);
}

rtc::Optional<int> CodecSampleRate(uint8_t payload_type) {
  if (payload_type == FLAGS_pcmu || payload_type == FLAGS_pcma ||
      payload_type == FLAGS_ilbc || payload_type == FLAGS_pcm16b ||
      payload_type == FLAGS_cn_nb || payload_type == FLAGS_avt)
    return rtc::Optional<int>(8000);
  if (payload_type == FLAGS_isac || payload_type == FLAGS_pcm16b_wb ||
      payload_type == FLAGS_g722 || payload_type == FLAGS_cn_wb ||
      payload_type == FLAGS_avt_16)
    return rtc::Optional<int>(16000);
  if (payload_type == FLAGS_isac_swb || payload_type == FLAGS_pcm16b_swb32 ||
      payload_type == FLAGS_cn_swb32 || payload_type == FLAGS_avt_32)
    return rtc::Optional<int>(32000);
  if (payload_type == FLAGS_opus || payload_type == FLAGS_pcm16b_swb48 ||
      payload_type == FLAGS_cn_swb48 || payload_type == FLAGS_avt_48)
    return rtc::Optional<int>(48000);
  if (payload_type == FLAGS_red)
    return rtc::Optional<int>(0);
  return rtc::Optional<int>();
}

// Class to let through only the packets with a given SSRC. Should be used as an
// outer layer on another NetEqInput object.
class FilterSsrcInput : public NetEqInput {
 public:
  FilterSsrcInput(std::unique_ptr<NetEqInput> source, uint32_t ssrc)
      : source_(std::move(source)), ssrc_(ssrc) {
    FindNextWithCorrectSsrc();
    RTC_CHECK(source_->NextHeader()) << "Found no packet with SSRC = 0x"
                                     << std::hex << ssrc_;
  }

  // All methods but PopPacket() simply relay to the |source_| object.
  rtc::Optional<int64_t> NextPacketTime() const override {
    return source_->NextPacketTime();
  }
  rtc::Optional<int64_t> NextOutputEventTime() const override {
    return source_->NextOutputEventTime();
  }

  // Returns the next packet, and throws away upcoming packets that do not match
  // the desired SSRC.
  std::unique_ptr<PacketData> PopPacket() override {
    std::unique_ptr<PacketData> packet_to_return = source_->PopPacket();
    RTC_DCHECK(!packet_to_return || packet_to_return->header.ssrc == ssrc_);
    // Pre-fetch the next packet with correct SSRC. Hence, |source_| will always
    // be have a valid packet (or empty if no more packets are available) when
    // this method returns.
    FindNextWithCorrectSsrc();
    return packet_to_return;
  }

  void AdvanceOutputEvent() override { source_->AdvanceOutputEvent(); }

  bool ended() const override { return source_->ended(); }

  rtc::Optional<RTPHeader> NextHeader() const override {
    return source_->NextHeader();
  }

 private:
  void FindNextWithCorrectSsrc() {
    while (source_->NextHeader() && source_->NextHeader()->ssrc != ssrc_) {
      source_->PopPacket();
    }
  }

  std::unique_ptr<NetEqInput> source_;
  uint32_t ssrc_;
};

// A callback class which prints whenver the inserted packet stream changes
// the SSRC.
class SsrcSwitchDetector : public NetEqPostInsertPacket {
 public:
  // Takes a pointer to another callback object, which will be invoked after
  // this object finishes. This does not transfer ownership, and null is a
  // valid value.
  explicit SsrcSwitchDetector(NetEqPostInsertPacket* other_callback)
      : other_callback_(other_callback) {}

  void AfterInsertPacket(const NetEqInput::PacketData& packet,
                         NetEq* neteq) override {
    if (last_ssrc_ && packet.header.ssrc != *last_ssrc_) {
      std::cout << "Changing streams from 0x" << std::hex << *last_ssrc_
                << " to 0x" << std::hex << packet.header.ssrc
                << std::dec << " (payload type "
                << static_cast<int>(packet.header.payloadType) << ")"
                << std::endl;
    }
    last_ssrc_ = rtc::Optional<uint32_t>(packet.header.ssrc);
    if (other_callback_) {
      other_callback_->AfterInsertPacket(packet, neteq);
    }
  }

 private:
  NetEqPostInsertPacket* other_callback_;
  rtc::Optional<uint32_t> last_ssrc_;
};

class StatsGetter : public NetEqGetAudioCallback {
 public:
  // This struct is a replica of webrtc::NetEqNetworkStatistics, but with all
  // values stored in double precision.
  struct Stats {
    double current_buffer_size_ms = 0.0;
    double preferred_buffer_size_ms = 0.0;
    double jitter_peaks_found = 0.0;
    double packet_loss_rate = 0.0;
    double expand_rate = 0.0;
    double speech_expand_rate = 0.0;
    double preemptive_rate = 0.0;
    double accelerate_rate = 0.0;
    double secondary_decoded_rate = 0.0;
    double secondary_discarded_rate = 0.0;
    double clockdrift_ppm = 0.0;
    double added_zero_samples = 0.0;
    double mean_waiting_time_ms = 0.0;
    double median_waiting_time_ms = 0.0;
    double min_waiting_time_ms = 0.0;
    double max_waiting_time_ms = 0.0;
  };

  // Takes a pointer to another callback object, which will be invoked after
  // this object finishes. This does not transfer ownership, and null is a
  // valid value.
  explicit StatsGetter(NetEqGetAudioCallback* other_callback)
      : other_callback_(other_callback) {}

  void BeforeGetAudio(NetEq* neteq) override {
    if (other_callback_) {
      other_callback_->BeforeGetAudio(neteq);
    }
  }

  void AfterGetAudio(int64_t time_now_ms,
                     const AudioFrame& audio_frame,
                     bool muted,
                     NetEq* neteq) override {
    if (++counter_ >= 100) {
      counter_ = 0;
      NetEqNetworkStatistics stats;
      RTC_CHECK_EQ(neteq->NetworkStatistics(&stats), 0);
      stats_.push_back(stats);
    }
    if (other_callback_) {
      other_callback_->AfterGetAudio(time_now_ms, audio_frame, muted, neteq);
    }
  }

  double AverageSpeechExpandRate() const {
    double sum_speech_expand =
        std::accumulate(stats_.begin(), stats_.end(), double{0.0},
                        [](double a, NetEqNetworkStatistics b) {
                          return a + static_cast<double>(b.speech_expand_rate);
                        });
    return sum_speech_expand / 16384.0 / stats_.size();
  }

  Stats AverageStats() const {
    Stats sum_stats = std::accumulate(
        stats_.begin(), stats_.end(), Stats(),
        [](Stats a, NetEqNetworkStatistics b) {
          a.current_buffer_size_ms += b.current_buffer_size_ms;
          a.preferred_buffer_size_ms += b.preferred_buffer_size_ms;
          a.jitter_peaks_found += b.jitter_peaks_found;
          a.packet_loss_rate += b.packet_loss_rate / 16384.0;
          a.expand_rate += b.expand_rate / 16384.0;
          a.speech_expand_rate += b.speech_expand_rate / 16384.0;
          a.preemptive_rate += b.preemptive_rate / 16384.0;
          a.accelerate_rate += b.accelerate_rate / 16384.0;
          a.secondary_decoded_rate += b.secondary_decoded_rate / 16384.0;
          a.secondary_discarded_rate += b.secondary_discarded_rate / 16384.0;
          a.clockdrift_ppm += b.clockdrift_ppm;
          a.added_zero_samples += b.added_zero_samples;
          a.mean_waiting_time_ms += b.mean_waiting_time_ms;
          a.median_waiting_time_ms += b.median_waiting_time_ms;
          a.min_waiting_time_ms += b.min_waiting_time_ms;
          a.max_waiting_time_ms += b.max_waiting_time_ms;
          return a;
        });

    sum_stats.current_buffer_size_ms /= stats_.size();
    sum_stats.preferred_buffer_size_ms /= stats_.size();
    sum_stats.jitter_peaks_found /= stats_.size();
    sum_stats.packet_loss_rate /= stats_.size();
    sum_stats.expand_rate /= stats_.size();
    sum_stats.speech_expand_rate /= stats_.size();
    sum_stats.preemptive_rate /= stats_.size();
    sum_stats.accelerate_rate /= stats_.size();
    sum_stats.secondary_decoded_rate /= stats_.size();
    sum_stats.secondary_discarded_rate /= stats_.size();
    sum_stats.clockdrift_ppm /= stats_.size();
    sum_stats.added_zero_samples /= stats_.size();
    sum_stats.mean_waiting_time_ms /= stats_.size();
    sum_stats.median_waiting_time_ms /= stats_.size();
    sum_stats.min_waiting_time_ms /= stats_.size();
    sum_stats.max_waiting_time_ms /= stats_.size();

    return sum_stats;
  }

 private:
  NetEqGetAudioCallback* other_callback_;
  size_t counter_ = 0;
  std::vector<NetEqNetworkStatistics> stats_;
};

int RunTest(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage = "Tool for decoding an RTP dump file using NetEq.\n"
      "Run " + program_name + " --helpshort for usage.\n"
      "Example usage:\n" + program_name +
      " input.rtp output.{pcm, wav}\n";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_codec_map) {
    PrintCodecMapping();
  }

  if (argc != 3) {
    if (FLAGS_codec_map) {
      // We have already printed the codec map. Just end the program.
      return 0;
    }
    // Print usage information.
    std::cout << google::ProgramUsage();
    return 0;
  }

  // Gather RTP header extensions in a map.
  NetEqPacketSourceInput::RtpHeaderExtensionMap rtp_ext_map = {
      {FLAGS_audio_level, kRtpExtensionAudioLevel},
      {FLAGS_abs_send_time, kRtpExtensionAbsoluteSendTime}};

  const std::string input_file_name = argv[1];
  std::unique_ptr<NetEqInput> input;
  if (RtpFileSource::ValidRtpDump(input_file_name) ||
      RtpFileSource::ValidPcap(input_file_name)) {
    input.reset(new NetEqRtpDumpInput(input_file_name, rtp_ext_map));
  } else {
    input.reset(new NetEqEventLogInput(input_file_name, rtp_ext_map));
  }

  std::cout << "Input file: " << input_file_name << std::endl;
  RTC_CHECK(input) << "Cannot open input file";
  RTC_CHECK(!input->ended()) << "Input file is empty";

  // Check if an SSRC value was provided.
  if (!FLAGS_ssrc.empty()) {
    uint32_t ssrc;
    RTC_CHECK(ParseSsrc(FLAGS_ssrc, &ssrc)) << "Flag verification has failed.";
    input.reset(new FilterSsrcInput(std::move(input), ssrc));
  }

  // Check the sample rate.
  rtc::Optional<int> sample_rate_hz;
  std::set<std::pair<int, uint32_t>> discarded_pt_and_ssrc;
  while (input->NextHeader()) {
    rtc::Optional<RTPHeader> first_rtp_header = input->NextHeader();
    RTC_DCHECK(first_rtp_header);
    sample_rate_hz = CodecSampleRate(first_rtp_header->payloadType);
    if (sample_rate_hz) {
      std::cout << "Found valid packet with payload type "
                << static_cast<int>(first_rtp_header->payloadType)
                << " and SSRC 0x" << std::hex << first_rtp_header->ssrc
                << std::dec << std::endl;
      break;
    }
    // Discard this packet and move to the next. Keep track of discarded payload
    // types and SSRCs.
    discarded_pt_and_ssrc.emplace(first_rtp_header->payloadType,
                                  first_rtp_header->ssrc);
    input->PopPacket();
  }
  if (!discarded_pt_and_ssrc.empty()) {
    std::cout << "Discarded initial packets with the following payload types "
                 "and SSRCs:"
              << std::endl;
    for (const auto& d : discarded_pt_and_ssrc) {
      std::cout << "PT " << d.first << "; SSRC 0x" << std::hex
                << static_cast<int>(d.second) << std::dec << std::endl;
    }
  }
  if (!sample_rate_hz) {
    std::cout << "Cannot find any packets with known payload types"
              << std::endl;
    RTC_NOTREACHED();
  }

  // Open the output file now that we know the sample rate. (Rate is only needed
  // for wav files.)
  const std::string output_file_name = argv[2];
  std::unique_ptr<AudioSink> output;
  if (output_file_name.size() >= 4 &&
      output_file_name.substr(output_file_name.size() - 4) == ".wav") {
    // Open a wav file.
    output.reset(new OutputWavFile(output_file_name, *sample_rate_hz));
  } else {
    // Open a pcm file.
    output.reset(new OutputAudioFile(output_file_name));
  }

  std::cout << "Output file: " << output_file_name << std::endl;

  NetEqTest::DecoderMap codecs = {
      {FLAGS_pcmu, std::make_pair(NetEqDecoder::kDecoderPCMu, "pcmu")},
      {FLAGS_pcma, std::make_pair(NetEqDecoder::kDecoderPCMa, "pcma")},
      {FLAGS_ilbc, std::make_pair(NetEqDecoder::kDecoderILBC, "ilbc")},
      {FLAGS_isac, std::make_pair(NetEqDecoder::kDecoderISAC, "isac")},
      {FLAGS_isac_swb,
       std::make_pair(NetEqDecoder::kDecoderISACswb, "isac-swb")},
      {FLAGS_opus, std::make_pair(NetEqDecoder::kDecoderOpus, "opus")},
      {FLAGS_pcm16b, std::make_pair(NetEqDecoder::kDecoderPCM16B, "pcm16-nb")},
      {FLAGS_pcm16b_wb,
       std::make_pair(NetEqDecoder::kDecoderPCM16Bwb, "pcm16-wb")},
      {FLAGS_pcm16b_swb32,
       std::make_pair(NetEqDecoder::kDecoderPCM16Bswb32kHz, "pcm16-swb32")},
      {FLAGS_pcm16b_swb48,
       std::make_pair(NetEqDecoder::kDecoderPCM16Bswb48kHz, "pcm16-swb48")},
      {FLAGS_g722, std::make_pair(NetEqDecoder::kDecoderG722, "g722")},
      {FLAGS_avt, std::make_pair(NetEqDecoder::kDecoderAVT, "avt")},
      {FLAGS_avt_16, std::make_pair(NetEqDecoder::kDecoderAVT16kHz, "avt-16")},
      {FLAGS_avt_32,
       std::make_pair(NetEqDecoder::kDecoderAVT32kHz, "avt-32")},
      {FLAGS_avt_48,
       std::make_pair(NetEqDecoder::kDecoderAVT48kHz, "avt-48")},
      {FLAGS_red, std::make_pair(NetEqDecoder::kDecoderRED, "red")},
      {FLAGS_cn_nb, std::make_pair(NetEqDecoder::kDecoderCNGnb, "cng-nb")},
      {FLAGS_cn_wb, std::make_pair(NetEqDecoder::kDecoderCNGwb, "cng-wb")},
      {FLAGS_cn_swb32,
       std::make_pair(NetEqDecoder::kDecoderCNGswb32kHz, "cng-swb32")},
      {FLAGS_cn_swb48,
       std::make_pair(NetEqDecoder::kDecoderCNGswb48kHz, "cng-swb48")}};

  // Check if a replacement audio file was provided.
  std::unique_ptr<AudioDecoder> replacement_decoder;
  NetEqTest::ExtDecoderMap ext_codecs;
  if (!FLAGS_replacement_audio_file.empty()) {
    // Find largest unused payload type.
    int replacement_pt = 127;
    while (!(codecs.find(replacement_pt) == codecs.end() &&
             ext_codecs.find(replacement_pt) == ext_codecs.end())) {
      --replacement_pt;
      RTC_CHECK_GE(replacement_pt, 0);
    }

    auto std_set_int32_to_uint8 = [](const std::set<int32_t>& a) {
      std::set<uint8_t> b;
      for (auto& x : a) {
        b.insert(static_cast<uint8_t>(x));
      }
      return b;
    };

    std::set<uint8_t> cn_types = std_set_int32_to_uint8(
        {FLAGS_cn_nb, FLAGS_cn_wb, FLAGS_cn_swb32, FLAGS_cn_swb48});
    std::set<uint8_t> forbidden_types =
        std_set_int32_to_uint8({FLAGS_g722, FLAGS_red, FLAGS_avt,
                                FLAGS_avt_16, FLAGS_avt_32, FLAGS_avt_48});
    input.reset(new NetEqReplacementInput(std::move(input), replacement_pt,
                                          cn_types, forbidden_types));

    replacement_decoder.reset(new FakeDecodeFromFile(
        std::unique_ptr<InputAudioFile>(
            new InputAudioFile(FLAGS_replacement_audio_file)),
        48000, false));
    NetEqTest::ExternalDecoderInfo ext_dec_info = {
        replacement_decoder.get(), NetEqDecoder::kDecoderArbitrary,
        "replacement codec"};
    ext_codecs[replacement_pt] = ext_dec_info;
  }

  NetEqTest::Callbacks callbacks;
  std::unique_ptr<NetEqDelayAnalyzer> delay_analyzer;
  if (FLAGS_matlabplot) {
    delay_analyzer.reset(new NetEqDelayAnalyzer);
  }

  SsrcSwitchDetector ssrc_switch_detector(delay_analyzer.get());
  callbacks.post_insert_packet = &ssrc_switch_detector;
  StatsGetter stats_getter(delay_analyzer.get());
  callbacks.get_audio_callback = &stats_getter;
  NetEq::Config config;
  config.sample_rate_hz = *sample_rate_hz;
  NetEqTest test(config, codecs, ext_codecs, std::move(input),
                 std::move(output), callbacks);

  int64_t test_duration_ms = test.Run();

  if (FLAGS_matlabplot) {
    auto matlab_script_name = output_file_name;
    std::replace(matlab_script_name.begin(), matlab_script_name.end(), '.',
                 '_');
    std::cout << "Creating Matlab plot script " << matlab_script_name + ".m"
              << std::endl;
    delay_analyzer->CreateMatlabScript(matlab_script_name + ".m");
  }

  printf("Simulation statistics:\n");
  printf("  output duration: %" PRId64 " ms\n", test_duration_ms);
  auto stats = stats_getter.AverageStats();
  printf("  packet_loss_rate: %f %%\n", 100.0 * stats.packet_loss_rate);
  printf("  expand_rate: %f %%\n", 100.0 * stats.expand_rate);
  printf("  speech_expand_rate: %f %%\n", 100.0 * stats.speech_expand_rate);
  printf("  preemptive_rate: %f %%\n", 100.0 * stats.preemptive_rate);
  printf("  accelerate_rate: %f %%\n", 100.0 * stats.accelerate_rate);
  printf("  secondary_decoded_rate: %f %%\n",
         100.0 * stats.secondary_decoded_rate);
  printf("  secondary_discarded_rate: %f %%\n",
         100.0 * stats.secondary_discarded_rate);
  printf("  clockdrift_ppm: %f ppm\n", stats.clockdrift_ppm);
  printf("  mean_waiting_time_ms: %f ms\n", stats.mean_waiting_time_ms);
  printf("  median_waiting_time_ms: %f ms\n", stats.median_waiting_time_ms);
  printf("  min_waiting_time_ms: %f ms\n", stats.min_waiting_time_ms);
  printf("  max_waiting_time_ms: %f ms\n", stats.max_waiting_time_ms);

  return 0;
}

}  // namespace
}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  webrtc::test::RunTest(argc, argv);
}
