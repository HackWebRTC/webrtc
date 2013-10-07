/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/video_coding/main/test/rtp_file_reader.h"
#include "webrtc/modules/video_coding/main/test/rtp_player.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using namespace webrtc::rtpplayer;

const uint32_t kMaxPacketSize = 1500;
const int kDefaultTransmissionTimeOffsetExtensionId = 2;

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Usage: rtp_to_text <input_file.rtp> <output_file.rtp>\n");
    return -1;
  }
  webrtc::scoped_ptr<RtpPacketSourceInterface> rtp_reader(
      CreateRtpFileReader(argv[1]));
  if (!rtp_reader.get()) {
    printf("Cannot open input file %s\n", argv[1]);
    return -1;
  }
  uint8_t packet_buffer[kMaxPacketSize];
  uint8_t* packet = packet_buffer;
  uint32_t packet_length = kMaxPacketSize;
  uint32_t time_ms = 0;
  FILE* out_file = fopen(argv[2], "wt");
  if (!out_file) {
    printf("Cannot open output file %s\n", argv[2]);
    return -1;
  }
  printf("Input file: %s, Output file: %s\n\n", argv[1], argv[2]);
  fprintf(out_file, "seqnum timestamp ts_offset abs_sendtime recvtime "
          "markerbit ssrc size\n");
  webrtc::scoped_ptr<webrtc::RtpHeaderParser> parser(
      webrtc::RtpHeaderParser::Create());
  parser->RegisterRtpHeaderExtension(
      webrtc::kRtpExtensionTransmissionTimeOffset,
      kDefaultTransmissionTimeOffsetExtensionId);
  int packet_counter = 0;
  while (rtp_reader->NextPacket(packet, &packet_length, &time_ms) == 0) {
    webrtc::RTPHeader header;
    parser->Parse(packet, packet_length, &header);
    fprintf(out_file, "%u %u %d %u %u %d %u %u\n", header.sequenceNumber,
            header.timestamp, header.extension.transmissionTimeOffset,
            header.extension.absoluteSendTime, time_ms, header.markerBit,
            header.ssrc, packet_length);
    packet_length = kMaxPacketSize;
    ++packet_counter;
  }
  printf("Parsed %d packets\n", packet_counter);
  return 0;
}
