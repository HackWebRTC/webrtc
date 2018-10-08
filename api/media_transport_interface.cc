/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is EXPERIMENTAL interface for media transport.
//
// The goal is to refactor WebRTC code so that audio and video frames
// are sent / received through the media transport interface. This will
// enable different media transport implementations, including QUIC-based
// media transport.

#include "api/media_transport_interface.h"

namespace webrtc {

MediaTransportEncodedAudioFrame::MediaTransportEncodedAudioFrame(
    int sampling_rate_hz,
    int starting_sample_index,
    int samples_per_channel,
    int sequence_number,
    FrameType frame_type,
    uint8_t payload_type,
    std::vector<uint8_t> encoded_data)
    : sampling_rate_hz_(sampling_rate_hz),
      starting_sample_index_(starting_sample_index),
      samples_per_channel_(samples_per_channel),
      sequence_number_(sequence_number),
      frame_type_(frame_type),
      payload_type_(payload_type),
      encoded_data_(std::move(encoded_data)) {}

MediaTransportEncodedAudioFrame::~MediaTransportEncodedAudioFrame() = default;

MediaTransportEncodedVideoFrame::MediaTransportEncodedVideoFrame(
    int64_t frame_id,
    std::vector<int64_t> referenced_frame_ids,
    VideoCodecType codec_type,
    const webrtc::EncodedImage& encoded_image)
    : codec_type_(codec_type),
      encoded_image_(encoded_image),
      frame_id_(frame_id),
      referenced_frame_ids_(std::move(referenced_frame_ids)) {}

MediaTransportEncodedVideoFrame::~MediaTransportEncodedVideoFrame() = default;

}  // namespace webrtc
