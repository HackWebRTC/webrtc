/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_ENCODED_FRAME_H_
#define API_VIDEO_ENCODED_FRAME_H_

#include "modules/video_coding/encoded_frame.h"

namespace webrtc {
namespace video_coding {

// TODO(philipel): Rename FrameObject to EncodedFrame.
// TODO(philipel): Remove webrtc::VCMEncodedFrame inheritance.
class FrameObject : public webrtc::VCMEncodedFrame {
 public:
  static const uint8_t kMaxFrameReferences = 5;

  FrameObject() = default;
  virtual ~FrameObject() {}

  virtual bool GetBitstream(uint8_t* destination) const = 0;

  // The capture timestamp of this frame.
  virtual uint32_t Timestamp() const = 0;

  // When this frame was received.
  virtual int64_t ReceivedTime() const = 0;

  // When this frame should be rendered.
  virtual int64_t RenderTime() const = 0;

  // This information is currently needed by the timing calculation class.
  // TODO(philipel): Remove this function when a new timing class has
  //                 been implemented.
  virtual bool delayed_by_retransmission() const { return 0; }

  size_t size() const { return _length; }

  bool is_keyframe() const { return num_references == 0; }

  // The tuple (|picture_id|, |spatial_layer|) uniquely identifies a frame
  // object. For codec types that don't necessarily have picture ids they
  // have to be constructed from the header data relevant to that codec.
  int64_t picture_id = 0;
  uint8_t spatial_layer = 0;
  uint32_t timestamp = 0;

  // TODO(philipel): Add simple modify/access functions to prevent adding too
  // many |references|.
  size_t num_references = 0;
  int64_t references[kMaxFrameReferences];
  bool inter_layer_predicted = false;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // API_VIDEO_ENCODED_FRAME_H_
