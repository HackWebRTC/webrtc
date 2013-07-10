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

#ifndef TALK_MEDIA_BASE_VIDEOADAPTER_H_  // NOLINT
#define TALK_MEDIA_BASE_VIDEOADAPTER_H_

#include "talk/base/common.h"  // For ASSERT
#include "talk/base/criticalsection.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/media/base/videocommon.h"

namespace cricket {

class VideoFrame;

// VideoAdapter adapts an input video frame to an output frame based on the
// specified input and output formats. The adaptation includes dropping frames
// to reduce frame rate and scaling frames. VideoAdapter is thread safe.
class VideoAdapter {
 public:
  VideoAdapter();
  virtual ~VideoAdapter();

  void SetInputFormat(const VideoFrame& in_frame);
  void SetInputFormat(const VideoFormat& format);
  void SetOutputFormat(const VideoFormat& format);
  // Constrain output resolution to this many pixels overall
  void SetOutputNumPixels(int num_pixels);
  int GetOutputNumPixels() const;

  const VideoFormat& input_format();
  const VideoFormat& output_format();
  // If the parameter black is true, the adapted frames will be black.
  void SetBlackOutput(bool black);

  // Adapt the input frame from the input format to the output format. Return
  // true and set the output frame to NULL if the input frame is dropped. Return
  // true and set the out frame to output_frame_ if the input frame is adapted
  // successfully. Return false otherwise.
  // output_frame_ is owned by the VideoAdapter that has the best knowledge on
  // the output frame.
  bool AdaptFrame(const VideoFrame* in_frame, const VideoFrame** out_frame);

 protected:
  float FindClosestScale(int width, int height, int target_num_pixels);
  float FindLowerScale(int width, int height, int target_num_pixels);

 private:
  bool StretchToOutputFrame(const VideoFrame* in_frame);

  VideoFormat input_format_;
  VideoFormat output_format_;
  int output_num_pixels_;
  bool black_output_;  // Flag to tell if we need to black output_frame_.
  bool is_black_;  // Flag to tell if output_frame_ is currently black.
  int64 interval_next_frame_;
  talk_base::scoped_ptr<VideoFrame> output_frame_;
  // The critical section to protect the above variables.
  talk_base::CriticalSection critical_section_;

