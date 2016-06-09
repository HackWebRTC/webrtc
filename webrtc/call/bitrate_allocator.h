/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_BITRATE_ALLOCATOR_H_
#define WEBRTC_CALL_BITRATE_ALLOCATOR_H_

#include <stdint.h>

#include <list>
#include <map>
#include <utility>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"

namespace webrtc {

// Used by all send streams with adaptive bitrate, to get the currently
// allocated bitrate for the send stream. The current network properties are
// given at the same time, to let the send stream decide about possible loss
// protection.
class BitrateAllocatorObserver {
 public:
  virtual void OnBitrateUpdated(uint32_t bitrate_bps,
                                uint8_t fraction_loss,
                                int64_t rtt) = 0;
  virtual ~BitrateAllocatorObserver() {}
};

// Usage: this class will register multiple RtcpBitrateObserver's one at each
// RTCP module. It will aggregate the results and run one bandwidth estimation
// and push the result to the encoders via BitrateAllocatorObserver(s).
class BitrateAllocator {
 public:
  BitrateAllocator();

  // Allocate target_bitrate across the registered BitrateAllocatorObservers.
  // Returns actual bitrate allocated, which might be higher than target_bitrate
  // if for instance EnforceMinBitrate() is enabled.
  uint32_t OnNetworkChanged(uint32_t target_bitrate_bps,
                            uint8_t fraction_loss,
                            int64_t rtt);

  // Set the start and max send bitrate used by the bandwidth management.
  //
  // |observer| updates bitrates if already in use.
  // |min_bitrate_bps| = 0 equals no min bitrate.
  // |max_bitrate_bps| = 0 equals no max bitrate.
  // |enforce_min_bitrate| = 'true' will allocate at least |min_bitrate_bps| for
  //    this observer, even if the BWE is too low, 'false' will allocate 0 to
  //    the observer if BWE doesn't allow |min_bitrate_bps|.
  // Returns initial bitrate allocated for |observer|.
  // Note that |observer|->OnBitrateUpdated() will be called within the scope of
  // this method with the current rtt, fraction_loss and available bitrate and
  // that the bitrate in OnBitrateUpdated will be zero if the |observer| is
  // currently not allowed to send data.
  int AddObserver(BitrateAllocatorObserver* observer,
                  uint32_t min_bitrate_bps,
                  uint32_t max_bitrate_bps,
                  bool enforce_min_bitrate);

  // Removes a previously added observer, but will not trigger a new bitrate
  // allocation.
  void RemoveObserver(BitrateAllocatorObserver* observer);

 private:
  // Note: All bitrates for member variables and methods are in bps.
  struct ObserverConfig {
    ObserverConfig(BitrateAllocatorObserver* observer,
                   uint32_t min_bitrate_bps,
                   uint32_t max_bitrate_bps,
                   bool enforce_min_bitrate)
        : observer(observer),
          min_bitrate_bps(min_bitrate_bps),
          max_bitrate_bps(max_bitrate_bps),
          enforce_min_bitrate(enforce_min_bitrate) {}
    BitrateAllocatorObserver* const observer;
    uint32_t min_bitrate_bps;
    uint32_t max_bitrate_bps;
    bool enforce_min_bitrate;
  };

  typedef std::list<ObserverConfig> ObserverConfigList;
  ObserverConfigList::iterator FindObserverConfig(
      const BitrateAllocatorObserver* observer)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  typedef std::multimap<uint32_t, const ObserverConfig*> ObserverSortingMap;
  typedef std::map<BitrateAllocatorObserver*, int> ObserverAllocation;

  ObserverAllocation AllocateBitrates(uint32_t bitrate)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  ObserverAllocation ZeroRateAllocation() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  ObserverAllocation LowRateAllocation(uint32_t bitrate)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  ObserverAllocation NormalRateAllocation(uint32_t bitrate,
                                          uint32_t sum_min_bitrates)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  ObserverAllocation MaxRateAllocation(uint32_t bitrate,
                                       uint32_t sum_max_bitrates)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  uint32_t LastAllocatedBitrate(const ObserverConfig& observer_config)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  // The minimum bitrate required by this observer, including enable-hysteresis
  // if the observer is in a paused state.
  uint32_t MinBitrateWithHysteresis(const ObserverConfig& observer_config)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  // Splits |bitrate| evenly to observers already in |allocation|.
  // |include_zero_allocations| decides if zero allocations should be part of
  // the distribution or not. The allowed max bitrate is |max_multiplier| x
  // observer max bitrate.
  void DistributeBitrateEvenly(uint32_t bitrate,
                               bool include_zero_allocations,
                               int max_multiplier,
                               ObserverAllocation* allocation)
          EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  bool EnoughBitrateForAllObservers(uint32_t bitrate, uint32_t sum_min_bitrates)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);


  rtc::CriticalSection crit_sect_;
  // Stored in a list to keep track of the insertion order.
  ObserverConfigList bitrate_observer_configs_ GUARDED_BY(crit_sect_);
  uint32_t last_bitrate_bps_ GUARDED_BY(crit_sect_);
  uint32_t last_non_zero_bitrate_bps_ GUARDED_BY(crit_sect_);
  uint8_t last_fraction_loss_ GUARDED_BY(crit_sect_);
  int64_t last_rtt_ GUARDED_BY(crit_sect_);
  ObserverAllocation last_allocation_ GUARDED_BY(crit_sect_);
};
}  // namespace webrtc
#endif  // WEBRTC_CALL_BITRATE_ALLOCATOR_H_
