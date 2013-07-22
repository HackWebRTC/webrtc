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
// The number of milliseconds of data to require before acting on cpu sampling
// information.
static const size_t kCpuLoadMinSampleTime = 5000;
// The amount of weight to give to each new cpu load sample. The lower the
// value, the slower we'll adapt to changing cpu conditions.
static const float kCpuLoadWeightCoefficient = 0.4f;
// The seed value for the cpu load moving average.
static const float kCpuLoadInitialAverage = 0.5f;

// TODO(fbarchard): Consider making scale factor table settable, to allow
// application to select quality vs performance tradeoff.
// TODO(fbarchard): Add framerate scaling to tables for 1/2 framerate.
// List of scale factors that adapter will scale by.
#if defined(IOS) || defined(ANDROID)
// Mobile needs 1/4 scale for VGA (640 x 360) to QQVGA (160 x 90)
// or 1/4 scale for HVGA (480 x 270) to QQHVGA (120 x 67)
static const int kMinNumPixels = 120 * 67;
static float kScaleFactors[] = {
  1.f / 1.f,  // Full size.
  3.f / 4.f,  // 3/4 scale.
  1.f / 2.f,  // 1/2 scale.
  3.f / 8.f,  // 3/8 scale.
  1.f / 4.f,  // 1/4 scale.
};
#else
// Desktop needs 1/8 scale for HD (1280 x 720) to QQVGA (160 x 90)
static const int kMinNumPixels = 160 * 100;
static float kScaleFactors[] = {
  1.f / 1.f,  // Full size.
  3.f / 4.f,  // 3/4 scale.
  1.f / 2.f,  // 1/2 scale.
  3.f / 8.f,  // 3/8 scale.
  1.f / 4.f,  // 1/4 scale.
  3.f / 16.f,  // 3/16 scale.
  1.f / 8.f  // 1/8 scale.
};
#endif

static const int kNumScaleFactors = ARRAY_SIZE(kScaleFactors);

// Find the scale factor that, when applied to width and height, is closest
// to num_pixels.
float VideoAdapter::FindClosestScale(int width, int height,
                                     int target_num_pixels) {
  if (!target_num_pixels) {
    return 0.f;
  }
  int best_distance = INT_MAX;
  int best_index = kNumScaleFactors - 1;  // Default to max scale.
  for (int i = 0; i < kNumScaleFactors; ++i) {
    int test_num_pixels = static_cast<int>(width * kScaleFactors[i] *
                                           height * kScaleFactors[i]);
    int diff = test_num_pixels - target_num_pixels;
    if (diff < 0) {
      diff = -diff;
    }
    if (diff < best_distance) {
      best_distance = diff;
      best_index = i;
      if (best_distance == 0) {  // Found exact match.
        break;
      }
    }
  }
  return kScaleFactors[best_index];
}

// Finds the scale factor that, when applied to width and height, produces
// fewer than num_pixels.
float VideoAdapter::FindLowerScale(int width, int height,
                                   int target_num_pixels) {
  if (!target_num_pixels) {
    return 0.f;
  }
  int best_distance = INT_MAX;
  int best_index = kNumScaleFactors - 1;  // Default to max scale.
  for (int i = 0; i < kNumScaleFactors; ++i) {
    int test_num_pixels = static_cast<int>(width * kScaleFactors[i] *
                                           height * kScaleFactors[i]);
    int diff = target_num_pixels - test_num_pixels;
    if (diff >= 0 && diff < best_distance) {
      best_distance = diff;
      best_index = i;
      if (best_distance == 0) {  // Found exact match.
        break;
      }
    }
  }
  return kScaleFactors[best_index];
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
      black_output_(false),
      is_black_(false),
      interval_next_frame_(0) {
}

VideoAdapter::~VideoAdapter() {
}

void VideoAdapter::SetInputFormat(const VideoFrame& in_frame) {
  talk_base::CritScope cs(&critical_section_);
  input_format_.width = static_cast<int>(in_frame.GetWidth());
  input_format_.height = static_cast<int>(in_frame.GetHeight());
}

void VideoAdapter::SetInputFormat(const VideoFormat& format) {
  talk_base::CritScope cs(&critical_section_);
  input_format_ = format;
  output_format_.interval = talk_base::_max(
      output_format_.interval, input_format_.interval);
}

void VideoAdapter::SetOutputFormat(const VideoFormat& format) {
  talk_base::CritScope cs(&critical_section_);
  output_format_ = format;
  output_num_pixels_ = output_format_.width * output_format_.height;
  output_format_.interval = talk_base::_max(
      output_format_.interval, input_format_.interval);
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
                              const VideoFrame** out_frame) {
  talk_base::CritScope cs(&critical_section_);
  if (!in_frame || !out_frame) {
    return false;
  }

  // Update input to actual frame dimensions.
  SetInputFormat(*in_frame);

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
    *out_frame = NULL;
    return true;
  }

  if (output_num_pixels_) {
    float scale = VideoAdapter::FindClosestScale(
        static_cast<int>(in_frame->GetWidth()),
        static_cast<int>(in_frame->GetHeight()),
        output_num_pixels_);
    output_format_.width = static_cast<int>(in_frame->GetWidth() * scale + .5f);
    output_format_.height = static_cast<int>(in_frame->GetHeight() * scale +
                                             .5f);
  }

  if (!StretchToOutputFrame(in_frame)) {
    return false;
  }

  *out_frame = output_frame_.get();
  return true;
}

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
    : cpu_adaptation_(false),
      cpu_smoothing_(false),
      gd_adaptation_(true),
      view_adaptation_(true),
      view_switch_(false),
      cpu_downgrade_count_(0),
      cpu_adapt_wait_time_(0),
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

