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

BundleFilter::BundleFilter() {
}

BundleFilter::~BundleFilter() {
}

bool BundleFilter::DemuxPacket(const uint8_t* data, size_t len) {
  // For RTP packets, we check whether the payload type can be found.
  if (!IsRtpPacket(data, len)) {
    return false;
  }

  int payload_type = 0;
  if (!GetRtpPayloadType(data, len, &payload_type)) {
    return false;
  }
  return FindPayloadType(payload_type);
}

void BundleFilter::AddPayloadType(int payload_type) {
  payload_types_.insert(payload_type);
}

bool BundleFilter::FindPayloadType(int pl_type) const {
  return payload_types_.find(pl_type) != payload_types_.end();
}

void BundleFilter::ClearAllPayloadTypes() {
  payload_types_.clear();
}

}  // namespace cricket
