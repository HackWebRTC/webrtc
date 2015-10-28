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

#include <algorithm>
#include <vector>

#include "talk/app/webrtc/java/jni/androidmediadecoder_jni.h"
#include "talk/app/webrtc/java/jni/androidmediacodeccommon.h"
#include "talk/app/webrtc/java/jni/classreferenceholder.h"
#include "talk/app/webrtc/java/jni/native_handle_impl.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_video/interface/i420_buffer_pool.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/system_wrappers/include/logcat_trace_context.h"
#include "webrtc/system_wrappers/include/tick_util.h"
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
using webrtc::VideoFrame;
using webrtc::RTPFragmentationHeader;
using webrtc::TickTime;
using webrtc::VideoCodec;
using webrtc::VideoCodecType;
using webrtc::kVideoCodecH264;
using webrtc::kVideoCodecVP8;

namespace webrtc_jni {

class MediaCodecVideoDecoder : public webrtc::VideoDecoder,
                               public rtc::MessageHandler {
 public:
  explicit MediaCodecVideoDecoder(
      JNIEnv* jni, VideoCodecType codecType, jobject render_egl_context);
  virtual ~MediaCodecVideoDecoder();

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
  int32_t ProcessHWErrorOnCodecThread();

  // Type of video codec.
  VideoCodecType codecType_;

  bool key_frame_required_;
  bool inited_;
  bool sw_fallback_required_;
  bool use_surface_;
  VideoCodec codec_;
  webrtc::I420BufferPool decoded_frame_pool_;
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
  jmethodID j_return_decoded_byte_buffer_method_;
  // MediaCodecVideoDecoder fields.
  jfieldID j_input_buffers_field_;
  jfieldID j_output_buffers_field_;
  jfieldID j_color_format_field_;
  jfieldID j_width_field_;
  jfieldID j_height_field_;
  jfieldID j_stride_field_;
  jfieldID j_slice_height_field_;
  jfieldID j_surface_texture_field_;
  // MediaCodecVideoDecoder.DecodedTextureBuffer fields.
  jfieldID j_textureID_field_;
  jfieldID j_texture_presentation_timestamp_us_field_;
  // MediaCodecVideoDecoder.DecodedByteBuffer fields.
  jfieldID j_info_index_field_;
  jfieldID j_info_offset_field_;
  jfieldID j_info_size_field_;
  jfieldID j_info_presentation_timestamp_us_field_;

  // Global references; must be deleted in Release().
  std::vector<jobject> input_buffers_;
  jobject surface_texture_;
  jobject previous_surface_texture_;

  // Render EGL context - owned by factory, should not be allocated/destroyed
  // by VideoDecoder.
  jobject render_egl_context_;
};

MediaCodecVideoDecoder::MediaCodecVideoDecoder(
    JNIEnv* jni, VideoCodecType codecType, jobject render_egl_context) :
    codecType_(codecType),
    render_egl_context_(render_egl_context),
    key_frame_required_(true),
    inited_(false),
    sw_fallback_required_(false),
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
  RTC_CHECK(codec_thread_->Start()) << "Failed to start MediaCodecVideoDecoder";

