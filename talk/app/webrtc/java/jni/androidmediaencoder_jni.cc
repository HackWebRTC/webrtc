/*
 * libjingle
 * Copyright 2015 Google Inc.
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
 *
 */

#include "talk/app/webrtc/java/jni/androidmediaencoder_jni.h"
#include "talk/app/webrtc/java/jni/classreferenceholder.h"
#include "talk/app/webrtc/java/jni/androidmediacodeccommon.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/modules/rtp_rtcp/source/h264_bitstream_parser.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/utility/include/quality_scaler.h"
#include "webrtc/modules/video_coding/utility/include/vp8_header_parser.h"
#include "webrtc/system_wrappers/include/field_trial.h"
#include "webrtc/system_wrappers/include/logcat_trace_context.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/video_common.h"

using rtc::Bind;
using rtc::Thread;
using rtc::ThreadManager;
using rtc::scoped_ptr;

using webrtc::CodecSpecificInfo;
using webrtc::EncodedImage;
using webrtc::VideoFrame;
using webrtc::RTPFragmentationHeader;
using webrtc::VideoCodec;
using webrtc::VideoCodecType;
using webrtc::kVideoCodecH264;
using webrtc::kVideoCodecVP8;

namespace webrtc_jni {

// H.264 start code length.
#define H264_SC_LENGTH 4
// Maximum allowed NALUs in one output frame.
#define MAX_NALUS_PERFRAME 32
// Maximum supported HW video encoder resolution.
#define MAX_VIDEO_WIDTH 1280
#define MAX_VIDEO_HEIGHT 1280
// Maximum supported HW video encoder fps.
#define MAX_VIDEO_FPS 30

// MediaCodecVideoEncoder is a webrtc::VideoEncoder implementation that uses
// Android's MediaCodec SDK API behind the scenes to implement (hopefully)
// HW-backed video encode.  This C++ class is implemented as a very thin shim,
// delegating all of the interesting work to org.webrtc.MediaCodecVideoEncoder.
// MediaCodecVideoEncoder is created, operated, and destroyed on a single
// thread, currently the libjingle Worker thread.
class MediaCodecVideoEncoder : public webrtc::VideoEncoder,
                               public rtc::MessageHandler {
 public:
  virtual ~MediaCodecVideoEncoder();
  explicit MediaCodecVideoEncoder(JNIEnv* jni, VideoCodecType codecType);

  // webrtc::VideoEncoder implementation.  Everything trampolines to
  // |codec_thread_| for execution.
  int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                     int32_t /* number_of_cores */,
                     size_t /* max_payload_size */) override;
  int32_t Encode(const webrtc::VideoFrame& input_image,
                 const webrtc::CodecSpecificInfo* /* codec_specific_info */,
                 const std::vector<webrtc::FrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t SetChannelParameters(uint32_t /* packet_loss */,
                               int64_t /* rtt */) override;
  int32_t SetRates(uint32_t new_bit_rate, uint32_t frame_rate) override;

  // rtc::MessageHandler implementation.
  void OnMessage(rtc::Message* msg) override;

  void OnDroppedFrame() override;

  int GetTargetFramerate() override;

 private:
  // CHECK-fail if not running on |codec_thread_|.
  void CheckOnCodecThread();

  // Release() and InitEncode() in an attempt to restore the codec to an
  // operable state.  Necessary after all manner of OMX-layer errors.
  void ResetCodec();

  // Implementation of webrtc::VideoEncoder methods above, all running on the
  // codec thread exclusively.
  //
  // If width==0 then this is assumed to be a re-initialization and the
  // previously-current values are reused instead of the passed parameters
  // (makes it easier to reason about thread-safety).
  int32_t InitEncodeOnCodecThread(int width, int height, int kbps, int fps);
  int32_t EncodeOnCodecThread(
      const webrtc::VideoFrame& input_image,
      const std::vector<webrtc::FrameType>* frame_types);
  int32_t RegisterEncodeCompleteCallbackOnCodecThread(
      webrtc::EncodedImageCallback* callback);
  int32_t ReleaseOnCodecThread();
  int32_t SetRatesOnCodecThread(uint32_t new_bit_rate, uint32_t frame_rate);

  // Helper accessors for MediaCodecVideoEncoder$OutputBufferInfo members.
  int GetOutputBufferInfoIndex(JNIEnv* jni, jobject j_output_buffer_info);
  jobject GetOutputBufferInfoBuffer(JNIEnv* jni, jobject j_output_buffer_info);
  bool GetOutputBufferInfoIsKeyFrame(JNIEnv* jni, jobject j_output_buffer_info);
  jlong GetOutputBufferInfoPresentationTimestampUs(
      JNIEnv* jni, jobject j_output_buffer_info);

  // Deliver any outputs pending in the MediaCodec to our |callback_| and return
  // true on success.
  bool DeliverPendingOutputs(JNIEnv* jni);

  // Search for H.264 start codes.
  int32_t NextNaluPosition(uint8_t *buffer, size_t buffer_size);

  // Type of video codec.
  VideoCodecType codecType_;

  // Valid all the time since RegisterEncodeCompleteCallback() Invoke()s to
  // |codec_thread_| synchronously.
  webrtc::EncodedImageCallback* callback_;

  // State that is constant for the lifetime of this object once the ctor
  // returns.
  scoped_ptr<Thread> codec_thread_;  // Thread on which to operate MediaCodec.
  ScopedGlobalRef<jclass> j_media_codec_video_encoder_class_;
  ScopedGlobalRef<jobject> j_media_codec_video_encoder_;
  jmethodID j_init_encode_method_;
  jmethodID j_dequeue_input_buffer_method_;
  jmethodID j_encode_method_;
  jmethodID j_release_method_;
  jmethodID j_set_rates_method_;
  jmethodID j_dequeue_output_buffer_method_;
  jmethodID j_release_output_buffer_method_;
  jfieldID j_color_format_field_;
  jfieldID j_info_index_field_;
  jfieldID j_info_buffer_field_;
  jfieldID j_info_is_key_frame_field_;
  jfieldID j_info_presentation_timestamp_us_field_;

  // State that is valid only between InitEncode() and the next Release().
  // Touched only on codec_thread_ so no explicit synchronization necessary.
  int width_;   // Frame width in pixels.
  int height_;  // Frame height in pixels.
  bool inited_;
  uint16_t picture_id_;
  enum libyuv::FourCC encoder_fourcc_;  // Encoder color space format.
  int last_set_bitrate_kbps_;  // Last-requested bitrate in kbps.
  int last_set_fps_;  // Last-requested frame rate.
  int64_t current_timestamp_us_;  // Current frame timestamps in us.
  int frames_received_;  // Number of frames received by encoder.
  int frames_encoded_;  // Number of frames encoded by encoder.
  int frames_dropped_;  // Number of frames dropped by encoder.
  int frames_in_queue_;  // Number of frames in encoder queue.
  int64_t start_time_ms_;  // Start time for statistics.
  int current_frames_;  // Number of frames in the current statistics interval.
  int current_bytes_;  // Encoded bytes in the current statistics interval.
  int current_encoding_time_ms_;  // Overall encoding time in the current second
  int64_t last_input_timestamp_ms_;  // Timestamp of last received yuv frame.
  int64_t last_output_timestamp_ms_;  // Timestamp of last encoded frame.
  std::vector<int32_t> timestamps_;  // Video frames timestamp queue.
  std::vector<int64_t> render_times_ms_;  // Video frames render time queue.
  std::vector<int64_t> frame_rtc_times_ms_;  // Time when video frame is sent to
                                             // encoder input.
  int32_t output_timestamp_;  // Last output frame timestamp from timestamps_ Q.
  int64_t output_render_time_ms_; // Last output frame render time from
                                  // render_times_ms_ queue.
  // Frame size in bytes fed to MediaCodec.
  int yuv_size_;
  // True only when between a callback_->Encoded() call return a positive value
  // and the next Encode() call being ignored.
  bool drop_next_input_frame_;
  // Global references; must be deleted in Release().
  std::vector<jobject> input_buffers_;
  webrtc::QualityScaler quality_scaler_;
  // Dynamic resolution change, off by default.
  bool scale_;

  // H264 bitstream parser, used to extract QP from encoded bitstreams.
  webrtc::H264BitstreamParser h264_bitstream_parser_;
};

MediaCodecVideoEncoder::~MediaCodecVideoEncoder() {
  // Call Release() to ensure no more callbacks to us after we are deleted.
  Release();
}

MediaCodecVideoEncoder::MediaCodecVideoEncoder(
    JNIEnv* jni, VideoCodecType codecType) :
    codecType_(codecType),
    callback_(NULL),
    inited_(false),
    picture_id_(0),
    codec_thread_(new Thread()),
    j_media_codec_video_encoder_class_(
        jni,
        FindClass(jni, "org/webrtc/MediaCodecVideoEncoder")),
    j_media_codec_video_encoder_(
        jni,
        jni->NewObject(*j_media_codec_video_encoder_class_,
                       GetMethodID(jni,
                                   *j_media_codec_video_encoder_class_,
                                   "<init>",
                                   "()V"))) {
  ScopedLocalRefFrame local_ref_frame(jni);
  // It would be nice to avoid spinning up a new thread per MediaCodec, and
  // instead re-use e.g. the PeerConnectionFactory's |worker_thread_|, but bug
  // 2732 means that deadlocks abound.  This class synchronously trampolines
  // to |codec_thread_|, so if anything else can be coming to _us_ from
  // |codec_thread_|, or from any thread holding the |_sendCritSect| described
  // in the bug, we have a problem.  For now work around that with a dedicated
  // thread.
  codec_thread_->SetName("MediaCodecVideoEncoder", NULL);
  RTC_CHECK(codec_thread_->Start()) << "Failed to start MediaCodecVideoEncoder";

  jclass j_output_buffer_info_class =
      FindClass(jni, "org/webrtc/MediaCodecVideoEncoder$OutputBufferInfo");
  j_init_encode_method_ = GetMethodID(
      jni,
      *j_media_codec_video_encoder_class_,
      "initEncode",
      "(Lorg/webrtc/MediaCodecVideoEncoder$VideoCodecType;IIII)"
      "[Ljava/nio/ByteBuffer;");
  j_dequeue_input_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "dequeueInputBuffer", "()I");
  j_encode_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "encode", "(ZIIJ)Z");
  j_release_method_ =
      GetMethodID(jni, *j_media_codec_video_encoder_class_, "release", "()V");
  j_set_rates_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "setRates", "(II)Z");
  j_dequeue_output_buffer_method_ = GetMethodID(
      jni,
      *j_media_codec_video_encoder_class_,
      "dequeueOutputBuffer",
      "()Lorg/webrtc/MediaCodecVideoEncoder$OutputBufferInfo;");
  j_release_output_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "releaseOutputBuffer", "(I)Z");

  j_color_format_field_ =
      GetFieldID(jni, *j_media_codec_video_encoder_class_, "colorFormat", "I");
  j_info_index_field_ =
      GetFieldID(jni, j_output_buffer_info_class, "index", "I");
  j_info_buffer_field_ = GetFieldID(
      jni, j_output_buffer_info_class, "buffer", "Ljava/nio/ByteBuffer;");
  j_info_is_key_frame_field_ =
      GetFieldID(jni, j_output_buffer_info_class, "isKeyFrame", "Z");
  j_info_presentation_timestamp_us_field_ = GetFieldID(
      jni, j_output_buffer_info_class, "presentationTimestampUs", "J");
  CHECK_EXCEPTION(jni) << "MediaCodecVideoEncoder ctor failed";
  AllowBlockingCalls();
}

