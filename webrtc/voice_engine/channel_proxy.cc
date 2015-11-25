/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/channel_proxy.h"

#include "webrtc/base/checks.h"
#include "webrtc/voice_engine/channel.h"

namespace webrtc {
namespace voe {
ChannelProxy::ChannelProxy() : channel_owner_(nullptr) {}

ChannelProxy::ChannelProxy(const ChannelOwner& channel_owner) :
    channel_owner_(channel_owner) {
  RTC_CHECK(channel_owner_.channel());
}

void ChannelProxy::SetRTCPStatus(bool enable) {
  RTC_DCHECK(channel_owner_.channel());
  channel_owner_.channel()->SetRTCPStatus(enable);
}

void ChannelProxy::SetLocalSSRC(uint32_t ssrc) {
  RTC_DCHECK(channel_owner_.channel());
  int error = channel_owner_.channel()->SetLocalSSRC(ssrc);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetRTCP_CNAME(const std::string& c_name) {
  // Note: VoERTP_RTCP::SetRTCP_CNAME() accepts a char[256] array.
  std::string c_name_limited = c_name.substr(0, 255);
  RTC_DCHECK(channel_owner_.channel());
  int error = channel_owner_.channel()->SetRTCP_CNAME(c_name_limited.c_str());
  RTC_DCHECK_EQ(0, error);
}
}  // namespace voe
}  // namespace webrtc
