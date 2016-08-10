/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_

#include <vector>
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmb_item.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class TMMBRSet : public std::vector<rtcp::TmmbItem> {
 public:
  void VerifyAndAllocateSet(uint32_t minimumSize);
  void VerifyAndAllocateSetKeepingData(uint32_t minimumSize);
  // Number of valid data items in set.
  uint32_t lengthOfSet() const { return size(); }
  // Presently allocated max size of set.
  uint32_t sizeOfSet() const { return capacity(); }
  void clearSet() { clear(); }
  uint32_t Tmmbr(int i) const { return (*this)[i].bitrate_bps() / 1000; }
  uint32_t PacketOH(int i) const { return (*this)[i].packet_overhead(); }
  uint32_t Ssrc(int i) const { return (*this)[i].ssrc(); }
  void SetEntry(unsigned int i,
                uint32_t tmmbrSet,
                uint32_t packetOHSet,
                uint32_t ssrcSet);

  void AddEntry(uint32_t tmmbrSet, uint32_t packetOHSet, uint32_t ssrcSet);

  // Remove one entry from table, and move all others down.
  void RemoveEntry(uint32_t sourceIdx);
};

class TMMBRHelp {
 public:
  TMMBRSet* CandidateSet();

  TMMBRSet* VerifyAndAllocateCandidateSet(const uint32_t minimumSize);
  std::vector<rtcp::TmmbItem> FindTMMBRBoundingSet();

  static bool IsOwner(const std::vector<rtcp::TmmbItem>& bounding,
                      uint32_t ssrc);

  bool CalcMinBitRate(uint32_t* minBitrateKbit) const;

  static void FindBoundingSet(std::vector<rtcp::TmmbItem> candidates,
                              std::vector<rtcp::TmmbItem>* bounding_set);

 private:
  TMMBRSet _candidateSet;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_TMMBR_HELP_H_
