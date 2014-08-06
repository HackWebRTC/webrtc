/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_RTP_FILE_READER_H_
#define WEBRTC_TEST_RTP_FILE_READER_H_

#include <string>

#include "webrtc/common_types.h"

namespace webrtc {
namespace test {
class RtpFileReader {
 public:
  enum FileFormat {
    kPcap,
    kRtpDump,
  };

  struct Packet {
    static const size_t kMaxPacketBufferSize = 1500;
    uint8_t data[kMaxPacketBufferSize];
    size_t length;

    uint32_t time_ms;
  };

  virtual ~RtpFileReader() {}
  static RtpFileReader* Create(FileFormat format,
                               const std::string& filename);

  virtual bool NextPacket(Packet* packet) = 0;
};
}  // namespace test
}  // namespace webrtc
#endif  // WEBRTC_TEST_RTP_FILE_READER_H_
