/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_DTLSTRANSPORTINTERFACE_H_
#define API_DTLSTRANSPORTINTERFACE_H_

#include "rtc_base/refcount.h"

namespace webrtc {

// A DTLS transport, as represented to the outside world.
// Its role is to report state changes and errors, and make sure information
// about remote certificates is available.
class DtlsTransportInterface : public rtc::RefCountInterface {
 public:
  // TODO(hta): Need a notifier interface to transmit state changes and
  // error events. The generic NotifierInterface of mediasteraminterface.h
  // may be suitable, or may be copyable.
};

}  // namespace webrtc

#endif  // API_DTLSTRANSPORTINTERFACE_H_
