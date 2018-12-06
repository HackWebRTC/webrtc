/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_DTLSTRANSPORT_H_
#define PC_DTLSTRANSPORT_H_

#include <memory>

#include "api/dtlstransportinterface.h"
#include "p2p/base/dtlstransport.h"

namespace webrtc {

// This implementation wraps a cricket::DtlsTransport, and takes
// ownership of it.
class DtlsTransport : public DtlsTransportInterface {
 public:
  explicit DtlsTransport(
      std::unique_ptr<cricket::DtlsTransportInternal> internal);
  cricket::DtlsTransportInternal* internal() {
    return internal_dtls_transport_.get();
  }
  void clear() { internal_dtls_transport_.reset(); }

 private:
  std::unique_ptr<cricket::DtlsTransportInternal> internal_dtls_transport_;
};

}  // namespace webrtc
#endif  // PC_DTLSTRANSPORT_H_