// A CPU request for new resolution
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
  if (cpu_smoothing_) {
    system_load = system_load_average_;
  }
  // If we haven't started taking samples yet, wait until we have at least
  // the correct number of samples per the wait time.
  if (cpu_adapt_wait_time_ == 0) {
    cpu_adapt_wait_time_ = talk_base::TimeAfter(kCpuLoadMinSampleTime);
  }
  AdaptRequest request = FindCpuRequest(current_cpus, max_cpus,
                                        process_load, system_load);
  // Make sure we're not adapting too quickly.
  if (request != KEEP) {
    if (talk_base::TimeIsLater(talk_base::Time(),
                               cpu_adapt_wait_time_)) {
      LOG(LS_VERBOSE) << "VAdapt CPU load high/low but do not adapt until "
                      << talk_base::TimeUntil(cpu_adapt_wait_time_) << " ms";
      request = KEEP;
    }
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
               << " Process: " << process_load
               << " System: " << system_load
               << " Steps: " << cpu_downgrade_count_
               << " Changed: " << (changed ? "true" : "false")
               << " To: " << new_width << "x" << new_height;
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
  // Find resolution that respects ViewRequest or less pixels.
  int view_desired_num_pixels = view_desired_num_pixels_;
  int min_num_pixels = view_desired_num_pixels_;
  if (!input.IsSize0x0()) {
    float scale = FindLowerScale(input.width, input.height, min_num_pixels);
    min_num_pixels = view_desired_num_pixels =
        static_cast<int>(input.width * input.height * scale * scale + .5f);
  }
  // Reduce resolution further, if necessary, based on encoder bandwidth (GD).
  if (encoder_desired_num_pixels_ &&
      (encoder_desired_num_pixels_ < min_num_pixels)) {
    min_num_pixels = encoder_desired_num_pixels_;
  }
  // Reduce resolution further, if necessary, based on CPU.
  if (cpu_adaptation_ && cpu_desired_num_pixels_ &&
      (cpu_desired_num_pixels_ < min_num_pixels)) {
    min_num_pixels = cpu_desired_num_pixels_;
  }

  // Determine which factors are keeping adapter resolution low.
  // Caveat: Does not consider framerate.
  adapt_reason_ = static_cast<AdaptReason>(0);
  if (view_desired_num_pixels == min_num_pixels) {
    adapt_reason_ |= ADAPTREASON_VIEW;
  }
  if (encoder_desired_num_pixels_ == min_num_pixels) {
    adapt_reason_ |= ADAPTREASON_BANDWIDTH;
  }
  if (cpu_desired_num_pixels_ == min_num_pixels) {
    adapt_reason_ |= ADAPTREASON_CPU;
  }

  // Prevent going below QQVGA.
  if (min_num_pixels > 0 && min_num_pixels < kMinNumPixels) {
    min_num_pixels = kMinNumPixels;
  }
  SetOutputNumPixels(min_num_pixels);

  // Find closest scale factor that matches input resolution to min_num_pixels
  // and set that for output resolution.  This is not needed for VideoAdapter,
  // but provides feedback to unittests and users on expected resolution.
  // Actual resolution is based on input frame.
  float scale = 1.0f;
  if (!input.IsSize0x0()) {
    scale = FindClosestScale(input.width, input.height, min_num_pixels);
  }
  if (scale == 1.0f) {
    adapt_reason_ = 0;
  }
  *new_width = new_output.width = static_cast<int>(input.width * scale + .5f);
  *new_height = new_output.height = static_cast<int>(input.height * scale +
                                                     .5f);
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
    cpu_adapt_wait_time_ = talk_base::TimeAfter(kCpuLoadMinSampleTime);
    system_load_average_ = kCpuLoadInitialAverage;
  }

  return changed;
}

}  // namespace cricket