int32_t MediaCodecVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    int32_t /* number_of_cores */,
    size_t /* max_payload_size */) {
  const int kMinWidth = 320;
  const int kMinHeight = 180;
  const int kLowQpThresholdDenominator = 3;
  if (codec_settings == NULL) {
    ALOGE << "NULL VideoCodec instance";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Factory should guard against other codecs being used with us.
  RTC_CHECK(codec_settings->codecType == codecType_)
      << "Unsupported codec " << codec_settings->codecType << " for "
      << codecType_;

  ALOGD << "InitEncode request";
  scale_ = webrtc::field_trial::FindFullName(
      "WebRTC-MediaCodecVideoEncoder-AutomaticResize") == "Enabled";
  ALOGD << "Encoder automatic resize " << (scale_ ? "enabled" : "disabled");
  if (scale_) {
    if (codecType_ == kVideoCodecVP8) {
      // QP is obtained from VP8-bitstream for HW, so the QP corresponds to the
      // (internal) range: [0, 127]. And we cannot change QP_max in HW, so it is
      // always = 127. Note that in SW, QP is that of the user-level range [0,
      // 63].
      const int kMaxQp = 127;
      // TODO(pbos): Investigate whether high-QP thresholds make sense for VP8.
      // This effectively disables high QP as VP8 QP can't go above this
      // threshold.
      const int kDisabledBadQpThreshold = kMaxQp + 1;
      quality_scaler_.Init(kMaxQp / kLowQpThresholdDenominator,
                           kDisabledBadQpThreshold, true);
    } else if (codecType_ == kVideoCodecH264) {
      // H264 QP is in the range [0, 51].
      const int kMaxQp = 51;
      const int kBadQpThreshold = 40;
      quality_scaler_.Init(kMaxQp / kLowQpThresholdDenominator, kBadQpThreshold,
                           false);
    } else {
      // When adding codec support to additional hardware codecs, also configure
      // their QP thresholds for scaling.
      RTC_NOTREACHED() << "Unsupported codec without configured QP thresholds.";
    }
    quality_scaler_.SetMinResolution(kMinWidth, kMinHeight);
    quality_scaler_.ReportFramerate(codec_settings->maxFramerate);
  }
  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoEncoder::InitEncodeOnCodecThread,
           this,
           codec_settings->width,
           codec_settings->height,
           codec_settings->startBitrate,
           codec_settings->maxFramerate));
}

