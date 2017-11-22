/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_SETREMOTEDESCRIPTIONOBSERVERINTERFACE_H_
#define API_SETREMOTEDESCRIPTIONOBSERVERINTERFACE_H_

#include <utility>
#include <vector>

#include "api/jsep.h"
#include "api/mediastreaminterface.h"
#include "api/rtcerror.h"
#include "api/rtpreceiverinterface.h"
#include "rtc_base/bind.h"
#include "rtc_base/messagehandler.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"

namespace webrtc {

// An observer for PeerConnectionInterface::SetRemoteDescription(). The
// callback is invoked such that the state of the peer connection can be
// examined to accurately reflect the effects of the SetRemoteDescription
// operation.
class SetRemoteDescriptionObserverInterface : public rtc::RefCountInterface {
 public:
  // On success, |error.ok()| is true.
  virtual void OnSetRemoteDescriptionComplete(RTCError error) = 0;
};

// Upon completion, posts a task to execute the callback of the
// SetSessionDescriptionObserver asynchronously on the same thread. At this
// point, the state of the peer connection might no longer reflect the effects
// of the SetRemoteDescription operation, as the peer connection could have been
// modified during the post.
// TODO(hbos): Remove this class once we remove the version of
// PeerConnectionInterface::SetRemoteDescription() that takes a
// SetSessionDescriptionObserver as an argument.
class SetRemoteDescriptionObserverAdapter
    : public rtc::RefCountedObject<SetRemoteDescriptionObserverInterface>,
      public rtc::MessageHandler {
 public:
  SetRemoteDescriptionObserverAdapter(
      rtc::scoped_refptr<SetSessionDescriptionObserver> wrapper);

  // SetRemoteDescriptionObserverInterface implementation.
  void OnSetRemoteDescriptionComplete(RTCError error) override;

  // rtc::MessageHandler implementation.
  void OnMessage(rtc::Message* msg) override;

 private:
  class MessageData;

  rtc::scoped_refptr<SetSessionDescriptionObserver> wrapper_;
};

}  // namespace webrtc

#endif  // API_SETREMOTEDESCRIPTIONOBSERVERINTERFACE_H_
