/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/setremotedescriptionobserverinterface.h"

#include <string>

namespace webrtc {

// The message keeps the observer alive through reference counting.
class SetRemoteDescriptionObserverAdapter::MessageData
    : public rtc::MessageData {
 public:
  static MessageData* Create(
      rtc::scoped_refptr<SetRemoteDescriptionObserverAdapter> observer,
      RTCError error) {
    return new MessageData(std::move(observer), std::move(error));
  }

  const RTCError& error() const { return error_; }

 private:
  MessageData(rtc::scoped_refptr<SetRemoteDescriptionObserverAdapter> observer,
              RTCError error)
      : observer_(std::move(observer)), error_(std::move(error)) {}

  rtc::scoped_refptr<SetRemoteDescriptionObserverAdapter> observer_;
  RTCError error_;
};

SetRemoteDescriptionObserverAdapter::SetRemoteDescriptionObserverAdapter(
    rtc::scoped_refptr<SetSessionDescriptionObserver> wrapper)
    : wrapper_(std::move(wrapper)) {}

void SetRemoteDescriptionObserverAdapter::OnSetRemoteDescriptionComplete(
    RTCError error) {
  // MessageData keeps a reference to |this|, ensuring it is not deleted until
  // OnMessage().
  rtc::Thread::Current()->Post(RTC_FROM_HERE, this, 0,
                               MessageData::Create(this, std::move(error)));
}

void SetRemoteDescriptionObserverAdapter::OnMessage(rtc::Message* msg) {
  MessageData* message = static_cast<MessageData*>(msg->pdata);
  if (message->error().ok())
    wrapper_->OnSuccess();
  else
    wrapper_->OnFailure(message->error().message());
  // Delete the message data, this may delete |this|. Don't use member variables
  // after this line.
  delete message;
}

}  // namespace webrtc
