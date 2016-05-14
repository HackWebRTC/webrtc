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

#include "webrtc/call/bitrate_allocator.h"

#include <algorithm>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

namespace webrtc {

// Allow packets to be transmitted in up to 2 times max video bitrate if the
// bandwidth estimate allows it.
const int kTransmissionMaxBitrateMultiplier = 2;
const int kDefaultBitrateBps = 300000;

BitrateAllocator::BitrateAllocator()
    : bitrate_observer_configs_(),
      enforce_min_bitrate_(true),
      last_bitrate_bps_(kDefaultBitrateBps),
      last_non_zero_bitrate_bps_(kDefaultBitrateBps),
      last_fraction_loss_(0),
      last_rtt_(0) {}

uint32_t BitrateAllocator::OnNetworkChanged(uint32_t bitrate,
                                            uint8_t fraction_loss,
                                            int64_t rtt) {
  rtc::CritScope lock(&crit_sect_);
  last_bitrate_bps_ = bitrate;
  last_non_zero_bitrate_bps_ =
      bitrate > 0 ? bitrate : last_non_zero_bitrate_bps_;
  last_fraction_loss_ = fraction_loss;
  last_rtt_ = rtt;

  uint32_t allocated_bitrate_bps = 0;
  ObserverAllocation allocation = AllocateBitrates(bitrate);
  for (const auto& kv : allocation) {
    kv.first->OnBitrateUpdated(kv.second, last_fraction_loss_, last_rtt_);
    allocated_bitrate_bps += kv.second;
  }
  return allocated_bitrate_bps;
}

int BitrateAllocator::AddObserver(BitrateAllocatorObserver* observer,
                                  uint32_t min_bitrate_bps,
                                  uint32_t max_bitrate_bps,
                                  bool enforce_min_bitrate) {
  rtc::CritScope lock(&crit_sect_);
  // TODO(mflodman): Enforce this per observer.
  EnforceMinBitrate(enforce_min_bitrate);

  auto it = FindObserverConfig(observer);

  // Allow the max bitrate to be exceeded for FEC and retransmissions.
  // TODO(holmer): We have to get rid of this hack as it makes it difficult to
  // properly allocate bitrate. The allocator should instead distribute any
  // extra bitrate after all streams have maxed out.
  max_bitrate_bps *= kTransmissionMaxBitrateMultiplier;
  if (it != bitrate_observer_configs_.end()) {
    // Update current configuration.
    it->min_bitrate_bps = min_bitrate_bps;
    it->max_bitrate_bps = max_bitrate_bps;
  } else {
    // Add new settings.
    bitrate_observer_configs_.push_back(ObserverConfig(
        observer, min_bitrate_bps, max_bitrate_bps, enforce_min_bitrate));
  }

  int new_observer_bitrate_bps = 0;
  if (last_bitrate_bps_ > 0) {  // We have a bitrate to allocate.
    ObserverAllocation allocation = AllocateBitrates(last_bitrate_bps_);
    for (auto& kv : allocation) {
      // Update all observers with the new allocation.
      kv.first->OnBitrateUpdated(kv.second, last_fraction_loss_, last_rtt_);
      if (kv.first == observer)
        new_observer_bitrate_bps = kv.second;
    }
  } else {
    // Currently, an encoder is not allowed to produce frames.
    // But we still have to return the initial config bitrate + let the
    // observer know that it can not produce frames.
    ObserverAllocation allocation =
        AllocateBitrates(last_non_zero_bitrate_bps_);
    observer->OnBitrateUpdated(0, last_fraction_loss_, last_rtt_);
    new_observer_bitrate_bps = allocation[observer];
  }
  return new_observer_bitrate_bps;
}

void BitrateAllocator::RemoveObserver(BitrateAllocatorObserver* observer) {
  rtc::CritScope lock(&crit_sect_);
  auto it = FindObserverConfig(observer);
  if (it != bitrate_observer_configs_.end()) {
    bitrate_observer_configs_.erase(it);
  }
}

void BitrateAllocator::EnforceMinBitrate(bool enforce_min_bitrate) {
  enforce_min_bitrate_ = enforce_min_bitrate;
}

BitrateAllocator::ObserverConfigList::iterator
BitrateAllocator::FindObserverConfig(
    const BitrateAllocatorObserver* observer) {
  for (auto it = bitrate_observer_configs_.begin();
       it != bitrate_observer_configs_.end(); ++it) {
    if (it->observer == observer)
      return it;
  }
  return bitrate_observer_configs_.end();
}

BitrateAllocator::ObserverAllocation BitrateAllocator::AllocateBitrates(
    uint32_t bitrate) {
  if (bitrate_observer_configs_.empty())
    return ObserverAllocation();

  if (bitrate == 0)
    return ZeroRateAllocation();

  uint32_t sum_min_bitrates = 0;
  for (const auto& observer_config : bitrate_observer_configs_)
    sum_min_bitrates += observer_config.min_bitrate_bps;
  if (bitrate <= sum_min_bitrates)
    return LowRateAllocation(bitrate);

  return NormalRateAllocation(bitrate, sum_min_bitrates);
}

BitrateAllocator::ObserverAllocation BitrateAllocator::NormalRateAllocation(
    uint32_t bitrate,
    uint32_t sum_min_bitrates) {
  uint32_t num_remaining_observers =
      static_cast<uint32_t>(bitrate_observer_configs_.size());
  RTC_DCHECK_GT(num_remaining_observers, 0u);

  uint32_t bitrate_per_observer =
      (bitrate - sum_min_bitrates) / num_remaining_observers;
  // Use map to sort list based on max bitrate.
  ObserverSortingMap list_max_bitrates;
  for (const auto& config : bitrate_observer_configs_) {
    list_max_bitrates.insert(std::pair<uint32_t, const ObserverConfig*>(
        config.max_bitrate_bps, &config));
  }

  ObserverAllocation allocation;
  ObserverSortingMap::iterator max_it = list_max_bitrates.begin();
  while (max_it != list_max_bitrates.end()) {
    num_remaining_observers--;
    uint32_t observer_allowance =
        max_it->second->min_bitrate_bps + bitrate_per_observer;
    if (max_it->first < observer_allowance) {
      // We have more than enough for this observer.
      // Carry the remainder forward.
      uint32_t remainder = observer_allowance - max_it->first;
      if (num_remaining_observers != 0)
        bitrate_per_observer += remainder / num_remaining_observers;
      allocation[max_it->second->observer] = max_it->first;
    } else {
      allocation[max_it->second->observer] = observer_allowance;
    }
    list_max_bitrates.erase(max_it);
    // Prepare next iteration.
    max_it = list_max_bitrates.begin();
  }
  return allocation;
}

BitrateAllocator::ObserverAllocation BitrateAllocator::ZeroRateAllocation() {
  ObserverAllocation allocation;
  // Zero bitrate to all observers.
  for (const auto& observer_config : bitrate_observer_configs_)
    allocation[observer_config.observer] = 0;
  return allocation;
}

BitrateAllocator::ObserverAllocation BitrateAllocator::LowRateAllocation(
    uint32_t bitrate) {
  ObserverAllocation allocation;
  if (enforce_min_bitrate_) {
    // Min bitrate to all observers.
    for (const auto& observer_config : bitrate_observer_configs_)
      allocation[observer_config.observer] = observer_config.min_bitrate_bps;
  } else {
    // Allocate up to |min_bitrate_bps| to one observer at a time, until
    // |bitrate| is depleted.
    uint32_t remainder = bitrate;
    for (const auto& observer_config : bitrate_observer_configs_) {
      uint32_t allocated_bitrate =
          std::min(remainder, observer_config.min_bitrate_bps);
      allocation[observer_config.observer] = allocated_bitrate;
      remainder -= allocated_bitrate;
    }
  }
  return allocation;
}
}  // namespace webrtc
