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

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/video_coding/packet.h"

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
  friend RtpFrameObject;
  // Since we want the packet buffer to be as packet type agnostic
  // as possible we extract only the information needed in order
  // to determin whether a sequence of packets is continuous or not.
  struct ContinuityInfo {
    uint16_t seq_num = 0;
    bool frame_begin = false;
    bool frame_end = false;
    bool used = false;
    bool continuous = false;
  };

  bool ExpandBufferSize() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  bool IsContinuous(uint16_t seq_num) const EXCLUSIVE_LOCKS_REQUIRED(crit_);
  void FindCompleteFrames(uint16_t seq_num) EXCLUSIVE_LOCKS_REQUIRED(crit_);
  bool GetBitstream(const RtpFrameObject& frame, uint8_t* destination);
  void ReturnFrame(RtpFrameObject* frame);

  rtc::CriticalSection crit_;

  // Buffer size_ and max_size_ must always be a power of two.
  size_t size_ GUARDED_BY(crit_);
  const size_t max_size_;

  uint16_t last_seq_num_ GUARDED_BY(crit_);
  uint16_t first_seq_num_ GUARDED_BY(crit_);
  bool initialized_ GUARDED_BY(crit_);
  std::vector<VCMPacket> data_buffer_ GUARDED_BY(crit_);
  std::vector<ContinuityInfo> sequence_buffer_ GUARDED_BY(crit_);

  OnCompleteFrameCallback* const frame_callback_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_PACKET_BUFFER_H_
