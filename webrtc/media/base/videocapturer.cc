/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Implementation file of class VideoCapturer.

#include "webrtc/media/base/videocapturer.h"

#include <algorithm>

#include "libyuv/scale_argb.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/systeminfo.h"
#include "webrtc/media/base/videoframefactory.h"
#include "webrtc/media/engine/webrtcvideoframe.h"
#include "webrtc/media/engine/webrtcvideoframefactory.h"

namespace cricket {

namespace {

static const int64_t kMaxDistance = ~(static_cast<int64_t>(1) << 63);
#ifdef WEBRTC_LINUX
static const int kYU12Penalty = 16;  // Needs to be higher than MJPG index.
#endif
static const int kDefaultScreencastFps = 5;

}  // namespace

/////////////////////////////////////////////////////////////////////
// Implementation of struct CapturedFrame
/////////////////////////////////////////////////////////////////////
CapturedFrame::CapturedFrame()
    : width(0),
      height(0),
      fourcc(0),
      pixel_width(0),
      pixel_height(0),
      time_stamp(0),
      data_size(0),
      rotation(webrtc::kVideoRotation_0),
      data(NULL) {}

// TODO(fbarchard): Remove this function once lmimediaengine stops using it.
bool CapturedFrame::GetDataSize(uint32_t* size) const {
  if (!size || data_size == CapturedFrame::kUnknownDataSize) {
    return false;
  }
  *size = data_size;
  return true;
}

/////////////////////////////////////////////////////////////////////
// Implementation of class VideoCapturer
/////////////////////////////////////////////////////////////////////
VideoCapturer::VideoCapturer() : apply_rotation_(false) {
  thread_checker_.DetachFromThread();
  Construct();
}

void VideoCapturer::Construct() {
  ratio_w_ = 0;
  ratio_h_ = 0;
  enable_camera_list_ = false;
  square_pixel_aspect_ratio_ = false;
  capture_state_ = CS_STOPPED;
  SignalFrameCaptured.connect(this, &VideoCapturer::OnFrameCaptured);
  scaled_width_ = 0;
  scaled_height_ = 0;
  enable_video_adapter_ = true;
  // There are lots of video capturers out there that don't call
  // set_frame_factory.  We can either go change all of them, or we
  // can set this default.
  // TODO(pthatcher): Remove this hack and require the frame factory
  // to be passed in the constructor.
  set_frame_factory(new WebRtcVideoFrameFactory());
}

const std::vector<VideoFormat>* VideoCapturer::GetSupportedFormats() const {
  return &filtered_supported_formats_;
}

bool VideoCapturer::StartCapturing(const VideoFormat& capture_format) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  CaptureState result = Start(capture_format);
  const bool success = (result == CS_RUNNING) || (result == CS_STARTING);
  if (!success) {
    return false;
  }
  if (result == CS_RUNNING) {
    SetCaptureState(result);
  }
  return true;
}

void VideoCapturer::SetSupportedFormats(
    const std::vector<VideoFormat>& formats) {
  // This method is OK to call during initialization on a separate thread.
  RTC_DCHECK(capture_state_ == CS_STOPPED ||
             thread_checker_.CalledOnValidThread());
  supported_formats_ = formats;
  UpdateFilteredSupportedFormats();
}

bool VideoCapturer::GetBestCaptureFormat(const VideoFormat& format,
                                         VideoFormat* best_format) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(fbarchard): Directly support max_format.
  UpdateFilteredSupportedFormats();
  const std::vector<VideoFormat>* supported_formats = GetSupportedFormats();

  if (supported_formats->empty()) {
    return false;
  }
  LOG(LS_INFO) << " Capture Requested " << format.ToString();
  int64_t best_distance = kMaxDistance;
  std::vector<VideoFormat>::const_iterator best = supported_formats->end();
  std::vector<VideoFormat>::const_iterator i;
  for (i = supported_formats->begin(); i != supported_formats->end(); ++i) {
    int64_t distance = GetFormatDistance(format, *i);
    // TODO(fbarchard): Reduce to LS_VERBOSE if/when camera capture is
    // relatively bug free.
    LOG(LS_INFO) << " Supported " << i->ToString() << " distance " << distance;
    if (distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }
  if (supported_formats->end() == best) {
    LOG(LS_ERROR) << " No acceptable camera format found";
    return false;
  }

  if (best_format) {
    best_format->width = best->width;
    best_format->height = best->height;
    best_format->fourcc = best->fourcc;
    best_format->interval = best->interval;
    LOG(LS_INFO) << " Best " << best_format->ToString() << " Interval "
                 << best_format->interval << " distance " << best_distance;
  }
  return true;
}

