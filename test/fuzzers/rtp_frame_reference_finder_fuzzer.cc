/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/rtp_packet_infos.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

namespace {
class DataReader {
 public:
  DataReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  template <typename T>
  void CopyTo(T* object) {
    static_assert(std::is_pod<T>(), "");
    uint8_t* destination = reinterpret_cast<uint8_t*>(object);
    size_t object_size = sizeof(T);
    size_t num_bytes = std::min(size_ - offset_, object_size);
    memcpy(destination, data_ + offset_, num_bytes);
    offset_ += num_bytes;

    // If we did not have enough data, fill the rest with 0.
    object_size -= num_bytes;
    memset(destination + num_bytes, 0, object_size);
  }

  template <typename T>
  T GetNum() {
    T res;
    if (offset_ + sizeof(res) < size_) {
      memcpy(&res, data_ + offset_, sizeof(res));
      offset_ += sizeof(res);
      return res;
    }

    offset_ = size_;
    return T(0);
  }

  bool MoreToRead() { return offset_ < size_; }

 private:
  const uint8_t* data_;
  size_t size_;
  size_t offset_ = 0;
};

class NullCallback : public video_coding::OnCompleteFrameCallback {
  void OnCompleteFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) override {}
};

RtpGenericFrameDescriptor GenerateRtpGenericFrameDescriptor(
    DataReader* reader) {
  RtpGenericFrameDescriptor res;
  res.SetFirstPacketInSubFrame(true);
  res.SetFrameId(reader->GetNum<uint16_t>());

  int spatial_layer =
      reader->GetNum<uint8_t>() % RtpGenericFrameDescriptor::kMaxSpatialLayers;
  res.SetSpatialLayersBitmask(1 << spatial_layer);
  res.SetTemporalLayer(reader->GetNum<uint8_t>() %
                       RtpGenericFrameDescriptor::kMaxTemporalLayers);

  int num_diffs = (reader->GetNum<uint8_t>() %
                   RtpGenericFrameDescriptor::kMaxNumFrameDependencies);
  for (int i = 0; i < num_diffs; ++i) {
    res.AddFrameDependencyDiff(reader->GetNum<uint16_t>() % (1 << 14));
  }

  return res;
}
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  DataReader reader(data, size);
  NullCallback cb;
  video_coding::RtpFrameReferenceFinder reference_finder(&cb);

  auto codec = static_cast<VideoCodecType>(reader.GetNum<uint8_t>() % 4);

  while (reader.MoreToRead()) {
    uint16_t first_seq_num = reader.GetNum<uint16_t>();
    uint16_t last_seq_num = reader.GetNum<uint16_t>();
    bool marker_bit = reader.GetNum<uint8_t>();

    RTPVideoHeader video_header;
    switch (reader.GetNum<uint8_t>() % 3) {
      case 0:
        video_header.frame_type = VideoFrameType::kEmptyFrame;
        break;
      case 1:
        video_header.frame_type = VideoFrameType::kVideoFrameKey;
        break;
      case 2:
        video_header.frame_type = VideoFrameType::kVideoFrameDelta;
        break;
    }

    switch (codec) {
      case kVideoCodecVP8:
        reader.CopyTo(
            &video_header.video_type_header.emplace<RTPVideoHeaderVP8>());
        break;
      case kVideoCodecVP9:
        reader.CopyTo(
            &video_header.video_type_header.emplace<RTPVideoHeaderVP9>());
        break;
      case kVideoCodecH264:
        reader.CopyTo(
            &video_header.video_type_header.emplace<RTPVideoHeaderH264>());
        break;
      default:
        break;
    }

    reader.CopyTo(&video_header.frame_marking);

    // clang-format off
    auto frame = std::make_unique<video_coding::RtpFrameObject>(
        first_seq_num,
        last_seq_num,
        marker_bit,
        /*times_nacked=*/0,
        /*first_packet_received_time=*/0,
        /*last_packet_received_time=*/0,
        /*rtp_timestamp=*/0,
        /*ntp_time_ms=*/0,
        VideoSendTiming(),
        /*payload_type=*/0,
        codec,
        kVideoRotation_0,
        VideoContentType::UNSPECIFIED,
        video_header,
        /*color_space=*/absl::nullopt,
        GenerateRtpGenericFrameDescriptor(&reader),
        RtpPacketInfos(),
        EncodedImageBuffer::Create(/*size=*/0));
    // clang-format on

    reference_finder.ManageFrame(std::move(frame));
  }
}

}  // namespace webrtc
