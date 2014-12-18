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

#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class CriticalSectionWrapper;

class SendStatisticsProxy : public RtcpStatisticsCallback,
                            public StreamDataCountersCallback,
                            public BitrateStatisticsObserver,
                            public FrameCountObserver,
                            public ViEEncoderObserver,
                            public ViECaptureObserver,
                            public SendSideDelayObserver {
 public:
  static const int kStatsTimeoutMs;

  SendStatisticsProxy(Clock* clock, const VideoSendStream::Config& config);
  virtual ~SendStatisticsProxy();

  VideoSendStream::Stats GetStats();

  virtual void OnSendEncodedImage(const EncodedImage& encoded_image,
                                  const RTPVideoHeader* rtp_video_header);

 protected:
  // From RtcpStatisticsCallback.
  virtual void StatisticsUpdated(const RtcpStatistics& statistics,
                                 uint32_t ssrc) OVERRIDE;
  virtual void CNameChanged(const char *cname, uint32_t ssrc) OVERRIDE;
  // From StreamDataCountersCallback.
  virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                   uint32_t ssrc) OVERRIDE;

  // From BitrateStatisticsObserver.
  virtual void Notify(const BitrateStatistics& total_stats,
                      const BitrateStatistics& retransmit_stats,
                      uint32_t ssrc) OVERRIDE;

  // From FrameCountObserver.
  virtual void FrameCountUpdated(const FrameCounts& frame_counts,
                                 uint32_t ssrc) OVERRIDE;

  // From ViEEncoderObserver.
  virtual void OutgoingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate) OVERRIDE;

  virtual void SuspendChange(int video_channel, bool is_suspended) OVERRIDE;

  // From ViECaptureObserver.
  virtual void BrightnessAlarm(const int capture_id,
                               const Brightness brightness) OVERRIDE {}

  virtual void CapturedFrameRate(const int capture_id,
                                 const unsigned char frame_rate) OVERRIDE;

  virtual void NoPictureAlarm(const int capture_id,
                              const CaptureAlarm alarm) OVERRIDE {}

  virtual void SendSideDelayUpdated(int avg_delay_ms,
                                    int max_delay_ms,
                                    uint32_t ssrc) OVERRIDE;

 private:
  struct StatsUpdateTimes {
    StatsUpdateTimes() : resolution_update_ms(0) {}
    int64_t resolution_update_ms;
  };
  void PurgeOldStats() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  SsrcStats* GetStatsEntry(uint32_t ssrc) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  Clock* const clock_;
  const VideoSendStream::Config config_;
  scoped_ptr<CriticalSectionWrapper> crit_;
  VideoSendStream::Stats stats_ GUARDED_BY(crit_);
  std::map<uint32_t, StatsUpdateTimes> update_times_ GUARDED_BY(crit_);
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_SEND_STATISTICS_PROXY_H_
