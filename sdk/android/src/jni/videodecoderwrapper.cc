/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videodecoderwrapper.h"

#include "api/video/video_frame.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "sdk/android/src/jni/classreferenceholder.h"

namespace webrtc {
namespace jni {

VideoDecoderWrapper::VideoDecoderWrapper(JNIEnv* jni, jobject decoder)
    : android_video_buffer_factory_(jni),
      decoder_(jni, decoder),
      encoded_image_class_(jni, FindClass(jni, "org/webrtc/EncodedImage")),
      frame_type_class_(jni,
                        FindClass(jni, "org/webrtc/EncodedImage$FrameType")),
      settings_class_(jni, FindClass(jni, "org/webrtc/VideoDecoder$Settings")),
      video_frame_class_(jni, FindClass(jni, "org/webrtc/VideoFrame")),
      video_codec_status_class_(jni,
                                FindClass(jni, "org/webrtc/VideoCodecStatus")),
      integer_class_(jni, jni->FindClass("java/lang/Integer")) {
  encoded_image_constructor_ =
      jni->GetMethodID(*encoded_image_class_, "<init>",
                       "(Ljava/nio/ByteBuffer;IIJLorg/webrtc/"
                       "EncodedImage$FrameType;IZLjava/lang/Integer;)V");
  settings_constructor_ =
      jni->GetMethodID(*settings_class_, "<init>", "(III)V");

  empty_frame_field_ = jni->GetStaticFieldID(
      *frame_type_class_, "EmptyFrame", "Lorg/webrtc/EncodedImage$FrameType;");
  video_frame_key_field_ =
      jni->GetStaticFieldID(*frame_type_class_, "VideoFrameKey",
                            "Lorg/webrtc/EncodedImage$FrameType;");
  video_frame_delta_field_ =
      jni->GetStaticFieldID(*frame_type_class_, "VideoFrameDelta",
                            "Lorg/webrtc/EncodedImage$FrameType;");

  video_frame_get_timestamp_ns_method_ =
      jni->GetMethodID(*video_frame_class_, "getTimestampNs", "()J");

  jclass decoder_class = jni->GetObjectClass(decoder);
  init_decode_method_ =
      jni->GetMethodID(decoder_class, "initDecode",
                       "(Lorg/webrtc/VideoDecoder$Settings;Lorg/webrtc/"
                       "VideoDecoder$Callback;)Lorg/webrtc/VideoCodecStatus;");
  release_method_ = jni->GetMethodID(decoder_class, "release",
                                     "()Lorg/webrtc/VideoCodecStatus;");
  decode_method_ = jni->GetMethodID(decoder_class, "decode",
                                    "(Lorg/webrtc/EncodedImage;Lorg/webrtc/"
                                    "VideoDecoder$DecodeInfo;)Lorg/webrtc/"
                                    "VideoCodecStatus;");
  get_prefers_late_decoding_method_ =
      jni->GetMethodID(decoder_class, "getPrefersLateDecoding", "()Z");
  get_implementation_name_method_ = jni->GetMethodID(
      decoder_class, "getImplementationName", "()Ljava/lang/String;");

  get_number_method_ =
      jni->GetMethodID(*video_codec_status_class_, "getNumber", "()I");

  integer_constructor_ = jni->GetMethodID(*integer_class_, "<init>", "(I)V");
  int_value_method_ = jni->GetMethodID(*integer_class_, "intValue", "()I");

  initialized_ = false;
  // QP parsing starts enabled and we disable it if the decoder provides frames.
  qp_parsing_enabled_ = true;

  implementation_name_ = GetImplementationName(jni);
}

int32_t VideoDecoderWrapper::InitDecode(const VideoCodec* codec_settings,
                                        int32_t number_of_cores) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;
  return InitDecodeInternal(jni);
}

int32_t VideoDecoderWrapper::InitDecodeInternal(JNIEnv* jni) {
  jobject settings =
      jni->NewObject(*settings_class_, settings_constructor_, number_of_cores_,
                     codec_settings_.width, codec_settings_.height);

  jclass callback_class =
      FindClass(jni, "org/webrtc/VideoDecoderWrapperCallback");
  jmethodID callback_constructor =
      jni->GetMethodID(callback_class, "<init>", "(J)V");
  jobject callback = jni->NewObject(callback_class, callback_constructor,
                                    jlongFromPointer(this));

  jobject ret =
      jni->CallObjectMethod(*decoder_, init_decode_method_, settings, callback);
  if (jni->CallIntMethod(ret, get_number_method_) == WEBRTC_VIDEO_CODEC_OK) {
    initialized_ = true;
  }

  // The decoder was reinitialized so re-enable the QP parsing in case it stops
  // providing QP values.
  qp_parsing_enabled_ = true;

  return HandleReturnCode(jni, ret);
}

int32_t VideoDecoderWrapper::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  if (!initialized_) {
    // Most likely initializing the codec failed.
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  FrameExtraInfo frame_extra_info;
  frame_extra_info.capture_time_ns =
      input_image.capture_time_ms_ * rtc::kNumNanosecsPerMillisec;
  frame_extra_info.timestamp_rtp = input_image._timeStamp;
  frame_extra_info.qp =
      qp_parsing_enabled_ ? ParseQP(input_image) : rtc::Optional<uint8_t>();
  frame_extra_infos_.push_back(frame_extra_info);

  jobject jinput_image =
      ConvertEncodedImageToJavaEncodedImage(jni, input_image);
  jobject ret =
      jni->CallObjectMethod(*decoder_, decode_method_, jinput_image, nullptr);
  return HandleReturnCode(jni, ret);
}

int32_t VideoDecoderWrapper::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VideoDecoderWrapper::Release() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject ret = jni->CallObjectMethod(*decoder_, release_method_);
  frame_extra_infos_.clear();
  initialized_ = false;
  return HandleReturnCode(jni, ret);
}

