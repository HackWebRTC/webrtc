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

#include <vector>

#include "talk/app/webrtc/java/jni/androidmediadecoder_jni.h"
#include "talk/app/webrtc/java/jni/androidmediacodeccommon.h"
#include "talk/app/webrtc/java/jni/classreferenceholder.h"
#include "talk/app/webrtc/java/jni/native_handle_impl.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/system_wrappers/interface/logcat_trace_context.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/video_common.h"

using rtc::Bind;
using rtc::Thread;
using rtc::ThreadManager;
using rtc::scoped_ptr;

using webrtc::CodecSpecificInfo;
using webrtc::DecodedImageCallback;
using webrtc::EncodedImage;
using webrtc::I420VideoFrame;
using webrtc::RTPFragmentationHeader;
using webrtc::TickTime;
using webrtc::VideoCodec;
using webrtc::VideoCodecType;
using webrtc::kVideoCodecH264;
using webrtc::kVideoCodecVP8;

namespace webrtc_jni {

jobject MediaCodecVideoDecoderFactory::render_egl_context_ = NULL;

class MediaCodecVideoDecoder : public webrtc::VideoDecoder,
                               public rtc::MessageHandler {
 public:
  explicit MediaCodecVideoDecoder(JNIEnv* jni, VideoCodecType codecType);
  virtual ~MediaCodecVideoDecoder();

  static int SetAndroidObjects(JNIEnv* jni, jobject render_egl_context);

  int32_t InitDecode(const VideoCodec* codecSettings, int32_t numberOfCores)
      override;

  int32_t Decode(
      const EncodedImage& inputImage, bool missingFrames,
      const RTPFragmentationHeader* fragmentation,
      const CodecSpecificInfo* codecSpecificInfo = NULL,
      int64_t renderTimeMs = -1) override;

  int32_t RegisterDecodeCompleteCallback(DecodedImageCallback* callback)
      override;

  int32_t Release() override;

  int32_t Reset() override;
  // rtc::MessageHandler implementation.
  void OnMessage(rtc::Message* msg) override;

 private:
  // CHECK-fail if not running on |codec_thread_|.
  void CheckOnCodecThread();

  int32_t InitDecodeOnCodecThread();
  int32_t ReleaseOnCodecThread();
  int32_t DecodeOnCodecThread(const EncodedImage& inputImage);
  // Deliver any outputs pending in the MediaCodec to our |callback_| and return
  // true on success.
  bool DeliverPendingOutputs(JNIEnv* jni, int dequeue_timeout_us);

  // Type of video codec.
  VideoCodecType codecType_;

  bool key_frame_required_;
  bool inited_;
  bool use_surface_;
  int error_count_;
  VideoCodec codec_;
  I420VideoFrame decoded_image_;
  NativeHandleImpl native_handle_;
  DecodedImageCallback* callback_;
  int frames_received_;  // Number of frames received by decoder.
  int frames_decoded_;  // Number of frames decoded by decoder.
  int64_t start_time_ms_;  // Start time for statistics.
  int current_frames_;  // Number of frames in the current statistics interval.
  int current_bytes_;  // Encoded bytes in the current statistics interval.
  int current_decoding_time_ms_;  // Overall decoding time in the current second
  uint32_t max_pending_frames_;  // Maximum number of pending input frames
  std::vector<int32_t> timestamps_;
  std::vector<int64_t> ntp_times_ms_;
  std::vector<int64_t> frame_rtc_times_ms_;  // Time when video frame is sent to
                                             // decoder input.
  int32_t output_timestamp_;  // Last output frame timestamp from timestamps_ Q.
  int64_t output_ntp_time_ms_; // Last output frame ntp time from
                               // ntp_times_ms_ queue.

  // State that is constant for the lifetime of this object once the ctor
  // returns.
  scoped_ptr<Thread> codec_thread_;  // Thread on which to operate MediaCodec.
  ScopedGlobalRef<jclass> j_media_codec_video_decoder_class_;
  ScopedGlobalRef<jobject> j_media_codec_video_decoder_;
  jmethodID j_init_decode_method_;
  jmethodID j_release_method_;
  jmethodID j_dequeue_input_buffer_method_;
  jmethodID j_queue_input_buffer_method_;
  jmethodID j_dequeue_output_buffer_method_;
  jmethodID j_release_output_buffer_method_;
  // MediaCodecVideoDecoder fields.
  jfieldID j_input_buffers_field_;
  jfieldID j_output_buffers_field_;
  jfieldID j_color_format_field_;
  jfieldID j_width_field_;
  jfieldID j_height_field_;
  jfieldID j_stride_field_;
  jfieldID j_slice_height_field_;
  jfieldID j_surface_texture_field_;
  jfieldID j_textureID_field_;
  // MediaCodecVideoDecoder.DecoderOutputBufferInfo fields.
  jfieldID j_info_index_field_;
  jfieldID j_info_offset_field_;
  jfieldID j_info_size_field_;
  jfieldID j_info_presentation_timestamp_us_field_;

  // Global references; must be deleted in Release().
  std::vector<jobject> input_buffers_;
  jobject surface_texture_;
  jobject previous_surface_texture_;
};

MediaCodecVideoDecoder::MediaCodecVideoDecoder(
    JNIEnv* jni, VideoCodecType codecType) :
    codecType_(codecType),
    key_frame_required_(true),
    inited_(false),
    error_count_(0),
    surface_texture_(NULL),
    previous_surface_texture_(NULL),
    codec_thread_(new Thread()),
    j_media_codec_video_decoder_class_(
        jni,
        FindClass(jni, "org/webrtc/MediaCodecVideoDecoder")),
          j_media_codec_video_decoder_(
              jni,
              jni->NewObject(*j_media_codec_video_decoder_class_,
                   GetMethodID(jni,
                              *j_media_codec_video_decoder_class_,
                              "<init>",
                              "()V"))) {
  ScopedLocalRefFrame local_ref_frame(jni);
  codec_thread_->SetName("MediaCodecVideoDecoder", NULL);
  CHECK(codec_thread_->Start()) << "Failed to start MediaCodecVideoDecoder";

  j_init_decode_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "initDecode",
      "(Lorg/webrtc/MediaCodecVideoDecoder$VideoCodecType;"
      "IIZZLandroid/opengl/EGLContext;)Z");
  j_release_method_ =
      GetMethodID(jni, *j_media_codec_video_decoder_class_, "release", "()V");
  j_dequeue_input_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "dequeueInputBuffer", "()I");
  j_queue_input_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "queueInputBuffer", "(IIJ)Z");
  j_dequeue_output_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "dequeueOutputBuffer",
      "(I)Lorg/webrtc/MediaCodecVideoDecoder$DecoderOutputBufferInfo;");
  j_release_output_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "releaseOutputBuffer", "(IZ)Z");

  j_input_buffers_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_,
      "inputBuffers", "[Ljava/nio/ByteBuffer;");
  j_output_buffers_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_,
      "outputBuffers", "[Ljava/nio/ByteBuffer;");
  j_color_format_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "colorFormat", "I");
  j_width_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "width", "I");
  j_height_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "height", "I");
  j_stride_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "stride", "I");
  j_slice_height_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "sliceHeight", "I");
  j_textureID_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "textureID", "I");
  j_surface_texture_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "surfaceTexture",
      "Landroid/graphics/SurfaceTexture;");

  jclass j_decoder_output_buffer_info_class = FindClass(jni,
      "org/webrtc/MediaCodecVideoDecoder$DecoderOutputBufferInfo");
  j_info_index_field_ = GetFieldID(
      jni, j_decoder_output_buffer_info_class, "index", "I");
  j_info_offset_field_ = GetFieldID(
      jni, j_decoder_output_buffer_info_class, "offset", "I");
  j_info_size_field_ = GetFieldID(
      jni, j_decoder_output_buffer_info_class, "size", "I");
  j_info_presentation_timestamp_us_field_ = GetFieldID(
      jni, j_decoder_output_buffer_info_class, "presentationTimestampUs", "J");

  CHECK_EXCEPTION(jni) << "MediaCodecVideoDecoder ctor failed";
  use_surface_ = true;
  if (MediaCodecVideoDecoderFactory::render_egl_context_ == NULL) {
    use_surface_ = false;
  }
  memset(&codec_, 0, sizeof(codec_));
  AllowBlockingCalls();
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  // Call Release() to ensure no more callbacks to us after we are deleted.
  Release();
  // Delete global references.
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  if (previous_surface_texture_ != NULL) {
    jni->DeleteGlobalRef(previous_surface_texture_);
  }
  if (surface_texture_ != NULL) {
    jni->DeleteGlobalRef(surface_texture_);
  }
}

