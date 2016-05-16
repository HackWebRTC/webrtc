/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for RtpReceivers
// http://w3c.github.io/webrtc-pc/#rtcrtpreceiver-interface

#ifndef WEBRTC_API_RTPRECEIVERINTERFACE_H_
#define WEBRTC_API_RTPRECEIVERINTERFACE_H_

#include <string>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/proxy.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace webrtc {

class RtpReceiverInterface : public rtc::RefCountInterface {
 public:
  virtual rtc::scoped_refptr<MediaStreamTrackInterface> track() const = 0;

  // Not to be confused with "mid", this is a field we can temporarily use
  // to uniquely identify a receiver until we implement Unified Plan SDP.
  virtual std::string id() const = 0;

  virtual void Stop() = 0;

  // The WebRTC specification only defines RTCRtpParameters in terms of senders,
  // but this API also applies them to receivers, similar to ORTC:
  // http://ortc.org/wp-content/uploads/2016/03/ortc.html#rtcrtpparameters*.
  virtual RtpParameters GetParameters() const = 0;
  virtual bool SetParameters(const RtpParameters& parameters) = 0;

 protected:
  virtual ~RtpReceiverInterface() {}
};

// Define proxy for RtpReceiverInterface.
BEGIN_SIGNALING_PROXY_MAP(RtpReceiver)
PROXY_CONSTMETHOD0(rtc::scoped_refptr<MediaStreamTrackInterface>, track)
PROXY_CONSTMETHOD0(std::string, id)
PROXY_METHOD0(void, Stop)
PROXY_CONSTMETHOD0(RtpParameters, GetParameters);
PROXY_METHOD1(bool, SetParameters, const RtpParameters&)
END_SIGNALING_PROXY()

}  // namespace webrtc

#endif  // WEBRTC_API_RTPRECEIVERINTERFACE_H_
