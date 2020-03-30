/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "modules/rtp_rtcp/source/transformable_encoded_frame.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {
namespace {

std::unique_ptr<RTPFragmentationHeader> CreateFragmentationHeader(
    const RTPFragmentationHeader* fragmentation_header) {
  if (!fragmentation_header)
    return nullptr;
  auto ret = std::make_unique<RTPFragmentationHeader>();
  ret->CopyFrom(*fragmentation_header);
  return ret;
}

class TransformableVideoSenderFrame : public TransformableVideoFrameInterface {
 public:
  TransformableVideoSenderFrame(
      const EncodedImage& encoded_image,
      const RTPVideoHeader& video_header,
      int payload_type,
      absl::optional<VideoCodecType> codec_type,
      uint32_t rtp_timestamp,
      const RTPFragmentationHeader* fragmentation_header,
      absl::optional<int64_t> expected_retransmission_time_ms,
      uint32_t ssrc)
      : encoded_data_(encoded_image.GetEncodedData()),
        header_(video_header),
        frame_type_(encoded_image._frameType),
        payload_type_(payload_type),
        codec_type_(codec_type),
        timestamp_(rtp_timestamp),
        capture_time_ms_(encoded_image.capture_time_ms_),
        expected_retransmission_time_ms_(expected_retransmission_time_ms),
        ssrc_(ssrc),
        fragmentation_header_(CreateFragmentationHeader(fragmentation_header)) {
  }

  ~TransformableVideoSenderFrame() override = default;

  // Implements TransformableVideoFrameInterface.
  rtc::ArrayView<const uint8_t> GetData() const override {
    return *encoded_data_;
  }

  void SetData(rtc::ArrayView<const uint8_t> data) override {
    encoded_data_ = EncodedImageBuffer::Create(data.data(), data.size());
  }

  uint32_t GetTimestamp() const override { return timestamp_; }
  uint32_t GetSsrc() const override { return ssrc_; }

  bool IsKeyFrame() const override {
    return frame_type_ == VideoFrameType::kVideoFrameKey;
  }

  std::vector<uint8_t> GetAdditionalData() const override {
    return RtpDescriptorAuthentication(header_);
  }

  const RTPVideoHeader& GetHeader() const { return header_; }
  int GetPayloadType() const { return payload_type_; }
  absl::optional<VideoCodecType> GetCodecType() const { return codec_type_; }
  int64_t GetCaptureTimeMs() const { return capture_time_ms_; }

  RTPFragmentationHeader* GetFragmentationHeader() const {
    return fragmentation_header_.get();
  }

  const absl::optional<int64_t>& GetExpectedRetransmissionTimeMs() const {
    return expected_retransmission_time_ms_;
  }

 private:
  rtc::scoped_refptr<EncodedImageBufferInterface> encoded_data_;
  const RTPVideoHeader header_;
  const VideoFrameType frame_type_;
  const int payload_type_;
  const absl::optional<VideoCodecType> codec_type_ = absl::nullopt;
  const uint32_t timestamp_;
  const int64_t capture_time_ms_;
  const absl::optional<int64_t> expected_retransmission_time_ms_;
  const uint32_t ssrc_;
  const std::unique_ptr<RTPFragmentationHeader> fragmentation_header_;
};
}  // namespace

RTPSenderVideoFrameTransformerDelegate::RTPSenderVideoFrameTransformerDelegate(
    RTPSenderVideo* sender,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer)
    : sender_(sender), frame_transformer_(std::move(frame_transformer)) {}

void RTPSenderVideoFrameTransformerDelegate::Init() {
  frame_transformer_->RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>(this));
}

