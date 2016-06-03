/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/rtp_frame_reference_finder.h"

#include <algorithm>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/packet_buffer.h"

namespace webrtc {
namespace video_coding {

RtpFrameReferenceFinder::RtpFrameReferenceFinder(
    OnCompleteFrameCallback* frame_callback)
    : last_picture_id_(-1),
      last_unwrap_(-1),
      current_ss_idx_(0),
      frame_callback_(frame_callback) {}

void RtpFrameReferenceFinder::ManageFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  rtc::CritScope lock(&crit_);
  switch (frame->codec_type()) {
    case kVideoCodecULPFEC:
    case kVideoCodecRED:
    case kVideoCodecUnknown:
      RTC_NOTREACHED();
      break;
    case kVideoCodecVP8:
      ManageFrameVp8(std::move(frame));
      break;
    case kVideoCodecVP9:
      ManageFrameVp9(std::move(frame));
      break;
    case kVideoCodecH264:
    case kVideoCodecI420:
    case kVideoCodecGeneric:
      ManageFrameGeneric(std::move(frame), kNoPictureId);
      break;
  }
}

void RtpFrameReferenceFinder::RetryStashedFrames() {
  size_t num_stashed_frames = stashed_frames_.size();

  // Clean up stashed frames if there are too many.
  while (stashed_frames_.size() > kMaxStashedFrames)
    stashed_frames_.pop();

  // Since frames are stashed if there is not enough data to determine their
  // frame references we should at most check |stashed_frames_.size()| in
  // order to not pop and push frames in and endless loop.
  for (size_t i = 0; i < num_stashed_frames && !stashed_frames_.empty(); ++i) {
    std::unique_ptr<RtpFrameObject> frame = std::move(stashed_frames_.front());
    stashed_frames_.pop();
    ManageFrame(std::move(frame));
  }
}

void RtpFrameReferenceFinder::ManageFrameGeneric(
    std::unique_ptr<RtpFrameObject> frame,
    int picture_id) {
  // If |picture_id| is specified then we use that to set the frame references,
  // otherwise we use sequence number.
  if (picture_id != kNoPictureId) {
    if (last_unwrap_ == -1)
      last_unwrap_ = picture_id;

    frame->picture_id = UnwrapPictureId(picture_id % kPicIdLength);
    frame->num_references = frame->frame_type() == kVideoFrameKey ? 0 : 1;
    frame->references[0] = frame->picture_id - 1;
    frame_callback_->OnCompleteFrame(std::move(frame));
    return;
  }

  if (frame->frame_type() == kVideoFrameKey)
    last_seq_num_gop_[frame->last_seq_num()] = frame->last_seq_num();

  // We have received a frame but not yet a keyframe, stash this frame.
  if (last_seq_num_gop_.empty()) {
    stashed_frames_.emplace(std::move(frame));
    return;
  }

  // Clean up info for old keyframes but make sure to keep info
  // for the last keyframe.
  auto clean_to = last_seq_num_gop_.lower_bound(frame->last_seq_num() - 100);
  if (clean_to != last_seq_num_gop_.end())
    last_seq_num_gop_.erase(last_seq_num_gop_.begin(), clean_to);

  // Find the last sequence number of the last frame for the keyframe
  // that this frame indirectly references.
  auto seq_num_it = last_seq_num_gop_.upper_bound(frame->last_seq_num());
  seq_num_it--;

  // Make sure the packet sequence numbers are continuous, otherwise stash
  // this frame.
  if (frame->frame_type() == kVideoFrameDelta) {
    if (seq_num_it->second !=
        static_cast<uint16_t>(frame->first_seq_num() - 1)) {
      stashed_frames_.emplace(std::move(frame));
      return;
    }
  }

  RTC_DCHECK(AheadOrAt(frame->last_seq_num(), seq_num_it->first));

  // Since keyframes can cause reordering we can't simply assign the
  // picture id according to some incrementing counter.
  frame->picture_id = frame->last_seq_num();
  frame->num_references = frame->frame_type() == kVideoFrameDelta;
  frame->references[0] = seq_num_it->second;
  seq_num_it->second = frame->picture_id;

  last_picture_id_ = frame->picture_id;
  frame_callback_->OnCompleteFrame(std::move(frame));
  RetryStashedFrames();
}

void RtpFrameReferenceFinder::ManageFrameVp8(
    std::unique_ptr<RtpFrameObject> frame) {
  RTPVideoTypeHeader* rtp_codec_header = frame->GetCodecHeader();
  if (!rtp_codec_header)
    return;

  const RTPVideoHeaderVP8& codec_header = rtp_codec_header->VP8;

  if (codec_header.pictureId == kNoPictureId ||
      codec_header.temporalIdx == kNoTemporalIdx ||
      codec_header.tl0PicIdx == kNoTl0PicIdx) {
    ManageFrameGeneric(std::move(frame), codec_header.pictureId);
    return;
  }

  frame->picture_id = codec_header.pictureId % kPicIdLength;

  if (last_unwrap_ == -1)
    last_unwrap_ = codec_header.pictureId;

  if (last_picture_id_ == -1)
    last_picture_id_ = frame->picture_id;

  // Find if there has been a gap in fully received frames and save the picture
  // id of those frames in |not_yet_received_frames_|.
  if (AheadOf<uint16_t, kPicIdLength>(frame->picture_id, last_picture_id_)) {
    last_picture_id_ = Add<kPicIdLength>(last_picture_id_, 1);
    while (last_picture_id_ != frame->picture_id) {
      not_yet_received_frames_.insert(last_picture_id_);
      last_picture_id_ = Add<kPicIdLength>(last_picture_id_, 1);
    }
  }

  // Clean up info for base layers that are too old.
  uint8_t old_tl0_pic_idx = codec_header.tl0PicIdx - kMaxLayerInfo;
  auto clean_layer_info_to = layer_info_.lower_bound(old_tl0_pic_idx);
  layer_info_.erase(layer_info_.begin(), clean_layer_info_to);

  // Clean up info about not yet received frames that are too old.
  uint16_t old_picture_id =
      Subtract<kPicIdLength>(frame->picture_id, kMaxNotYetReceivedFrames);
  auto clean_frames_to = not_yet_received_frames_.lower_bound(old_picture_id);
  not_yet_received_frames_.erase(not_yet_received_frames_.begin(),
                                 clean_frames_to);

  if (frame->frame_type() == kVideoFrameKey) {
    frame->num_references = 0;
    layer_info_[codec_header.tl0PicIdx].fill(-1);
    CompletedFrameVp8(std::move(frame));
    return;
  }

  auto layer_info_it = layer_info_.find(codec_header.temporalIdx == 0
                                            ? codec_header.tl0PicIdx - 1
                                            : codec_header.tl0PicIdx);

  // If we don't have the base layer frame yet, stash this frame.
  if (layer_info_it == layer_info_.end()) {
    stashed_frames_.emplace(std::move(frame));
    return;
  }

  // A non keyframe base layer frame has been received, copy the layer info
  // from the previous base layer frame and set a reference to the previous
  // base layer frame.
  if (codec_header.temporalIdx == 0) {
    layer_info_it =
        layer_info_
            .insert(make_pair(codec_header.tl0PicIdx, layer_info_it->second))
            .first;
    frame->num_references = 1;
    frame->references[0] = layer_info_it->second[0];
    CompletedFrameVp8(std::move(frame));
    return;
  }

  // Layer sync frame, this frame only references its base layer frame.
  if (codec_header.layerSync) {
    frame->num_references = 1;
    frame->references[0] = layer_info_it->second[0];

    CompletedFrameVp8(std::move(frame));
    return;
  }

  // Find all references for this frame.
  frame->num_references = 0;
  for (uint8_t layer = 0; layer <= codec_header.temporalIdx; ++layer) {
    RTC_DCHECK_NE(-1, layer_info_it->second[layer]);

    // If we have not yet received a frame between this frame and the referenced
    // frame then we have to wait for that frame to be completed first.
    auto not_received_frame_it =
        not_yet_received_frames_.upper_bound(layer_info_it->second[layer]);
    if (not_received_frame_it != not_yet_received_frames_.end() &&
        AheadOf<uint16_t, kPicIdLength>(frame->picture_id,
                                        *not_received_frame_it)) {
      stashed_frames_.emplace(std::move(frame));
      return;
    }

    ++frame->num_references;
    frame->references[layer] = layer_info_it->second[layer];
  }

  CompletedFrameVp8(std::move(frame));
}

void RtpFrameReferenceFinder::CompletedFrameVp8(
    std::unique_ptr<RtpFrameObject> frame) {
  RTPVideoTypeHeader* rtp_codec_header = frame->GetCodecHeader();
  if (!rtp_codec_header)
    return;

  const RTPVideoHeaderVP8& codec_header = rtp_codec_header->VP8;

  uint8_t tl0_pic_idx = codec_header.tl0PicIdx;
  uint8_t temporal_index = codec_header.temporalIdx;
  auto layer_info_it = layer_info_.find(tl0_pic_idx);

  // Update this layer info and newer.
  while (layer_info_it != layer_info_.end()) {
    if (layer_info_it->second[temporal_index] != -1 &&
        AheadOf<uint16_t, kPicIdLength>(layer_info_it->second[temporal_index],
                                        frame->picture_id)) {
      // The frame was not newer, then no subsequent layer info have to be
      // update.
      break;
    }

    layer_info_it->second[codec_header.temporalIdx] = frame->picture_id;
    ++tl0_pic_idx;
    layer_info_it = layer_info_.find(tl0_pic_idx);
  }
  not_yet_received_frames_.erase(frame->picture_id);

  for (size_t i = 0; i < frame->num_references; ++i)
    frame->references[i] = UnwrapPictureId(frame->references[i]);
  frame->picture_id = UnwrapPictureId(frame->picture_id);

  frame_callback_->OnCompleteFrame(std::move(frame));
  RetryStashedFrames();
}

void RtpFrameReferenceFinder::ManageFrameVp9(
    std::unique_ptr<RtpFrameObject> frame) {
  RTPVideoTypeHeader* rtp_codec_header = frame->GetCodecHeader();
  if (!rtp_codec_header)
    return;

  const RTPVideoHeaderVP9& codec_header = rtp_codec_header->VP9;

  if (codec_header.picture_id == kNoPictureId ||
      codec_header.temporal_idx == kNoTemporalIdx) {
    ManageFrameGeneric(std::move(frame), codec_header.picture_id);
    return;
  }

  frame->spatial_layer = codec_header.spatial_idx;
  frame->inter_layer_predicted = codec_header.inter_layer_predicted;
  frame->picture_id = codec_header.picture_id % kPicIdLength;

  if (last_unwrap_ == -1)
    last_unwrap_ = codec_header.picture_id;

  if (last_picture_id_ == -1)
    last_picture_id_ = frame->picture_id;

  if (codec_header.flexible_mode) {
    frame->num_references = codec_header.num_ref_pics;
    for (size_t i = 0; i < frame->num_references; ++i) {
      frame->references[i] =
          Subtract<1 << 16>(frame->picture_id, codec_header.pid_diff[i]);
    }

    CompletedFrameVp9(std::move(frame));
    return;
  }

  if (codec_header.ss_data_available) {
    // Scalability structures can only be sent with tl0 frames.
    if (codec_header.temporal_idx != 0) {
      LOG(LS_WARNING) << "Received scalability structure on a non base layer"
                         " frame. Scalability structure ignored.";
    } else {
      current_ss_idx_ = Add<kMaxGofSaved>(current_ss_idx_, 1);
      scalability_structures_[current_ss_idx_] = codec_header.gof;
      scalability_structures_[current_ss_idx_].pid_start = frame->picture_id;

      auto pid_and_gof = std::make_pair(
          frame->picture_id, &scalability_structures_[current_ss_idx_]);
      gof_info_.insert(std::make_pair(codec_header.tl0_pic_idx, pid_and_gof));
    }
  }

  // Clean up info for base layers that are too old.
  uint8_t old_tl0_pic_idx = codec_header.tl0_pic_idx - kMaxGofSaved;
  auto clean_gof_info_to = gof_info_.lower_bound(old_tl0_pic_idx);
  gof_info_.erase(gof_info_.begin(), clean_gof_info_to);

  if (frame->frame_type() == kVideoFrameKey) {
    // When using GOF all keyframes must include the scalability structure.
    if (!codec_header.ss_data_available)
      LOG(LS_WARNING) << "Received keyframe without scalability structure";

    frame->num_references = 0;
    GofInfoVP9* gof = gof_info_.find(codec_header.tl0_pic_idx)->second.second;
    FrameReceivedVp9(frame->picture_id, *gof);
    CompletedFrameVp9(std::move(frame));
    return;
  }

  auto gof_info_it = gof_info_.find(
      (codec_header.temporal_idx == 0 && !codec_header.ss_data_available)
          ? codec_header.tl0_pic_idx - 1
          : codec_header.tl0_pic_idx);

  // Gof info for this frame is not available yet, stash this frame.
  if (gof_info_it == gof_info_.end()) {
    stashed_frames_.emplace(std::move(frame));
    return;
  }

  GofInfoVP9* gof = gof_info_it->second.second;
  uint16_t picture_id_tl0 = gof_info_it->second.first;

  FrameReceivedVp9(frame->picture_id, *gof);

  // Make sure we don't miss any frame that could potentially have the
  // up switch flag set.
  if (MissingRequiredFrameVp9(frame->picture_id, *gof)) {
    stashed_frames_.emplace(std::move(frame));
    return;
  }

  if (codec_header.temporal_up_switch) {
    auto pid_tidx =
        std::make_pair(frame->picture_id, codec_header.temporal_idx);
    up_switch_.insert(pid_tidx);
  }

  // If this is a base layer frame that contains a scalability structure
  // then gof info has already been inserted earlier, so we only want to
  // insert if we haven't done so already.
  if (codec_header.temporal_idx == 0 && !codec_header.ss_data_available) {
    auto pid_and_gof = std::make_pair(frame->picture_id, gof);
    gof_info_.insert(std::make_pair(codec_header.tl0_pic_idx, pid_and_gof));
  }

  // Clean out old info about up switch frames.
  uint16_t old_picture_id = Subtract<kPicIdLength>(last_picture_id_, 50);
  auto up_switch_erase_to = up_switch_.lower_bound(old_picture_id);
  up_switch_.erase(up_switch_.begin(), up_switch_erase_to);

  RTC_DCHECK(
      (AheadOrAt<uint16_t, kPicIdLength>(frame->picture_id, picture_id_tl0)));

  size_t diff =
      ForwardDiff<uint16_t, kPicIdLength>(gof->pid_start, frame->picture_id);
  size_t gof_idx = diff % gof->num_frames_in_gof;

  // Populate references according to the scalability structure.
  frame->num_references = gof->num_ref_pics[gof_idx];
  for (size_t i = 0; i < frame->num_references; ++i) {
    frame->references[i] =
        Subtract<kPicIdLength>(frame->picture_id, gof->pid_diff[gof_idx][i]);

    // If this is a reference to a frame earlier than the last up switch point,
    // then ignore this reference.
    if (UpSwitchInIntervalVp9(frame->picture_id, codec_header.temporal_idx,
                              frame->references[i])) {
      --frame->num_references;
    }
  }

  CompletedFrameVp9(std::move(frame));
}

bool RtpFrameReferenceFinder::MissingRequiredFrameVp9(uint16_t picture_id,
                                                      const GofInfoVP9& gof) {
  size_t diff = ForwardDiff<uint16_t, kPicIdLength>(gof.pid_start, picture_id);
  size_t gof_idx = diff % gof.num_frames_in_gof;
  size_t temporal_idx = gof.temporal_idx[gof_idx];

  // For every reference this frame has, check if there is a frame missing in
  // the interval (|ref_pid|, |picture_id|) in any of the lower temporal
  // layers. If so, we are missing a required frame.
  uint8_t num_references = gof.num_ref_pics[gof_idx];
  for (size_t i = 0; i < num_references; ++i) {
    uint16_t ref_pid =
        Subtract<kPicIdLength>(picture_id, gof.pid_diff[gof_idx][i]);
    for (size_t l = 0; l < temporal_idx; ++l) {
      auto missing_frame_it = missing_frames_for_layer_[l].lower_bound(ref_pid);
      if (missing_frame_it != missing_frames_for_layer_[l].end() &&
          AheadOf<uint16_t, kPicIdLength>(picture_id, *missing_frame_it)) {
        return true;
      }
    }
  }
  return false;
}

void RtpFrameReferenceFinder::FrameReceivedVp9(uint16_t picture_id,
                                               const GofInfoVP9& gof) {
  RTC_DCHECK_NE(-1, last_picture_id_);

  // If there is a gap, find which temporal layer the missing frames
  // belong to and add the frame as missing for that temporal layer.
  // Otherwise, remove this frame from the set of missing frames.
  if (AheadOf<uint16_t, kPicIdLength>(picture_id, last_picture_id_)) {
    size_t diff =
        ForwardDiff<uint16_t, kPicIdLength>(gof.pid_start, last_picture_id_);
    size_t gof_idx = diff % gof.num_frames_in_gof;

    last_picture_id_ = Add<kPicIdLength>(last_picture_id_, 1);
    while (last_picture_id_ != picture_id) {
      ++gof_idx;
      RTC_DCHECK_NE(0ul, gof_idx % gof.num_frames_in_gof);
      size_t temporal_idx = gof.temporal_idx[gof_idx];
      missing_frames_for_layer_[temporal_idx].insert(last_picture_id_);
      last_picture_id_ = Add<kPicIdLength>(last_picture_id_, 1);
    }
  } else {
    size_t diff =
        ForwardDiff<uint16_t, kPicIdLength>(gof.pid_start, picture_id);
    size_t gof_idx = diff % gof.num_frames_in_gof;
    size_t temporal_idx = gof.temporal_idx[gof_idx];
    missing_frames_for_layer_[temporal_idx].erase(picture_id);
  }
}

bool RtpFrameReferenceFinder::UpSwitchInIntervalVp9(uint16_t picture_id,
                                                    uint8_t temporal_idx,
                                                    uint16_t pid_ref) {
  for (auto up_switch_it = up_switch_.upper_bound(pid_ref);
       up_switch_it != up_switch_.end() &&
       AheadOf<uint16_t, kPicIdLength>(picture_id, up_switch_it->first);
       ++up_switch_it) {
    if (up_switch_it->second < temporal_idx)
      return true;
  }

  return false;
}

void RtpFrameReferenceFinder::CompletedFrameVp9(
    std::unique_ptr<RtpFrameObject> frame) {
  for (size_t i = 0; i < frame->num_references; ++i)
    frame->references[i] = UnwrapPictureId(frame->references[i]);
  frame->picture_id = UnwrapPictureId(frame->picture_id);

  frame_callback_->OnCompleteFrame(std::move(frame));
  RetryStashedFrames();
}

uint16_t RtpFrameReferenceFinder::UnwrapPictureId(uint16_t picture_id) {
  RTC_DCHECK_NE(-1, last_unwrap_);

  uint16_t unwrap_truncated = last_unwrap_ % kPicIdLength;
  uint16_t diff = MinDiff<uint16_t, kPicIdLength>(unwrap_truncated, picture_id);

  if (AheadOf<uint16_t, kPicIdLength>(picture_id, unwrap_truncated))
    last_unwrap_ = Add<1 << 16>(last_unwrap_, diff);
  else
    last_unwrap_ = Subtract<1 << 16>(last_unwrap_, diff);

  return last_unwrap_;
}

}  // namespace video_coding
}  // namespace webrtc