void VideoCapturer::ConstrainSupportedFormats(const VideoFormat& max_format) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  max_format_.reset(new VideoFormat(max_format));
  LOG(LS_VERBOSE) << " ConstrainSupportedFormats " << max_format.ToString();
  UpdateFilteredSupportedFormats();
}

std::string VideoCapturer::ToString(const CapturedFrame* captured_frame) const {
  std::string fourcc_name = GetFourccName(captured_frame->fourcc) + " ";
  for (std::string::const_iterator i = fourcc_name.begin();
       i < fourcc_name.end(); ++i) {
    // Test character is printable; Avoid isprint() which asserts on negatives.
    if (*i < 32 || *i >= 127) {
      fourcc_name = "";
      break;
    }
  }

  std::ostringstream ss;
  ss << fourcc_name << captured_frame->width << "x" << captured_frame->height;
  return ss.str();
}

void VideoCapturer::set_frame_factory(VideoFrameFactory* frame_factory) {
  frame_factory_.reset(frame_factory);
  if (frame_factory) {
    frame_factory->SetApplyRotation(apply_rotation_);
  }
}

bool VideoCapturer::GetInputSize(int* width, int* height) {
  rtc::CritScope cs(&frame_stats_crit_);
  if (!input_size_valid_) {
    return false;
  }
  *width = input_width_;
  *height = input_height_;

  return true;
}

void VideoCapturer::RemoveSink(
    rtc::VideoSinkInterface<cricket::VideoFrame>* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  broadcaster_.RemoveSink(sink);
  OnSinkWantsChanged(broadcaster_.wants());
}

void VideoCapturer::AddOrUpdateSink(
    rtc::VideoSinkInterface<cricket::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  broadcaster_.AddOrUpdateSink(sink, wants);
  OnSinkWantsChanged(broadcaster_.wants());
}

void VideoCapturer::OnSinkWantsChanged(const rtc::VideoSinkWants& wants) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  apply_rotation_ = wants.rotation_applied;
  if (frame_factory_) {
    frame_factory_->SetApplyRotation(apply_rotation_);
  }

  if (video_adapter()) {
    video_adapter()->OnResolutionRequest(wants.max_pixel_count,
                                         wants.max_pixel_count_step_up);
  }
}

