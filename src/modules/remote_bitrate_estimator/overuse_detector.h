/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_OVERUSE_DETECTOR_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_OVERUSE_DETECTOR_H_

#include <list>

#include "modules/interface/module_common_types.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "typedefs.h"  // NOLINT(build/include)

#ifdef WEBRTC_BWE_MATLAB
#include "../test/BWEStandAlone/MatlabPlot.h"
#endif

namespace webrtc {
enum RateControlRegion;

class OverUseDetector {
 public:
  OverUseDetector();
  ~OverUseDetector();
  void Update(const WebRtc_UWord16 packetSize,
              const WebRtc_UWord32 timestamp,
              const WebRtc_Word64 nowMS);
  BandwidthUsage State() const;
  void Reset();
  double NoiseVar() const;
  void SetRateControlRegion(RateControlRegion region);

 private:
  struct FrameSample {
    FrameSample() : size_(0), completeTimeMs_(-1), timestamp_(-1) {}

    WebRtc_UWord32 size_;
    WebRtc_Word64  completeTimeMs_;
    WebRtc_Word64  timestamp_;
  };

  static bool OldTimestamp(uint32_t newTimestamp,
                           uint32_t existingTimestamp,
                           bool* wrapped);

  void CompensatedTimeDelta(const FrameSample& currentFrame,
                            const FrameSample& prevFrame,
                            WebRtc_Word64& tDelta,
                            double& tsDelta,
                            bool wrapped);
  void UpdateKalman(WebRtc_Word64 tDelta,
                    double tsDelta,
                    WebRtc_UWord32 frameSize,
                    WebRtc_UWord32 prevFrameSize);
  double UpdateMinFramePeriod(double tsDelta);
  void UpdateNoiseEstimate(double residual, double tsDelta, bool stableState);
  BandwidthUsage Detect(double tsDelta);
  double CurrentDrift();

  bool firstPacket_;
  FrameSample currentFrame_;
  FrameSample prevFrame_;
  WebRtc_UWord16 numOfDeltas_;
  double slope_;
  double offset_;
  double E_[2][2];
  double processNoise_[2];
  double avgNoise_;
  double varNoise_;
  double threshold_;
  std::list<double> tsDeltaHist_;
  double prevOffset_;
  double timeOverUsing_;
  WebRtc_UWord16 overUseCounter_;
  BandwidthUsage hypothesis_;

#ifdef WEBRTC_BWE_MATLAB
  MatlabPlot* plot1_;
  MatlabPlot* plot2_;
  MatlabPlot* plot3_;
  MatlabPlot* plot4_;
#endif
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_OVERUSE_DETECTOR_H_