int32_t MediaCodecVideoEncoder::Encode(
    const webrtc::VideoFrame& frame,
    const webrtc::CodecSpecificInfo* /* codec_specific_info */,
    const std::vector<webrtc::FrameType>* frame_types) {
  return codec_thread_->Invoke<int32_t>(Bind(
      &MediaCodecVideoEncoder::EncodeOnCodecThread, this, frame, frame_types));
}

int32_t MediaCodecVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoEncoder::RegisterEncodeCompleteCallbackOnCodecThread,
           this,
           callback));
}

int32_t MediaCodecVideoEncoder::Release() {
  ALOGD << "EncoderRelease request";
  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoEncoder::ReleaseOnCodecThread, this));
}

int32_t MediaCodecVideoEncoder::SetChannelParameters(uint32_t /* packet_loss */,
                                                     int64_t /* rtt */) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::SetRates(uint32_t new_bit_rate,
                                         uint32_t frame_rate) {
  if (scale_)
    quality_scaler_.ReportFramerate(frame_rate);

  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoEncoder::SetRatesOnCodecThread,
           this,
           new_bit_rate,
           frame_rate));
}

void MediaCodecVideoEncoder::OnMessage(rtc::Message* msg) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  // We only ever send one message to |this| directly (not through a Bind()'d
  // functor), so expect no ID/data.
  RTC_CHECK(!msg->message_id) << "Unexpected message!";
  RTC_CHECK(!msg->pdata) << "Unexpected message!";
  CheckOnCodecThread();
  if (!inited_) {
    return;
  }

  // It would be nice to recover from a failure here if one happened, but it's
  // unclear how to signal such a failure to the app, so instead we stay silent
  // about it and let the next app-called API method reveal the borkedness.
  DeliverPendingOutputs(jni);
  codec_thread_->PostDelayed(kMediaCodecPollMs, this);
}

