/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>  // abs
#if _WIN32
#include <windows.h>
#endif

#include "modules/rtp_rtcp/source/overuse_detector.h"
#include "modules/rtp_rtcp/source/remote_rate_control.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "system_wrappers/interface/trace.h"

#ifdef WEBRTC_BWE_MATLAB
extern MatlabEngine eng;  // global variable defined elsewhere
#endif

#define INIT_CAPACITY_SLOPE 8.0/512.0
#define DETECTOR_THRESHOLD 25.0
#define OVER_USING_TIME_THRESHOLD 100
#define MIN_FRAME_PERIOD_HISTORY_LEN 60

namespace webrtc {
OverUseDetector::OverUseDetector()
    : firstPacket_(true),
      currentFrame_(),
      prevFrame_(),
      numOfDeltas_(0),
      slope_(INIT_CAPACITY_SLOPE),
      offset_(0),
      E_(),
      processNoise_(),
      avgNoise_(0.0),
      varNoise_(500),
      threshold_(DETECTOR_THRESHOLD),
      tsDeltaHist_(),
      prevOffset_(0.0),
      timeOverUsing_(-1),
      overUseCounter_(0),
#ifndef WEBRTC_BWE_MATLAB
      hypothesis_(kBwNormal) {
#else
      plot1_(NULL),
      plot2_(NULL),
      plot3_(NULL),
      plot4_(NULL) {
#endif
  E_[0][0] = 100;
  E_[1][1] = 1e-1;
  E_[0][1] = E_[1][0] = 0;
  processNoise_[0] = 1e-10;
  processNoise_[1] = 1e-2;
}

OverUseDetector::~OverUseDetector() {
#ifdef WEBRTC_BWE_MATLAB
  if (plot1_) {
    eng.DeletePlot(plot1_);
    plot1_ = NULL;
  }
  if (plot2_) {
    eng.DeletePlot(plot2_);
    plot2_ = NULL;
  }
  if (plot3_) {
    eng.DeletePlot(plot3_);
    plot3_ = NULL;
  }
  if (plot4_) {
    eng.DeletePlot(plot4_);
    plot4_ = NULL;
  }
#endif

  tsDeltaHist_.clear();
}

void OverUseDetector::Reset() {
  firstPacket_ = true;
  currentFrame_.size_ = 0;
  currentFrame_.completeTimeMs_ = -1;
  currentFrame_.timestamp_ = -1;
  prevFrame_.size_ = 0;
  prevFrame_.completeTimeMs_ = -1;
  prevFrame_.timestamp_ = -1;
  numOfDeltas_ = 0;
  slope_ = INIT_CAPACITY_SLOPE;
  offset_ = 0;
  E_[0][0] = 100;
  E_[1][1] = 1e-1;
  E_[0][1] = E_[1][0] = 0;
  processNoise_[0] = 1e-10;
  processNoise_[1] = 1e-2;
  avgNoise_ = 0.0;
  varNoise_ = 500;
  threshold_ = DETECTOR_THRESHOLD;
  prevOffset_ = 0.0;
  timeOverUsing_ = -1;
  overUseCounter_ = 0;
  hypothesis_ = kBwNormal;
  tsDeltaHist_.clear();
}

bool OverUseDetector::Update(const WebRtcRTPHeader& rtpHeader,
                             const WebRtc_UWord16 packetSize,
                             const WebRtc_Word64 nowMS) {
#ifdef WEBRTC_BWE_MATLAB
  // Create plots
  const WebRtc_Word64 startTimeMs = nowMS;
  if (plot1_ == NULL) {
    plot1_ = eng.NewPlot(new MatlabPlot());
    plot1_->AddLine(1000, "b.", "scatter");
  }
  if (plot2_ == NULL) {
    plot2_ = eng.NewPlot(new MatlabPlot());
    plot2_->AddTimeLine(30, "b", "offset", startTimeMs);
    plot2_->AddTimeLine(30, "r--", "limitPos", startTimeMs);
    plot2_->AddTimeLine(30, "k.", "trigger", startTimeMs);
    plot2_->AddTimeLine(30, "ko", "detection", startTimeMs);
    //  plot2_->AddTimeLine(30, "g", "slowMean", startTimeMs);
  }
  if (plot3_ == NULL) {
    plot3_ = eng.NewPlot(new MatlabPlot());
    plot3_->AddTimeLine(30, "b", "noiseVar", startTimeMs);
  }
  if (plot4_ == NULL) {
    plot4_ = eng.NewPlot(new MatlabPlot());
    //  plot4_->AddTimeLine(60, "b", "p11", startTimeMs);
    //  plot4_->AddTimeLine(60, "r", "p12", startTimeMs);
    plot4_->AddTimeLine(60, "g", "p22", startTimeMs);
    //  plot4_->AddTimeLine(60, "g--", "p22_hat", startTimeMs);
    //  plot4_->AddTimeLine(30, "b.-", "deltaFs", startTimeMs);
  }

#endif

  bool wrapped = false;
  bool completeFrame = false;
  if (currentFrame_.timestamp_ == -1) {
    currentFrame_.timestamp_ = rtpHeader.header.timestamp;
  } else if (ModuleRTPUtility::OldTimestamp(
      rtpHeader.header.timestamp,
      static_cast<WebRtc_UWord32>(currentFrame_.timestamp_),
      &wrapped)) {
    // Don't update with old data
    return completeFrame;
  } else if (rtpHeader.header.timestamp != currentFrame_.timestamp_) {
    // First packet of a later frame, the previous frame sample is ready
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
                 "Frame complete at %I64i", currentFrame_.completeTimeMs_);
    if (prevFrame_.completeTimeMs_ >= 0) {  // This is our second frame
      WebRtc_Word64 tDelta = 0;
      double tsDelta = 0;
      // Check for wrap
      ModuleRTPUtility::OldTimestamp(
          static_cast<WebRtc_UWord32>(prevFrame_.timestamp_),
          static_cast<WebRtc_UWord32>(currentFrame_.timestamp_),
          &wrapped);
      CompensatedTimeDelta(currentFrame_, prevFrame_, tDelta, tsDelta,
                           wrapped);
      UpdateKalman(tDelta, tsDelta, currentFrame_.size_,
                   prevFrame_.size_);
    }
    // The new timestamp is now the current frame,
    // and the old timestamp becomes the previous frame.
    prevFrame_ = currentFrame_;
    currentFrame_.timestamp_ = rtpHeader.header.timestamp;
    currentFrame_.size_ = 0;
    currentFrame_.completeTimeMs_ = -1;
    completeFrame = true;
  }
  // Accumulate the frame size
  currentFrame_.size_ += packetSize;
  currentFrame_.completeTimeMs_ = nowMS;
  return completeFrame;
}

BandwidthUsage OverUseDetector::State() const {
  return hypothesis_;
}

double OverUseDetector::NoiseVar() const {
  return varNoise_;
}

void OverUseDetector::SetRateControlRegion(RateControlRegion region) {
  switch (region) {
    case kRcMaxUnknown: {
      threshold_ = DETECTOR_THRESHOLD;
      break;
    }
    case kRcAboveMax:
    case kRcNearMax: {
      threshold_ = DETECTOR_THRESHOLD / 2;
      break;
    }
  }
}

void OverUseDetector::CompensatedTimeDelta(const FrameSample& currentFrame,
                                           const FrameSample& prevFrame,
                                           WebRtc_Word64& tDelta,
                                           double& tsDelta,
                                           bool wrapped) {
  numOfDeltas_++;
  if (numOfDeltas_ > 1000) {
    numOfDeltas_ = 1000;
  }
  // Add wrap-around compensation
  WebRtc_Word64 wrapCompensation = 0;
  if (wrapped) {
    wrapCompensation = static_cast<WebRtc_Word64>(1)<<32;
  }
  tsDelta = (currentFrame.timestamp_
             + wrapCompensation
             - prevFrame.timestamp_) / 90.0;
  tDelta = currentFrame.completeTimeMs_ - prevFrame.completeTimeMs_;
  assert(tsDelta > 0);
}

double OverUseDetector::CurrentDrift() {
  return 1.0;
}

void OverUseDetector::UpdateKalman(WebRtc_Word64 tDelta,
                                   double tsDelta,
                                   WebRtc_UWord32 frameSize,
                                   WebRtc_UWord32 prevFrameSize) {
  const double minFramePeriod = UpdateMinFramePeriod(tsDelta);
  const double drift = CurrentDrift();
  // Compensate for drift
  const double tTsDelta = tDelta - tsDelta / drift;
  double fsDelta = static_cast<double>(frameSize) - prevFrameSize;

  // Update the Kalman filter
  const double scaleFactor =  minFramePeriod / (1000.0 / 30.0);
  E_[0][0] += processNoise_[0] * scaleFactor;
  E_[1][1] += processNoise_[1] * scaleFactor;

  if ((hypothesis_ == kBwOverusing && offset_ < prevOffset_) ||
      (hypothesis_ == kBwUnderUsing && offset_ > prevOffset_)) {
    E_[1][1] += 10 * processNoise_[1] * scaleFactor;
  }

  const double h[2] = {fsDelta, 1.0};
  const double Eh[2] = {E_[0][0]*h[0] + E_[0][1]*h[1],
                        E_[1][0]*h[0] + E_[1][1]*h[1]};

  const double residual = tTsDelta - slope_*h[0] - offset_;

  const bool stableState =
      (BWE_MIN(numOfDeltas_, 60) * abs(offset_) < threshold_);
  // We try to filter out very late frames. For instance periodic key
  // frames doesn't fit the Gaussian model well.
  if (abs(residual) < 3 * sqrt(varNoise_)) {
    UpdateNoiseEstimate(residual, minFramePeriod, stableState);
  } else {
    UpdateNoiseEstimate(3 * sqrt(varNoise_), minFramePeriod, stableState);
  }

  const double denom = varNoise_ + h[0]*Eh[0] + h[1]*Eh[1];

  const double K[2] = {Eh[0] / denom,
                       Eh[1] / denom};

  const double IKh[2][2] = {{1.0 - K[0]*h[0], -K[0]*h[1]},
                            {-K[1]*h[0], 1.0 - K[1]*h[1]}};
  const double e00 = E_[0][0];
  const double e01 = E_[0][1];

  // Update state
  E_[0][0] = e00 * IKh[0][0] + E_[1][0] * IKh[0][1];
  E_[0][1] = e01 * IKh[0][0] + E_[1][1] * IKh[0][1];
  E_[1][0] = e00 * IKh[1][0] + E_[1][0] * IKh[1][1];
  E_[1][1] = e01 * IKh[1][0] + E_[1][1] * IKh[1][1];

  // Covariance matrix, must be positive semi-definite
  assert(E_[0][0] + E_[1][1] >= 0 &&
         E_[0][0] * E_[1][1] - E_[0][1] * E_[1][0] >= 0 &&
         E_[0][0] >= 0);

#ifdef WEBRTC_BWE_MATLAB
  // plot4_->Append("p11",E_[0][0]);
  // plot4_->Append("p12",E_[0][1]);
  plot4_->Append("p22", E_[1][1]);
  // plot4_->Append("p22_hat", 0.5*(processNoise_[1] +
  //    sqrt(processNoise_[1]*(processNoise_[1] + 4*varNoise_))));
  // plot4_->Append("deltaFs", fsDelta);
  plot4_->Plot();
#endif
  slope_ = slope_ + K[0] * residual;
  prevOffset_ = offset_;
  offset_ = offset_ + K[1] * residual;

  Detect(tsDelta);

#ifdef WEBRTC_BWE_MATLAB
  plot1_->Append("scatter",
                 static_cast<double>(currentFrame_.size_) - prevFrame_.size_,
                 static_cast<double>(tDelta-tsDelta));
  plot1_->MakeTrend("scatter", "slope", slope_, offset_, "k-");
  plot1_->MakeTrend("scatter", "thresholdPos",
                    slope_, offset_ + 2 * sqrt(varNoise_), "r-");
  plot1_->MakeTrend("scatter", "thresholdNeg",
                    slope_, offset_ - 2 * sqrt(varNoise_), "r-");
  plot1_->Plot();

  plot2_->Append("offset", offset_);
  plot2_->Append("limitPos", threshold_/BWE_MIN(numOfDeltas_, 60));
  plot2_->Plot();

  plot3_->Append("noiseVar", varNoise_);
  plot3_->Plot();
#endif
}

double OverUseDetector::UpdateMinFramePeriod(double tsDelta) {
  double minFramePeriod = tsDelta;
  if (tsDeltaHist_.size() >= MIN_FRAME_PERIOD_HISTORY_LEN) {
    std::list<double>::iterator firstItem = tsDeltaHist_.begin();
    tsDeltaHist_.erase(firstItem);
  }
  std::list<double>::iterator it = tsDeltaHist_.begin();
  for (; it != tsDeltaHist_.end(); it++) {
    minFramePeriod = BWE_MIN(*it, minFramePeriod);
  }
  tsDeltaHist_.push_back(tsDelta);
  return minFramePeriod;
}

void OverUseDetector::UpdateNoiseEstimate(double residual,
                                          double tsDelta,
                                          bool stableState) {
  if (!stableState) {
    return;
  }
  // Faster filter during startup to faster adapt to the jitter level
  // of the network alpha is tuned for 30 frames per second, but
  double alpha = 0.01;
  if (numOfDeltas_ > 10*30) {
    alpha = 0.002;
  }
  // Only update the noise estimate if we're not over-using
  // beta is a function of alpha and the time delta since
  // the previous update.
  const double beta = pow(1 - alpha, tsDelta * 30.0 / 1000.0);
  avgNoise_ = beta * avgNoise_
              + (1 - beta) * residual;
  varNoise_ = beta * varNoise_
              + (1 - beta) * (avgNoise_ - residual) * (avgNoise_ - residual);
  if (varNoise_ < 1e-7) {
    varNoise_ = 1e-7;
  }
}

BandwidthUsage OverUseDetector::Detect(double tsDelta) {
  if (numOfDeltas_ < 2) {
    return kBwNormal;
  }
  const double T = BWE_MIN(numOfDeltas_, 60) * offset_;
  if (abs(T) > threshold_) {
    if (offset_ > 0) {
      if (timeOverUsing_ == -1) {
        // Initialize the timer. Assume that we've been
        // over-using half of the time since the previous
        // sample.
        timeOverUsing_ = tsDelta / 2;
      } else {
        // Increment timer
        timeOverUsing_ += tsDelta;
      }
      overUseCounter_++;
      if (timeOverUsing_ > OVER_USING_TIME_THRESHOLD
          && overUseCounter_ > 1) {
        if (offset_ >= prevOffset_) {
#ifdef _DEBUG
          if (hypothesis_ != kBwOverusing) {
            WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, "BWE: kBwOverusing");
          }
#endif
          timeOverUsing_ = 0;
          overUseCounter_ = 0;
          hypothesis_ = kBwOverusing;
#ifdef WEBRTC_BWE_MATLAB
          plot2_->Append("detection", offset_);  // plot it later
#endif
        }
      }
#ifdef WEBRTC_BWE_MATLAB
      plot2_->Append("trigger", offset_);  // plot it later
#endif
    } else {
#ifdef _DEBUG
      if (hypothesis_ != kBwUnderUsing) {
        WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, "BWE: kBwUnderUsing");
      }
#endif
      timeOverUsing_ = -1;
      overUseCounter_ = 0;
      hypothesis_ = kBwUnderUsing;
    }
  } else {
#ifdef _DEBUG
    if (hypothesis_ != kBwNormal) {
      WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, "BWE: kBwNormal");
    }
#endif
    timeOverUsing_ = -1;
    overUseCounter_ = 0;
    hypothesis_ = kBwNormal;
  }
  return hypothesis_;
}

}  // namespace webrtc
