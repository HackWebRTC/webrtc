/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_
#define WEBRTC_MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_

#include <array>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <utility>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/sequence_number_util.h"

namespace webrtc {
namespace video_coding {

class RtpFrameObject;
class OnCompleteFrameCallback;

class RtpFrameReferenceFinder {
 public:
  explicit RtpFrameReferenceFinder(OnCompleteFrameCallback* frame_callback);
  void ManageFrame(std::unique_ptr<RtpFrameObject> frame);

 private:
  static const uint16_t kPicIdLength = 1 << 7;
  static const uint8_t kMaxTemporalLayers = 5;
  static const int kMaxLayerInfo = 10;
  static const int kMaxStashedFrames = 10;
  static const int kMaxNotYetReceivedFrames = 20;
  static const int kMaxGofSaved = 15;

  rtc::CriticalSection crit_;

  // Retry finding references for all frames that previously didn't have
  // all information needed.
  void RetryStashedFrames() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Find references for generic frames.
  void ManageFrameGeneric(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Find references for Vp8 frames
  void ManageFrameVp8(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Updates all necessary state used to determine frame references
  // for Vp8 and then calls the |frame_callback| callback with the
  // completed frame.
  void CompletedFrameVp8(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Find references for Vp9 frames
  void ManageFrameVp9(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Unwrap the picture id and the frame references  and then call the
  // |frame_callback| callback with the completed frame.
  void CompletedFrameVp9(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Check if we are missing a frame necessary to determine the references
  // for this frame.
  bool MissingRequiredFrameVp9(uint16_t picture_id, const GofInfoVP9& gof)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Updates which frames that have been received. If there is a gap,
  // missing frames will be added to |missing_frames_for_layer_| or
  // if this is an already missing frame then it will be removed.
  void FrameReceivedVp9(uint16_t picture_id, const GofInfoVP9& gof)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Check if there is a frame with the up-switch flag set in the interval
  // (|pid_ref|, |picture_id|) with temporal layer smaller than |temporal_idx|.
  bool UpSwitchInIntervalVp9(uint16_t picture_id,
                             uint8_t temporal_idx,
                             uint16_t pid_ref) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // All picture ids are unwrapped to 16 bits.
  uint16_t UnwrapPictureId(uint16_t picture_id) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Holds the last sequence number of the last frame that has been created
  // given the last sequence number of a given keyframe.
  std::map<uint16_t, uint16_t, DescendingSeqNumComp<uint16_t>> last_seq_num_gop_
      GUARDED_BY(crit_);

  // Save the last picture id in order to detect when there is a gap in frames
  // that have not yet been fully received.
  int last_picture_id_ GUARDED_BY(crit_);

  // The last unwrapped picture id. Used to unwrap the picture id from a length
  // of |kPicIdLength| to 16 bits.
  int last_unwrap_ GUARDED_BY(crit_);

  // Frames earlier than the last received frame that have not yet been
  // fully received.
  std::set<uint16_t, DescendingSeqNumComp<uint16_t, kPicIdLength>>
      not_yet_received_frames_ GUARDED_BY(crit_);

  // Frames that have been fully received but didn't have all the information
  // needed to determine their references.
  std::queue<std::unique_ptr<RtpFrameObject>> stashed_frames_ GUARDED_BY(crit_);

  // Holds the information about the last completed frame for a given temporal
  // layer given a Tl0 picture index.
  std::map<uint8_t,
           std::array<int16_t, kMaxTemporalLayers>,
           DescendingSeqNumComp<uint8_t>>
      layer_info_ GUARDED_BY(crit_);

  // Where the current scalability structure is in the
  // |scalability_structures_| array.
  uint8_t current_ss_idx_;

  // Holds received scalability structures.
  std::array<GofInfoVP9, kMaxGofSaved> scalability_structures_
      GUARDED_BY(crit_);

  // Holds the picture id and the Gof information for a given TL0 picture index.
  std::map<uint8_t,
           std::pair<uint16_t, GofInfoVP9*>,
           DescendingSeqNumComp<uint8_t>>
      gof_info_ GUARDED_BY(crit_);

  // Keep track of which picture id and which temporal layer that had the
  // up switch flag set.
  std::map<uint16_t, uint8_t> up_switch_ GUARDED_BY(crit_);

  // For every temporal layer, keep a set of which frames that are missing.
  std::array<std::set<uint16_t, DescendingSeqNumComp<uint16_t, kPicIdLength>>,
             kMaxTemporalLayers>
      missing_frames_for_layer_ GUARDED_BY(crit_);

  OnCompleteFrameCallback* frame_callback_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_
