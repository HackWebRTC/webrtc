// libjingle
// Copyright 2010 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "talk/media/base/videoadapter.h"

#include <limits.h>  // For INT_MAX

#include "talk/base/logging.h"
#include "talk/base/timeutils.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/videoframe.h"

namespace cricket {

// TODO(fbarchard): Make downgrades settable
static const int kMaxCpuDowngrades = 2;  // Downgrade at most 2 times for CPU.
// The number of cpu samples to require before adapting. This value depends on
// the cpu monitor sampling frequency being 2000ms.
static const int kCpuLoadMinSamples = 3;
// The amount of weight to give to each new cpu load sample. The lower the
// value, the slower we'll adapt to changing cpu conditions.
static const float kCpuLoadWeightCoefficient = 0.4f;
// The seed value for the cpu load moving average.
static const float kCpuLoadInitialAverage = 0.5f;

// Desktop needs 1/8 scale for HD (1280 x 720) to QQVGA (160 x 90)
static const float kScaleFactors[] = {
  1.f / 1.f,   // Full size.
  3.f / 4.f,   // 3/4 scale.
  1.f / 2.f,   // 1/2 scale.
  3.f / 8.f,   // 3/8 scale.
  1.f / 4.f,   // 1/4 scale.
  3.f / 16.f,  // 3/16 scale.
  1.f / 8.f,   // 1/8 scale.
  0.f  // End of table.
};

// TODO(fbarchard): Use this table (optionally) for CPU and GD as well.
static const float kViewScaleFactors[] = {
  1.f / 1.f,   // Full size.
  3.f / 4.f,   // 3/4 scale.
  2.f / 3.f,   // 2/3 scale.  // Allow 1080p to 720p.
  1.f / 2.f,   // 1/2 scale.
  3.f / 8.f,   // 3/8 scale.
  1.f / 3.f,   // 1/3 scale.  // Allow 1080p to 360p.
  1.f / 4.f,   // 1/4 scale.
  3.f / 16.f,  // 3/16 scale.
  1.f / 8.f,   // 1/8 scale.
  0.f  // End of table.
};

const float* VideoAdapter::GetViewScaleFactors() const {
  return scale_third_ ? kViewScaleFactors : kScaleFactors;
}

// For resolutions that would scale down a little instead of up a little,
// bias toward scaling up a little.  This will tend to choose 3/4 scale instead
// of 2/3 scale, when the 2/3 is not an exact match.
static const float kUpBias = -0.9f;
// Find the scale factor that, when applied to width and height, is closest
// to num_pixels.
float VideoAdapter::FindScale(const float* scale_factors,
                              const float upbias,
                              int width, int height,
                              int target_num_pixels) {
  const float kMinNumPixels = 160 * 90;
  if (!target_num_pixels) {
    return 0.f;
  }
  float best_distance = static_cast<float>(INT_MAX);
  float best_scale = 1.f;  // Default to unscaled if nothing matches.
  float pixels = static_cast<float>(width * height);
  for (int i = 0; ; ++i) {
    float scale = scale_factors[i];
    float test_num_pixels = pixels * scale * scale;
    // Do not consider scale factors that produce too small images.
    // Scale factor of 0 at end of table will also exit here.
    if (test_num_pixels < kMinNumPixels) {
      break;
    }
    float diff = target_num_pixels - test_num_pixels;
    // If resolution is higher than desired, bias the difference based on
    // preference for slightly larger for nearest, or avoid completely if
    // looking for lower resolutions only.
    if (diff < 0) {
      diff = diff * kUpBias;
    }
    if (diff < best_distance) {
      best_distance = diff;
      best_scale = scale;
      if (best_distance == 0) {  // Found exact match.
        break;
      }
    }
  }
  return best_scale;
}

// Find the closest scale factor.
float VideoAdapter::FindClosestScale(int width, int height,
                                         int target_num_pixels) {
  return FindScale(kScaleFactors, kUpBias,
                   width, height, target_num_pixels);
}

// Find the closest view scale factor.
float VideoAdapter::FindClosestViewScale(int width, int height,
                                         int target_num_pixels) {
  return FindScale(GetViewScaleFactors(), kUpBias,
                   width, height, target_num_pixels);
}

// Finds the scale factor that, when applied to width and height, produces
// fewer than num_pixels.
static const float kUpAvoidBias = -1000000000.f;
float VideoAdapter::FindLowerScale(int width, int height,
                                   int target_num_pixels) {
  return FindScale(GetViewScaleFactors(), kUpAvoidBias,
                   width, height, target_num_pixels);
}

// There are several frame sizes used by Adapter.  This explains them
// input_format - set once by server to frame size expected from the camera.
// output_format - size that output would like to be.  Includes framerate.
// output_num_pixels - size that output should be constrained to.  Used to
//   compute output_format from in_frame.
// in_frame - actual camera captured frame size, which is typically the same
//   as input_format.  This can also be rotated or cropped for aspect ratio.
// out_frame - actual frame output by adapter.  Should be a direct scale of
//   in_frame maintaining rotation and aspect ratio.
// OnOutputFormatRequest - server requests you send this resolution based on
//   view requests.
// OnEncoderResolutionRequest - encoder requests you send this resolution based
//   on bandwidth
// OnCpuLoadUpdated - cpu monitor requests you send this resolution based on
//   cpu load.

///////////////////////////////////////////////////////////////////////
// Implementation of VideoAdapter
VideoAdapter::VideoAdapter()
    : output_num_pixels_(INT_MAX),
      scale_third_(false),
      frames_in_(0),
      frames_out_(0),
      frames_scaled_(0),
      adaption_changes_(0),
      previous_width_(0),
      previous_height_(0),
      black_output_(false),
      is_black_(false),
      interval_next_frame_(0) {
}

VideoAdapter::~VideoAdapter() {
}

void VideoAdapter::SetInputFormat(const VideoFormat& format) {
  talk_base::CritScope cs(&critical_section_);
  int64 old_input_interval = input_format_.interval;
  input_format_ = format;
  output_format_.interval = talk_base::_max(
      output_format_.interval, input_format_.interval);
  if (old_input_interval != input_format_.interval) {
    LOG(LS_INFO) << "VAdapt input interval changed from "
      << old_input_interval << " to " << input_format_.interval;
  }
}

void CoordinatedVideoAdapter::SetInputFormat(const VideoFormat& format) {
  int previous_width = input_format().width;
  int previous_height = input_format().height;
  bool is_resolution_change = previous_width > 0 && format.width > 0 &&
                              (previous_width != format.width ||
                               previous_height != format.height);
  VideoAdapter::SetInputFormat(format);
  if (is_resolution_change) {
    int width, height;
    // Trigger the adaptation logic again, to potentially reset the adaptation
    // state for things like view requests that may not longer be capping
    // output (or may now cap output).
    AdaptToMinimumFormat(&width, &height);
    LOG(LS_INFO) << "VAdapt Input Resolution Change: "
                 << "Previous input resolution: "
                 << previous_width << "x" << previous_height
                 << " New input resolution: "
                 << format.width << "x" << format.height
                 << " New output resolution: "
                 << width << "x" << height;
  }
}

void VideoAdapter::SetOutputFormat(const VideoFormat& format) {
  talk_base::CritScope cs(&critical_section_);
  int64 old_output_interval = output_format_.interval;
  output_format_ = format;
  output_num_pixels_ = output_format_.width * output_format_.height;
  output_format_.interval = talk_base::_max(
      output_format_.interval, input_format_.interval);
  if (old_output_interval != output_format_.interval) {
    LOG(LS_INFO) << "VAdapt output interval changed from "
      << old_output_interval << " to " << output_format_.interval;
  }
}

const VideoFormat& VideoAdapter::input_format() {
  talk_base::CritScope cs(&critical_section_);
  return input_format_;
}

const VideoFormat& VideoAdapter::output_format() {
  talk_base::CritScope cs(&critical_section_);
  return output_format_;
}

void VideoAdapter::SetBlackOutput(bool black) {
  talk_base::CritScope cs(&critical_section_);
  black_output_ = black;
}

// Constrain output resolution to this many pixels overall
void VideoAdapter::SetOutputNumPixels(int num_pixels) {
  output_num_pixels_ = num_pixels;
}

int VideoAdapter::GetOutputNumPixels() const {
  return output_num_pixels_;
}

// TODO(fbarchard): Add AdaptFrameRate function that only drops frames but
// not resolution.
bool VideoAdapter::AdaptFrame(const VideoFrame* in_frame,
                              VideoFrame** out_frame) {
  talk_base::CritScope cs(&critical_section_);
  if (!in_frame || !out_frame) {
    return false;
  }
  ++frames_in_;

  // Update input to actual frame dimensions.
  VideoFormat format(static_cast<int>(in_frame->GetWidth()),
                     static_cast<int>(in_frame->GetHeight()),
                     input_format_.interval, input_format_.fourcc);
  SetInputFormat(format);

  // Drop the input frame if necessary.
  bool should_drop = false;
  if (!output_num_pixels_) {
    // Drop all frames as the output format is 0x0.
    should_drop = true;
  } else {
    // Drop some frames based on input fps and output fps.
    // Normally output fps is less than input fps.
    // TODO(fbarchard): Consider adjusting interval to reflect the adjusted
    // interval between frames after dropping some frames.
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
                   << " Input: " << in_frame->GetWidth()
                   << "x" << in_frame->GetHeight()
                   << " i" << input_format_.interval
                   << " Output: i" << output_format_.interval;
    }
    *out_frame = NULL;
    return true;
  }