void VideoCapturer::OnFrameCaptured(VideoCapturer*,
                                    const CapturedFrame* captured_frame) {
  if (!broadcaster_.frame_wanted()) {
    return;
  }

  // Use a temporary buffer to scale
  std::unique_ptr<uint8_t[]> scale_buffer;
  if (IsScreencast()) {
    int scaled_width, scaled_height;
    int desired_screencast_fps =
        capture_format_.get()
            ? VideoFormat::IntervalToFps(capture_format_->interval)
            : kDefaultScreencastFps;
    ComputeScale(captured_frame->width, captured_frame->height,
                 desired_screencast_fps, &scaled_width, &scaled_height);

    if (FOURCC_ARGB == captured_frame->fourcc &&
        (scaled_width != captured_frame->width ||
         scaled_height != captured_frame->height)) {
      if (scaled_width != scaled_width_ || scaled_height != scaled_height_) {
        LOG(LS_INFO) << "Scaling Screencast from " << captured_frame->width
                     << "x" << captured_frame->height << " to " << scaled_width
                     << "x" << scaled_height;
        scaled_width_ = scaled_width;
        scaled_height_ = scaled_height;
      }
      CapturedFrame* modified_frame =
          const_cast<CapturedFrame*>(captured_frame);
      const int modified_frame_size = scaled_width * scaled_height * 4;
      scale_buffer.reset(new uint8_t[modified_frame_size]);
      // Compute new width such that width * height is less than maximum but
      // maintains original captured frame aspect ratio.
      // Round down width to multiple of 4 so odd width won't round up beyond
      // maximum, and so chroma channel is even width to simplify spatial
      // resampling.
      libyuv::ARGBScale(reinterpret_cast<const uint8_t*>(captured_frame->data),
                        captured_frame->width * 4, captured_frame->width,
                        captured_frame->height, scale_buffer.get(),
                        scaled_width * 4, scaled_width, scaled_height,
                        libyuv::kFilterBilinear);
      modified_frame->width = scaled_width;
      modified_frame->height = scaled_height;
      modified_frame->data_size = scaled_width * 4 * scaled_height;
      modified_frame->data = scale_buffer.get();
    }
  }

  const int kYuy2Bpp = 2;
  const int kArgbBpp = 4;
  // TODO(fbarchard): Make a helper function to adjust pixels to square.
  // TODO(fbarchard): Hook up experiment to scaling.
  // Temporary buffer is scoped here so it will persist until i420_frame.Init()
  // makes a copy of the frame, converting to I420.
  std::unique_ptr<uint8_t[]> temp_buffer;
  // YUY2 can be scaled vertically using an ARGB scaler.  Aspect ratio is only
  // a problem on OSX.  OSX always converts webcams to YUY2 or UYVY.
  bool can_scale =
      FOURCC_YUY2 == CanonicalFourCC(captured_frame->fourcc) ||
      FOURCC_UYVY == CanonicalFourCC(captured_frame->fourcc);

  // If pixels are not square, optionally use vertical scaling to make them
  // square.  Square pixels simplify the rest of the pipeline, including
  // effects and rendering.
  if (can_scale && square_pixel_aspect_ratio_ &&
      captured_frame->pixel_width != captured_frame->pixel_height) {
    int scaled_width, scaled_height;
    // modified_frame points to the captured_frame but with const casted away
    // so it can be modified.
    CapturedFrame* modified_frame = const_cast<CapturedFrame*>(captured_frame);
    // Compute the frame size that makes pixels square pixel aspect ratio.
    ComputeScaleToSquarePixels(captured_frame->width, captured_frame->height,
                               captured_frame->pixel_width,
                               captured_frame->pixel_height,
                               &scaled_width, &scaled_height);

    if (scaled_width != scaled_width_ || scaled_height != scaled_height_) {
      LOG(LS_INFO) << "Scaling WebCam from "
                   << captured_frame->width << "x"
                   << captured_frame->height << " to "
                   << scaled_width << "x" << scaled_height
                   << " for PAR "
                   << captured_frame->pixel_width << "x"
                   << captured_frame->pixel_height;
      scaled_width_ = scaled_width;
      scaled_height_ = scaled_height;
    }
    const int modified_frame_size = scaled_width * scaled_height * kYuy2Bpp;
    uint8_t* temp_buffer_data;
    // Pixels are wide and short; Increasing height. Requires temporary buffer.
    if (scaled_height > captured_frame->height) {
      temp_buffer.reset(new uint8_t[modified_frame_size]);
      temp_buffer_data = temp_buffer.get();
    } else {
      // Pixels are narrow and tall; Decreasing height. Scale will be done
      // in place.
      temp_buffer_data = reinterpret_cast<uint8_t*>(captured_frame->data);
    }

    // Use ARGBScaler to vertically scale the YUY2 image, adjusting for 16 bpp.
    libyuv::ARGBScale(reinterpret_cast<const uint8_t*>(captured_frame->data),
                      captured_frame->width * kYuy2Bpp,  // Stride for YUY2.
                      captured_frame->width * kYuy2Bpp / kArgbBpp,  // Width.
                      abs(captured_frame->height),                  // Height.
                      temp_buffer_data,
                      scaled_width * kYuy2Bpp,             // Stride for YUY2.
                      scaled_width * kYuy2Bpp / kArgbBpp,  // Width.
                      abs(scaled_height),                  // New height.
                      libyuv::kFilterBilinear);
    modified_frame->width = scaled_width;
    modified_frame->height = scaled_height;
    modified_frame->pixel_width = 1;
    modified_frame->pixel_height = 1;
    modified_frame->data_size = modified_frame_size;
    modified_frame->data = temp_buffer_data;
  }

  // Size to crop captured frame to.  This adjusts the captured frames
  // aspect ratio to match the final view aspect ratio, considering pixel
  // aspect ratio and rotation.  The final size may be scaled down by video
  // adapter to better match ratio_w_ x ratio_h_.
  // Note that abs() of frame height is passed in, because source may be
  // inverted, but output will be positive.
  int cropped_width = captured_frame->width;
  int cropped_height = captured_frame->height;

  // TODO(fbarchard): Improve logic to pad or crop.
  // MJPG can crop vertically, but not horizontally.  This logic disables crop.
  // Alternatively we could pad the image with black, or implement a 2 step
  // crop.
  bool can_crop = true;
  if (captured_frame->fourcc == FOURCC_MJPG) {
    float cam_aspect = static_cast<float>(captured_frame->width) /
        static_cast<float>(captured_frame->height);
    float view_aspect = static_cast<float>(ratio_w_) /
        static_cast<float>(ratio_h_);
    can_crop = cam_aspect <= view_aspect;
  }
  if (can_crop && !IsScreencast()) {
    // TODO(ronghuawu): The capturer should always produce the native
    // resolution and the cropping should be done in downstream code.
    ComputeCrop(ratio_w_, ratio_h_, captured_frame->width,
                abs(captured_frame->height), captured_frame->pixel_width,
                captured_frame->pixel_height, captured_frame->rotation,
                &cropped_width, &cropped_height);
  }

  int adapted_width = cropped_width;
  int adapted_height = cropped_height;
  if (enable_video_adapter_ && !IsScreencast()) {
    const VideoFormat adapted_format =
        video_adapter_.AdaptFrameResolution(cropped_width, cropped_height);
    if (adapted_format.IsSize0x0()) {
      // VideoAdapter dropped the frame.
      return;
    }
    adapted_width = adapted_format.width;
    adapted_height = adapted_format.height;
  }

  if (!frame_factory_) {
    LOG(LS_ERROR) << "No video frame factory.";
    return;
  }

  std::unique_ptr<VideoFrame> adapted_frame(
      frame_factory_->CreateAliasedFrame(captured_frame,
                                         cropped_width, cropped_height,
                                         adapted_width, adapted_height));

  if (!adapted_frame) {
    // TODO(fbarchard): LOG more information about captured frame attributes.
    LOG(LS_ERROR) << "Couldn't convert to I420! "
                  << "From " << ToString(captured_frame) << " To "
                  << cropped_width << " x " << cropped_height;
    return;
  }

  OnFrame(this, adapted_frame.get());
  UpdateInputSize(captured_frame);
}

