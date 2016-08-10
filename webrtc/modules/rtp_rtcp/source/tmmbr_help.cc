/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"

namespace webrtc {
void TMMBRSet::VerifyAndAllocateSet(uint32_t minimumSize) {
  clear();
  reserve(minimumSize);
}

void TMMBRSet::VerifyAndAllocateSetKeepingData(uint32_t minimumSize) {
  reserve(minimumSize);
}

void TMMBRSet::SetEntry(unsigned int i,
                        uint32_t tmmbrSet,
                        uint32_t packetOHSet,
                        uint32_t ssrcSet) {
  RTC_DCHECK_LT(i, capacity());
  if (i >= size()) {
    resize(i + 1);
  }
  (*this)[i].set_bitrate_bps(tmmbrSet * 1000);
  (*this)[i].set_packet_overhead(packetOHSet);
  (*this)[i].set_ssrc(ssrcSet);
}

void TMMBRSet::AddEntry(uint32_t tmmbrSet,
                        uint32_t packetOHSet,
                        uint32_t ssrcSet) {
  RTC_DCHECK_LT(size(), capacity());
  SetEntry(size(), tmmbrSet, packetOHSet, ssrcSet);
}

void TMMBRSet::RemoveEntry(uint32_t sourceIdx) {
  RTC_DCHECK_LT(sourceIdx, size());
  erase(begin() + sourceIdx);
}

TMMBRSet* TMMBRHelp::VerifyAndAllocateCandidateSet(uint32_t minimumSize) {
  _candidateSet.VerifyAndAllocateSet(minimumSize);
  return &_candidateSet;
}

TMMBRSet* TMMBRHelp::CandidateSet() {
  return &_candidateSet;
}

std::vector<rtcp::TmmbItem> TMMBRHelp::FindTMMBRBoundingSet() {
  // Work on local variable, will be modified
  TMMBRSet candidateSet;
  candidateSet.VerifyAndAllocateSet(_candidateSet.capacity());

  for (size_t i = 0; i < _candidateSet.size(); i++) {
    if (_candidateSet.Tmmbr(i)) {
      candidateSet.AddEntry(_candidateSet.Tmmbr(i), _candidateSet.PacketOH(i),
                            _candidateSet.Ssrc(i));
    } else {
      // make sure this is zero if tmmbr = 0
      RTC_DCHECK_EQ(_candidateSet.PacketOH(i), 0u);
      // Old code:
      // _candidateSet.ptrPacketOHSet[i] = 0;
    }
  }

  // Number of set candidates
  int32_t numSetCandidates = candidateSet.lengthOfSet();
  // Find bounding set
  std::vector<rtcp::TmmbItem> bounding;
  if (numSetCandidates > 0) {
    FindBoundingSet(std::move(candidateSet), &bounding);
    size_t numBoundingSet = bounding.size();
    RTC_DCHECK_GE(numBoundingSet, 1u);
    RTC_DCHECK_LE(numBoundingSet, _candidateSet.size());
  }
  return bounding;
}

void TMMBRHelp::FindBoundingSet(std::vector<rtcp::TmmbItem> candidates,
                                std::vector<rtcp::TmmbItem>* bounding_set) {
  RTC_DCHECK(bounding_set);
  RTC_DCHECK(!candidates.empty());
  size_t num_candidates = candidates.size();

  if (num_candidates == 1) {
    RTC_DCHECK(candidates[0].bitrate_bps());
    *bounding_set = std::move(candidates);
    return;
  }

  // 1. Sort by increasing packet overhead.
  std::sort(candidates.begin(), candidates.end(),
            [](const rtcp::TmmbItem& lhs, const rtcp::TmmbItem& rhs) {
              return lhs.packet_overhead() < rhs.packet_overhead();
            });

  // 2. For tuples with same overhead, keep the one with the lowest bitrate.
  for (auto it = candidates.begin(); it != candidates.end();) {
    RTC_DCHECK(it->bitrate_bps());
    auto current_min = it;
    auto next_it = it + 1;
    // Use fact candidates are sorted by overhead, so candidates with same
    // overhead are adjusted.
    while (next_it != candidates.end() &&
           next_it->packet_overhead() == current_min->packet_overhead()) {
      if (next_it->bitrate_bps() < current_min->bitrate_bps()) {
        current_min->set_bitrate_bps(0);
        current_min = next_it;
      } else {
        next_it->set_bitrate_bps(0);
      }
      ++next_it;
      --num_candidates;
    }
    it = next_it;
  }

  // 3. Select and remove tuple with lowest tmmbr.
  // (If more than 1, choose the one with highest overhead).
  auto min_bitrate_it = candidates.end();
  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    if (it->bitrate_bps()) {
      min_bitrate_it = it;
      break;
    }
  }