  float scale = 1.f;
  if (output_num_pixels_) {
    scale = VideoAdapter::FindClosestViewScale(
        static_cast<int>(in_frame->GetWidth()),
        static_cast<int>(in_frame->GetHeight()),
        output_num_pixels_);
    output_format_.width = static_cast<int>(in_frame->GetWidth() * scale + .5f);
    output_format_.height = static_cast<int>(in_frame->GetHeight() * scale +
                                             .5f);
  }

  if (!StretchToOutputFrame(in_frame)) {
    LOG(LS_VERBOSE) << "VAdapt Stretch Failed.";
    return false;
  }

  *out_frame = output_frame_.get();

  ++frames_out_;
  if (in_frame->GetWidth() != (*out_frame)->GetWidth() ||
      in_frame->GetHeight() != (*out_frame)->GetHeight()) {
    ++frames_scaled_;
  }
  // Show VAdapt log every 90 frames output. (3 seconds)
  // TODO(fbarchard): Consider GetLogSeverity() to change interval to less
  // for LS_VERBOSE and more for LS_INFO.
  bool show = (frames_out_) % 90 == 0;

  // TODO(fbarchard): LOG the previous output resolution and track input
  // resolution changes as well.  Consider dropping the statistics into their
  // own class which could be queried publically.
  bool changed = false;
  if (previous_width_ && (previous_width_ != (*out_frame)->GetWidth() ||
      previous_height_ != (*out_frame)->GetHeight())) {
    show = true;
    ++adaption_changes_;
    changed = true;
  }
  if (show) {
    // TODO(fbarchard): Reduce to LS_VERBOSE when adapter info is not needed
    // in default calls.
    LOG(LS_INFO) << "VAdapt Frame: scaled " << frames_scaled_
                 << " / out " << frames_out_
                 << " / in " << frames_in_
                 << " Changes: " << adaption_changes_
                 << " Input: " << in_frame->GetWidth()
                 << "x" << in_frame->GetHeight()
                 << " i" << input_format_.interval
                 << " Scale: " << scale
                 << " Output: " << (*out_frame)->GetWidth()
                 << "x" << (*out_frame)->GetHeight()
                 << " i" << output_format_.interval
                 << " Changed: " << (changed ? "true" : "false");
  }
  previous_width_ = (*out_frame)->GetWidth();
  previous_height_ = (*out_frame)->GetHeight();

