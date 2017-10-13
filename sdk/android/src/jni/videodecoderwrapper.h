/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_VIDEODECODERWRAPPER_H_
#define SDK_ANDROID_SRC_JNI_VIDEODECODERWRAPPER_H_

#include <jni.h>
#include <deque>

#include "api/video_codecs/video_decoder.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/native_handle_impl.h"

namespace webrtc {
namespace jni {

// Wraps a Java decoder and delegates all calls to it. Passes
// VideoDecoderWrapperCallback to the decoder on InitDecode. Wraps the received
// frames to AndroidVideoBuffer.
class VideoDecoderWrapper : public VideoDecoder {
 public:
  VideoDecoderWrapper(JNIEnv* jni, jobject decoder);

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override;

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 const RTPFragmentationHeader* fragmentation,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  int32_t Release() override;

  // Returns true if the decoder prefer to decode frames late.
  // That is, it can not decode infinite number of frames before the decoded
  // frame is consumed.
  bool PrefersLateDecoding() const override;

  const char* ImplementationName() const override;

  // Wraps the frame to a AndroidVideoBuffer and passes it to the callback.
  void OnDecodedFrame(JNIEnv* jni,
                      jobject jframe,
                      jobject jdecode_time_ms,
                      jobject jqp);

 private:
  struct FrameExtraInfo {
    uint64_t capture_time_ns;  // Used as an identifier of the frame.

    uint32_t timestamp_rtp;
    rtc::Optional<uint8_t> qp;
  };

  int32_t InitDecodeInternal(JNIEnv* jni);

  // Takes Java VideoCodecStatus, handles it and returns WEBRTC_VIDEO_CODEC_*
  // status code.
  int32_t HandleReturnCode(JNIEnv* jni, jobject code);

  rtc::Optional<uint8_t> ParseQP(const EncodedImage& input_image);

  std::string GetImplementationName(JNIEnv* jni) const;

  VideoCodec codec_settings_;
  int32_t number_of_cores_;

  bool initialized_;
  AndroidVideoBufferFactory android_video_buffer_factory_;
  std::deque<FrameExtraInfo> frame_extra_infos_;
  bool qp_parsing_enabled_;
  H264BitstreamParser h264_bitstream_parser_;
  std::string implementation_name_;

  DecodedImageCallback* callback_;

  const ScopedGlobalRef<jobject> decoder_;
  const ScopedGlobalRef<jclass> encoded_image_class_;
  const ScopedGlobalRef<jclass> frame_type_class_;
  const ScopedGlobalRef<jclass> settings_class_;
  const ScopedGlobalRef<jclass> video_frame_class_;
  const ScopedGlobalRef<jclass> video_codec_status_class_;
  const ScopedGlobalRef<jclass> integer_class_;

  jmethodID encoded_image_constructor_;
  jmethodID settings_constructor_;

  jfieldID empty_frame_field_;
  jfieldID video_frame_key_field_;
  jfieldID video_frame_delta_field_;

  jmethodID video_frame_get_timestamp_ns_method_;

  jmethodID init_decode_method_;
  jmethodID release_method_;
  jmethodID decode_method_;
  jmethodID get_prefers_late_decoding_method_;
  jmethodID get_implementation_name_method_;

  jmethodID get_number_method_;

  jmethodID integer_constructor_;
  jmethodID int_value_method_;

  jobject ConvertEncodedImageToJavaEncodedImage(JNIEnv* jni,
                                                const EncodedImage& image);
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_VIDEODECODERWRAPPER_H_
