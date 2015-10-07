/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/rtccertificate.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/timeutils.h"

namespace rtc {

scoped_refptr<RTCCertificate> RTCCertificate::Create(
    scoped_ptr<SSLIdentity> identity) {
  return new RefCountedObject<RTCCertificate>(identity.release());
}

RTCCertificate::RTCCertificate(SSLIdentity* identity)
    : identity_(identity) {
  RTC_DCHECK(identity_);
}

RTCCertificate::~RTCCertificate() {
}

uint64_t RTCCertificate::expires_timestamp_ns() const {
  // TODO(hbos): Update once SSLIdentity/SSLCertificate supports expires field.
  return 0;
}

bool RTCCertificate::HasExpired() const {
  return expires_timestamp_ns() <= TimeNanos();
}

const SSLCertificate& RTCCertificate::ssl_certificate() const {
  return identity_->certificate();
}

}  // namespace rtc
