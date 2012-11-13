/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"

#include "system_wrappers/interface/tick_util.h"

namespace webrtc {

RemoteBitrateEstimatorSingleStream::RemoteBitrateEstimatorSingleStream(
    RemoteBitrateObserver* observer, const OverUseDetectorOptions& options)
    : options_(options),
      observer_(observer),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()) {
  assert(observer_);
}

void RemoteBitrateEstimatorSingleStream::IncomingPacket(
    unsigned int ssrc,
    int payload_size,
    int64_t arrival_time,
    uint32_t rtp_timestamp) {
  CriticalSectionScoped cs(crit_sect_.get());
  SsrcOveruseDetectorMap::iterator it = overuse_detectors_.find(ssrc);
  if (it == overuse_detectors_.end()) {
    // This is a new SSRC. Adding to map.
    // TODO(holmer): If the channel changes SSRC the old SSRC will still be
    // around in this map until the channel is deleted. This is OK since the
    // callback will no longer be called for the old SSRC. This will be
    // automatically cleaned up when we have one RemoteBitrateEstimator per REMB
    // group.
    std::pair<SsrcOveruseDetectorMap::iterator, bool> insert_result =
        overuse_detectors_.insert(std::make_pair(ssrc, OveruseDetector(
            options_)));
    it = insert_result.first;
  }
  OveruseDetector* overuse_detector = &it->second;
  incoming_bitrate_.Update(payload_size, arrival_time);
  const BandwidthUsage prior_state = overuse_detector->State();
  overuse_detector->Update(payload_size, -1, rtp_timestamp, arrival_time);
  if (prior_state != overuse_detector->State() &&
      overuse_detector->State() == kBwOverusing) {
    // The first overuse should immediately trigger a new estimate.
    UpdateEstimate(ssrc, arrival_time);
  }
}

void RemoteBitrateEstimatorSingleStream::UpdateEstimate(unsigned int ssrc,
                                                        int64_t time_now) {
  CriticalSectionScoped cs(crit_sect_.get());
  SsrcOveruseDetectorMap::iterator it = overuse_detectors_.find(ssrc);
  if (it == overuse_detectors_.end()) {
    return;
  }
  OveruseDetector* overuse_detector = &it->second;
  const RateControlInput input(overuse_detector->State(),
                               incoming_bitrate_.BitRate(time_now),
                               overuse_detector->NoiseVar());
  const RateControlRegion region = remote_rate_.Update(&input, time_now);
  unsigned int target_bitrate = remote_rate_.UpdateBandwidthEstimate(time_now);
  if (remote_rate_.ValidEstimate()) {
    observer_->OnReceiveBitrateChanged(target_bitrate);
  }
  overuse_detector->SetRateControlRegion(region);
}

void RemoteBitrateEstimatorSingleStream::SetRtt(unsigned int rtt) {
  CriticalSectionScoped cs(crit_sect_.get());
  remote_rate_.SetRtt(rtt);
}

void RemoteBitrateEstimatorSingleStream::RemoveStream(unsigned int ssrc) {
  CriticalSectionScoped cs(crit_sect_.get());
  // Ignoring the return value which is the number of elements erased.
  overuse_detectors_.erase(ssrc);
}

bool RemoteBitrateEstimatorSingleStream::LatestEstimate(
    unsigned int ssrc, unsigned int* bitrate_bps) const {
  CriticalSectionScoped cs(crit_sect_.get());
  assert(bitrate_bps != NULL);
  if (!remote_rate_.ValidEstimate()) {
    return false;
  }
  // TODO(holmer): For now we're returning the estimate bandwidth per stream as
  // it corresponds better to how the ViE API is designed. Will fix this when
  // the API changes.
  if (overuse_detectors_.size() > 0)
    *bitrate_bps = remote_rate_.LatestEstimate() / overuse_detectors_.size();
  else
    *bitrate_bps = 0;
  return true;
}

}  // namespace webrtc