  for (auto it = min_bitrate_it; it != candidates.end(); ++it) {
    if (it->bitrate_bps() &&
        it->bitrate_bps() <= min_bitrate_it->bitrate_bps()) {
      // Get min bitrate.
      min_bitrate_it = it;
    }
  }

  bounding_set->clear();
  bounding_set->reserve(num_candidates);
  std::vector<float> intersection(num_candidates);
  std::vector<float> max_packet_rate(num_candidates);

  // First member of selected list.
  bounding_set->push_back(*min_bitrate_it);
  intersection[0] = 0;
  // Calculate its maximum packet rate (where its line crosses x-axis).
  uint16_t packet_overhead = bounding_set->back().packet_overhead();
  if (packet_overhead == 0) {
    // Avoid division by zero.
    max_packet_rate[0] = std::numeric_limits<float>::max();
  } else {
    max_packet_rate[0] = bounding_set->back().bitrate_bps() /
                         static_cast<float>(packet_overhead);
  }
  // Remove from candidate list.
  min_bitrate_it->set_bitrate_bps(0);
  --num_candidates;

  // 4. Discard from candidate list all tuple with lower overhead
  // (next tuple must be steeper).
  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    if (it->bitrate_bps() &&
        it->packet_overhead() < bounding_set->front().packet_overhead()) {
      it->set_bitrate_bps(0);
      --num_candidates;
    }
  }

  bool get_new_candidate = true;
  rtcp::TmmbItem cur_candidate;
  while (num_candidates > 0) {
    if (get_new_candidate) {
      // 5. Remove first remaining tuple from candidate list.
      for (auto it = candidates.begin(); it != candidates.end(); ++it) {
        if (it->bitrate_bps()) {
          cur_candidate = *it;
          it->set_bitrate_bps(0);
          break;
        }
      }
    }

    // 6. Calculate packet rate and intersection of the current
    // line with line of last tuple in selected list.
    RTC_DCHECK_NE(cur_candidate.packet_overhead(),
                  bounding_set->back().packet_overhead());
    float packet_rate = static_cast<float>(cur_candidate.bitrate_bps() -
                                           bounding_set->back().bitrate_bps()) /
                        (cur_candidate.packet_overhead() -
                         bounding_set->back().packet_overhead());

    // 7. If the packet rate is equal or lower than intersection of
    //    last tuple in selected list,
    //    remove last tuple in selected list & go back to step 6.
    if (packet_rate <= intersection[bounding_set->size() - 1]) {
      // Remove last tuple and goto step 6.
      bounding_set->pop_back();
      get_new_candidate = false;
    } else {
      // 8. If packet rate is lower than maximum packet rate of
      // last tuple in selected list, add current tuple to selected
      // list.
      if (packet_rate < max_packet_rate[bounding_set->size() - 1]) {
        bounding_set->push_back(cur_candidate);
        intersection[bounding_set->size() - 1] = packet_rate;
        uint16_t packet_overhead = bounding_set->back().packet_overhead();
        RTC_DCHECK_NE(packet_overhead, 0);
        max_packet_rate[bounding_set->size() - 1] =
            bounding_set->back().bitrate_bps() /
            static_cast<float>(packet_overhead);
      }
      --num_candidates;
      get_new_candidate = true;
    }

    // 9. Go back to step 5 if any tuple remains in candidate list.
  }
}

bool TMMBRHelp::IsOwner(const std::vector<rtcp::TmmbItem>& bounding,
                        uint32_t ssrc) {
  for (const rtcp::TmmbItem& item : bounding) {
    if (item.ssrc() == ssrc) {
      return true;
    }
  }
  return false;
}

bool TMMBRHelp::CalcMinBitRate(uint32_t* minBitrateKbit) const {
  if (_candidateSet.size() == 0) {
    // Empty bounding set.
    return false;
  }
  *minBitrateKbit = std::numeric_limits<uint32_t>::max();

  for (size_t i = 0; i < _candidateSet.lengthOfSet(); ++i) {
    uint32_t curNetBitRateKbit = _candidateSet.Tmmbr(i);
    if (curNetBitRateKbit < MIN_VIDEO_BW_MANAGEMENT_BITRATE) {
      curNetBitRateKbit = MIN_VIDEO_BW_MANAGEMENT_BITRATE;
    }
    *minBitrateKbit = curNetBitRateKbit < *minBitrateKbit ? curNetBitRateKbit
                                                          : *minBitrateKbit;
  }
  return true;
}
}  // namespace webrtc
