/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_RECEIVE_STREAM_H_
#define WEBRTC_AUDIO_RECEIVE_STREAM_H_

#include <string>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/config.h"

namespace webrtc {

class AudioReceiveStream {
 public:
  struct Config {
    Config() {}
    std::string ToString() const;

    // Receive-stream specific RTP settings.
    struct Rtp {
      Rtp() : remote_ssrc(0) {}
      std::string ToString() const;

      // Synchronization source (stream identifier) to be received.
      uint32_t remote_ssrc;

      // RTP header extensions used for the received stream.
      std::vector<RtpExtension> extensions;
    } rtp;
  };

 protected:
  virtual ~AudioReceiveStream() {}
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_RECEIVE_STREAM_H_
