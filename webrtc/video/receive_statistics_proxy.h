/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_
#define WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_

#include <string>

#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/remote_bitrate_estimator/rate_statistics.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_renderer.h"

namespace webrtc {

class Clock;
class CriticalSectionWrapper;
class ViECodec;
class ViEDecoderObserver;

class ReceiveStatisticsProxy : public ViEDecoderObserver,
                               public VCMReceiveStatisticsCallback,
                               public RtcpStatisticsCallback,
                               public StreamDataCountersCallback {
 public:
  ReceiveStatisticsProxy(uint32_t ssrc, Clock* clock);
  virtual ~ReceiveStatisticsProxy();

  VideoReceiveStream::Stats GetStats() const;

  void OnDecodedFrame();
  void OnRenderedFrame();

  // Overrides VCMReceiveStatisticsCallback
  virtual void OnReceiveRatesUpdated(uint32_t bitRate,
                                     uint32_t frameRate) OVERRIDE;
  virtual void OnFrameCountsUpdated(const FrameCounts& frame_counts) OVERRIDE;
  virtual void OnDiscardedPacketsUpdated(int discarded_packets) OVERRIDE;

  // Overrides ViEDecoderObserver.
  virtual void IncomingCodecChanged(const int video_channel,
                                    const VideoCodec& video_codec) OVERRIDE {}
  virtual void IncomingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate_bps) OVERRIDE;
  virtual void DecoderTiming(int decode_ms,
                             int max_decode_ms,
                             int current_delay_ms,
                             int target_delay_ms,
                             int jitter_buffer_ms,
                             int min_playout_delay_ms,
                             int render_delay_ms) OVERRIDE;
  virtual void RequestNewKeyFrame(const int video_channel) OVERRIDE {}

  // Overrides RtcpStatisticsCallback.
  virtual void StatisticsUpdated(const webrtc::RtcpStatistics& statistics,
                                 uint32_t ssrc) OVERRIDE;
  virtual void CNameChanged(const char* cname, uint32_t ssrc) OVERRIDE;

  // Overrides StreamDataCountersCallback.
  virtual void DataCountersUpdated(const webrtc::StreamDataCounters& counters,
                                   uint32_t ssrc) OVERRIDE;

 private:
  Clock* const clock_;

  scoped_ptr<CriticalSectionWrapper> crit_;
  VideoReceiveStream::Stats stats_ GUARDED_BY(crit_);
  RateStatistics decode_fps_estimator_ GUARDED_BY(crit_);
  RateStatistics renders_fps_estimator_ GUARDED_BY(crit_);
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_RECEIVE_STATISTICS_PROXY_H_
