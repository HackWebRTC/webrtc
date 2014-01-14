/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/app/webrtc/videosource.h"

#include <vector>

#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/session/media/channelmanager.h"

using cricket::CaptureState;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;

namespace {

const double kRoundingTruncation = 0.0005;

enum {
  MSG_VIDEOCAPTURESTATECONNECT,
  MSG_VIDEOCAPTURESTATEDISCONNECT,
  MSG_VIDEOCAPTURESTATECHANGE,
};

// Default resolution. If no constraint is specified, this is the resolution we
// will use.
static const cricket::VideoFormatPod kDefaultFormat =
    {640, 480, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY};

// List of formats used if the camera doesn't support capability enumeration.
static const cricket::VideoFormatPod kVideoFormats[] = {
  {1920, 1080, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
  {1280, 720, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
  {960, 720, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
  {640, 360, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
  {640, 480, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
  {320, 240, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
  {320, 180, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY}
};

MediaSourceInterface::SourceState
GetReadyState(cricket::CaptureState state) {
  switch (state) {
    case cricket::CS_STARTING:
      return MediaSourceInterface::kInitializing;
    case cricket::CS_RUNNING:
      return MediaSourceInterface::kLive;
    case cricket::CS_FAILED:
    case cricket::CS_NO_DEVICE:
    case cricket::CS_STOPPED:
      return MediaSourceInterface::kEnded;
    case cricket::CS_PAUSED:
      return MediaSourceInterface::kMuted;
    default:
      ASSERT(false && "GetReadyState unknown state");
  }
  return MediaSourceInterface::kEnded;
}

void SetUpperLimit(int new_limit, int* original_limit) {
  if (*original_limit < 0 || new_limit < *original_limit)
    *original_limit = new_limit;
}

// Updates |format_upper_limit| from |constraint|.
// If constraint.maxFoo is smaller than format_upper_limit.foo,
// set format_upper_limit.foo to constraint.maxFoo.
void SetUpperLimitFromConstraint(
    const MediaConstraintsInterface::Constraint& constraint,
    cricket::VideoFormat* format_upper_limit) {
  if (constraint.key == MediaConstraintsInterface::kMaxWidth) {
    int value = talk_base::FromString<int>(constraint.value);
    SetUpperLimit(value, &(format_upper_limit->width));
  } else if (constraint.key == MediaConstraintsInterface::kMaxHeight) {
    int value = talk_base::FromString<int>(constraint.value);
    SetUpperLimit(value, &(format_upper_limit->height));
  }
}

// Fills |format_out| with the max width and height allowed by |constraints|.
void FromConstraintsForScreencast(
    const MediaConstraintsInterface::Constraints& constraints,
    cricket::VideoFormat* format_out) {
  typedef MediaConstraintsInterface::Constraints::const_iterator
      ConstraintsIterator;

  cricket::VideoFormat upper_limit(-1, -1, 0, 0);
  for (ConstraintsIterator constraints_it = constraints.begin();
       constraints_it != constraints.end(); ++constraints_it)
    SetUpperLimitFromConstraint(*constraints_it, &upper_limit);

  if (upper_limit.width >= 0)
    format_out->width = upper_limit.width;
  if (upper_limit.height >= 0)
    format_out->height = upper_limit.height;
}

// Returns true if |constraint| is fulfilled. |format_out| can differ from
// |format_in| if the format is changed by the constraint. Ie - the frame rate
// can be changed by setting maxFrameRate.
bool NewFormatWithConstraints(
    const MediaConstraintsInterface::Constraint& constraint,
    const cricket::VideoFormat& format_in,
    bool mandatory,
    cricket::VideoFormat* format_out) {
  ASSERT(format_out != NULL);
  *format_out = format_in;

  if (constraint.key == MediaConstraintsInterface::kMinWidth) {
    int value = talk_base::FromString<int>(constraint.value);
    return (value <= format_in.width);
  } else if (constraint.key == MediaConstraintsInterface::kMaxWidth) {
    int value = talk_base::FromString<int>(constraint.value);
    return (value >= format_in.width);
  } else if (constraint.key == MediaConstraintsInterface::kMinHeight) {
    int value = talk_base::FromString<int>(constraint.value);
    return (value <= format_in.height);
  } else if (constraint.key == MediaConstraintsInterface::kMaxHeight) {
    int value = talk_base::FromString<int>(constraint.value);
    return (value >= format_in.height);
  } else if (constraint.key == MediaConstraintsInterface::kMinFrameRate) {
    int value = talk_base::FromString<int>(constraint.value);
    return (value <= cricket::VideoFormat::IntervalToFps(format_in.interval));
  } else if (constraint.key == MediaConstraintsInterface::kMaxFrameRate) {
    int value = talk_base::FromString<int>(constraint.value);
    if (value == 0) {
      if (mandatory) {
        // TODO(ronghuawu): Convert the constraint value to float when sub-1fps
        // is supported by the capturer.
        return false;
      } else {
        value = 1;
      }
    }
    if (value <= cricket::VideoFormat::IntervalToFps(format_in.interval)) {
      format_out->interval = cricket::VideoFormat::FpsToInterval(value);
      return true;
    } else {
      return false;
    }
  } else if (constraint.key == MediaConstraintsInterface::kMinAspectRatio) {
    double value = talk_base::FromString<double>(constraint.value);
    // The aspect ratio in |constraint.value| has been converted to a string and
    // back to a double, so it may have a rounding error.
    // E.g if the value 1/3 is converted to a string, the string will not have
    // infinite length.
    // We add a margin of 0.0005 which is high enough to detect the same aspect
    // ratio but small enough to avoid matching wrong aspect ratios.
    double ratio = static_cast<double>(format_in.width) / format_in.height;
    return  (value <= ratio + kRoundingTruncation);
  } else if (constraint.key == MediaConstraintsInterface::kMaxAspectRatio) {
    double value = talk_base::FromString<double>(constraint.value);
    double ratio = static_cast<double>(format_in.width) / format_in.height;
    // Subtract 0.0005 to avoid rounding problems. Same as above.
    const double kRoundingTruncation = 0.0005;
    return  (value >= ratio - kRoundingTruncation);
  } else if (constraint.key == MediaConstraintsInterface::kNoiseReduction ||
             constraint.key == MediaConstraintsInterface::kLeakyBucket ||
             constraint.key ==
                 MediaConstraintsInterface::kTemporalLayeredScreencast ||
             constraint.key ==
                 MediaConstraintsInterface::kCpuOveruseDetection) {
    // These are actually options, not constraints, so they can be satisfied
    // regardless of the format.
    return true;
  }
  LOG(LS_WARNING) << "Found unknown MediaStream constraint. Name:"
      <<  constraint.key << " Value:" << constraint.value;
  return false;
}

// Removes cricket::VideoFormats from |formats| that don't meet |constraint|.
void FilterFormatsByConstraint(
    const MediaConstraintsInterface::Constraint& constraint,
    bool mandatory,
    std::vector<cricket::VideoFormat>* formats) {
  std::vector<cricket::VideoFormat>::iterator format_it =
      formats->begin();
  while (format_it != formats->end()) {
    // Modify the format_it to fulfill the constraint if possible.
    // Delete it otherwise.
    if (!NewFormatWithConstraints(constraint, (*format_it),
                                  mandatory, &(*format_it))) {
      format_it = formats->erase(format_it);
    } else {
      ++format_it;
    }
  }
}

// Returns a vector of cricket::VideoFormat that best match |constraints|.
std::vector<cricket::VideoFormat> FilterFormats(
    const MediaConstraintsInterface::Constraints& mandatory,
    const MediaConstraintsInterface::Constraints& optional,
    const std::vector<cricket::VideoFormat>& supported_formats) {
  typedef MediaConstraintsInterface::Constraints::const_iterator
      ConstraintsIterator;
  std::vector<cricket::VideoFormat> candidates = supported_formats;

  for (ConstraintsIterator constraints_it = mandatory.begin();
       constraints_it != mandatory.end(); ++constraints_it)
    FilterFormatsByConstraint(*constraints_it, true, &candidates);

  if (candidates.size() == 0)
    return candidates;

  // Ok - all mandatory checked and we still have a candidate.
  // Let's try filtering using the optional constraints.
  for (ConstraintsIterator  constraints_it = optional.begin();
       constraints_it != optional.end(); ++constraints_it) {
    std::vector<cricket::VideoFormat> current_candidates = candidates;
    FilterFormatsByConstraint(*constraints_it, false, &current_candidates);
    if (current_candidates.size() > 0) {
      candidates = current_candidates;
    }
  }

  // We have done as good as we can to filter the supported resolutions.
  return candidates;
}

// Find the format that best matches the default video size.
// Constraints are optional and since the performance of a video call
// might be bad due to bitrate limitations, CPU, and camera performance,
// it is better to select a resolution that is as close as possible to our
// default and still meets the contraints.
const cricket::VideoFormat& GetBestCaptureFormat(
    const std::vector<cricket::VideoFormat>& formats) {
  ASSERT(formats.size() > 0);

  int default_area = kDefaultFormat.width * kDefaultFormat.height;

  std::vector<cricket::VideoFormat>::const_iterator it = formats.begin();
  std::vector<cricket::VideoFormat>::const_iterator best_it = formats.begin();
  int best_diff = abs(default_area - it->width* it->height);
  for (; it != formats.end(); ++it) {
    int diff = abs(default_area - it->width* it->height);
    if (diff < best_diff) {
      best_diff = diff;
      best_it = it;
    }
  }
  return *best_it;
}

// Set |option| to the highest-priority value of |key| in the constraints.
// Return false if the key is mandatory, and the value is invalid.
bool ExtractOption(const MediaConstraintsInterface* all_constraints,
    const std::string& key, cricket::Settable<bool>* option) {
  size_t mandatory = 0;
  bool value;
  if (FindConstraint(all_constraints, key, &value, &mandatory)) {
    option->Set(value);
    return true;
  }

  return mandatory == 0;
}

// Search |all_constraints| for known video options.  Apply all options that are
// found with valid values, and return false if any mandatory video option was
// found with an invalid value.
bool ExtractVideoOptions(const MediaConstraintsInterface* all_constraints,
                         cricket::VideoOptions* options) {
  bool all_valid = true;

  all_valid &= ExtractOption(all_constraints,
      MediaConstraintsInterface::kNoiseReduction,
      &(options->video_noise_reduction));
  all_valid &= ExtractOption(all_constraints,
      MediaConstraintsInterface::kLeakyBucket,
      &(options->video_leaky_bucket));
  all_valid &= ExtractOption(all_constraints,
      MediaConstraintsInterface::kTemporalLayeredScreencast,
      &(options->video_temporal_layer_screencast));
  all_valid &= ExtractOption(all_constraints,
      MediaConstraintsInterface::kCpuOveruseDetection,
      &(options->cpu_overuse_detection));

  return all_valid;
}

class FrameInputWrapper : public cricket::VideoRenderer {
 public:
  explicit FrameInputWrapper(cricket::VideoCapturer* capturer)
      : capturer_(capturer) {
    ASSERT(capturer_ != NULL);
  }

  virtual ~FrameInputWrapper() {}

  // VideoRenderer implementation.
  virtual bool SetSize(int width, int height, int reserved) OVERRIDE {
    return true;
  }

  virtual bool RenderFrame(const cricket::VideoFrame* frame) OVERRIDE {
    if (!capturer_->IsRunning()) {
      return true;
    }

    // This signal will be made on media engine render thread. The clients
    // of this signal should have no assumptions on what thread this signal
    // come from.
    capturer_->SignalVideoFrame(capturer_, frame);
    return true;
  }

 private:
  cricket::VideoCapturer* capturer_;
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(FrameInputWrapper);
};

}  // anonymous namespace

namespace webrtc {

talk_base::scoped_refptr<VideoSource> VideoSource::Create(
    cricket::ChannelManager* channel_manager,
    cricket::VideoCapturer* capturer,
    const webrtc::MediaConstraintsInterface* constraints) {
  ASSERT(channel_manager != NULL);
  ASSERT(capturer != NULL);
  talk_base::scoped_refptr<VideoSource> source(
      new talk_base::RefCountedObject<VideoSource>(channel_manager,
                                                   capturer));
  source->Initialize(constraints);
  return source;
}

VideoSource::VideoSource(cricket::ChannelManager* channel_manager,
                         cricket::VideoCapturer* capturer)
    : channel_manager_(channel_manager),
      video_capturer_(capturer),
      state_(kInitializing) {
  channel_manager_->SignalVideoCaptureStateChange.connect(
      this, &VideoSource::OnStateChange);
}

VideoSource::~VideoSource() {
  channel_manager_->StopVideoCapture(video_capturer_.get(), format_);
  channel_manager_->SignalVideoCaptureStateChange.disconnect(this);
}

void VideoSource::Initialize(
    const webrtc::MediaConstraintsInterface* constraints) {

  std::vector<cricket::VideoFormat> formats;
  if (video_capturer_->GetSupportedFormats() &&
      video_capturer_->GetSupportedFormats()->size() > 0) {
    formats = *video_capturer_->GetSupportedFormats();
  } else if (video_capturer_->IsScreencast()) {
    // The screen capturer can accept any resolution and we will derive the
    // format from the constraints if any.
    // Note that this only affects tab capturing, not desktop capturing,
    // since desktop capturer does not respect the VideoFormat passed in.
    formats.push_back(cricket::VideoFormat(kDefaultFormat));
  } else {
    // The VideoCapturer implementation doesn't support capability enumeration.
    // We need to guess what the camera support.
    for (int i = 0; i < ARRAY_SIZE(kVideoFormats); ++i) {
      formats.push_back(cricket::VideoFormat(kVideoFormats[i]));
    }
  }

  if (constraints) {
    MediaConstraintsInterface::Constraints mandatory_constraints =
        constraints->GetMandatory();
    MediaConstraintsInterface::Constraints optional_constraints;
    optional_constraints = constraints->GetOptional();

    if (video_capturer_->IsScreencast()) {
      // Use the maxWidth and maxHeight allowed by constraints for screencast.
      FromConstraintsForScreencast(mandatory_constraints, &(formats[0]));
    }

    formats = FilterFormats(mandatory_constraints, optional_constraints,
                            formats);
  }

  if (formats.size() == 0) {
    LOG(LS_WARNING) << "Failed to find a suitable video format.";
    SetState(kEnded);
    return;
  }

  cricket::VideoOptions options;
  if (!ExtractVideoOptions(constraints, &options)) {
    LOG(LS_WARNING) << "Could not satisfy mandatory options.";
    SetState(kEnded);
    return;
  }
  options_.SetAll(options);

  format_ = GetBestCaptureFormat(formats);
  // Start the camera with our best guess.
  // TODO(perkj): Should we try again with another format it it turns out that
  // the camera doesn't produce frames with the correct format? Or will
  // cricket::VideCapturer be able to re-scale / crop to the requested
  // resolution?
  if (!channel_manager_->StartVideoCapture(video_capturer_.get(), format_)) {
    SetState(kEnded);
    return;
  }
  // Initialize hasn't succeeded until a successful state change has occurred.
}

cricket::VideoRenderer* VideoSource::FrameInput() {
  // Defer creation of frame_input_ until it's needed, e.g. the local video
  // sources will never need it.
  if (!frame_input_) {
    frame_input_.reset(new FrameInputWrapper(video_capturer_.get()));
  }
  return frame_input_.get();
}

void VideoSource::AddSink(cricket::VideoRenderer* output) {
  channel_manager_->AddVideoRenderer(video_capturer_.get(), output);
}

void VideoSource::RemoveSink(cricket::VideoRenderer* output) {
  channel_manager_->RemoveVideoRenderer(video_capturer_.get(), output);
}

// OnStateChange listens to the ChannelManager::SignalVideoCaptureStateChange.
// This signal is triggered for all video capturers. Not only the one we are
// interested in.
void VideoSource::OnStateChange(cricket::VideoCapturer* capturer,
                                     cricket::CaptureState capture_state) {
  if (capturer == video_capturer_.get()) {
    SetState(GetReadyState(capture_state));
  }
}

void VideoSource::SetState(SourceState new_state) {
  if (VERIFY(state_ != new_state)) {
    state_ = new_state;
    FireOnChanged();
  }
}

}  // namespace webrtc
