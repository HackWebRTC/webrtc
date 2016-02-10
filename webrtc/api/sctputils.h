/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_SCTPUTILS_H_
#define WEBRTC_API_SCTPUTILS_H_

#include <string>

#include "webrtc/api/datachannelinterface.h"

namespace rtc {
class Buffer;
}  // namespace rtc

namespace webrtc {
struct DataChannelInit;

// Read the message type and return true if it's an OPEN message.
bool IsOpenMessage(const rtc::Buffer& payload);

bool ParseDataChannelOpenMessage(const rtc::Buffer& payload,
                                 std::string* label,
                                 DataChannelInit* config);

bool ParseDataChannelOpenAckMessage(const rtc::Buffer& payload);

bool WriteDataChannelOpenMessage(const std::string& label,
                                 const DataChannelInit& config,
                                 rtc::Buffer* payload);

void WriteDataChannelOpenAckMessage(rtc::Buffer* payload);
}  // namespace webrtc

#endif  // WEBRTC_API_SCTPUTILS_H_
