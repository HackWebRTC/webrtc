/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_LAYER_FILTERING_TRANSPORT_H_
#define WEBRTC_TEST_LAYER_FILTERING_TRANSPORT_H_

#include "webrtc/test/direct_transport.h"
#include "webrtc/test/fake_network_pipe.h"

namespace webrtc {

namespace test {

class LayerFilteringTransport : public test::DirectTransport {
 public:
  LayerFilteringTransport(const FakeNetworkPipe::Config& config,
                          uint8_t vp8_video_payload_type,
                          uint8_t vp9_video_payload_type,
                          uint8_t tl_discard_threshold,
                          uint8_t sl_discard_threshold);
  bool SendRtp(const uint8_t* data, size_t length) override;

 private:
  // Used to distinguish between VP8 and VP9.
  const uint8_t vp8_video_payload_type_;
  const uint8_t vp9_video_payload_type_;
  // Discard all temporal/spatial layers with id greater or equal the
  // threshold. 0 to disable.
  const uint8_t tl_discard_threshold_;
  const uint8_t sl_discard_threshold_;
  uint16_t current_seq_num_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_LAYER_FILTERING_TRANSPORT_H_
