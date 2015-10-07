/*
 * libjingle
 * Copyright 2010 Google Inc.
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

// Implementation file of class VideoCapturer.

#include "talk/media/base/videocapturer.h"

#include <algorithm>

#include "libyuv/scale_argb.h"
#include "talk/media/base/videoframefactory.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/systeminfo.h"

#if defined(HAVE_WEBRTC_VIDEO)
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "talk/media/webrtc/webrtcvideoframefactory.h"
#endif  // HAVE_WEBRTC_VIDEO

namespace cricket {

namespace {

// TODO(thorcarpenter): This is a BIG hack to flush the system with black
// frames. Frontends should coordinate to update the video state of a muted
// user. When all frontends to this consider removing the black frame business.
const int kNumBlackFramesOnMute = 30;

// MessageHandler constants.
enum {
  MSG_DO_PAUSE = 0,
  MSG_DO_UNPAUSE,
  MSG_STATE_CHANGE
};

static const int64_t kMaxDistance = ~(static_cast<int64_t>(1) << 63);
#ifdef LINUX
static const int kYU12Penalty = 16;  // Needs to be higher than MJPG index.
#endif
static const int kDefaultScreencastFps = 5;
typedef rtc::TypedMessageData<CaptureState> StateChangeParams;

// Limit stats data collections to ~20 seconds of 30fps data before dropping
// old data in case stats aren't reset for long periods of time.
static const size_t kMaxAccumulatorSize = 600;

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
      rotation(0),
      data(NULL) {}

// TODO(fbarchard): Remove this function once lmimediaengine stops using it.
bool CapturedFrame::GetDataSize(uint32_t* size) const {
  if (!size || data_size == CapturedFrame::kUnknownDataSize) {
    return false;
  }
  *size = data_size;
  return true;
}

webrtc::VideoRotation CapturedFrame::GetRotation() const {
  ASSERT(rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270);
  return static_cast<webrtc::VideoRotation>(rotation);
}

/////////////////////////////////////////////////////////////////////
// Implementation of class VideoCapturer
/////////////////////////////////////////////////////////////////////
VideoCapturer::VideoCapturer()
    : thread_(rtc::Thread::Current()),
      adapt_frame_drops_data_(kMaxAccumulatorSize),
      frame_time_data_(kMaxAccumulatorSize),
      apply_rotation_(true) {
  Construct();
}

VideoCapturer::VideoCapturer(rtc::Thread* thread)
    : thread_(thread),
      adapt_frame_drops_data_(kMaxAccumulatorSize),
      frame_time_data_(kMaxAccumulatorSize),
      apply_rotation_(true) {
  Construct();
}

void VideoCapturer::Construct() {
  ClearAspectRatio();
  enable_camera_list_ = false;
  square_pixel_aspect_ratio_ = false;
  capture_state_ = CS_STOPPED;
  SignalFrameCaptured.connect(this, &VideoCapturer::OnFrameCaptured);
  scaled_width_ = 0;
  scaled_height_ = 0;
  screencast_max_pixels_ = 0;
  muted_ = false;
  black_frame_count_down_ = kNumBlackFramesOnMute;
  enable_video_adapter_ = true;
  adapt_frame_drops_ = 0;
  previous_frame_time_ = 0.0;
#ifdef HAVE_WEBRTC_VIDEO
  // There are lots of video capturers out there that don't call
  // set_frame_factory.  We can either go change all of them, or we
  // can set this default.
  // TODO(pthatcher): Remove this hack and require the frame factory
  // to be passed in the constructor.
  set_frame_factory(new WebRtcVideoFrameFactory());
#endif
}

const std::vector<VideoFormat>* VideoCapturer::GetSupportedFormats() const {
  return &filtered_supported_formats_;
}

bool VideoCapturer::StartCapturing(const VideoFormat& capture_format) {
  previous_frame_time_ = frame_length_time_reporter_.TimerNow();
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

void VideoCapturer::UpdateAspectRatio(int ratio_w, int ratio_h) {
  if (ratio_w == 0 || ratio_h == 0) {
    LOG(LS_WARNING) << "UpdateAspectRatio ignored invalid ratio: "
                    << ratio_w << "x" << ratio_h;
    return;
  }
  ratio_w_ = ratio_w;
  ratio_h_ = ratio_h;
}

void VideoCapturer::ClearAspectRatio() {
  ratio_w_ = 0;
  ratio_h_ = 0;
}

// Override this to have more control of how your device is started/stopped.
bool VideoCapturer::Pause(bool pause) {
  if (pause) {
    if (capture_state() == CS_PAUSED) {
      return true;
    }
    bool is_running = capture_state() == CS_STARTING ||
        capture_state() == CS_RUNNING;
    if (!is_running) {
      LOG(LS_ERROR) << "Cannot pause a stopped camera.";
      return false;
    }
    LOG(LS_INFO) << "Pausing a camera.";
    rtc::scoped_ptr<VideoFormat> capture_format_when_paused(
        capture_format_ ? new VideoFormat(*capture_format_) : NULL);
    Stop();
    SetCaptureState(CS_PAUSED);
    // If you override this function be sure to restore the capture format
    // after calling Stop().
    SetCaptureFormat(capture_format_when_paused.get());
  } else {  // Unpause.
    if (capture_state() != CS_PAUSED) {
      LOG(LS_WARNING) << "Cannot unpause a camera that hasn't been paused.";
      return false;
    }
    if (!capture_format_) {
      LOG(LS_ERROR) << "Missing capture_format_, cannot unpause a camera.";
      return false;
    }
    if (muted_) {
      LOG(LS_WARNING) << "Camera cannot be unpaused while muted.";
      return false;
    }
    LOG(LS_INFO) << "Unpausing a camera.";
    if (!Start(*capture_format_)) {
      LOG(LS_ERROR) << "Camera failed to start when unpausing.";
      return false;
    }
  }
  return true;
}

bool VideoCapturer::Restart(const VideoFormat& capture_format) {
  if (!IsRunning()) {
    return StartCapturing(capture_format);
  }

  if (GetCaptureFormat() != NULL && *GetCaptureFormat() == capture_format) {
    // The reqested format is the same; nothing to do.
    return true;
  }

  Stop();
  return StartCapturing(capture_format);
}

bool VideoCapturer::MuteToBlackThenPause(bool muted) {
  if (muted == IsMuted()) {
    return true;
  }

  LOG(LS_INFO) << (muted ? "Muting" : "Unmuting") << " this video capturer.";
  muted_ = muted;  // Do this before calling Pause().
  if (muted) {
    // Reset black frame count down.
    black_frame_count_down_ = kNumBlackFramesOnMute;
    // Following frames will be overritten with black, then the camera will be
    // paused.
    return true;
  }
  // Start the camera.
  thread_->Clear(this, MSG_DO_PAUSE);
  return Pause(false);
}

// Note that the last caller decides whether rotation should be applied if there
// are multiple send streams using the same camera.
bool VideoCapturer::SetApplyRotation(bool enable) {
  apply_rotation_ = enable;
  if (frame_factory_) {
    frame_factory_->SetApplyRotation(apply_rotation_);
  }
  return true;
}

void VideoCapturer::SetSupportedFormats(
    const std::vector<VideoFormat>& formats) {
  supported_formats_ = formats;
  UpdateFilteredSupportedFormats();
}

bool VideoCapturer::GetBestCaptureFormat(const VideoFormat& format,
                                         VideoFormat* best_format) {
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

void VideoCapturer::GetStats(VariableInfo<int>* adapt_drops_stats,
                             VariableInfo<int>* effect_drops_stats,
                             VariableInfo<double>* frame_time_stats,
                             VideoFormat* last_captured_frame_format) {
  rtc::CritScope cs(&frame_stats_crit_);
  GetVariableSnapshot(adapt_frame_drops_data_, adapt_drops_stats);
  GetVariableSnapshot(frame_time_data_, frame_time_stats);
  *last_captured_frame_format = last_captured_frame_format_;

  adapt_frame_drops_data_.Reset();
  frame_time_data_.Reset();
}

void VideoCapturer::OnFrameCaptured(VideoCapturer*,
                                    const CapturedFrame* captured_frame) {
  if (muted_) {
    if (black_frame_count_down_ == 0) {
      thread_->Post(this, MSG_DO_PAUSE, NULL);
    } else {
      --black_frame_count_down_;
    }
  }

  if (SignalVideoFrame.is_empty()) {
    return;
  }

  // Use a temporary buffer to scale
  rtc::scoped_ptr<uint8_t[]> scale_buffer;

  if (IsScreencast()) {
    int scaled_width, scaled_height;
    if (screencast_max_pixels_ > 0) {
      ComputeScaleMaxPixels(captured_frame->width, captured_frame->height,
          screencast_max_pixels_, &scaled_width, &scaled_height);
    } else {
      int desired_screencast_fps = capture_format_.get() ?
          VideoFormat::IntervalToFps(capture_format_->interval) :
          kDefaultScreencastFps;
      ComputeScale(captured_frame->width, captured_frame->height,
                   desired_screencast_fps, &scaled_width, &scaled_height);
    }

    if (FOURCC_ARGB == captured_frame->fourcc &&
        (scaled_width != captured_frame->width ||
        scaled_height != captured_frame->height)) {
      if (scaled_width != scaled_width_ || scaled_height != scaled_height_) {
        LOG(LS_INFO) << "Scaling Screencast from "
                     << captured_frame->width << "x"
                     << captured_frame->height << " to "
                     << scaled_width << "x" << scaled_height;
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
  // TODO(fbarchard): Avoid scale and convert if muted.
  // Temporary buffer is scoped here so it will persist until i420_frame.Init()
  // makes a copy of the frame, converting to I420.
  rtc::scoped_ptr<uint8_t[]> temp_buffer;
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
      ++adapt_frame_drops_;
      return;
    }
    adapted_width = adapted_format.width;
    adapted_height = adapted_format.height;
  }

  if (!frame_factory_) {
    LOG(LS_ERROR) << "No video frame factory.";
    return;
  }

  rtc::scoped_ptr<VideoFrame> adapted_frame(
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

  if (muted_) {
    // TODO(pthatcher): Use frame_factory_->CreateBlackFrame() instead.
    adapted_frame->SetToBlack();
  }
  SignalVideoFrame(this, adapted_frame.get());

  UpdateStats(captured_frame);
}

void VideoCapturer::SetCaptureState(CaptureState state) {
  if (state == capture_state_) {
    // Don't trigger a state changed callback if the state hasn't changed.
    return;
  }
  StateChangeParams* state_params = new StateChangeParams(state);
  capture_state_ = state;
  thread_->Post(this, MSG_STATE_CHANGE, state_params);
}

void VideoCapturer::OnMessage(rtc::Message* message) {
  switch (message->message_id) {
    case MSG_STATE_CHANGE: {
      rtc::scoped_ptr<StateChangeParams> p(
          static_cast<StateChangeParams*>(message->pdata));
      SignalStateChange(this, p->data());
      break;
    }
    case MSG_DO_PAUSE: {
      Pause(true);
      break;
    }
    case MSG_DO_UNPAUSE: {
      Pause(false);
      break;
    }
    default: {
      ASSERT(false);
    }
  }
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
#ifdef LINUX
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
  if (!enable_camera_list_) {
    return false;
  }
  return format.width > max_format_->width ||
         format.height > max_format_->height;
}

void VideoCapturer::UpdateStats(const CapturedFrame* captured_frame) {
  // Update stats protected from fetches from different thread.
  rtc::CritScope cs(&frame_stats_crit_);

  last_captured_frame_format_.width = captured_frame->width;
  last_captured_frame_format_.height = captured_frame->height;
  // TODO(ronghuawu): Useful to report interval as well?
  last_captured_frame_format_.interval = 0;
  last_captured_frame_format_.fourcc = captured_frame->fourcc;

  double time_now = frame_length_time_reporter_.TimerNow();
  if (previous_frame_time_ != 0.0) {
    adapt_frame_drops_data_.AddSample(adapt_frame_drops_);
    frame_time_data_.AddSample(time_now - previous_frame_time_);
  }
  previous_frame_time_ = time_now;
  adapt_frame_drops_ = 0;
}

template<class T>
void VideoCapturer::GetVariableSnapshot(
    const rtc::RollingAccumulator<T>& data,
    VariableInfo<T>* stats) {
  stats->max_val = data.ComputeMax();
  stats->mean = data.ComputeMean();
  stats->min_val = data.ComputeMin();
  stats->variance = data.ComputeVariance();
}

}  // namespace cricket
