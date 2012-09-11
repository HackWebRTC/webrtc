/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_STREAM_SYNCHRONIZATION_H_
#define WEBRTC_VIDEO_ENGINE_STREAM_SYNCHRONIZATION_H_

#include <list>

#include "typedefs.h"  // NOLINT

namespace webrtc {

namespace synchronization {
struct RtcpMeasurement {
  RtcpMeasurement();
  RtcpMeasurement(uint32_t ntp_secs, uint32_t ntp_frac, uint32_t timestamp);
  uint32_t ntp_secs;
  uint32_t ntp_frac;
  uint32_t rtp_timestamp;
};

typedef std::list<RtcpMeasurement> RtcpList;

// Converts an RTP timestamp to the NTP domain in milliseconds using two
// (RTP timestamp, NTP timestamp) pairs.
bool RtpToNtpMs(int64_t rtp_timestamp, const RtcpList& rtcp,
                int64_t* timestamp_in_ms);

// Returns 1 there has been a forward wrap around, 0 if there has been no wrap
// around and -1 if there has been a backwards wrap around (i.e. reordering).
int CheckForWrapArounds(uint32_t rtp_timestamp, uint32_t rtcp_rtp_timestamp);
}  // namespace synchronization

struct ViESyncDelay;

class StreamSynchronization {
 public:
  struct Measurements {
    Measurements() : rtcp(), latest_receive_time_ms(0), latest_timestamp(0) {}
    synchronization::RtcpList rtcp;
    int64_t latest_receive_time_ms;
    uint32_t latest_timestamp;
  };

  StreamSynchronization(int audio_channel_id, int video_channel_id);
  ~StreamSynchronization();

  bool ComputeDelays(int relative_delay_ms,
                     int current_audio_delay_ms,
                     int* extra_audio_delay_ms,
                     int* total_video_delay_target_ms);

  // On success |relative_delay| contains the number of milliseconds later video
  // is rendered relative audio. If audio is played back later than video a
  // |relative_delay| will be negative.
  static bool ComputeRelativeDelay(const Measurements& audio_measurement,
                                   const Measurements& video_measurement,
                                   int* relative_delay_ms);

 private:
  ViESyncDelay* channel_delay_;
  int audio_channel_id_;
  int video_channel_id_;
};
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_STREAM_SYNCHRONIZATION_H_
