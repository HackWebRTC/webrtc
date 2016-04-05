/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_VIDEOADAPTER_H_
#define WEBRTC_MEDIA_BASE_VIDEOADAPTER_H_

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/optional.h"
#include "webrtc/media/base/videocommon.h"

namespace cricket {

// VideoAdapter adapts an input video frame to an output frame based on the
// specified input and output formats. The adaptation includes dropping frames
// to reduce frame rate and scaling frames.
// VideoAdapter is thread safe.
class VideoAdapter {
 public:
  VideoAdapter();
  virtual ~VideoAdapter();

  // Sets the expected frame interval. This controls how often frames should
  // be dropped if |OnOutputFormatRequest| is called with a lower frame
  // interval.
  void SetExpectedInputFrameInterval(int64_t interval);

  // Return the adapted resolution given the input resolution. The returned
  // resolution will be 0x0 if the frame should be dropped.
  VideoFormat AdaptFrameResolution(int in_width, int in_height);

  // Requests the output frame size and frame interval from
  // |AdaptFrameResolution| to not be larger than |format|.
  void OnOutputFormatRequest(const VideoFormat& format);

  // Requests the output frame size from |AdaptFrameResolution| to not have
  // more than |max_pixel_count| pixels and have "one step" up more pixels than
  // max_pixel_count_step_up.
  void OnResolutionRequest(rtc::Optional<int> max_pixel_count,
                           rtc::Optional<int> max_pixel_count_step_up);

  const VideoFormat& input_format() const;

 private:
  void SetInputFormat(const VideoFormat& format);
  bool Adapt(int max_num_pixels, int max_pixel_count_step_up);

  VideoFormat input_format_;
  VideoFormat output_format_;
  int output_num_pixels_;
  int frames_in_;         // Number of input frames.
  int frames_out_;        // Number of output frames.
  int frames_scaled_;     // Number of frames scaled.
  int adaption_changes_;  // Number of changes in scale factor.
  int previous_width_;    // Previous adapter output width.
  int previous_height_;   // Previous adapter output height.
  int64_t interval_next_frame_;

  // Max number of pixels requested via calls to OnOutputFormatRequest,
  // OnResolutionRequest respectively.
  // The adapted output format is the minimum of these.
  int format_request_max_pixel_count_;
  int resolution_request_max_pixel_count_;

  // The critical section to protect the above variables.
  rtc::CriticalSection critical_section_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoAdapter);
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_VIDEOADAPTER_H_
