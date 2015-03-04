/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/bitrate_controller/include/bitrate_allocator.h"

#include <algorithm>
#include <utility>

#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

namespace webrtc {

BitrateAllocator::BitrateAllocator()
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      bitrate_observers_(),
      enforce_min_bitrate_(true) {
}

BitrateAllocator::~BitrateAllocator() {
  for (auto& kv : bitrate_observers_)
    delete kv.second;
}

void BitrateAllocator::OnNetworkChanged(uint32_t bitrate,
                                        uint8_t fraction_loss,
                                        int64_t rtt) {
  CriticalSectionScoped lock(crit_sect_.get());
  // Sanity check.
  if (bitrate_observers_.empty())
    return;

  uint32_t sum_min_bitrates = 0;
  BitrateObserverConfList::iterator it;
  for (auto& kv : bitrate_observers_)
    sum_min_bitrates += kv.second->min_bitrate_;
  if (bitrate <= sum_min_bitrates)
    return LowRateAllocation(bitrate, fraction_loss, rtt, sum_min_bitrates);
  else
    return NormalRateAllocation(bitrate, fraction_loss, rtt, sum_min_bitrates);
}

int BitrateAllocator::AddBitrateObserver(BitrateObserver* observer,
                                         uint32_t start_bitrate,
                                         uint32_t min_bitrate,
                                         uint32_t max_bitrate) {
  CriticalSectionScoped lock(crit_sect_.get());

  BitrateObserverConfList::iterator it =
      FindObserverConfigurationPair(observer);

  int new_bwe_candidate_bps = -1;
  if (it != bitrate_observers_.end()) {
    // Update current configuration.
    it->second->start_bitrate_ = start_bitrate;
    it->second->min_bitrate_ = min_bitrate;
    it->second->max_bitrate_ = max_bitrate;
    // Set the send-side bandwidth to the max of the sum of start bitrates and
    // the current estimate, so that if the user wants to immediately use more
    // bandwidth, that can be enforced.
    new_bwe_candidate_bps = 0;
    for (auto& kv : bitrate_observers_)
      new_bwe_candidate_bps += kv.second->start_bitrate_;
  } else {
    // Add new settings.
    bitrate_observers_.push_back(BitrateObserverConfiguration(
        observer,
        new BitrateConfiguration(start_bitrate, min_bitrate, max_bitrate)));
    bitrate_observers_modified_ = true;

    // TODO(andresp): This is a ugly way to set start bitrate.
    //
    // Only change start bitrate if we have exactly one observer. By definition
    // you can only have one start bitrate, once we have our first estimate we
    // will adapt from there.
    if (bitrate_observers_.size() == 1)
      new_bwe_candidate_bps = start_bitrate;
  }
  return new_bwe_candidate_bps;
}

void BitrateAllocator::RemoveBitrateObserver(BitrateObserver* observer) {
  CriticalSectionScoped lock(crit_sect_.get());
  BitrateObserverConfList::iterator it =
      FindObserverConfigurationPair(observer);
  if (it != bitrate_observers_.end()) {
    delete it->second;
    bitrate_observers_.erase(it);
    bitrate_observers_modified_ = true;
  }
}

void BitrateAllocator::GetMinMaxBitrateSumBps(int* min_bitrate_sum_bps,
                                              int* max_bitrate_sum_bps) const {
  *min_bitrate_sum_bps = 0;
  *max_bitrate_sum_bps = 0;

  CriticalSectionScoped lock(crit_sect_.get());
  BitrateObserverConfList::const_iterator it;
  for (it = bitrate_observers_.begin(); it != bitrate_observers_.end(); ++it) {
    *min_bitrate_sum_bps += it->second->min_bitrate_;
    *max_bitrate_sum_bps += it->second->max_bitrate_;
  }
  if (*max_bitrate_sum_bps == 0) {
    // No max configured use 1Gbit/s.
    *max_bitrate_sum_bps = 1000000000;
  }
  // TODO(holmer): Enforcing a min bitrate should be per stream, allowing some
  // streams to auto-mute while others keep sending.
  if (!enforce_min_bitrate_) {
    // If not enforcing min bitrate, allow the bandwidth estimation to
    // go as low as 10 kbps.
    *min_bitrate_sum_bps = std::min(*min_bitrate_sum_bps, 10000);
  }
}

BitrateAllocator::BitrateObserverConfList::iterator
BitrateAllocator::FindObserverConfigurationPair(
    const BitrateObserver* observer) {
  BitrateObserverConfList::iterator it = bitrate_observers_.begin();
  for (; it != bitrate_observers_.end(); ++it) {
    if (it->first == observer) {
      return it;
    }
  }
  return bitrate_observers_.end();
}

void BitrateAllocator::EnforceMinBitrate(bool enforce_min_bitrate) {
  CriticalSectionScoped lock(crit_sect_.get());
  enforce_min_bitrate_ = enforce_min_bitrate;
}

void BitrateAllocator::NormalRateAllocation(uint32_t bitrate,
                                            uint8_t fraction_loss,
                                            int64_t rtt,
                                            uint32_t sum_min_bitrates) {
  uint32_t number_of_observers = bitrate_observers_.size();
  uint32_t bitrate_per_observer =
      (bitrate - sum_min_bitrates) / number_of_observers;
  // Use map to sort list based on max bitrate.
  ObserverSortingMap list_max_bitrates;
  BitrateObserverConfList::iterator it;
  for (it = bitrate_observers_.begin(); it != bitrate_observers_.end(); ++it) {
    list_max_bitrates.insert(std::pair<uint32_t, ObserverConfiguration*>(
        it->second->max_bitrate_,
        new ObserverConfiguration(it->first, it->second->min_bitrate_)));
  }
  ObserverSortingMap::iterator max_it = list_max_bitrates.begin();
  while (max_it != list_max_bitrates.end()) {
    number_of_observers--;
    uint32_t observer_allowance =
        max_it->second->min_bitrate_ + bitrate_per_observer;
    if (max_it->first < observer_allowance) {
      // We have more than enough for this observer.
      // Carry the remainder forward.
      uint32_t remainder = observer_allowance - max_it->first;
      if (number_of_observers != 0) {
        bitrate_per_observer += remainder / number_of_observers;
      }
      max_it->second->observer_->OnNetworkChanged(max_it->first, fraction_loss,
                                                  rtt);
    } else {
      max_it->second->observer_->OnNetworkChanged(observer_allowance,
                                                  fraction_loss, rtt);
    }
    delete max_it->second;
    list_max_bitrates.erase(max_it);
    // Prepare next iteration.
    max_it = list_max_bitrates.begin();
  }
}

void BitrateAllocator::LowRateAllocation(uint32_t bitrate,
                                         uint8_t fraction_loss,
                                         int64_t rtt,
                                         uint32_t sum_min_bitrates) {
  if (enforce_min_bitrate_) {
    // Min bitrate to all observers.
    BitrateObserverConfList::iterator it;
    for (it = bitrate_observers_.begin(); it != bitrate_observers_.end();
         ++it) {
      it->first->OnNetworkChanged(it->second->min_bitrate_, fraction_loss, rtt);
    }
  } else {
    // Allocate up to |min_bitrate_| to one observer at a time, until
    // |bitrate| is depleted.
    uint32_t remainder = bitrate;
    BitrateObserverConfList::iterator it;
    for (it = bitrate_observers_.begin(); it != bitrate_observers_.end();
         ++it) {
      uint32_t allocation = std::min(remainder, it->second->min_bitrate_);
      it->first->OnNetworkChanged(allocation, fraction_loss, rtt);
      remainder -= allocation;
    }
  }
}
}  // namespace webrtc