  return true;
}

// Scale or Blacken the frame.  Returns true if successful.
bool VideoAdapter::StretchToOutputFrame(const VideoFrame* in_frame) {
  int output_width = output_format_.width;
  int output_height = output_format_.height;

  // Create and stretch the output frame if it has not been created yet or its
  // size is not same as the expected.
  bool stretched = false;
  if (!output_frame_ ||
      output_frame_->GetWidth() != static_cast<size_t>(output_width) ||
      output_frame_->GetHeight() != static_cast<size_t>(output_height)) {
    output_frame_.reset(
        in_frame->Stretch(output_width, output_height, true, true));
    if (!output_frame_) {
      LOG(LS_WARNING) << "Adapter failed to stretch frame to "
                      << output_width << "x" << output_height;
      return false;
    }
    stretched = true;
    is_black_ = false;
  }

  if (!black_output_) {
    if (!stretched) {
      // The output frame does not need to be blacken and has not been stretched
      // from the input frame yet, stretch the input frame. This is the most
      // common case.
      in_frame->StretchToFrame(output_frame_.get(), true, true);
    }
    is_black_ = false;
  } else {
    if (!is_black_) {
      output_frame_->SetToBlack();
      is_black_ = true;
    }
    output_frame_->SetElapsedTime(in_frame->GetElapsedTime());
    output_frame_->SetTimeStamp(in_frame->GetTimeStamp());
  }

  return true;
}

