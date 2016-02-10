/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_FAKEMEDIACONTROLLER_H_
#define WEBRTC_API_FAKEMEDIACONTROLLER_H_

#include "webrtc/api/mediacontroller.h"
#include "webrtc/base/checks.h"

namespace cricket {

class FakeMediaController : public webrtc::MediaControllerInterface {
 public:
  explicit FakeMediaController(cricket::ChannelManager* channel_manager,
                               webrtc::Call* call)
      : channel_manager_(channel_manager), call_(call) {
    RTC_DCHECK(nullptr != channel_manager_);
    RTC_DCHECK(nullptr != call_);
  }
  ~FakeMediaController() override {}
  webrtc::Call* call_w() override { return call_; }
  cricket::ChannelManager* channel_manager() const override {
    return channel_manager_;
  }

 private:
  cricket::ChannelManager* channel_manager_;
  webrtc::Call* call_;
};
}  // namespace cricket
#endif  // WEBRTC_API_FAKEMEDIACONTROLLER_H_
