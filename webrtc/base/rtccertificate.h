/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_RTCCERTIFICATE_H_
#define WEBRTC_BASE_RTCCERTIFICATE_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/sslidentity.h"

namespace rtc {

// A thin abstraction layer between "lower level crypto stuff" like
// SSLCertificate and WebRTC usage. Takes ownership of some lower level objects,
// reference counting protects these from premature destruction.
class RTCCertificate : public RefCountInterface {
 public:
  // Takes ownership of |identity|.
  static scoped_refptr<RTCCertificate> Create(scoped_ptr<SSLIdentity> identity);

  uint64 expires_timestamp_ns() const;
  bool HasExpired() const;
  const SSLCertificate& ssl_certificate() const;

  // TODO(hbos): If possible, remove once RTCCertificate and its
  // ssl_certificate() is used in all relevant places. Should not pass around
  // raw SSLIdentity* for the sake of accessing SSLIdentity::certificate().
  // However, some places might need SSLIdentity* for its public/private key...
  SSLIdentity* identity() const { return identity_.get(); }

 protected:
  explicit RTCCertificate(SSLIdentity* identity);
  ~RTCCertificate() override;

 private:
  // The SSLIdentity is the owner of the SSLCertificate. To protect our
  // ssl_certificate() we take ownership of |identity_|.
  scoped_ptr<SSLIdentity> identity_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_RTCCERTIFICATE_H_
