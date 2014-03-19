/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_ENCODER_SETTINGS_H_
#define WEBRTC_TEST_ENCODER_SETTINGS_H_

#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace test {
VideoSendStream::Config::EncoderSettings CreateEncoderSettings(
    VideoEncoder* encoder,
    const char* payload_name,
    int payload_type,
    size_t num_streams);

VideoCodec CreateDecoderVideoCodec(
    const VideoSendStream::Config::EncoderSettings& settings);
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_ENCODER_SETTINGS_H_