  DISALLOW_COPY_AND_ASSIGN(VideoAdapter);
};

// CoordinatedVideoAdapter adapts the video input to the encoder by coordinating
// the format request from the server, the resolution request from the encoder,
// and the CPU load.
class CoordinatedVideoAdapter
    : public VideoAdapter, public sigslot::has_slots<>  {
 public:
  enum AdaptRequest { UPGRADE, KEEP, DOWNGRADE };
  enum {
    ADAPTREASON_CPU = 1,
    ADAPTREASON_BANDWIDTH = 2,
    ADAPTREASON_VIEW = 4
  };
  typedef int AdaptReason;

  CoordinatedVideoAdapter();
  virtual ~CoordinatedVideoAdapter() {}

  // Enable or disable video adaptation due to the change of the CPU load.
  void set_cpu_adaptation(bool enable) { cpu_adaptation_ = enable; }
  bool cpu_adaptation() const { return cpu_adaptation_; }
  // Enable or disable video adaptation due to the change of the GD
  void set_gd_adaptation(bool enable) { gd_adaptation_ = enable; }
  bool gd_adaptation() const { return gd_adaptation_; }
  // Enable or disable video adaptation due to the change of the View
  void set_view_adaptation(bool enable) { view_adaptation_ = enable; }
  bool view_adaptation() const { return view_adaptation_; }
  // Enable or disable video adaptation to fast switch View
  void set_view_switch(bool enable) { view_switch_ = enable; }
  bool view_switch() const { return view_switch_; }

  CoordinatedVideoAdapter::AdaptReason adapt_reason() const {
    return adapt_reason_;
  }

  // When the video is decreased, set the waiting time for CPU adaptation to
  // decrease video again.
  void set_cpu_downgrade_wait_time(uint32 cpu_downgrade_wait_time) {
    if (cpu_downgrade_wait_time_ != static_cast<int>(cpu_downgrade_wait_time)) {
      LOG(LS_INFO) << "VAdapt Change Cpu Downgrade Wait Time from: "
                   << cpu_downgrade_wait_time_ << " to "
                   << cpu_downgrade_wait_time;
      cpu_downgrade_wait_time_ = static_cast<int>(cpu_downgrade_wait_time);
    }
  }
  // CPU system load high threshold for reducing resolution.  e.g. 0.85f
  void set_high_system_threshold(float high_system_threshold) {
    ASSERT(high_system_threshold <= 1.0f);
    ASSERT(high_system_threshold >= 0.0f);
    if (high_system_threshold_ != high_system_threshold) {
      LOG(LS_INFO) << "VAdapt Change High System Threshold from: "
                   << high_system_threshold_ << " to " << high_system_threshold;
      high_system_threshold_ = high_system_threshold;
    }
  }
  float high_system_threshold() const { return high_system_threshold_; }
  // CPU system load low threshold for increasing resolution.  e.g. 0.70f
  void set_low_system_threshold(float low_system_threshold) {
    ASSERT(low_system_threshold <= 1.0f);
    ASSERT(low_system_threshold >= 0.0f);
    if (low_system_threshold_ != low_system_threshold) {
      LOG(LS_INFO) << "VAdapt Change Low System Threshold from: "
                   << low_system_threshold_ << " to " << low_system_threshold;
      low_system_threshold_ = low_system_threshold;
    }
  }
  float low_system_threshold() const { return low_system_threshold_; }
  // CPU process load threshold for reducing resolution.  e.g. 0.10f
  void set_process_threshold(float process_threshold) {
    ASSERT(process_threshold <= 1.0f);
    ASSERT(process_threshold >= 0.0f);
    if (process_threshold_ != process_threshold) {
      LOG(LS_INFO) << "VAdapt Change High Process Threshold from: "
                   << process_threshold_ << " to " << process_threshold;
      process_threshold_ = process_threshold;
    }
  }
  float process_threshold() const { return process_threshold_; }

  // Handle the format request from the server via Jingle update message.
  void OnOutputFormatRequest(const VideoFormat& format);
  // Handle the resolution request from the encoder due to bandwidth changes.
  void OnEncoderResolutionRequest(int width, int height, AdaptRequest request);
  // Handle the CPU load provided by a CPU monitor.
  void OnCpuLoadUpdated(int current_cpus, int max_cpus,
                        float process_load, float system_load);

  sigslot::signal0<> SignalCpuAdaptationUnable;

 private:
  // Adapt to the minimum of the formats the server requests, the CPU wants, and
  // the encoder wants.  Returns true if resolution changed.
  bool AdaptToMinimumFormat(int* new_width, int* new_height);
  bool IsMinimumFormat(int pixels);
  void StepPixelCount(CoordinatedVideoAdapter::AdaptRequest request,
                      int* num_pixels);
  CoordinatedVideoAdapter::AdaptRequest FindCpuRequest(
    int current_cpus, int max_cpus,
    float process_load, float system_load);

  bool cpu_adaptation_;  // True if cpu adaptation is enabled.
  bool gd_adaptation_;  // True if gd adaptation is enabled.
  bool view_adaptation_;  // True if view adaptation is enabled.
  bool view_switch_;  // True if view switch is enabled.
  int cpu_downgrade_count_;
  int cpu_downgrade_wait_time_;
  // cpu system load thresholds relative to max cpus.
  float high_system_threshold_;
  float low_system_threshold_;
  // cpu process load thresholds relative to current cpus.
  float process_threshold_;
  // Video formats that the server view requests, the CPU wants, and the encoder
  // wants respectively. The adapted output format is the minimum of these.
  int view_desired_num_pixels_;
  int64 view_desired_interval_;
  int encoder_desired_num_pixels_;
  int cpu_desired_num_pixels_;
  CoordinatedVideoAdapter::AdaptReason adapt_reason_;
  // The critical section to protect handling requests.
  talk_base::CriticalSection request_critical_section_;

  DISALLOW_COPY_AND_ASSIGN(CoordinatedVideoAdapter);
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_VIDEOADAPTER_H_  // NOLINT