///////////////////////////////////////////////////////////////////////
// Implementation of CoordinatedVideoAdapter
CoordinatedVideoAdapter::CoordinatedVideoAdapter()
    : cpu_adaptation_(true),
      cpu_smoothing_(false),
      gd_adaptation_(true),
      view_adaptation_(true),
      view_switch_(false),
      cpu_downgrade_count_(0),
      cpu_load_min_samples_(kCpuLoadMinSamples),
      cpu_load_num_samples_(0),
      high_system_threshold_(kHighSystemCpuThreshold),
      low_system_threshold_(kLowSystemCpuThreshold),
      process_threshold_(kProcessCpuThreshold),
      view_desired_num_pixels_(INT_MAX),
      view_desired_interval_(0),
      encoder_desired_num_pixels_(INT_MAX),
      cpu_desired_num_pixels_(INT_MAX),
      adapt_reason_(0),
      system_load_average_(kCpuLoadInitialAverage) {
}

// Helper function to UPGRADE or DOWNGRADE a number of pixels
void CoordinatedVideoAdapter::StepPixelCount(
    CoordinatedVideoAdapter::AdaptRequest request,
    int* num_pixels) {
  switch (request) {
    case CoordinatedVideoAdapter::DOWNGRADE:
      *num_pixels /= 2;
      break;

    case CoordinatedVideoAdapter::UPGRADE:
      *num_pixels *= 2;
      break;

    default:  // No change in pixel count
      break;
  }
  return;
}

// Find the adaptation request of the cpu based on the load. Return UPGRADE if
// the load is low, DOWNGRADE if the load is high, and KEEP otherwise.
CoordinatedVideoAdapter::AdaptRequest CoordinatedVideoAdapter::FindCpuRequest(
    int current_cpus, int max_cpus,
    float process_load, float system_load) {
  // Downgrade if system is high and plugin is at least more than midrange.
  if (system_load >= high_system_threshold_ * max_cpus &&
      process_load >= process_threshold_ * current_cpus) {
    return CoordinatedVideoAdapter::DOWNGRADE;
  // Upgrade if system is low.
  } else if (system_load < low_system_threshold_ * max_cpus) {
    return CoordinatedVideoAdapter::UPGRADE;
  }
  return CoordinatedVideoAdapter::KEEP;
}

// A remote view request for a new resolution.
void CoordinatedVideoAdapter::OnOutputFormatRequest(const VideoFormat& format) {
  talk_base::CritScope cs(&request_critical_section_);
  if (!view_adaptation_) {
    return;
  }
  // Set output for initial aspect ratio in mediachannel unittests.
  int old_num_pixels = GetOutputNumPixels();
  SetOutputFormat(format);
  SetOutputNumPixels(old_num_pixels);
  view_desired_num_pixels_ = format.width * format.height;
  view_desired_interval_ = format.interval;
  int new_width, new_height;
  bool changed = AdaptToMinimumFormat(&new_width, &new_height);
  LOG(LS_INFO) << "VAdapt View Request: "
               << format.width << "x" << format.height
               << " Pixels: " << view_desired_num_pixels_
               << " Changed: " << (changed ? "true" : "false")
               << " To: " << new_width << "x" << new_height;
}

// A Bandwidth GD request for new resolution
void CoordinatedVideoAdapter::OnEncoderResolutionRequest(
    int width, int height, AdaptRequest request) {
  talk_base::CritScope cs(&request_critical_section_);
  if (!gd_adaptation_) {
    return;
  }
  int old_encoder_desired_num_pixels = encoder_desired_num_pixels_;
  if (KEEP != request) {
    int new_encoder_desired_num_pixels = width * height;
    int old_num_pixels = GetOutputNumPixels();
    if (new_encoder_desired_num_pixels != old_num_pixels) {
      LOG(LS_VERBOSE) << "VAdapt GD resolution stale.  Ignored";
    } else {
      // Update the encoder desired format based on the request.
      encoder_desired_num_pixels_ = new_encoder_desired_num_pixels;
      StepPixelCount(request, &encoder_desired_num_pixels_);
    }
  }
  int new_width, new_height;
  bool changed = AdaptToMinimumFormat(&new_width, &new_height);

  // Ignore up or keep if no change.
  if (DOWNGRADE != request && view_switch_ && !changed) {
    encoder_desired_num_pixels_ = old_encoder_desired_num_pixels;
    LOG(LS_VERBOSE) << "VAdapt ignoring GD request.";
  }

  LOG(LS_INFO) << "VAdapt GD Request: "
               << (DOWNGRADE == request ? "down" :
                   (UPGRADE == request ? "up" : "keep"))
               << " From: " << width << "x" << height
               << " Pixels: " << encoder_desired_num_pixels_
               << " Changed: " << (changed ? "true" : "false")
               << " To: " << new_width << "x" << new_height;
}