int32_t MediaCodecVideoDecoder::InitDecode(const VideoCodec* inst,
    int32_t numberOfCores) {
  if (inst == NULL) {
    ALOGE("NULL VideoCodec instance");
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Factory should guard against other codecs being used with us.
  CHECK(inst->codecType == codecType_) << "Unsupported codec " <<
      inst->codecType << " for " << codecType_;

  int ret_val = Release();
  if (ret_val < 0) {
    return ret_val;
  }
  // Save VideoCodec instance for later.
  if (&codec_ != inst) {
    codec_ = *inst;
  }
  codec_.maxFramerate = (codec_.maxFramerate >= 1) ? codec_.maxFramerate : 1;

  // Always start with a complete key frame.
  key_frame_required_ = true;
  frames_received_ = 0;
  frames_decoded_ = 0;

  // Call Java init.
  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoDecoder::InitDecodeOnCodecThread, this));
}

int32_t MediaCodecVideoDecoder::InitDecodeOnCodecThread() {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  ALOGD("InitDecodeOnCodecThread Type: %d. %d x %d. Fps: %d. Errors: %d",
      (int)codecType_, codec_.width, codec_.height,
      codec_.maxFramerate, error_count_);
  bool use_sw_codec = false;
  if (error_count_ > 1) {
    // If more than one critical errors happen for HW codec, switch to SW codec.
    use_sw_codec = true;
  }

  jobject j_video_codec_enum = JavaEnumFromIndex(
      jni, "MediaCodecVideoDecoder$VideoCodecType", codecType_);
  bool success = jni->CallBooleanMethod(
      *j_media_codec_video_decoder_,
      j_init_decode_method_,
      j_video_codec_enum,
      codec_.width,
      codec_.height,
      use_sw_codec,
      use_surface_,
      MediaCodecVideoDecoderFactory::render_egl_context_);
  CHECK_EXCEPTION(jni);
  if (!success) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  inited_ = true;

  switch (codecType_) {
    case kVideoCodecVP8:
      max_pending_frames_ = kMaxPendingFramesVp8;
      break;
    case kVideoCodecH264:
      max_pending_frames_ = kMaxPendingFramesH264;
      break;
    default:
      max_pending_frames_ = 0;
  }
  start_time_ms_ = GetCurrentTimeMs();
  current_frames_ = 0;
  current_bytes_ = 0;
  current_decoding_time_ms_ = 0;
  output_timestamp_ = 0;
  output_ntp_time_ms_ = 0;
  timestamps_.clear();
  ntp_times_ms_.clear();
  frame_rtc_times_ms_.clear();

  jobjectArray input_buffers = (jobjectArray)GetObjectField(
      jni, *j_media_codec_video_decoder_, j_input_buffers_field_);
  size_t num_input_buffers = jni->GetArrayLength(input_buffers);
  input_buffers_.resize(num_input_buffers);
  for (size_t i = 0; i < num_input_buffers; ++i) {
    input_buffers_[i] =
        jni->NewGlobalRef(jni->GetObjectArrayElement(input_buffers, i));
    CHECK_EXCEPTION(jni);
  }

  if (use_surface_) {
    jobject surface_texture = GetObjectField(
        jni, *j_media_codec_video_decoder_, j_surface_texture_field_);
    if (previous_surface_texture_ != NULL) {
      jni->DeleteGlobalRef(previous_surface_texture_);
    }
    previous_surface_texture_ = surface_texture_;
    surface_texture_ = jni->NewGlobalRef(surface_texture);
  }
  codec_thread_->PostDelayed(kMediaCodecPollMs, this);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoDecoder::Release() {
  ALOGD("DecoderRelease request");
  return codec_thread_->Invoke<int32_t>(
        Bind(&MediaCodecVideoDecoder::ReleaseOnCodecThread, this));
}

int32_t MediaCodecVideoDecoder::ReleaseOnCodecThread() {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ALOGD("DecoderReleaseOnCodecThread: Frames received: %d.", frames_received_);
  ScopedLocalRefFrame local_ref_frame(jni);
  for (size_t i = 0; i < input_buffers_.size(); i++) {
    jni->DeleteGlobalRef(input_buffers_[i]);
  }
  input_buffers_.clear();
  jni->CallVoidMethod(*j_media_codec_video_decoder_, j_release_method_);
  CHECK_EXCEPTION(jni);
  rtc::MessageQueueManager::Clear(this);
  inited_ = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

void MediaCodecVideoDecoder::CheckOnCodecThread() {
  CHECK(codec_thread_ == ThreadManager::Instance()->CurrentThread())
      << "Running on wrong thread!";
}

int32_t MediaCodecVideoDecoder::Decode(
    const EncodedImage& inputImage,
    bool missingFrames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codecSpecificInfo,
    int64_t renderTimeMs) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (callback_ == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (inputImage._buffer == NULL && inputImage._length > 0) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Check if encoded frame dimension has changed.
  if ((inputImage._encodedWidth * inputImage._encodedHeight > 0) &&
      (inputImage._encodedWidth != codec_.width ||
      inputImage._encodedHeight != codec_.height)) {
    codec_.width = inputImage._encodedWidth;
    codec_.height = inputImage._encodedHeight;
    InitDecode(&codec_, 1);
  }

  // Always start with a complete key frame.
  if (key_frame_required_) {
    if (inputImage._frameType != webrtc::kKeyFrame) {
      ALOGE("Key frame is required");
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (!inputImage._completeFrame) {
      ALOGE("Complete frame is required");
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    key_frame_required_ = false;
  }
  if (inputImage._length == 0) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return codec_thread_->Invoke<int32_t>(Bind(
      &MediaCodecVideoDecoder::DecodeOnCodecThread, this, inputImage));
}

int32_t MediaCodecVideoDecoder::DecodeOnCodecThread(
    const EncodedImage& inputImage) {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  // Try to drain the decoder and wait until output is not too
  // much behind the input.
  if (frames_received_ > frames_decoded_ + max_pending_frames_) {
    ALOGV("Received: %d. Decoded: %d. Wait for output...",
        frames_received_, frames_decoded_);
    if (!DeliverPendingOutputs(jni, kMediaCodecTimeoutMs * 1000)) {
      error_count_++;
      Reset();
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (frames_received_ > frames_decoded_ + max_pending_frames_) {
      ALOGE("Output buffer dequeue timeout");
      error_count_++;
      Reset();
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  // Get input buffer.
  int j_input_buffer_index = jni->CallIntMethod(*j_media_codec_video_decoder_,
                                                j_dequeue_input_buffer_method_);
  CHECK_EXCEPTION(jni);
  if (j_input_buffer_index < 0) {
    ALOGE("dequeueInputBuffer error");
    error_count_++;
    Reset();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Copy encoded data to Java ByteBuffer.
  jobject j_input_buffer = input_buffers_[j_input_buffer_index];
  uint8* buffer =
      reinterpret_cast<uint8*>(jni->GetDirectBufferAddress(j_input_buffer));
  CHECK(buffer) << "Indirect buffer??";
  int64 buffer_capacity = jni->GetDirectBufferCapacity(j_input_buffer);
  CHECK_EXCEPTION(jni);
  if (buffer_capacity < inputImage._length) {
    ALOGE("Input frame size %d is bigger than buffer size %d.",
        inputImage._length, buffer_capacity);
    error_count_++;
    Reset();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  jlong timestamp_us = (frames_received_ * 1000000) / codec_.maxFramerate;
  ALOGV("Decoder frame in # %d. Type: %d. Buffer # %d. TS: %lld. Size: %d",
      frames_received_, inputImage._frameType, j_input_buffer_index,
      timestamp_us / 1000, inputImage._length);
  memcpy(buffer, inputImage._buffer, inputImage._length);

  // Save input image timestamps for later output.
  frames_received_++;
  current_bytes_ += inputImage._length;
  timestamps_.push_back(inputImage._timeStamp);
  ntp_times_ms_.push_back(inputImage.ntp_time_ms_);
  frame_rtc_times_ms_.push_back(GetCurrentTimeMs());

  // Feed input to decoder.
  bool success = jni->CallBooleanMethod(*j_media_codec_video_decoder_,
                                        j_queue_input_buffer_method_,
                                        j_input_buffer_index,
                                        inputImage._length,
                                        timestamp_us);
  CHECK_EXCEPTION(jni);
  if (!success) {
    ALOGE("queueInputBuffer error");
    error_count_++;
    Reset();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Try to drain the decoder
  if (!DeliverPendingOutputs(jni, 0)) {
    ALOGE("DeliverPendingOutputs error");
    error_count_++;
    Reset();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

bool MediaCodecVideoDecoder::DeliverPendingOutputs(
    JNIEnv* jni, int dequeue_timeout_us) {
  if (frames_received_ <= frames_decoded_) {
    // No need to query for output buffers - decoder is drained.
    return true;
  }
  // Get decoder output.
  jobject j_decoder_output_buffer_info = jni->CallObjectMethod(
      *j_media_codec_video_decoder_,
      j_dequeue_output_buffer_method_,
      dequeue_timeout_us);

  CHECK_EXCEPTION(jni);
  if (IsNull(jni, j_decoder_output_buffer_info)) {
    return true;
  }

  // Extract output buffer info from Java DecoderOutputBufferInfo.
  int output_buffer_index =
      GetIntField(jni, j_decoder_output_buffer_info, j_info_index_field_);
  if (output_buffer_index < 0) {
    ALOGE("dequeueOutputBuffer error : %d", output_buffer_index);
    return false;
  }
  int output_buffer_offset =
      GetIntField(jni, j_decoder_output_buffer_info, j_info_offset_field_);
  int output_buffer_size =
      GetIntField(jni, j_decoder_output_buffer_info, j_info_size_field_);
  long output_timestamps_ms = GetLongField(jni, j_decoder_output_buffer_info,
      j_info_presentation_timestamp_us_field_) / 1000;

  CHECK_EXCEPTION(jni);

  // Get decoded video frame properties.
  int color_format = GetIntField(jni, *j_media_codec_video_decoder_,
      j_color_format_field_);
  int width = GetIntField(jni, *j_media_codec_video_decoder_, j_width_field_);
  int height = GetIntField(jni, *j_media_codec_video_decoder_, j_height_field_);
  int stride = GetIntField(jni, *j_media_codec_video_decoder_, j_stride_field_);
  int slice_height = GetIntField(jni, *j_media_codec_video_decoder_,
      j_slice_height_field_);
  int texture_id = GetIntField(jni, *j_media_codec_video_decoder_,
      j_textureID_field_);

  // Extract data from Java ByteBuffer and create output yuv420 frame -
  // for non surface decoding only.
  if (!use_surface_) {
    if (output_buffer_size < width * height * 3 / 2) {
      ALOGE("Insufficient output buffer size: %d", output_buffer_size);
      return false;
    }
    jobjectArray output_buffers = reinterpret_cast<jobjectArray>(GetObjectField(
        jni, *j_media_codec_video_decoder_, j_output_buffers_field_));
    jobject output_buffer =
        jni->GetObjectArrayElement(output_buffers, output_buffer_index);
    uint8_t* payload = reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(
        output_buffer));
    CHECK_EXCEPTION(jni);
    payload += output_buffer_offset;

    // Create yuv420 frame.
    if (color_format == COLOR_FormatYUV420Planar) {
      decoded_image_.CreateFrame(
          stride * slice_height, payload,
          (stride * slice_height) / 4, payload + (stride * slice_height),
          (stride * slice_height) / 4,
          payload + (5 * stride * slice_height / 4),
          width, height,
          stride, stride / 2, stride / 2);
    } else {
      // All other supported formats are nv12.
      decoded_image_.CreateEmptyFrame(width, height, width,
          width / 2, width / 2);
      libyuv::NV12ToI420(
          payload, stride,
          payload + stride * slice_height, stride,
          decoded_image_.buffer(webrtc::kYPlane),
          decoded_image_.stride(webrtc::kYPlane),
          decoded_image_.buffer(webrtc::kUPlane),
          decoded_image_.stride(webrtc::kUPlane),
          decoded_image_.buffer(webrtc::kVPlane),
          decoded_image_.stride(webrtc::kVPlane),
          width, height);
    }
  }

  // Get frame timestamps from a queue.
  if (timestamps_.size() > 0) {
    output_timestamp_ = timestamps_.front();
    timestamps_.erase(timestamps_.begin());
  }
  if (ntp_times_ms_.size() > 0) {
    output_ntp_time_ms_ = ntp_times_ms_.front();
    ntp_times_ms_.erase(ntp_times_ms_.begin());
  }
  int64_t frame_decoding_time_ms = 0;
  if (frame_rtc_times_ms_.size() > 0) {
    frame_decoding_time_ms = GetCurrentTimeMs() - frame_rtc_times_ms_.front();
    frame_rtc_times_ms_.erase(frame_rtc_times_ms_.begin());
  }
  ALOGV("Decoder frame out # %d. %d x %d. %d x %d. Color: 0x%x. TS: %ld."
      " DecTime: %lld", frames_decoded_, width, height, stride, slice_height,
      color_format, output_timestamps_ms, frame_decoding_time_ms);

  // Return output buffer back to codec.
  bool success = jni->CallBooleanMethod(
      *j_media_codec_video_decoder_,
      j_release_output_buffer_method_,
      output_buffer_index,
      use_surface_);
  CHECK_EXCEPTION(jni);
  if (!success) {
    ALOGE("releaseOutputBuffer error");
    return false;
  }

  // Calculate and print decoding statistics - every 3 seconds.
  frames_decoded_++;
  current_frames_++;
  current_decoding_time_ms_ += frame_decoding_time_ms;
  int statistic_time_ms = GetCurrentTimeMs() - start_time_ms_;
  if (statistic_time_ms >= kMediaCodecStatisticsIntervalMs &&
      current_frames_ > 0) {
    ALOGD("Decoder bitrate: %d kbps, fps: %d, decTime: %d for last %d ms",
        current_bytes_ * 8 / statistic_time_ms,
        (current_frames_ * 1000 + statistic_time_ms / 2) / statistic_time_ms,
        current_decoding_time_ms_ / current_frames_, statistic_time_ms);
    start_time_ms_ = GetCurrentTimeMs();
    current_frames_ = 0;
    current_bytes_ = 0;
    current_decoding_time_ms_ = 0;
  }

  // Callback - output decoded frame.
  int32_t callback_status = WEBRTC_VIDEO_CODEC_OK;
  if (use_surface_) {
    native_handle_.SetTextureObject(surface_texture_, texture_id);
    I420VideoFrame texture_image(
        &native_handle_, width, height, output_timestamp_, 0);
    texture_image.set_ntp_time_ms(output_ntp_time_ms_);
    callback_status = callback_->Decoded(texture_image);
  } else {
    decoded_image_.set_timestamp(output_timestamp_);
    decoded_image_.set_ntp_time_ms(output_ntp_time_ms_);
    callback_status = callback_->Decoded(decoded_image_);
  }
  if (callback_status > 0) {
    ALOGE("callback error");
  }

  return true;
}

int32_t MediaCodecVideoDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoDecoder::Reset() {
  ALOGD("DecoderReset");
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  return InitDecode(&codec_, 1);
}

void MediaCodecVideoDecoder::OnMessage(rtc::Message* msg) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  if (!inited_) {
    return;
  }
  // We only ever send one message to |this| directly (not through a Bind()'d
  // functor), so expect no ID/data.
  CHECK(!msg->message_id) << "Unexpected message!";
  CHECK(!msg->pdata) << "Unexpected message!";
  CheckOnCodecThread();

  if (!DeliverPendingOutputs(jni, 0)) {
    error_count_++;
    Reset();
  }
  codec_thread_->PostDelayed(kMediaCodecPollMs, this);
}

int MediaCodecVideoDecoderFactory::SetAndroidObjects(JNIEnv* jni,
    jobject render_egl_context) {
  ALOGD("SetAndroidObjects for surface decoding.");
  if (render_egl_context_) {
    jni->DeleteGlobalRef(render_egl_context_);
  }
  if (IsNull(jni, render_egl_context)) {
    render_egl_context_ = NULL;
  } else {
    render_egl_context_ = jni->NewGlobalRef(render_egl_context);
    CHECK_EXCEPTION(jni) << "error calling NewGlobalRef for EGL Context.";
    jclass j_egl_context_class = FindClass(jni, "android/opengl/EGLContext");
    if (!jni->IsInstanceOf(render_egl_context_, j_egl_context_class)) {
      ALOGE("Wrong EGL Context.");
      jni->DeleteGlobalRef(render_egl_context_);
      render_egl_context_ = NULL;
    }
  }
  if (render_egl_context_ == NULL) {
    ALOGD("NULL VideoDecoder EGL context - HW surface decoding is disabled.");
  }
  return 0;
}

MediaCodecVideoDecoderFactory::MediaCodecVideoDecoderFactory() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jclass j_decoder_class = FindClass(jni, "org/webrtc/MediaCodecVideoDecoder");
  supported_codec_types_.clear();

  bool is_vp8_hw_supported = jni->CallStaticBooleanMethod(
      j_decoder_class,
      GetStaticMethodID(jni, j_decoder_class, "isVp8HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_vp8_hw_supported) {
    ALOGD("VP8 HW Decoder supported.");
    supported_codec_types_.push_back(kVideoCodecVP8);
  }

  bool is_h264_hw_supported = jni->CallStaticBooleanMethod(
      j_decoder_class,
      GetStaticMethodID(jni, j_decoder_class, "isH264HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_h264_hw_supported) {
    ALOGD("H264 HW Decoder supported.");
    supported_codec_types_.push_back(kVideoCodecH264);
  }
}

MediaCodecVideoDecoderFactory::~MediaCodecVideoDecoderFactory() {}

webrtc::VideoDecoder* MediaCodecVideoDecoderFactory::CreateVideoDecoder(
    VideoCodecType type) {
  if (supported_codec_types_.empty()) {
    return NULL;
  }
  for (std::vector<VideoCodecType>::const_iterator it =
      supported_codec_types_.begin(); it != supported_codec_types_.end();
      ++it) {
    if (*it == type) {
      ALOGD("Create HW video decoder for type %d.", (int)type);
      return new MediaCodecVideoDecoder(AttachCurrentThreadIfNeeded(), type);
    }
  }
  return NULL;
}

void MediaCodecVideoDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  delete decoder;
}

}  // namespace webrtc_jni

