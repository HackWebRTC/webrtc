/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdio.h>
#include <vector>

#include "google/gflags.h"
#include "webrtc/modules/audio_coding/neteq4/test/NETEQTEST_DummyRTPpacket.h"
#include "webrtc/modules/audio_coding/neteq4/test/NETEQTEST_RTPpacket.h"

// Flag validator.
static bool ValidatePayloadType(const char* flagname, int32_t value) {
  if (value >= 0 && value <= 127)  // Value is ok.
    return true;
  printf("Invalid value for --%s: %d\n", flagname, static_cast<int>(value));
  return false;
}

// Define command line flags.
DEFINE_int32(red, 117, "RTP payload type for RED");
static const bool pcmu_dummy =
    google::RegisterFlagValidator(&FLAGS_red, &ValidatePayloadType);

int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Tool for parsing an RTP dump file to text output.\n"
      "Run " +
      program_name +
      " --helpshort for usage.\n"
      "Example usage:\n" +
      program_name + " input.rtp output.txt\n\n" +
      "Output is sent to stdout if no output file is given." +
      "Note that this tool can read files with our without payloads.";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (argc != 2 && argc != 3) {
    // Print usage information.
    printf("%s", google::ProgramUsage());
    return 0;
  }

  FILE* in_file = fopen(argv[1], "rb");
  if (!in_file) {
    printf("Cannot open input file %s\n", argv[1]);
    return -1;
  }
  printf("Input file: %s\n", argv[1]);

  FILE* out_file;
  if (argc == 3) {
    out_file = fopen(argv[2], "wt");
    if (!out_file) {
      printf("Cannot open output file %s\n", argv[2]);
      return -1;
    }
    printf("Output file: %s\n\n", argv[2]);
  } else {
    out_file = stdout;
  }

  // Print file header.
  fprintf(out_file, "SeqNo  TimeStamp   SendTime  Size    PT  M       SSRC\n");

  // Read file header.
  NETEQTEST_RTPpacket::skipFileHeader(in_file);
  NETEQTEST_RTPpacket packet;

  while (packet.readFromFile(in_file) >= 0) {
    // Write packet data to file.
    fprintf(out_file,
            "%5u %10u %10u %5i %5i %2i %#08X\n",
            packet.sequenceNumber(),
            packet.timeStamp(),
            packet.time(),
            packet.dataLen(),
            packet.payloadType(),
            packet.markerBit(),
            packet.SSRC());
    if (packet.payloadType() == FLAGS_red) {
      webrtc::WebRtcRTPHeader red_header;
      int len;
      int red_index = 0;
      while ((len = packet.extractRED(red_index++, red_header)) >= 0) {
        fprintf(out_file,
                "* %5u %10u %10u %5i %5i\n",
                red_header.header.sequenceNumber,
                red_header.header.timestamp,
                packet.time(),
                len,
                red_header.header.payloadType);
      }
      assert(red_index > 1);  // We must get at least one payload.
    }
  }

  fclose(in_file);
  fclose(out_file);

  return 0;
}