void VideoCapturer::OnFrame(VideoCapturer* capturer, const VideoFrame* frame) {
  broadcaster_.OnFrame(*frame);
}

void VideoCapturer::SetCaptureState(CaptureState state) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (state == capture_state_) {
    // Don't trigger a state changed callback if the state hasn't changed.
    return;
  }
  capture_state_ = state;
  SignalStateChange(this, capture_state_);
}

// Get the distance between the supported and desired formats.
// Prioritization is done according to this algorithm:
// 1) Width closeness. If not same, we prefer wider.
// 2) Height closeness. If not same, we prefer higher.
// 3) Framerate closeness. If not same, we prefer faster.
// 4) Compression. If desired format has a specific fourcc, we need exact match;
//                otherwise, we use preference.
int64_t VideoCapturer::GetFormatDistance(const VideoFormat& desired,
                                         const VideoFormat& supported) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int64_t distance = kMaxDistance;

  // Check fourcc.
  uint32_t supported_fourcc = CanonicalFourCC(supported.fourcc);
  int64_t delta_fourcc = kMaxDistance;
  if (FOURCC_ANY == desired.fourcc) {
    // Any fourcc is OK for the desired. Use preference to find best fourcc.
    std::vector<uint32_t> preferred_fourccs;
    if (!GetPreferredFourccs(&preferred_fourccs)) {
      return distance;
    }

    for (size_t i = 0; i < preferred_fourccs.size(); ++i) {
      if (supported_fourcc == CanonicalFourCC(preferred_fourccs[i])) {
        delta_fourcc = i;
#ifdef WEBRTC_LINUX
        // For HD avoid YU12 which is a software conversion and has 2 bugs
        // b/7326348 b/6960899.  Reenable when fixed.
        if (supported.height >= 720 && (supported_fourcc == FOURCC_YU12 ||
                                        supported_fourcc == FOURCC_YV12)) {
          delta_fourcc += kYU12Penalty;
        }
#endif
        break;
      }
    }
  } else if (supported_fourcc == CanonicalFourCC(desired.fourcc)) {
    delta_fourcc = 0;  // Need exact match.
  }

  if (kMaxDistance == delta_fourcc) {
    // Failed to match fourcc.
    return distance;
  }

  // Check resolution and fps.
  int desired_width = desired.width;
  int desired_height = desired.height;
  int64_t delta_w = supported.width - desired_width;
  float supported_fps = VideoFormat::IntervalToFpsFloat(supported.interval);
  float delta_fps =
      supported_fps - VideoFormat::IntervalToFpsFloat(desired.interval);
  // Check height of supported height compared to height we would like it to be.
  int64_t aspect_h = desired_width
                         ? supported.width * desired_height / desired_width
                         : desired_height;
  int64_t delta_h = supported.height - aspect_h;

  distance = 0;
  // Set high penalty if the supported format is lower than the desired format.
  // 3x means we would prefer down to down to 3/4, than up to double.
  // But we'd prefer up to double than down to 1/2.  This is conservative,
  // strongly avoiding going down in resolution, similar to
  // the old method, but not completely ruling it out in extreme situations.
  // It also ignores framerate, which is often very low at high resolutions.
  // TODO(fbarchard): Improve logic to use weighted factors.
  static const int kDownPenalty = -3;
  if (delta_w < 0) {
    delta_w = delta_w * kDownPenalty;
  }
  if (delta_h < 0) {
    delta_h = delta_h * kDownPenalty;
  }
  // Require camera fps to be at least 80% of what is requested if resolution
  // matches.
  // Require camera fps to be at least 96% of what is requested, or higher,
  // if resolution differs. 96% allows for slight variations in fps. e.g. 29.97
  if (delta_fps < 0) {
    float min_desirable_fps = delta_w ?
    VideoFormat::IntervalToFpsFloat(desired.interval) * 28.f / 30.f :
    VideoFormat::IntervalToFpsFloat(desired.interval) * 23.f / 30.f;
    delta_fps = -delta_fps;
    if (supported_fps < min_desirable_fps) {
      distance |= static_cast<int64_t>(1) << 62;
    } else {
      distance |= static_cast<int64_t>(1) << 15;
    }
  }
  int64_t idelta_fps = static_cast<int>(delta_fps);

  // 12 bits for width and height and 8 bits for fps and fourcc.
  distance |=
      (delta_w << 28) | (delta_h << 16) | (idelta_fps << 8) | delta_fourcc;

  return distance;
}