  j_init_decode_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "initDecode",
      "(Lorg/webrtc/MediaCodecVideoDecoder$VideoCodecType;"
      "IILjavax/microedition/khronos/egl/EGLContext;)Z");
  j_release_method_ =
      GetMethodID(jni, *j_media_codec_video_decoder_class_, "release", "()V");
  j_dequeue_input_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "dequeueInputBuffer", "()I");
  j_queue_input_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "queueInputBuffer", "(IIJ)Z");
  j_dequeue_output_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "dequeueOutputBuffer",
      "(I)Ljava/lang/Object;");
  j_return_decoded_byte_buffer_method_ =
      GetMethodID(jni, *j_media_codec_video_decoder_class_,
                  "returnDecodedByteBuffer", "(I)V");

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
  j_surface_texture_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "surfaceTexture",
      "Landroid/graphics/SurfaceTexture;");

  jclass j_decoder_decoded_texture_buffer_class = FindClass(jni,
      "org/webrtc/MediaCodecVideoDecoder$DecodedTextureBuffer");
  j_textureID_field_ = GetFieldID(
      jni, j_decoder_decoded_texture_buffer_class, "textureID", "I");
  j_texture_presentation_timestamp_us_field_ =
      GetFieldID(jni, j_decoder_decoded_texture_buffer_class,
                 "presentationTimestampUs", "J");

  jclass j_decoder_decoded_byte_buffer_class = FindClass(jni,
      "org/webrtc/MediaCodecVideoDecoder$DecodedByteBuffer");
  j_info_index_field_ = GetFieldID(
      jni, j_decoder_decoded_byte_buffer_class, "index", "I");
  j_info_offset_field_ = GetFieldID(
      jni, j_decoder_decoded_byte_buffer_class, "offset", "I");
  j_info_size_field_ = GetFieldID(
      jni, j_decoder_decoded_byte_buffer_class, "size", "I");
  j_info_presentation_timestamp_us_field_ = GetFieldID(
      jni, j_decoder_decoded_byte_buffer_class, "presentationTimestampUs", "J");

  CHECK_EXCEPTION(jni) << "MediaCodecVideoDecoder ctor failed";
  use_surface_ = (render_egl_context_ != NULL);
  ALOGD << "MediaCodecVideoDecoder ctor. Use surface: " << use_surface_;
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
  ALOGD << "InitDecode.";
  if (inst == NULL) {
    ALOGE << "NULL VideoCodec instance";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Factory should guard against other codecs being used with us.
  RTC_CHECK(inst->codecType == codecType_)
      << "Unsupported codec " << inst->codecType << " for " << codecType_;

  if (sw_fallback_required_) {
    ALOGE << "InitDecode() - fallback to SW decoder";
    return WEBRTC_VIDEO_CODEC_OK;
  }
  // Save VideoCodec instance for later.
  if (&codec_ != inst) {
    codec_ = *inst;
  }
  // If maxFramerate is not set then assume 30 fps.
  codec_.maxFramerate = (codec_.maxFramerate >= 1) ? codec_.maxFramerate : 30;

  // Call Java init.
  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoDecoder::InitDecodeOnCodecThread, this));
}

