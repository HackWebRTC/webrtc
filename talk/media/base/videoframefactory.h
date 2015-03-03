/*
 * libjingle
 * Copyright 2014 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_MEDIA_BASE_VIDEOFRAMEFACTORY_H_
#define TALK_MEDIA_BASE_VIDEOFRAMEFACTORY_H_

#include "talk/media/base/videoframe.h"
#include "webrtc/base/scoped_ptr.h"

namespace cricket {

struct CapturedFrame;
class VideoFrame;

// Creates cricket::VideoFrames, or a subclass of cricket::VideoFrame
// depending on the subclass of VideoFrameFactory.
class VideoFrameFactory {
 public:
  VideoFrameFactory() : apply_rotation_(true) {}
  virtual ~VideoFrameFactory() {}

  // The returned frame aliases the aliased_frame if the input color
  // space allows for aliasing, otherwise a color conversion will
  // occur. Returns NULL if conversion fails.

  // The returned frame will be a center crop of |input_frame| with
  // size |cropped_width| x |cropped_height|.
  virtual VideoFrame* CreateAliasedFrame(const CapturedFrame* input_frame,
                                         int cropped_width,
                                         int cropped_height) const = 0;

  // The returned frame will be a center crop of |input_frame| with size
  // |cropped_width| x |cropped_height|, scaled to |output_width| x
  // |output_height|.
  virtual VideoFrame* CreateAliasedFrame(const CapturedFrame* input_frame,
                                         int cropped_input_width,
                                         int cropped_input_height,
                                         int output_width,
                                         int output_height) const;

  void SetApplyRotation(bool enable) { apply_rotation_ = enable; }

 protected:
  bool apply_rotation_;

 private:
  // An internal frame buffer to avoid reallocations. It is mutable because it
  // does not affect behaviour, only performance.
  mutable rtc::scoped_ptr<VideoFrame> output_frame_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_VIDEOFRAMEFACTORY_H_