bool VideoDecoderWrapper::PrefersLateDecoding() const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  return jni->CallBooleanMethod(*decoder_, get_prefers_late_decoding_method_);
}

const char* VideoDecoderWrapper::ImplementationName() const {
  return implementation_name_.c_str();
}

void VideoDecoderWrapper::OnDecodedFrame(JNIEnv* jni,
                                         jobject jframe,
                                         jobject jdecode_time_ms,
                                         jobject jqp) {
  const jlong capture_time_ns =
      jni->CallLongMethod(jframe, video_frame_get_timestamp_ns_method_);
  FrameExtraInfo frame_extra_info;
  do {
    if (frame_extra_infos_.empty()) {
      LOG(LS_WARNING) << "Java decoder produced an unexpected frame.";
      return;
    }

    frame_extra_info = frame_extra_infos_.front();
    frame_extra_infos_.pop_front();
    // If the decoder might drop frames so iterate through the queue until we
    // find a matching timestamp.
  } while (frame_extra_info.capture_time_ns != capture_time_ns);

  VideoFrame frame = android_video_buffer_factory_.CreateFrame(
      jni, jframe, frame_extra_info.timestamp_rtp);

  rtc::Optional<int32_t> decoding_time_ms;
  if (jdecode_time_ms != nullptr) {
    decoding_time_ms = rtc::Optional<int32_t>(
        jni->CallIntMethod(jdecode_time_ms, int_value_method_));
  }

  rtc::Optional<uint8_t> qp;
  if (jqp != nullptr) {
    qp = rtc::Optional<uint8_t>(jni->CallIntMethod(jqp, int_value_method_));
    // The decoder provides QP values itself, no need to parse the bitstream.
    qp_parsing_enabled_ = false;
  } else {
    qp = frame_extra_info.qp;
    // The decoder doesn't provide QP values, ensure bitstream parsing is
    // enabled.
    qp_parsing_enabled_ = true;
  }

  callback_->Decoded(frame, decoding_time_ms, qp);
}