void MediaCodecVideoEncoder::CheckOnCodecThread() {
  RTC_CHECK(codec_thread_ == ThreadManager::Instance()->CurrentThread())
      << "Running on wrong thread!";
}

void MediaCodecVideoEncoder::ResetCodec() {
  ALOGE << "ResetCodec";
  if (Release() != WEBRTC_VIDEO_CODEC_OK ||
      codec_thread_->Invoke<int32_t>(Bind(
          &MediaCodecVideoEncoder::InitEncodeOnCodecThread, this,
          width_, height_, 0, 0)) != WEBRTC_VIDEO_CODEC_OK) {
    // TODO(fischman): wouldn't it be nice if there was a way to gracefully
    // degrade to a SW encoder at this point?  There isn't one AFAICT :(
    // https://code.google.com/p/webrtc/issues/detail?id=2920
  }
}

int32_t MediaCodecVideoEncoder::InitEncodeOnCodecThread(
    int width, int height, int kbps, int fps) {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  ALOGD << "InitEncodeOnCodecThread Type: " <<  (int)codecType_ << ", " <<
      width << " x " << height << ". Bitrate: " << kbps <<
      " kbps. Fps: " << fps;
  if (kbps == 0) {
    kbps = last_set_bitrate_kbps_;
  }
  if (fps == 0) {
    fps = last_set_fps_;
  }

  width_ = width;
  height_ = height;
  last_set_bitrate_kbps_ = kbps;
  last_set_fps_ = fps;
  yuv_size_ = width_ * height_ * 3 / 2;
  frames_received_ = 0;
  frames_encoded_ = 0;
  frames_dropped_ = 0;
  frames_in_queue_ = 0;
  current_timestamp_us_ = 0;
  start_time_ms_ = GetCurrentTimeMs();
  current_frames_ = 0;
  current_bytes_ = 0;
  current_encoding_time_ms_ = 0;
  last_input_timestamp_ms_ = -1;
  last_output_timestamp_ms_ = -1;
  output_timestamp_ = 0;
  output_render_time_ms_ = 0;
  timestamps_.clear();
  render_times_ms_.clear();
  frame_rtc_times_ms_.clear();
  drop_next_input_frame_ = false;
  picture_id_ = static_cast<uint16_t>(rand()) & 0x7FFF;
  // We enforce no extra stride/padding in the format creation step.
  jobject j_video_codec_enum = JavaEnumFromIndex(
      jni, "MediaCodecVideoEncoder$VideoCodecType", codecType_);
  jobjectArray input_buffers = reinterpret_cast<jobjectArray>(
      jni->CallObjectMethod(*j_media_codec_video_encoder_,
                            j_init_encode_method_,
                            j_video_codec_enum,
                            width_,
                            height_,
                            kbps,
                            fps));
  CHECK_EXCEPTION(jni);
  if (IsNull(jni, input_buffers)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  inited_ = true;
  switch (GetIntField(jni, *j_media_codec_video_encoder_,
      j_color_format_field_)) {
    case COLOR_FormatYUV420Planar:
      encoder_fourcc_ = libyuv::FOURCC_YU12;
      break;
    case COLOR_FormatYUV420SemiPlanar:
    case COLOR_QCOM_FormatYUV420SemiPlanar:
    case COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m:
      encoder_fourcc_ = libyuv::FOURCC_NV12;
      break;
    default:
      LOG(LS_ERROR) << "Wrong color format.";
      return WEBRTC_VIDEO_CODEC_ERROR;
  }
  size_t num_input_buffers = jni->GetArrayLength(input_buffers);
  RTC_CHECK(input_buffers_.empty())
      << "Unexpected double InitEncode without Release";
  input_buffers_.resize(num_input_buffers);
  for (size_t i = 0; i < num_input_buffers; ++i) {
    input_buffers_[i] =
        jni->NewGlobalRef(jni->GetObjectArrayElement(input_buffers, i));
    int64_t yuv_buffer_capacity =
        jni->GetDirectBufferCapacity(input_buffers_[i]);
    CHECK_EXCEPTION(jni);
    RTC_CHECK(yuv_buffer_capacity >= yuv_size_) << "Insufficient capacity";
  }
  CHECK_EXCEPTION(jni);

  codec_thread_->PostDelayed(kMediaCodecPollMs, this);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::EncodeOnCodecThread(
    const webrtc::VideoFrame& frame,
    const std::vector<webrtc::FrameType>* frame_types) {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  frames_received_++;
  if (!DeliverPendingOutputs(jni)) {
    ResetCodec();
    // Continue as if everything's fine.
  }

  if (drop_next_input_frame_) {
    ALOGV("Encoder drop frame - failed callback.");
    drop_next_input_frame_ = false;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  RTC_CHECK(frame_types->size() == 1) << "Unexpected stream count";
  // Check framerate before spatial resolution change.
  if (scale_)
    quality_scaler_.OnEncodeFrame(frame);

  const VideoFrame& input_frame =
      scale_ ? quality_scaler_.GetScaledFrame(frame) : frame;

  if (input_frame.width() != width_ || input_frame.height() != height_) {
    ALOGD << "Frame resolution change from " << width_ << " x " << height_ <<
        " to " << input_frame.width() << " x " << input_frame.height();
    width_ = input_frame.width();
    height_ = input_frame.height();
    ResetCodec();
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // Check if we accumulated too many frames in encoder input buffers
  // or the encoder latency exceeds 70 ms and drop frame if so.
  if (frames_in_queue_ > 0 && last_input_timestamp_ms_ >= 0) {
    int encoder_latency_ms = last_input_timestamp_ms_ -
        last_output_timestamp_ms_;
    if (frames_in_queue_ > 2 || encoder_latency_ms > 70) {
      ALOGD << "Drop frame - encoder is behind by " << encoder_latency_ms <<
          " ms. Q size: " << frames_in_queue_;
      frames_dropped_++;
      // Report dropped frame to quality_scaler_.
      OnDroppedFrame();
      return WEBRTC_VIDEO_CODEC_OK;
    }
  }

  int j_input_buffer_index = jni->CallIntMethod(*j_media_codec_video_encoder_,
                                                j_dequeue_input_buffer_method_);
  CHECK_EXCEPTION(jni);
  if (j_input_buffer_index == -1) {
    // Video codec falls behind - no input buffer available.
    ALOGV("Encoder drop frame - no input buffers available");
    frames_dropped_++;
    // Report dropped frame to quality_scaler_.
    OnDroppedFrame();
    return WEBRTC_VIDEO_CODEC_OK;  // TODO(fischman): see webrtc bug 2887.
  }
  if (j_input_buffer_index == -2) {
    ResetCodec();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ALOGV("Encoder frame in # %d. TS: %lld. Q: %d",
      frames_received_ - 1, current_timestamp_us_ / 1000, frames_in_queue_);

  jobject j_input_buffer = input_buffers_[j_input_buffer_index];
  uint8_t* yuv_buffer =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_input_buffer));
  CHECK_EXCEPTION(jni);
  RTC_CHECK(yuv_buffer) << "Indirect buffer??";
  RTC_CHECK(!libyuv::ConvertFromI420(
      input_frame.buffer(webrtc::kYPlane), input_frame.stride(webrtc::kYPlane),
      input_frame.buffer(webrtc::kUPlane), input_frame.stride(webrtc::kUPlane),
      input_frame.buffer(webrtc::kVPlane), input_frame.stride(webrtc::kVPlane),
      yuv_buffer, width_, width_, height_, encoder_fourcc_))
      << "ConvertFromI420 failed";
  last_input_timestamp_ms_ = current_timestamp_us_ / 1000;
  frames_in_queue_++;

  // Save input image timestamps for later output
  timestamps_.push_back(input_frame.timestamp());
  render_times_ms_.push_back(input_frame.render_time_ms());
  frame_rtc_times_ms_.push_back(GetCurrentTimeMs());

  bool key_frame = frame_types->front() != webrtc::kVideoFrameDelta;
  bool encode_status = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                              j_encode_method_,
                                              key_frame,
                                              j_input_buffer_index,
                                              yuv_size_,
                                              current_timestamp_us_);
  CHECK_EXCEPTION(jni);
  current_timestamp_us_ += 1000000 / last_set_fps_;

  if (!encode_status || !DeliverPendingOutputs(jni)) {
    ResetCodec();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::RegisterEncodeCompleteCallbackOnCodecThread(
    webrtc::EncodedImageCallback* callback) {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::ReleaseOnCodecThread() {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ALOGD << "EncoderReleaseOnCodecThread: Frames received: " <<
      frames_received_ << ". Encoded: " << frames_encoded_ <<
      ". Dropped: " << frames_dropped_;
  ScopedLocalRefFrame local_ref_frame(jni);
  for (size_t i = 0; i < input_buffers_.size(); ++i)
    jni->DeleteGlobalRef(input_buffers_[i]);
  input_buffers_.clear();
  jni->CallVoidMethod(*j_media_codec_video_encoder_, j_release_method_);
  CHECK_EXCEPTION(jni);
  rtc::MessageQueueManager::Clear(this);
  inited_ = false;
  ALOGD << "EncoderReleaseOnCodecThread done.";
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::SetRatesOnCodecThread(uint32_t new_bit_rate,
                                                      uint32_t frame_rate) {
  CheckOnCodecThread();
  if (last_set_bitrate_kbps_ == new_bit_rate &&
      last_set_fps_ == frame_rate) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  if (new_bit_rate > 0) {
    last_set_bitrate_kbps_ = new_bit_rate;
  }
  if (frame_rate > 0) {
    last_set_fps_ = frame_rate;
  }
  bool ret = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                       j_set_rates_method_,
                                       last_set_bitrate_kbps_,
                                       last_set_fps_);
  CHECK_EXCEPTION(jni);
  if (!ret) {
    ResetCodec();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int MediaCodecVideoEncoder::GetOutputBufferInfoIndex(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetIntField(jni, j_output_buffer_info, j_info_index_field_);
}

jobject MediaCodecVideoEncoder::GetOutputBufferInfoBuffer(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetObjectField(jni, j_output_buffer_info, j_info_buffer_field_);
}

bool MediaCodecVideoEncoder::GetOutputBufferInfoIsKeyFrame(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetBooleanField(jni, j_output_buffer_info, j_info_is_key_frame_field_);
}

jlong MediaCodecVideoEncoder::GetOutputBufferInfoPresentationTimestampUs(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetLongField(
      jni, j_output_buffer_info, j_info_presentation_timestamp_us_field_);
}

bool MediaCodecVideoEncoder::DeliverPendingOutputs(JNIEnv* jni) {
  while (true) {
    jobject j_output_buffer_info = jni->CallObjectMethod(
        *j_media_codec_video_encoder_, j_dequeue_output_buffer_method_);
    CHECK_EXCEPTION(jni);
    if (IsNull(jni, j_output_buffer_info)) {
      break;
    }

    int output_buffer_index =
        GetOutputBufferInfoIndex(jni, j_output_buffer_info);
    if (output_buffer_index == -1) {
      ResetCodec();
      return false;
    }

    // Get key and config frame flags.
    jobject j_output_buffer =
        GetOutputBufferInfoBuffer(jni, j_output_buffer_info);
    bool key_frame = GetOutputBufferInfoIsKeyFrame(jni, j_output_buffer_info);

    // Get frame timestamps from a queue - for non config frames only.
    int64_t frame_encoding_time_ms = 0;
    last_output_timestamp_ms_ =
        GetOutputBufferInfoPresentationTimestampUs(jni, j_output_buffer_info) /
        1000;
    if (frames_in_queue_ > 0) {
      output_timestamp_ = timestamps_.front();
      timestamps_.erase(timestamps_.begin());
      output_render_time_ms_ = render_times_ms_.front();
      render_times_ms_.erase(render_times_ms_.begin());
      frame_encoding_time_ms = GetCurrentTimeMs() - frame_rtc_times_ms_.front();
      frame_rtc_times_ms_.erase(frame_rtc_times_ms_.begin());
      frames_in_queue_--;
    }

    // Extract payload.
    size_t payload_size = jni->GetDirectBufferCapacity(j_output_buffer);
    uint8_t* payload = reinterpret_cast<uint8_t*>(
        jni->GetDirectBufferAddress(j_output_buffer));
    CHECK_EXCEPTION(jni);

    ALOGV("Encoder frame out # %d. Key: %d. Size: %d. TS: %lld."
        " Latency: %lld. EncTime: %lld",
        frames_encoded_, key_frame, payload_size,
        last_output_timestamp_ms_,
        last_input_timestamp_ms_ - last_output_timestamp_ms_,
        frame_encoding_time_ms);

    // Calculate and print encoding statistics - every 3 seconds.
    frames_encoded_++;
    current_frames_++;
    current_bytes_ += payload_size;
    current_encoding_time_ms_ += frame_encoding_time_ms;
    int statistic_time_ms = GetCurrentTimeMs() - start_time_ms_;
    if (statistic_time_ms >= kMediaCodecStatisticsIntervalMs &&
        current_frames_ > 0) {
      ALOGD << "Encoded frames: " << frames_encoded_ << ". Bitrate: " <<
          (current_bytes_ * 8 / statistic_time_ms) <<
          ", target: " << last_set_bitrate_kbps_ << " kbps, fps: " <<
          ((current_frames_ * 1000 + statistic_time_ms / 2) / statistic_time_ms)
          << ", encTime: " <<
          (current_encoding_time_ms_ / current_frames_) << " for last " <<
          statistic_time_ms << " ms.";
      start_time_ms_ = GetCurrentTimeMs();
      current_frames_ = 0;
      current_bytes_ = 0;
      current_encoding_time_ms_ = 0;
    }

    // Callback - return encoded frame.
    int32_t callback_status = 0;
    if (callback_) {
      scoped_ptr<webrtc::EncodedImage> image(
          new webrtc::EncodedImage(payload, payload_size, payload_size));
      image->_encodedWidth = width_;
      image->_encodedHeight = height_;
      image->_timeStamp = output_timestamp_;
      image->capture_time_ms_ = output_render_time_ms_;
      image->_frameType =
          (key_frame ? webrtc::kVideoFrameKey : webrtc::kVideoFrameDelta);
      image->_completeFrame = true;
      image->adapt_reason_.quality_resolution_downscales =
          scale_ ? quality_scaler_.downscale_shift() : -1;

      webrtc::CodecSpecificInfo info;
      memset(&info, 0, sizeof(info));
      info.codecType = codecType_;
      if (codecType_ == kVideoCodecVP8) {
        info.codecSpecific.VP8.pictureId = picture_id_;
        info.codecSpecific.VP8.nonReference = false;
        info.codecSpecific.VP8.simulcastIdx = 0;
        info.codecSpecific.VP8.temporalIdx = webrtc::kNoTemporalIdx;
        info.codecSpecific.VP8.layerSync = false;
        info.codecSpecific.VP8.tl0PicIdx = webrtc::kNoTl0PicIdx;
        info.codecSpecific.VP8.keyIdx = webrtc::kNoKeyIdx;
        picture_id_ = (picture_id_ + 1) & 0x7FFF;
      }

      // Generate a header describing a single fragment.
      webrtc::RTPFragmentationHeader header;
      memset(&header, 0, sizeof(header));
      if (codecType_ == kVideoCodecVP8) {
        header.VerifyAndAllocateFragmentationHeader(1);
        header.fragmentationOffset[0] = 0;
        header.fragmentationLength[0] = image->_length;
        header.fragmentationPlType[0] = 0;
        header.fragmentationTimeDiff[0] = 0;
        if (scale_) {
          int qp;
          if (webrtc::vp8::GetQp(payload, payload_size, &qp))
            quality_scaler_.ReportQP(qp);
        }
      } else if (codecType_ == kVideoCodecH264) {
        if (scale_) {
          h264_bitstream_parser_.ParseBitstream(payload, payload_size);
          int qp;
          if (h264_bitstream_parser_.GetLastSliceQp(&qp))
            quality_scaler_.ReportQP(qp);
        }
        // For H.264 search for start codes.
        int32_t scPositions[MAX_NALUS_PERFRAME + 1] = {};
        int32_t scPositionsLength = 0;
        int32_t scPosition = 0;
        while (scPositionsLength < MAX_NALUS_PERFRAME) {
          int32_t naluPosition = NextNaluPosition(
              payload + scPosition, payload_size - scPosition);
          if (naluPosition < 0) {
            break;
          }
          scPosition += naluPosition;
          scPositions[scPositionsLength++] = scPosition;
          scPosition += H264_SC_LENGTH;
        }
        if (scPositionsLength == 0) {
          ALOGE << "Start code is not found!";
          ALOGE << "Data:" <<  image->_buffer[0] << " " << image->_buffer[1]
              << " " << image->_buffer[2] << " " << image->_buffer[3]
              << " " << image->_buffer[4] << " " << image->_buffer[5];
          ResetCodec();
          return false;
        }
        scPositions[scPositionsLength] = payload_size;
        header.VerifyAndAllocateFragmentationHeader(scPositionsLength);
        for (size_t i = 0; i < scPositionsLength; i++) {
          header.fragmentationOffset[i] = scPositions[i] + H264_SC_LENGTH;
          header.fragmentationLength[i] =
              scPositions[i + 1] - header.fragmentationOffset[i];
          header.fragmentationPlType[i] = 0;
          header.fragmentationTimeDiff[i] = 0;
        }
      }

      callback_status = callback_->Encoded(*image, &info, &header);
    }

    // Return output buffer back to the encoder.
    bool success = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                          j_release_output_buffer_method_,
                                          output_buffer_index);
    CHECK_EXCEPTION(jni);
    if (!success) {
      ResetCodec();
      return false;
    }

    if (callback_status > 0) {
      drop_next_input_frame_ = true;
      // Theoretically could handle callback_status<0 here, but unclear what
      // that would mean for us.
    }
  }

  return true;
}

int32_t MediaCodecVideoEncoder::NextNaluPosition(
    uint8_t *buffer, size_t buffer_size) {
  if (buffer_size < H264_SC_LENGTH) {
    return -1;
  }
  uint8_t *head = buffer;
  // Set end buffer pointer to 4 bytes before actual buffer end so we can
  // access head[1], head[2] and head[3] in a loop without buffer overrun.
  uint8_t *end = buffer + buffer_size - H264_SC_LENGTH;

  while (head < end) {
    if (head[0]) {
      head++;
      continue;
    }
    if (head[1]) { // got 00xx
      head += 2;
      continue;
    }
    if (head[2]) { // got 0000xx
      head += 3;
      continue;
    }
    if (head[3] != 0x01) { // got 000000xx
      head++; // xx != 1, continue searching.
      continue;
    }
    return (int32_t)(head - buffer);
  }
  return -1;
}

void MediaCodecVideoEncoder::OnDroppedFrame() {
  if (scale_)
    quality_scaler_.ReportDroppedFrame();
}

int MediaCodecVideoEncoder::GetTargetFramerate() {
  return scale_ ? quality_scaler_.GetTargetFramerate() : -1;
}

MediaCodecVideoEncoderFactory::MediaCodecVideoEncoderFactory() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jclass j_encoder_class = FindClass(jni, "org/webrtc/MediaCodecVideoEncoder");
  supported_codecs_.clear();

  bool is_vp8_hw_supported = jni->CallStaticBooleanMethod(
      j_encoder_class,
      GetStaticMethodID(jni, j_encoder_class, "isVp8HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_vp8_hw_supported) {
    ALOGD << "VP8 HW Encoder supported.";
    supported_codecs_.push_back(VideoCodec(kVideoCodecVP8, "VP8",
        MAX_VIDEO_WIDTH, MAX_VIDEO_HEIGHT, MAX_VIDEO_FPS));
  }

  bool is_h264_hw_supported = jni->CallStaticBooleanMethod(
      j_encoder_class,
      GetStaticMethodID(jni, j_encoder_class, "isH264HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_h264_hw_supported) {
    ALOGD << "H.264 HW Encoder supported.";
    supported_codecs_.push_back(VideoCodec(kVideoCodecH264, "H264",
        MAX_VIDEO_WIDTH, MAX_VIDEO_HEIGHT, MAX_VIDEO_FPS));
  }
}

MediaCodecVideoEncoderFactory::~MediaCodecVideoEncoderFactory() {}

webrtc::VideoEncoder* MediaCodecVideoEncoderFactory::CreateVideoEncoder(
    VideoCodecType type) {
  if (supported_codecs_.empty()) {
    return NULL;
  }
  for (std::vector<VideoCodec>::const_iterator it = supported_codecs_.begin();
         it != supported_codecs_.end(); ++it) {
    if (it->type == type) {
      ALOGD << "Create HW video encoder for type " << (int)type <<
          " (" << it->name << ").";
      return new MediaCodecVideoEncoder(AttachCurrentThreadIfNeeded(), type);
    }
  }
  return NULL;
}

const std::vector<MediaCodecVideoEncoderFactory::VideoCodec>&
MediaCodecVideoEncoderFactory::codecs() const {
  return supported_codecs_;
}

void MediaCodecVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  ALOGD << "Destroy video encoder.";
  delete encoder;
}

}  // namespace webrtc_jni

