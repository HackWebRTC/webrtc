/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_SEND_STATISTICS_PROXY_H_
#define WEBRTC_VIDEO_SEND_STATISTICS_PROXY_H_

#include <string>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/ratetracker.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/video_engine/overuse_frame_detector.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class SendStatisticsProxy : public CpuOveruseMetricsObserver,
                            public RtcpStatisticsCallback,
                            public RtcpPacketTypeCounterObserver,
                            public StreamDataCountersCallback,
                            public BitrateStatisticsObserver,
                            public FrameCountObserver,
                            public VideoEncoderRateObserver,
                            public SendSideDelayObserver {
 public:
  static const int kStatsTimeoutMs;

  SendStatisticsProxy(Clock* clock, const VideoSendStream::Config& config);
  virtual ~SendStatisticsProxy();

  VideoSendStream::Stats GetStats();

  virtual void OnSendEncodedImage(const EncodedImage& encoded_image,
                                  const RTPVideoHeader* rtp_video_header);
  // Used to update incoming frame rate.
  void OnIncomingFrame(int width, int height);

  // Used to update encode time of frames.
  void OnEncodedFrame(int encode_time_ms);

  // From VideoEncoderRateObserver.
  void OnSetRates(uint32_t bitrate_bps, int framerate) override;

  void OnOutgoingRate(uint32_t framerate, uint32_t bitrate);
  void OnSuspendChange(bool is_suspended);
  void OnInactiveSsrc(uint32_t ssrc);

 protected:
  // From CpuOveruseMetricsObserver.
  void CpuOveruseMetricsUpdated(const CpuOveruseMetrics& metrics) override;
  // From RtcpStatisticsCallback.
  void StatisticsUpdated(const RtcpStatistics& statistics,
                         uint32_t ssrc) override;
  void CNameChanged(const char* cname, uint32_t ssrc) override;
  // From RtcpPacketTypeCounterObserver.
  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override;
  // From StreamDataCountersCallback.
  void DataCountersUpdated(const StreamDataCounters& counters,
                           uint32_t ssrc) override;

  // From BitrateStatisticsObserver.
  void Notify(const BitrateStatistics& total_stats,
              const BitrateStatistics& retransmit_stats,
              uint32_t ssrc) override;

  // From FrameCountObserver.
  void FrameCountUpdated(const FrameCounts& frame_counts,
                         uint32_t ssrc) override;

  void SendSideDelayUpdated(int avg_delay_ms,
                            int max_delay_ms,
                            uint32_t ssrc) override;

 private:
  class SampleCounter {
   public:
    SampleCounter() : sum(0), num_samples(0) {}
    ~SampleCounter() {}
    void Add(int sample);
    int Avg(int min_required_samples) const;

   private:
    int sum;
    int num_samples;
  };
  class BoolSampleCounter {
   public:
    BoolSampleCounter() : sum(0), num_samples(0) {}
    ~BoolSampleCounter() {}
    void Add(bool sample);
    int Percent(int min_required_samples) const;
    int Permille(int min_required_samples) const;

   private:
    int Fraction(int min_required_samples, float multiplier) const;
    int sum;
    int num_samples;
  };
  struct StatsUpdateTimes {
    StatsUpdateTimes() : resolution_update_ms(0) {}
    int64_t resolution_update_ms;
    int64_t bitrate_update_ms;
  };
  void PurgeOldStats() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  VideoSendStream::StreamStats* GetStatsEntry(uint32_t ssrc)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);
  void UpdateHistograms() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  Clock* const clock_;
  const VideoSendStream::Config config_;
  mutable rtc::CriticalSection crit_;
  VideoSendStream::Stats stats_ GUARDED_BY(crit_);
  rtc::RateTracker input_frame_rate_tracker_ GUARDED_BY(crit_);
  rtc::RateTracker sent_frame_rate_tracker_ GUARDED_BY(crit_);
  uint32_t last_sent_frame_timestamp_ GUARDED_BY(crit_);
  std::map<uint32_t, StatsUpdateTimes> update_times_ GUARDED_BY(crit_);

  int max_sent_width_per_timestamp_ GUARDED_BY(crit_);
  int max_sent_height_per_timestamp_ GUARDED_BY(crit_);
  SampleCounter input_width_counter_ GUARDED_BY(crit_);
  SampleCounter input_height_counter_ GUARDED_BY(crit_);
  SampleCounter sent_width_counter_ GUARDED_BY(crit_);
  SampleCounter sent_height_counter_ GUARDED_BY(crit_);
  SampleCounter encode_time_counter_ GUARDED_BY(crit_);
  BoolSampleCounter key_frame_counter_ GUARDED_BY(crit_);
  BoolSampleCounter quality_limited_frame_counter_ GUARDED_BY(crit_);
  SampleCounter quality_downscales_counter_ GUARDED_BY(crit_);
  BoolSampleCounter bw_limited_frame_counter_ GUARDED_BY(crit_);
  SampleCounter bw_resolutions_disabled_counter_ GUARDED_BY(crit_);
  SampleCounter delay_counter_ GUARDED_BY(crit_);
  SampleCounter max_delay_counter_ GUARDED_BY(crit_);
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_SEND_STATISTICS_PROXY_H_
