/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <sstream>

#include "webrtc/modules/remote_bitrate_estimator/tools/bwe_rtp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/video_coding/main/test/rtp_file_reader.h"
#include "webrtc/modules/video_coding/main/test/rtp_player.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using webrtc::rtpplayer::RtpPacketSourceInterface;

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "Usage: rtp_to_text <extension type> <extension id>"
           " <input_file.rtp> [-t]\n");
    fprintf(stderr, "<extension type> can either be:\n"
           "  abs for absolute send time or\n"
           "  tsoffset for timestamp offset.\n"
           "<extension id> is the id associated with the extension.\n"
           "  -t is an optional flag, if set only packet arrival time will be"
           " output.\n");
    return -1;
  }
  RtpPacketSourceInterface* reader;
  webrtc::RtpHeaderParser* parser;
  if (!ParseArgsAndSetupEstimator(argc, argv, NULL, NULL, &reader, &parser,
                                  NULL, NULL)) {
    return -1;
  }
  bool arrival_time_only = (argc >= 5 && strncmp(argv[4], "-t", 2) == 0);
  webrtc::scoped_ptr<RtpPacketSourceInterface> rtp_reader(reader);
  webrtc::scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(parser);
  fprintf(stdout, "seqnum timestamp ts_offset abs_sendtime recvtime "
          "markerbit ssrc size\n");
  int packet_counter = 0;
  static const uint32_t kMaxPacketSize = 1500;
  uint8_t packet_buffer[kMaxPacketSize];
  uint8_t* packet = packet_buffer;
  uint32_t packet_length = kMaxPacketSize;
  uint32_t time_ms = 0;
  int non_zero_abs_send_time = 0;
  int non_zero_ts_offsets = 0;
  while (rtp_reader->NextPacket(packet, &packet_length, &time_ms) == 0) {
    webrtc::RTPHeader header;
    parser->Parse(packet, packet_length, &header);
    if (header.extension.absoluteSendTime != 0)
      ++non_zero_abs_send_time;
    if (header.extension.transmissionTimeOffset != 0)
      ++non_zero_ts_offsets;
    if (arrival_time_only) {
      std::stringstream ss;
      ss << static_cast<int64_t>(time_ms) * 1000000;
      fprintf(stdout, "%s\n", ss.str().c_str());
    } else {
      fprintf(stdout, "%u %u %d %u %u %d %u %u\n", header.sequenceNumber,
              header.timestamp, header.extension.transmissionTimeOffset,
              header.extension.absoluteSendTime, time_ms, header.markerBit,
              header.ssrc, packet_length);
    }
    packet_length = kMaxPacketSize;
    ++packet_counter;
  }
  fprintf(stderr, "Parsed %d packets\n", packet_counter);
  fprintf(stderr, "Packets with non-zero absolute send time: %d\n",
         non_zero_abs_send_time);
  fprintf(stderr, "Packets with non-zero timestamp offset: %d\n",
         non_zero_ts_offsets);
  return 0;
}
