/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/session/media/bundlefilter.h"

#include "talk/media/base/rtputils.h"
#include "webrtc/base/logging.h"

namespace cricket {

static const uint32_t kSsrc01 = 0x01;

BundleFilter::BundleFilter() {
}

BundleFilter::~BundleFilter() {
}

bool BundleFilter::DemuxPacket(const char* data, size_t len, bool rtcp) {
  // For rtp packets, we check whether the payload type can be found.
  // For rtcp packets, we check whether the ssrc can be found or is the special
  // value 1 except for SDES packets which always pass through. Plus, if
  // |streams_| is empty, we will allow all rtcp packets pass through provided
  // that they are valid rtcp packets in case that they are for early media.
  if (!rtcp) {
    // It may not be a RTP packet (e.g. SCTP).
    if (!IsRtpPacket(data, len))
      return false;

    int payload_type = 0;
    if (!GetRtpPayloadType(data, len, &payload_type)) {
      return false;
    }
    return FindPayloadType(payload_type);
  }

  // Rtcp packets using ssrc filter.
  int pl_type = 0;
  uint32_t ssrc = 0;
  if (!GetRtcpType(data, len, &pl_type)) return false;
  if (pl_type == kRtcpTypeSDES) {
    // SDES packet parsing not supported.
    LOG(LS_INFO) << "SDES packet received for demux.";
    return true;
  } else {
    if (!GetRtcpSsrc(data, len, &ssrc)) return false;
    if (ssrc == kSsrc01) {
      // SSRC 1 has a special meaning and indicates generic feedback on
      // some systems and should never be dropped.  If it is forwarded
      // incorrectly it will be ignored by lower layers anyway.
      return true;
    }
  }
  // Pass through if |streams_| is empty to allow early rtcp packets in.
  return !HasStreams() || FindStream(ssrc);
}

void BundleFilter::AddPayloadType(int payload_type) {
  payload_types_.insert(payload_type);
}

bool BundleFilter::AddStream(const StreamParams& stream) {
  if (GetStreamBySsrc(streams_, stream.first_ssrc())) {
    LOG(LS_WARNING) << "Stream already added to filter";
    return false;
  }
  streams_.push_back(stream);
  return true;
}

bool BundleFilter::RemoveStream(uint32_t ssrc) {
  return RemoveStreamBySsrc(&streams_, ssrc);
}

bool BundleFilter::HasStreams() const {
  return !streams_.empty();
}

bool BundleFilter::FindStream(uint32_t ssrc) const {
  return ssrc == 0 ? false : GetStreamBySsrc(streams_, ssrc) != nullptr;
}

bool BundleFilter::FindPayloadType(int pl_type) const {
  return payload_types_.find(pl_type) != payload_types_.end();
}

void BundleFilter::ClearAllPayloadTypes() {
  payload_types_.clear();
}

}  // namespace cricket
