/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_stream_decoder.h"

#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/video_coding_impl.h"
#include "rtc_base/checks.h"
#include "video/receive_statistics_proxy.h"

namespace webrtc {

VideoStreamDecoder::VideoStreamDecoder(
    vcm::VideoReceiver* video_receiver,
    VCMPacketRequestCallback* vcm_packet_request_callback,
    bool enable_nack,
    bool /* enable_fec */,
    ReceiveStatisticsProxy* receive_statistics_proxy,
    rtc::VideoSinkInterface<VideoFrame>* incoming_video_stream)
    : video_receiver_(video_receiver),
      receive_stats_callback_(receive_statistics_proxy),
      incoming_video_stream_(incoming_video_stream) {
  RTC_DCHECK(video_receiver_);

  static const int kMaxPacketAgeToNack = 450;
  static const int kMaxNackListSize = 250;
  video_receiver_->SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack, 0);
  video_receiver_->RegisterReceiveCallback(this);

  VCMPacketRequestCallback* packet_request_callback =
      enable_nack ? vcm_packet_request_callback : nullptr;
  video_receiver_->RegisterPacketRequestCallback(packet_request_callback);
}

VideoStreamDecoder::~VideoStreamDecoder() {
  // Note: There's an assumption at this point that the decoder thread is
  // *not* running. If it was, then there could be a race for each of these
  // callbacks.

  // Unset all the callback pointers that we set in the ctor.
  video_receiver_->RegisterPacketRequestCallback(nullptr);
  video_receiver_->RegisterReceiveCallback(nullptr);
}

// Do not acquire the lock of |video_receiver_| in this function. Decode
// callback won't necessarily be called from the decoding thread. The decoding
// thread may have held the lock when calling VideoDecoder::Decode, Reset, or
// Release. Acquiring the same lock in the path of decode callback can deadlock.
int32_t VideoStreamDecoder::FrameToRender(VideoFrame& video_frame,
                                          absl::optional<uint8_t> qp,
                                          VideoContentType content_type) {
  receive_stats_callback_->OnDecodedFrame(video_frame, qp, content_type);
  incoming_video_stream_->OnFrame(video_frame);
  return 0;
}

int32_t VideoStreamDecoder::ReceivedDecodedReferenceFrame(
    const uint64_t picture_id) {
  RTC_NOTREACHED();
  return 0;
}

void VideoStreamDecoder::OnIncomingPayloadType(int payload_type) {
  receive_stats_callback_->OnIncomingPayloadType(payload_type);
}

void VideoStreamDecoder::OnDecoderImplementationName(
    const char* implementation_name) {
  receive_stats_callback_->OnDecoderImplementationName(implementation_name);
}

}  // namespace webrtc
