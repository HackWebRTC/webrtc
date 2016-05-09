/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_PACKET_BUFFER_H_
#define WEBRTC_MODULES_VIDEO_CODING_PACKET_BUFFER_H_

#include <array>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/packet.h"
#include "webrtc/modules/video_coding/sequence_number_util.h"

namespace webrtc {
namespace video_coding {

class FrameObject;
class RtpFrameObject;

class OnCompleteFrameCallback {
 public:
  virtual ~OnCompleteFrameCallback() {}
  virtual void OnCompleteFrame(std::unique_ptr<FrameObject> frame) = 0;
};

class PacketBuffer {
 public:
  // Both |start_buffer_size| and |max_buffer_size| must be a power of 2.
  PacketBuffer(size_t start_buffer_size,
               size_t max_buffer_size,
               OnCompleteFrameCallback* frame_callback);

  bool InsertPacket(const VCMPacket& packet);
  void ClearTo(uint16_t seq_num);
  void Flush();

 private:
  static const uint16_t kPicIdLength = 1 << 7;
  static const uint8_t kMaxTemporalLayers = 5;
  static const int kMaxStashedFrames = 10;
  static const int kMaxLayerInfo = 10;
  static const int kMaxNotYetReceivedFrames = 20;
  static const int kMaxGofSaved = 15;

  friend RtpFrameObject;
  // Since we want the packet buffer to be as packet type agnostic
  // as possible we extract only the information needed in order
  // to determine whether a sequence of packets is continuous or not.
  struct ContinuityInfo {
    // The sequence number of the packet.
    uint16_t seq_num = 0;

    // If this is the first packet of the frame.
    bool frame_begin = false;

    // If this is the last packet of the frame.
    bool frame_end = false;

    // If this slot is currently used.
    bool used = false;

    // If all its previous packets have been inserted into the packet buffer.
    bool continuous = false;

    // If this packet has been used to create a frame already.
    bool frame_created = false;
  };

  // Expand the buffer.
  bool ExpandBufferSize() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Test if all previous packets has arrived for the given sequence number.
  bool IsContinuous(uint16_t seq_num) const EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Test if all packets of a frame has arrived, and if so, creates a frame.
  // May create multiple frames per invocation.
  void FindFrames(uint16_t seq_num) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Copy the bitstream for |frame| to |destination|.
  bool GetBitstream(const RtpFrameObject& frame, uint8_t* destination);

  // Mark all slots used by |frame| as not used.
  void ReturnFrame(RtpFrameObject* frame);

  // Find the references for this frame.
  void ManageFrame(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

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
  uint16_t UnwrapPictureId(uint16_t picture_id)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  rtc::CriticalSection crit_;

  // Buffer size_ and max_size_ must always be a power of two.
  size_t size_ GUARDED_BY(crit_);
  const size_t max_size_;

  // The fist sequence number currently in the buffer.
  uint16_t first_seq_num_ GUARDED_BY(crit_);

  // The last sequence number currently in the buffer.
  uint16_t last_seq_num_ GUARDED_BY(crit_);

  // If the packet buffer has received its first packet.
  bool first_packet_received_ GUARDED_BY(crit_);

  // Buffer that holds the inserted packets.
  std::vector<VCMPacket> data_buffer_ GUARDED_BY(crit_);

  // Buffer that holds the information about which slot that is currently in use
  // and information needed to determine the continuity between packets.
  std::vector<ContinuityInfo> sequence_buffer_ GUARDED_BY(crit_);

  // The callback that is called when a frame has been created and all its
  // references has been found.
  OnCompleteFrameCallback* const frame_callback_;

  // Holds the last sequence number of the last frame that has been created
  // given the last sequence number of a given keyframe.
  std::map<uint16_t, uint16_t, DescendingSeqNumComp<uint16_t>>
    last_seq_num_gop_ GUARDED_BY(crit_);

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
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_PACKET_BUFFER_H_