jobject VideoDecoderWrapper::ConvertEncodedImageToJavaEncodedImage(
    JNIEnv* jni,
    const EncodedImage& image) {
  jobject buffer = jni->NewDirectByteBuffer(image._buffer, image._length);
  jfieldID frame_type_field;
  switch (image._frameType) {
    case kEmptyFrame:
      frame_type_field = empty_frame_field_;
      break;
    case kVideoFrameKey:
      frame_type_field = video_frame_key_field_;
      break;
    case kVideoFrameDelta:
      frame_type_field = video_frame_delta_field_;
      break;
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
  jobject frame_type =
      jni->GetStaticObjectField(*frame_type_class_, frame_type_field);
  jobject qp = nullptr;
  if (image.qp_ != -1) {
    qp = jni->NewObject(*integer_class_, integer_constructor_, image.qp_);
  }
  return jni->NewObject(
      *encoded_image_class_, encoded_image_constructor_, buffer,
      static_cast<jint>(image._encodedWidth),
      static_cast<jint>(image._encodedHeight),
      static_cast<jlong>(image.capture_time_ms_ * rtc::kNumNanosecsPerMillisec),
      frame_type, static_cast<jint>(image.rotation_), image._completeFrame, qp);
}

int32_t VideoDecoderWrapper::HandleReturnCode(JNIEnv* jni, jobject code) {
  int32_t value = jni->CallIntMethod(code, get_number_method_);
  if (value < 0) {  // Any errors are represented by negative values.
    // Reset the codec.
    if (Release() == WEBRTC_VIDEO_CODEC_OK) {
      InitDecodeInternal(jni);
    }

    LOG(LS_WARNING) << "Falling back to software decoder.";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  } else {
    return value;
  }
}

rtc::Optional<uint8_t> VideoDecoderWrapper::ParseQP(
    const EncodedImage& input_image) {
  if (input_image.qp_ != -1) {
    return rtc::Optional<uint8_t>(input_image.qp_);
  }

  rtc::Optional<uint8_t> qp;
  switch (codec_settings_.codecType) {
    case kVideoCodecVP8: {
      int qp_int;
      if (vp8::GetQp(input_image._buffer, input_image._length, &qp_int)) {
        qp = rtc::Optional<uint8_t>(qp_int);
      }
      break;
    }
    case kVideoCodecVP9: {
      int qp_int;
      if (vp9::GetQp(input_image._buffer, input_image._length, &qp_int)) {
        qp = rtc::Optional<uint8_t>(qp_int);
      }
      break;
    }
    case kVideoCodecH264: {
      h264_bitstream_parser_.ParseBitstream(input_image._buffer,
                                            input_image._length);
      int qp_int;
      if (h264_bitstream_parser_.GetLastSliceQp(&qp_int)) {
        qp = rtc::Optional<uint8_t>(qp_int);
      }
      break;
    }
    default:
      break;  // Default is to not provide QP.
  }
  return qp;
}

std::string VideoDecoderWrapper::GetImplementationName(JNIEnv* jni) const {
  jstring jname = reinterpret_cast<jstring>(
      jni->CallObjectMethod(*decoder_, get_implementation_name_method_));
  return JavaToStdString(jni, jname);
}

JNI_FUNCTION_DECLARATION(void,
                         VideoDecoderWrapperCallback_nativeOnDecodedFrame,
                         JNIEnv* jni,
                         jclass,
                         jlong jnative_decoder,
                         jobject jframe,
                         jobject jdecode_time_ms,
                         jobject jqp) {
  VideoDecoderWrapper* native_decoder =
      reinterpret_cast<VideoDecoderWrapper*>(jnative_decoder);
  native_decoder->OnDecodedFrame(jni, jframe, jdecode_time_ms, jqp);
}

}  // namespace jni
}  // namespace webrtc
