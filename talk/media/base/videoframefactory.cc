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

#include "talk/media/base/videoframefactory.h"

#include <algorithm>
#include "talk/media/base/videocapturer.h"

namespace cricket {

VideoFrame* VideoFrameFactory::CreateAliasedFrame(
    const CapturedFrame* input_frame,
    int cropped_input_width,
    int cropped_input_height,
    int output_width,
    int output_height) const {
  rtc::scoped_ptr<VideoFrame> cropped_input_frame(CreateAliasedFrame(
      input_frame, cropped_input_width, cropped_input_height));
  if (!cropped_input_frame)
    return nullptr;

  if (cropped_input_width == output_width &&
      cropped_input_height == output_height) {
    // No scaling needed.
    return cropped_input_frame.release();
  }

  // If the frame is rotated, we need to switch the width and height.
  if (apply_rotation_ &&
      (input_frame->GetRotation() == webrtc::kVideoRotation_90 ||
       input_frame->GetRotation() == webrtc::kVideoRotation_270)) {
    std::swap(output_width, output_height);
  }

  // Create and stretch the output frame if it has not been created yet, is
  // still in use by others, or its size is not same as the expected.
  if (!output_frame_ || !output_frame_->IsExclusive() ||
      output_frame_->GetWidth() != static_cast<size_t>(output_width) ||
      output_frame_->GetHeight() != static_cast<size_t>(output_height)) {
    output_frame_.reset(
        cropped_input_frame->Stretch(output_width, output_height, true, true));
    if (!output_frame_) {
      LOG(LS_WARNING) << "Failed to stretch frame to " << output_width << "x"
                      << output_height;
      return NULL;
    }
  } else {
    cropped_input_frame->StretchToFrame(output_frame_.get(), true, true);
    output_frame_->SetTimeStamp(cropped_input_frame->GetTimeStamp());
  }
  return output_frame_->Copy();
}

}  // namespace cricket
