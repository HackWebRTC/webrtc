/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/videoadapter.h"

#include <algorithm>
#include <limits>

#include "webrtc/base/logging.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/media/base/videocommon.h"

namespace {

// Scale factors optimized for in libYUV that we accept.
// Must be sorted in decreasing scale factors for FindScaleLargerThan to work.
const float kScaleFactors[] = {
    1.f / 1.f,   // Full size.
    3.f / 4.f,   // 3/4 scale.
    1.f / 2.f,   // 1/2 scale.
    3.f / 8.f,   // 3/8 scale.
    1.f / 4.f,   // 1/4 scale.
    3.f / 16.f,  // 3/16 scale.
};

float FindScaleLessThanOrEqual(int width,
                               int height,
                               int target_num_pixels,
                               int* resulting_number_of_pixels) {
  float best_distance = std::numeric_limits<float>::max();
  float best_scale = 0.0f;  // Default to 0 if nothing matches.
  float pixels = width * height;
  float best_number_of_pixels = 0.0f;
  for (const auto& scale : kScaleFactors) {
    float test_num_pixels = pixels * scale * scale;
    float diff = target_num_pixels - test_num_pixels;
    if (diff < 0) {
      continue;
    }
    if (diff < best_distance) {
      best_distance = diff;
      best_scale = scale;
      best_number_of_pixels = test_num_pixels;
      if (best_distance == 0) {  // Found exact match.
        break;
      }
    }
  }
  if (resulting_number_of_pixels) {
    *resulting_number_of_pixels = static_cast<int>(best_number_of_pixels + .5f);
  }
  return best_scale;
}

float FindScaleLargerThan(int width,
                          int height,
                          int target_num_pixels,
                          int* resulting_number_of_pixels) {
  float best_distance = std::numeric_limits<float>::max();
  float best_scale = 1.f;  // Default to unscaled if nothing matches.
  float pixels = width * height;
  float best_number_of_pixels = pixels;  // Default to input number of pixels.
  for (const auto& scale : kScaleFactors) {
    float test_num_pixels = pixels * scale * scale;
    float diff = test_num_pixels - target_num_pixels;
    if (diff <= 0) {
      break;
    }
    if (diff < best_distance) {
      best_distance = diff;
      best_scale = scale;
      best_number_of_pixels = test_num_pixels;
    }
  }

  *resulting_number_of_pixels = static_cast<int>(best_number_of_pixels + .5f);
  return best_scale;
}

}  // namespace