int32_t MediaCodecVideoDecoder::InitDecodeOnCodecThread() {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  ALOGD << "InitDecodeOnCodecThread Type: " << (int)codecType_ << ". "
      << codec_.width << " x " << codec_.height << ". Fps: " <<
      (int)codec_.maxFramerate;

  // Release previous codec first if it was allocated before.
  int ret_val = ReleaseOnCodecThread();
  if (ret_val < 0) {
    ALOGE << "Release failure: " << ret_val << " - fallback to SW codec";
    sw_fallback_required_ = true;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Always start with a complete key frame.
  key_frame_required_ = true;
  frames_received_ = 0;
  frames_decoded_ = 0;

  jobject j_video_codec_enum = JavaEnumFromIndex(
      jni, "MediaCodecVideoDecoder$VideoCodecType", codecType_);
  bool success = jni->CallBooleanMethod(
      *j_media_codec_video_decoder_,
      j_init_decode_method_,
      j_video_codec_enum,
      codec_.width,
      codec_.height,
      use_surface_ ? render_egl_context_ : nullptr);
  if (CheckException(jni) || !success) {
    ALOGE << "Codec initialization error - fallback to SW codec.";
    sw_fallback_required_ = true;
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
  timestamps_.clear();
  ntp_times_ms_.clear();
  frame_rtc_times_ms_.clear();

  jobjectArray input_buffers = (jobjectArray)GetObjectField(
      jni, *j_media_codec_video_decoder_, j_input_buffers_field_);
  size_t num_input_buffers = jni->GetArrayLength(input_buffers);
  ALOGD << "Maximum amount of pending frames: " << max_pending_frames_;
  input_buffers_.resize(num_input_buffers);
  for (size_t i = 0; i < num_input_buffers; ++i) {
    input_buffers_[i] =
        jni->NewGlobalRef(jni->GetObjectArrayElement(input_buffers, i));
    if (CheckException(jni)) {
      ALOGE << "NewGlobalRef error - fallback to SW codec.";
      sw_fallback_required_ = true;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
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
  ALOGD << "DecoderRelease request";
  return codec_thread_->Invoke<int32_t>(
        Bind(&MediaCodecVideoDecoder::ReleaseOnCodecThread, this));
}

int32_t MediaCodecVideoDecoder::ReleaseOnCodecThread() {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ALOGD << "DecoderReleaseOnCodecThread: Frames received: " <<
      frames_received_ << ". Frames decoded: " << frames_decoded_;
  ScopedLocalRefFrame local_ref_frame(jni);
  for (size_t i = 0; i < input_buffers_.size(); i++) {
    jni->DeleteGlobalRef(input_buffers_[i]);
  }
  input_buffers_.clear();
  jni->CallVoidMethod(*j_media_codec_video_decoder_, j_release_method_);
  inited_ = false;
  rtc::MessageQueueManager::Clear(this);
  if (CheckException(jni)) {
    ALOGE << "Decoder release exception";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ALOGD << "DecoderReleaseOnCodecThread done";
  return WEBRTC_VIDEO_CODEC_OK;
}

void MediaCodecVideoDecoder::CheckOnCodecThread() {
  RTC_CHECK(codec_thread_ == ThreadManager::Instance()->CurrentThread())
      << "Running on wrong thread!";
}

int32_t MediaCodecVideoDecoder::ProcessHWErrorOnCodecThread() {
  CheckOnCodecThread();
  int ret_val = ReleaseOnCodecThread();
  if (ret_val < 0) {
    ALOGE << "ProcessHWError: Release failure";
  }
  if (codecType_ == kVideoCodecH264) {
    // For now there is no SW H.264 which can be used as fallback codec.
    // So try to restart hw codec for now.
    ret_val = InitDecodeOnCodecThread();
    ALOGE << "Reset H.264 codec done. Status: " << ret_val;
    if (ret_val == WEBRTC_VIDEO_CODEC_OK) {
      // H.264 codec was succesfully reset - return regular error code.
      return WEBRTC_VIDEO_CODEC_ERROR;
    } else {
      // Fail to restart H.264 codec - return error code which should stop the
      // call.
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }
  } else {
    sw_fallback_required_ = true;
    ALOGE << "Return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }
}

int32_t MediaCodecVideoDecoder::Decode(
    const EncodedImage& inputImage,
    bool missingFrames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codecSpecificInfo,
    int64_t renderTimeMs) {
  if (sw_fallback_required_) {
    ALOGE << "Decode() - fallback to SW codec";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }
  if (callback_ == NULL) {
    ALOGE << "Decode() - callback_ is NULL";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (inputImage._buffer == NULL && inputImage._length > 0) {
    ALOGE << "Decode() - inputImage is incorrect";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (!inited_) {
    ALOGE << "Decode() - decoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // Check if encoded frame dimension has changed.
  if ((inputImage._encodedWidth * inputImage._encodedHeight > 0) &&
      (inputImage._encodedWidth != codec_.width ||
      inputImage._encodedHeight != codec_.height)) {
    codec_.width = inputImage._encodedWidth;
    codec_.height = inputImage._encodedHeight;
    int32_t ret = InitDecode(&codec_, 1);
    if (ret < 0) {
      ALOGE << "InitDecode failure: " << ret << " - fallback to SW codec";
      sw_fallback_required_ = true;
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }
  }

  // Always start with a complete key frame.
  if (key_frame_required_) {
    if (inputImage._frameType != webrtc::kVideoFrameKey) {
      ALOGE << "Decode() - key frame is required";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (!inputImage._completeFrame) {
      ALOGE << "Decode() - complete frame is required";
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
      ALOGE << "DeliverPendingOutputs error. Frames received: " <<
          frames_received_ << ". Frames decoded: " << frames_decoded_;
      return ProcessHWErrorOnCodecThread();
    }
    if (frames_received_ > frames_decoded_ + max_pending_frames_) {
      ALOGE << "Output buffer dequeue timeout. Frames received: " <<
          frames_received_ << ". Frames decoded: " << frames_decoded_;
      return ProcessHWErrorOnCodecThread();
    }
  }

  // Get input buffer.
  int j_input_buffer_index = jni->CallIntMethod(*j_media_codec_video_decoder_,
                                                j_dequeue_input_buffer_method_);
  if (CheckException(jni) || j_input_buffer_index < 0) {
    ALOGE << "dequeueInputBuffer error";
    return ProcessHWErrorOnCodecThread();
  }

  // Copy encoded data to Java ByteBuffer.
  jobject j_input_buffer = input_buffers_[j_input_buffer_index];
  uint8_t* buffer =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_input_buffer));
  RTC_CHECK(buffer) << "Indirect buffer??";
  int64_t buffer_capacity = jni->GetDirectBufferCapacity(j_input_buffer);
  if (CheckException(jni) || buffer_capacity < inputImage._length) {
    ALOGE << "Input frame size "<<  inputImage._length <<
        " is bigger than buffer size " << buffer_capacity;
    return ProcessHWErrorOnCodecThread();
  }
  jlong timestamp_us = (frames_received_ * 1000000) / codec_.maxFramerate;
  if (frames_decoded_ < kMaxDecodedLogFrames) {
    ALOGD << "Decoder frame in # " << frames_received_ << ". Type: "
        << inputImage._frameType << ". Buffer # " <<
        j_input_buffer_index << ". TS: " << (int)(timestamp_us / 1000)
        << ". Size: " << inputImage._length;
  }
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
  if (CheckException(jni) || !success) {
    ALOGE << "queueInputBuffer error";
    return ProcessHWErrorOnCodecThread();
  }

  // Try to drain the decoder
  if (!DeliverPendingOutputs(jni, 0)) {
    ALOGE << "DeliverPendingOutputs error";
    return ProcessHWErrorOnCodecThread();
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
  jobject j_decoder_output_buffer = jni->CallObjectMethod(
      *j_media_codec_video_decoder_,
      j_dequeue_output_buffer_method_,
      dequeue_timeout_us);
  if (CheckException(jni)) {
    ALOGE << "dequeueOutputBuffer() error";
    return false;
  }
  if (IsNull(jni, j_decoder_output_buffer)) {
    // No decoded frame ready.
    return true;
  }

  // Get decoded video frame properties.
  int color_format = GetIntField(jni, *j_media_codec_video_decoder_,
      j_color_format_field_);
  int width = GetIntField(jni, *j_media_codec_video_decoder_, j_width_field_);
  int height = GetIntField(jni, *j_media_codec_video_decoder_, j_height_field_);
  int stride = GetIntField(jni, *j_media_codec_video_decoder_, j_stride_field_);
  int slice_height = GetIntField(jni, *j_media_codec_video_decoder_,
      j_slice_height_field_);

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer;
  long output_timestamps_ms = 0;
  if (use_surface_) {
    // Extract data from Java DecodedTextureBuffer.
    const int texture_id =
        GetIntField(jni, j_decoder_output_buffer, j_textureID_field_);
    const int64_t timestamp_us =
        GetLongField(jni, j_decoder_output_buffer,
                     j_texture_presentation_timestamp_us_field_);
    output_timestamps_ms = timestamp_us / rtc::kNumMicrosecsPerMillisec;
    // Create webrtc::VideoFrameBuffer with native texture handle.
    native_handle_.SetTextureObject(surface_texture_, texture_id);
    frame_buffer = new rtc::RefCountedObject<JniNativeHandleBuffer>(
        &native_handle_, width, height);
  } else {
    // Extract data from Java ByteBuffer and create output yuv420 frame -
    // for non surface decoding only.
    const int output_buffer_index =
        GetIntField(jni, j_decoder_output_buffer, j_info_index_field_);
    const int output_buffer_offset =
        GetIntField(jni, j_decoder_output_buffer, j_info_offset_field_);
    const int output_buffer_size =
        GetIntField(jni, j_decoder_output_buffer, j_info_size_field_);
    const int64_t timestamp_us = GetLongField(
        jni, j_decoder_output_buffer, j_info_presentation_timestamp_us_field_);
    output_timestamps_ms = timestamp_us / rtc::kNumMicrosecsPerMillisec;

    if (output_buffer_size < width * height * 3 / 2) {
      ALOGE << "Insufficient output buffer size: " << output_buffer_size;
      return false;
    }
    jobjectArray output_buffers = reinterpret_cast<jobjectArray>(GetObjectField(
        jni, *j_media_codec_video_decoder_, j_output_buffers_field_));
    jobject output_buffer =
        jni->GetObjectArrayElement(output_buffers, output_buffer_index);
    uint8_t* payload = reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(
        output_buffer));
    if (CheckException(jni)) {
      return false;
    }
    payload += output_buffer_offset;

    // Create yuv420 frame.
    frame_buffer = decoded_frame_pool_.CreateBuffer(width, height);
    if (color_format == COLOR_FormatYUV420Planar) {
      RTC_CHECK_EQ(0, stride % 2);
      RTC_CHECK_EQ(0, slice_height % 2);
      const int uv_stride = stride / 2;
      const int u_slice_height = slice_height / 2;
      const uint8_t* y_ptr = payload;
      const uint8_t* u_ptr = y_ptr + stride * slice_height;
      const uint8_t* v_ptr = u_ptr + uv_stride * u_slice_height;
      libyuv::I420Copy(y_ptr, stride,
                       u_ptr, uv_stride,
                       v_ptr, uv_stride,
                       frame_buffer->MutableData(webrtc::kYPlane),
                       frame_buffer->stride(webrtc::kYPlane),
                       frame_buffer->MutableData(webrtc::kUPlane),
                       frame_buffer->stride(webrtc::kUPlane),
                       frame_buffer->MutableData(webrtc::kVPlane),
                       frame_buffer->stride(webrtc::kVPlane),
                       width, height);
    } else {
      // All other supported formats are nv12.
      const uint8_t* y_ptr = payload;
      const uint8_t* uv_ptr = y_ptr + stride * slice_height;
      libyuv::NV12ToI420(
          y_ptr, stride,
          uv_ptr, stride,
          frame_buffer->MutableData(webrtc::kYPlane),
          frame_buffer->stride(webrtc::kYPlane),
          frame_buffer->MutableData(webrtc::kUPlane),
          frame_buffer->stride(webrtc::kUPlane),
          frame_buffer->MutableData(webrtc::kVPlane),
          frame_buffer->stride(webrtc::kVPlane),
          width, height);
    }
    // Return output byte buffer back to codec.
    jni->CallVoidMethod(
        *j_media_codec_video_decoder_,
        j_return_decoded_byte_buffer_method_,
        output_buffer_index);
    if (CheckException(jni)) {
      ALOGE << "returnDecodedByteBuffer error";
      return false;
    }
  }
  VideoFrame decoded_frame(frame_buffer, 0, 0, webrtc::kVideoRotation_0);

  // Get frame timestamps from a queue.
  if (timestamps_.size() > 0) {
    decoded_frame.set_timestamp(timestamps_.front());
    timestamps_.erase(timestamps_.begin());
  }
  if (ntp_times_ms_.size() > 0) {
    decoded_frame.set_ntp_time_ms(ntp_times_ms_.front());
    ntp_times_ms_.erase(ntp_times_ms_.begin());
  }
  int64_t frame_decoding_time_ms = 0;
  if (frame_rtc_times_ms_.size() > 0) {
    frame_decoding_time_ms = GetCurrentTimeMs() - frame_rtc_times_ms_.front();
    frame_rtc_times_ms_.erase(frame_rtc_times_ms_.begin());
  }
  if (frames_decoded_ < kMaxDecodedLogFrames) {
    ALOGD << "Decoder frame out # " << frames_decoded_ << ". " << width <<
        " x " << height << ". " << stride << " x " <<  slice_height <<
        ". Color: " << color_format << ". TS:" << (int)output_timestamps_ms <<
        ". DecTime: " << (int)frame_decoding_time_ms;
  }

  // Calculate and print decoding statistics - every 3 seconds.
  frames_decoded_++;
  current_frames_++;
  current_decoding_time_ms_ += frame_decoding_time_ms;
  int statistic_time_ms = GetCurrentTimeMs() - start_time_ms_;
  if (statistic_time_ms >= kMediaCodecStatisticsIntervalMs &&
      current_frames_ > 0) {
    ALOGD << "Decoded frames: " << frames_decoded_ << ". Bitrate: " <<
        (current_bytes_ * 8 / statistic_time_ms) << " kbps, fps: " <<
        ((current_frames_ * 1000 + statistic_time_ms / 2) / statistic_time_ms)
        << ". decTime: " << (current_decoding_time_ms_ / current_frames_) <<
        " for last " << statistic_time_ms << " ms.";
    start_time_ms_ = GetCurrentTimeMs();
    current_frames_ = 0;
    current_bytes_ = 0;
    current_decoding_time_ms_ = 0;
  }

  // Callback - output decoded frame.
  const int32_t callback_status = callback_->Decoded(decoded_frame);
  if (callback_status > 0) {
    ALOGE << "callback error";
  }

  return true;
}

int32_t MediaCodecVideoDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoDecoder::Reset() {
  ALOGD << "DecoderReset";
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
  RTC_CHECK(!msg->message_id) << "Unexpected message!";
  RTC_CHECK(!msg->pdata) << "Unexpected message!";
  CheckOnCodecThread();

  if (!DeliverPendingOutputs(jni, 0)) {
    ALOGE << "OnMessage: DeliverPendingOutputs error";
    ProcessHWErrorOnCodecThread();
    return;
  }
  codec_thread_->PostDelayed(kMediaCodecPollMs, this);
}

MediaCodecVideoDecoderFactory::MediaCodecVideoDecoderFactory() :
    render_egl_context_(NULL) {
  ALOGD << "MediaCodecVideoDecoderFactory ctor";
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jclass j_decoder_class = FindClass(jni, "org/webrtc/MediaCodecVideoDecoder");
  supported_codec_types_.clear();

  bool is_vp8_hw_supported = jni->CallStaticBooleanMethod(
      j_decoder_class,
      GetStaticMethodID(jni, j_decoder_class, "isVp8HwSupported", "()Z"));
  if (CheckException(jni)) {
    is_vp8_hw_supported = false;
  }
  if (is_vp8_hw_supported) {
    ALOGD << "VP8 HW Decoder supported.";
    supported_codec_types_.push_back(kVideoCodecVP8);
  }

  bool is_h264_hw_supported = jni->CallStaticBooleanMethod(
      j_decoder_class,
      GetStaticMethodID(jni, j_decoder_class, "isH264HwSupported", "()Z"));
  if (CheckException(jni)) {
    is_h264_hw_supported = false;
  }
  if (is_h264_hw_supported) {
    ALOGD << "H264 HW Decoder supported.";
    supported_codec_types_.push_back(kVideoCodecH264);
  }
}

MediaCodecVideoDecoderFactory::~MediaCodecVideoDecoderFactory() {
  ALOGD << "MediaCodecVideoDecoderFactory dtor";
  if (render_egl_context_) {
    JNIEnv* jni = AttachCurrentThreadIfNeeded();
    jni->DeleteGlobalRef(render_egl_context_);
    render_egl_context_ = NULL;
  }
}

void MediaCodecVideoDecoderFactory::SetEGLContext(
    JNIEnv* jni, jobject render_egl_context) {
  ALOGD << "MediaCodecVideoDecoderFactory::SetEGLContext";
  if (render_egl_context_) {
    jni->DeleteGlobalRef(render_egl_context_);
    render_egl_context_ = NULL;
  }
  if (!IsNull(jni, render_egl_context)) {
    render_egl_context_ = jni->NewGlobalRef(render_egl_context);
    if (CheckException(jni)) {
      ALOGE << "error calling NewGlobalRef for EGL Context.";
      render_egl_context_ = NULL;
    } else {
      jclass j_egl_context_class =
          FindClass(jni, "javax/microedition/khronos/egl/EGLContext");
      if (!jni->IsInstanceOf(render_egl_context_, j_egl_context_class)) {
        ALOGE << "Wrong EGL Context.";
        jni->DeleteGlobalRef(render_egl_context_);
        render_egl_context_ = NULL;
      }
    }
  }
  if (render_egl_context_ == NULL) {
    ALOGW << "NULL VideoDecoder EGL context - HW surface decoding is disabled.";
  }
}

webrtc::VideoDecoder* MediaCodecVideoDecoderFactory::CreateVideoDecoder(
    VideoCodecType type) {
  if (supported_codec_types_.empty()) {
    ALOGE << "No HW video decoder for type " << (int)type;
    return NULL;
  }
  for (VideoCodecType codec_type : supported_codec_types_) {
    if (codec_type == type) {
      ALOGD << "Create HW video decoder for type " << (int)type;
      return new MediaCodecVideoDecoder(
          AttachCurrentThreadIfNeeded(), type, render_egl_context_);
    }
  }
  ALOGE << "Can not find HW video decoder for type " << (int)type;
  return NULL;
}

void MediaCodecVideoDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  ALOGD << "Destroy video decoder.";
  delete decoder;
}

}  // namespace webrtc_jni