// A Bandwidth GD request for new resolution
void CoordinatedVideoAdapter::OnCpuResolutionRequest(AdaptRequest request) {
  talk_base::CritScope cs(&request_critical_section_);
  if (!cpu_adaptation_) {
    return;
  }
  // Update how many times we have downgraded due to the cpu load.
  switch (request) {
    case DOWNGRADE:
      // Ignore downgrades if we have downgraded the maximum times.
      if (cpu_downgrade_count_ < kMaxCpuDowngrades) {
        ++cpu_downgrade_count_;
      } else {
        LOG(LS_VERBOSE) << "VAdapt CPU load high but do not downgrade "
                           "because maximum downgrades reached";
        SignalCpuAdaptationUnable();
      }
      break;
    case UPGRADE:
      if (cpu_downgrade_count_ > 0) {
        bool is_min = IsMinimumFormat(cpu_desired_num_pixels_);
        if (is_min) {
          --cpu_downgrade_count_;
        } else {
          LOG(LS_VERBOSE) << "VAdapt CPU load low but do not upgrade "
                             "because cpu is not limiting resolution";
        }
      } else {
        LOG(LS_VERBOSE) << "VAdapt CPU load low but do not upgrade "
                           "because minimum downgrades reached";
      }
      break;
    case KEEP:
    default:
      break;
  }
  if (KEEP != request) {
    // TODO(fbarchard): compute stepping up/down from OutputNumPixels but
    // clamp to inputpixels / 4 (2 steps)
    cpu_desired_num_pixels_ =  cpu_downgrade_count_ == 0 ? INT_MAX :
        static_cast<int>(input_format().width * input_format().height >>
                         cpu_downgrade_count_);
  }
  int new_width, new_height;
  bool changed = AdaptToMinimumFormat(&new_width, &new_height);
  LOG(LS_INFO) << "VAdapt CPU Request: "
               << (DOWNGRADE == request ? "down" :
                   (UPGRADE == request ? "up" : "keep"))
               << " Steps: " << cpu_downgrade_count_
               << " Changed: " << (changed ? "true" : "false")
               << " To: " << new_width << "x" << new_height;
}

// A CPU request for new resolution
// TODO(fbarchard): Move outside adapter.
void CoordinatedVideoAdapter::OnCpuLoadUpdated(
    int current_cpus, int max_cpus, float process_load, float system_load) {
  talk_base::CritScope cs(&request_critical_section_);
  if (!cpu_adaptation_) {
    return;
  }
  // Update the moving average of system load. Even if we aren't smoothing,
  // we'll still calculate this information, in case smoothing is later enabled.
  system_load_average_ = kCpuLoadWeightCoefficient * system_load +
      (1.0f - kCpuLoadWeightCoefficient) * system_load_average_;
  ++cpu_load_num_samples_;
  if (cpu_smoothing_) {
    system_load = system_load_average_;
  }
  AdaptRequest request = FindCpuRequest(current_cpus, max_cpus,
                                        process_load, system_load);
  // Make sure we're not adapting too quickly.
  if (request != KEEP) {
    if (cpu_load_num_samples_ < cpu_load_min_samples_) {
      LOG(LS_VERBOSE) << "VAdapt CPU load high/low but do not adapt until "
                      << (cpu_load_min_samples_ - cpu_load_num_samples_)
                      << " more samples";
      request = KEEP;
    }
  }

  OnCpuResolutionRequest(request);
}