namespace cricket {

VideoAdapter::VideoAdapter()
    : output_num_pixels_(std::numeric_limits<int>::max()),
      frames_in_(0),
      frames_out_(0),
      frames_scaled_(0),
      adaption_changes_(0),
      previous_width_(0),
      previous_height_(0),
      interval_next_frame_(0),
      format_request_max_pixel_count_(std::numeric_limits<int>::max()),
      resolution_request_max_pixel_count_(std::numeric_limits<int>::max()) {}

VideoAdapter::~VideoAdapter() {}

void VideoAdapter::SetExpectedInputFrameInterval(int64_t interval) {
  // TODO(perkj): Consider measuring input frame rate instead.
  // Frame rate typically varies depending on lighting.
  rtc::CritScope cs(&critical_section_);
  input_format_.interval = interval;
}

void VideoAdapter::SetInputFormat(const VideoFormat& format) {
  bool is_resolution_change = (input_format().width != format.width ||
                               input_format().height != format.height);
  int64_t old_input_interval = input_format_.interval;
  input_format_ = format;
  output_format_.interval =
      std::max(output_format_.interval, input_format_.interval);
  if (old_input_interval != input_format_.interval) {
    LOG(LS_INFO) << "VAdapt input interval changed from "
      << old_input_interval << " to " << input_format_.interval;
  }
  if (is_resolution_change) {
    // Trigger the adaptation logic again, to potentially reset the adaptation
    // state for things like view requests that may not longer be capping
    // output (or may now cap output).
    Adapt(std::min(format_request_max_pixel_count_,
                   resolution_request_max_pixel_count_),
          0);
  }
}

const VideoFormat& VideoAdapter::input_format() const {
  rtc::CritScope cs(&critical_section_);
  return input_format_;
}

VideoFormat VideoAdapter::AdaptFrameResolution(int in_width, int in_height) {
  rtc::CritScope cs(&critical_section_);
  ++frames_in_;

  SetInputFormat(VideoFormat(
      in_width, in_height, input_format_.interval, input_format_.fourcc));

  // Drop the input frame if necessary.
  bool should_drop = false;
  if (!output_num_pixels_) {
    // Drop all frames as the output format is 0x0.
    should_drop = true;
  } else {
    // Drop some frames based on input fps and output fps.
    // Normally output fps is less than input fps.
    interval_next_frame_ += input_format_.interval;
    if (output_format_.interval > 0) {
      if (interval_next_frame_ >= output_format_.interval) {
        interval_next_frame_ %= output_format_.interval;
      } else {
        should_drop = true;
      }
    }
  }
  if (should_drop) {
    // Show VAdapt log every 90 frames dropped. (3 seconds)
    if ((frames_in_ - frames_out_) % 90 == 0) {
      // TODO(fbarchard): Reduce to LS_VERBOSE when adapter info is not needed
      // in default calls.
      LOG(LS_INFO) << "VAdapt Drop Frame: scaled " << frames_scaled_
                   << " / out " << frames_out_
                   << " / in " << frames_in_
                   << " Changes: " << adaption_changes_
                   << " Input: " << in_width
                   << "x" << in_height
                   << " i" << input_format_.interval
                   << " Output: i" << output_format_.interval;
    }

    return VideoFormat();  // Drop frame.
  }

  const float scale = FindScaleLessThanOrEqual(in_width, in_height,
                                               output_num_pixels_, nullptr);
  const int output_width = static_cast<int>(in_width * scale + .5f);
  const int output_height = static_cast<int>(in_height * scale + .5f);

  ++frames_out_;
  if (scale != 1)
    ++frames_scaled_;

  if (previous_width_ && (previous_width_ != output_width ||
                          previous_height_ != output_height)) {
    ++adaption_changes_;
    LOG(LS_INFO) << "Frame size changed: scaled " << frames_scaled_ << " / out "
                 << frames_out_ << " / in " << frames_in_
                 << " Changes: " << adaption_changes_ << " Input: " << in_width
                 << "x" << in_height << " i" << input_format_.interval
                 << " Scale: " << scale << " Output: " << output_width << "x"
                 << output_height << " i" << output_format_.interval;
  }

  output_format_.width = output_width;
  output_format_.height = output_height;
  previous_width_ = output_width;
  previous_height_ = output_height;

  return output_format_;
}

void VideoAdapter::OnOutputFormatRequest(const VideoFormat& format) {
  rtc::CritScope cs(&critical_section_);
  format_request_max_pixel_count_ = format.width * format.height;
  output_format_.interval = format.interval;
  Adapt(std::min(format_request_max_pixel_count_,
                 resolution_request_max_pixel_count_),
        0);
}

void VideoAdapter::OnResolutionRequest(
    rtc::Optional<int> max_pixel_count,
    rtc::Optional<int> max_pixel_count_step_up) {
  rtc::CritScope cs(&critical_section_);
  resolution_request_max_pixel_count_ =
      max_pixel_count.value_or(std::numeric_limits<int>::max());
  Adapt(std::min(format_request_max_pixel_count_,
                 resolution_request_max_pixel_count_),
        max_pixel_count_step_up.value_or(0));
}

bool VideoAdapter::Adapt(int max_num_pixels, int max_pixel_count_step_up) {
  float scale_lower =
      FindScaleLessThanOrEqual(input_format_.width, input_format_.height,
                               max_num_pixels, &max_num_pixels);
  float scale_upper =
      max_pixel_count_step_up > 0
          ? FindScaleLargerThan(input_format_.width, input_format_.height,
                                max_pixel_count_step_up,
                                &max_pixel_count_step_up)
          : 1.f;

  bool use_max_pixel_count_step_up =
      max_pixel_count_step_up > 0 && max_num_pixels > max_pixel_count_step_up;

  int old_num_pixels = output_num_pixels_;
  output_num_pixels_ =
      use_max_pixel_count_step_up ? max_pixel_count_step_up : max_num_pixels;
  // Log the new size.
  float scale = use_max_pixel_count_step_up ? scale_upper : scale_lower;
  int new_width = static_cast<int>(input_format_.width * scale + .5f);
  int new_height = static_cast<int>(input_format_.height * scale + .5f);

  bool changed = output_num_pixels_ != old_num_pixels;
  LOG(LS_INFO) << "OnResolutionRequest: "
               << " Max pixels: " << max_num_pixels
               << " Max pixels step up: " << max_pixel_count_step_up
               << " Output Pixels: " << output_num_pixels_
               << " Input: " << input_format_.width << "x"
               << input_format_.height << " Scale: " << scale
               << " Resolution: " << new_width << "x" << new_height
               << " Changed: " << (changed ? "true" : "false");

  return changed;
}

}  // namespace cricket
