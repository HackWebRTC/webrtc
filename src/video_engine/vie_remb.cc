/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_remb.h"

#include <cassert>

#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

const int kRembSendIntervallMs = 1000;

// % threshold for if we should send a new REMB asap.
const int kSendThresholdPercent = 97;

VieRemb::VieRemb(int engine_id)
    : engine_id_(engine_id),
      list_crit_(CriticalSectionWrapper::CreateCriticalSection()),
      last_remb_time_(TickTime::MillisecondTimestamp()),
      last_send_bitrate_(0) {
}

VieRemb::~VieRemb() {
}

void VieRemb::AddReceiveChannel(RtpRtcp* rtp_rtcp) {
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, engine_id_,
               "VieRemb::AddReceiveChannel");
  assert(rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());
  for (RtpModules::iterator it = receive_modules_.begin();
       it != receive_modules_.end(); ++it) {
    if ((*it) == rtp_rtcp)
      return;
  }

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, engine_id_, "AddRembChannel");
  // The module probably doesn't have a remote SSRC yet, so don't add it to the
  // map.
  receive_modules_.push_back(rtp_rtcp);
}

void VieRemb::RemoveReceiveChannel(RtpRtcp* rtp_rtcp) {
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, engine_id_,
               "VieRemb::RemoveReceiveChannel");
  assert(rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());
  unsigned int ssrc = rtp_rtcp->RemoteSSRC();
  for (RtpModules::iterator it = receive_modules_.begin();
       it != receive_modules_.end(); ++it) {
    if ((*it) == rtp_rtcp) {
      receive_modules_.erase(it);
      break;
    }
  }
  bitrates_.erase(ssrc);
}

void VieRemb::AddSendChannel(RtpRtcp* rtp_rtcp) {
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, engine_id_,
               "VieRemb::AddSendChannel");
  assert(rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());

  // TODO(mflodman) Allow multiple senders.
  assert(send_modules_.empty());

  send_modules_.push_back(rtp_rtcp);
}

void VieRemb::RemoveSendChannel(RtpRtcp* rtp_rtcp) {
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, engine_id_,
               "VieRemb::AddSendChannel");
  assert(rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());
  for (RtpModules::iterator it = send_modules_.begin();
       it != send_modules_.end(); ++it) {
    if ((*it) == rtp_rtcp) {
      send_modules_.erase(it);
      return;
    }
  }
}

void VieRemb::OnReceiveBitrateChanged(unsigned int ssrc, unsigned int bitrate) {
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, engine_id_,
               "VieRemb::UpdateBitrateEstimate(ssrc: %u, bitrate: %u)",
               ssrc, bitrate);
  CriticalSectionScoped cs(list_crit_.get());

  // Check if this is a new ssrc and add it to the map if it is.
  if (bitrates_.find(ssrc) == bitrates_.end()) {
    bitrates_[ssrc] = bitrate;
  }

  int new_remb_bitrate = last_send_bitrate_ - bitrates_[ssrc] + bitrate;
  if (new_remb_bitrate < kSendThresholdPercent * last_send_bitrate_ / 100) {
    // The new bitrate estimate is less than kSendThresholdPercent % of the last
    // report. Send a REMB asap.
    last_remb_time_ = TickTime::MillisecondTimestamp() - kRembSendIntervallMs;
  }
  bitrates_[ssrc] = bitrate;
}

WebRtc_Word32 VieRemb::Version(WebRtc_Word8* version,
                               WebRtc_UWord32& remaining_buffer_in_bytes,
                               WebRtc_UWord32& position) const {
  return 0;
}

WebRtc_Word32 VieRemb::ChangeUniqueId(const WebRtc_Word32 id) {
  return 0;
}

WebRtc_Word32 VieRemb::TimeUntilNextProcess() {
  return kRembSendIntervallMs -
      (TickTime::MillisecondTimestamp() - last_remb_time_);
}

WebRtc_Word32 VieRemb::Process() {
  int64_t now = TickTime::MillisecondTimestamp();
  if (now - last_remb_time_ < kRembSendIntervallMs)
    return 0;

  last_remb_time_ = now;

  // Calculate total receive bitrate estimate.
  list_crit_->Enter();
  int total_bitrate = 0;
  int num_bitrates = bitrates_.size();

  if (num_bitrates == 0) {
    list_crit_->Leave();
    return 0;
  }

  // TODO(mflodman) Use std::vector and change RTP module API.
  unsigned int* ssrcs = new unsigned int[num_bitrates];

  int idx = 0;
  for (SsrcBitrate::iterator it = bitrates_.begin(); it != bitrates_.end();
       ++it, ++idx) {
    total_bitrate += it->second;
    ssrcs[idx] = it->first;
  }

  // Send a REMB packet.
  RtpRtcp* sender = NULL;
  if (!send_modules_.empty()) {
    sender = send_modules_.front();
  }
  last_send_bitrate_ = total_bitrate;
  list_crit_->Leave();

  if (sender) {
    sender->SetREMBData(total_bitrate, num_bitrates, ssrcs);
  }
  delete [] ssrcs;
  return 0;
}

}  // namespace webrtc