// Called by cpu adapter on up requests.
bool CoordinatedVideoAdapter::IsMinimumFormat(int pixels) {
  // Find closest scale factor that matches input resolution to min_num_pixels
  // and set that for output resolution.  This is not needed for VideoAdapter,
  // but provides feedback to unittests and users on expected resolution.
  // Actual resolution is based on input frame.
  VideoFormat new_output = output_format();
  VideoFormat input = input_format();
  if (input_format().IsSize0x0()) {
    input = new_output;
  }
  float scale = 1.0f;
  if (!input.IsSize0x0()) {
    scale = FindClosestScale(input.width,
                             input.height,
                             pixels);
  }
  new_output.width = static_cast<int>(input.width * scale + .5f);
  new_output.height = static_cast<int>(input.height * scale + .5f);
  int new_pixels = new_output.width * new_output.height;
  int num_pixels = GetOutputNumPixels();
  return new_pixels <= num_pixels;
}

// Called by all coordinators when there is a change.
bool CoordinatedVideoAdapter::AdaptToMinimumFormat(int* new_width,
                                                   int* new_height) {
  VideoFormat new_output = output_format();
  VideoFormat input = input_format();
  if (input_format().IsSize0x0()) {
    input = new_output;
  }
  int old_num_pixels = GetOutputNumPixels();
  int min_num_pixels = INT_MAX;
  adapt_reason_ = 0;

  // Reduce resolution based on encoder bandwidth (GD).
  if (encoder_desired_num_pixels_ &&
      (encoder_desired_num_pixels_ < min_num_pixels)) {
    adapt_reason_ |= ADAPTREASON_BANDWIDTH;
    min_num_pixels = encoder_desired_num_pixels_;
  }
  // Reduce resolution based on CPU.
  if (cpu_adaptation_ && cpu_desired_num_pixels_ &&
      (cpu_desired_num_pixels_ <= min_num_pixels)) {
    if (cpu_desired_num_pixels_ < min_num_pixels) {
      adapt_reason_ = ADAPTREASON_CPU;
    } else {
      adapt_reason_ |= ADAPTREASON_CPU;
    }
    min_num_pixels = cpu_desired_num_pixels_;
  }
  // Round resolution for GD or CPU to allow 1/2 to map to 9/16.
  if (!input.IsSize0x0() && min_num_pixels != INT_MAX) {
    float scale = FindClosestScale(input.width, input.height, min_num_pixels);
    min_num_pixels = static_cast<int>(input.width * scale + .5f) *
        static_cast<int>(input.height * scale + .5f);
  }
  // Reduce resolution based on View Request.
  if (view_desired_num_pixels_ <= min_num_pixels) {
    if (view_desired_num_pixels_ < min_num_pixels) {
      adapt_reason_ = ADAPTREASON_VIEW;
    } else {
      adapt_reason_ |= ADAPTREASON_VIEW;
    }
    min_num_pixels = view_desired_num_pixels_;
  }
  // Snap to a scale factor.
  float scale = 1.0f;
  if (!input.IsSize0x0()) {
    scale = FindLowerScale(input.width, input.height, min_num_pixels);
    min_num_pixels = static_cast<int>(input.width * scale + .5f) *
        static_cast<int>(input.height * scale + .5f);
  }
  if (scale == 1.0f) {
    adapt_reason_ = 0;
  }
  *new_width = new_output.width = static_cast<int>(input.width * scale + .5f);
  *new_height = new_output.height = static_cast<int>(input.height * scale +
                                                     .5f);
  SetOutputNumPixels(min_num_pixels);

  new_output.interval = view_desired_interval_;
  SetOutputFormat(new_output);
  int new_num_pixels = GetOutputNumPixels();
  bool changed = new_num_pixels != old_num_pixels;

  static const char* kReasons[8] = {
    "None",
    "CPU",
    "BANDWIDTH",
    "CPU+BANDWIDTH",
    "VIEW",
    "CPU+VIEW",
    "BANDWIDTH+VIEW",
    "CPU+BANDWIDTH+VIEW",
  };

  LOG(LS_VERBOSE) << "VAdapt Status View: " << view_desired_num_pixels_
                  << " GD: " << encoder_desired_num_pixels_
                  << " CPU: " << cpu_desired_num_pixels_
                  << " Pixels: " << min_num_pixels
                  << " Input: " << input.width
                  << "x" << input.height
                  << " Scale: " << scale
                  << " Resolution: " << new_output.width
                  << "x" << new_output.height
                  << " Changed: " << (changed ? "true" : "false")
                  << " Reason: " << kReasons[adapt_reason_];

  if (changed) {
    // When any adaptation occurs, historic CPU load levels are no longer
    // accurate. Clear out our state so we can re-learn at the new normal.
    cpu_load_num_samples_ = 0;
    system_load_average_ = kCpuLoadInitialAverage;
  }

  return changed;
}

}  // namespace cricket