bool RTPSenderVideoFrameTransformerDelegate::TransformFrame(
    int payload_type,
    absl::optional<VideoCodecType> codec_type,
    uint32_t rtp_timestamp,
    const EncodedImage& encoded_image,
    const RTPFragmentationHeader* fragmentation,
    RTPVideoHeader video_header,
    absl::optional<int64_t> expected_retransmission_time_ms,
    uint32_t ssrc) {
  if (!encoder_queue_)
    encoder_queue_ = TaskQueueBase::Current();
  // TODO(bugs.webrtc.org/11380) remove once this version of TransformFrame() is
  // deprecated.
  frame_transformer_->TransformFrame(
      std::make_unique<TransformableEncodedFrame>(
          encoded_image.GetEncodedData(), video_header, payload_type,
          codec_type, rtp_timestamp, encoded_image.capture_time_ms_,
          fragmentation, expected_retransmission_time_ms),
      RtpDescriptorAuthentication(video_header), ssrc);
  frame_transformer_->Transform(std::make_unique<TransformableVideoSenderFrame>(
      encoded_image, video_header, payload_type, codec_type, rtp_timestamp,
      fragmentation, expected_retransmission_time_ms, ssrc));
  return true;
}

void RTPSenderVideoFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  rtc::CritScope lock(&sender_lock_);

  // The encoder queue gets destroyed after the sender; as long as the sender is
  // alive, it's safe to post.
  if (!sender_)
    return;
  rtc::scoped_refptr<RTPSenderVideoFrameTransformerDelegate> delegate = this;
  encoder_queue_->PostTask(ToQueuedTask(
      [delegate = std::move(delegate), frame = std::move(frame)]() mutable {
        delegate->SendVideo(std::move(frame));
      }));
}

void RTPSenderVideoFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<TransformableFrameInterface> frame) {
  rtc::CritScope lock(&sender_lock_);

  // The encoder queue gets destroyed after the sender; as long as the sender is
  // alive, it's safe to post.
  if (!sender_)
    return;
  rtc::scoped_refptr<RTPSenderVideoFrameTransformerDelegate> delegate = this;
  encoder_queue_->PostTask(ToQueuedTask(
      [delegate = std::move(delegate), frame = std::move(frame)]() mutable {
        delegate->SendVideo(std::move(frame));
      }));
}

void RTPSenderVideoFrameTransformerDelegate::SendVideo(
    std::unique_ptr<video_coding::EncodedFrame> frame) const {
  RTC_CHECK(encoder_queue_->IsCurrent());
  rtc::CritScope lock(&sender_lock_);
  if (!sender_)
    return;
  auto* transformed_frame =
      static_cast<TransformableEncodedFrame*>(frame.get());
  sender_->SendVideo(
      transformed_frame->PayloadType(), transformed_frame->codec_type(),
      transformed_frame->Timestamp(), transformed_frame->capture_time_ms(),
      transformed_frame->EncodedImage(),
      transformed_frame->fragmentation_header(),
      transformed_frame->video_header(),
      transformed_frame->expected_retransmission_time_ms());
}

void RTPSenderVideoFrameTransformerDelegate::SendVideo(
    std::unique_ptr<TransformableFrameInterface> transformed_frame) const {
  RTC_CHECK(encoder_queue_->IsCurrent());
  rtc::CritScope lock(&sender_lock_);
  if (!sender_)
    return;
  auto* transformed_video_frame =
      static_cast<TransformableVideoSenderFrame*>(transformed_frame.get());
  sender_->SendVideo(
      transformed_video_frame->GetPayloadType(),
      transformed_video_frame->GetCodecType(),
      transformed_video_frame->GetTimestamp(),
      transformed_video_frame->GetCaptureTimeMs(),
      transformed_video_frame->GetData(),
      transformed_video_frame->GetFragmentationHeader(),
      transformed_video_frame->GetHeader(),
      transformed_video_frame->GetExpectedRetransmissionTimeMs());
}

void RTPSenderVideoFrameTransformerDelegate::SetVideoStructureUnderLock(
    const FrameDependencyStructure* video_structure) {
  rtc::CritScope lock(&sender_lock_);
  RTC_CHECK(sender_);
  sender_->SetVideoStructureUnderLock(video_structure);
}

void RTPSenderVideoFrameTransformerDelegate::Reset() {
  frame_transformer_->UnregisterTransformedFrameCallback();
  frame_transformer_ = nullptr;
  {
    rtc::CritScope lock(&sender_lock_);
    sender_ = nullptr;
  }
}
}  // namespace webrtc