void VideoCapturer::UpdateFilteredSupportedFormats() {
  filtered_supported_formats_.clear();
  filtered_supported_formats_ = supported_formats_;
  if (!max_format_) {
    return;
  }
  std::vector<VideoFormat>::iterator iter = filtered_supported_formats_.begin();
  while (iter != filtered_supported_formats_.end()) {
    if (ShouldFilterFormat(*iter)) {
      iter = filtered_supported_formats_.erase(iter);
    } else {
      ++iter;
    }
  }
  if (filtered_supported_formats_.empty()) {
    // The device only captures at resolutions higher than |max_format_| this
    // indicates that |max_format_| should be ignored as it is better to capture
    // at too high a resolution than to not capture at all.
    filtered_supported_formats_ = supported_formats_;
  }
}

bool VideoCapturer::ShouldFilterFormat(const VideoFormat& format) const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!enable_camera_list_) {
    return false;
  }
  return format.width > max_format_->width ||
         format.height > max_format_->height;
}

void VideoCapturer::UpdateInputSize(const CapturedFrame* captured_frame) {
  // Update stats protected from fetches from different thread.
  rtc::CritScope cs(&frame_stats_crit_);

  input_size_valid_ = true;
  input_width_ = captured_frame->width;
  input_height_ = captured_frame->height;
}

}  // namespace cricket
