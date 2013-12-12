/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/examples/android/media_demo/jni/media_codec_video_decoder.h"

#include <android/log.h>

#include "webrtc/examples/android/media_demo/jni/jni_helpers.h"
#include "webrtc/modules/utility/interface/helpers_android.h"

namespace webrtc {

MediaCodecVideoDecoder::MediaCodecVideoDecoder(JavaVM* vm, jobject decoder)
    : vm_(vm), decoder_(NULL), j_start_(NULL), j_push_buffer_(NULL) {
  AttachThreadScoped ats(vm_);
  JNIEnv* jni = ats.env();
  // Make sure that the decoder is not recycled.
  decoder_ = jni->NewGlobalRef(decoder);

  // Get all function IDs.
  jclass decoderClass = jni->GetObjectClass(decoder);
  j_push_buffer_ =
      jni->GetMethodID(decoderClass, "pushBuffer", "(Ljava/nio/ByteBuffer;J)V");
  j_start_ = jni->GetMethodID(decoderClass, "start", "(II)Z");
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  AttachThreadScoped ats(vm_);
  JNIEnv* jni = ats.env();
  jni->DeleteGlobalRef(decoder_);
}

int32_t MediaCodecVideoDecoder::InitDecode(const VideoCodec* codecSettings,
                                           int32_t numberOfCores) {
  AttachThreadScoped ats(vm_);
  JNIEnv* jni = ats.env();
  if (!jni->CallBooleanMethod(decoder_, j_start_, codecSettings->width,
                              codecSettings->height)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoDecoder::Decode(
    const EncodedImage& inputImage, bool missingFrames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codecSpecificInfo, int64_t renderTimeMs) {

  AttachThreadScoped ats(vm_);
  JNIEnv* jni = ats.env();
  jobject byteBuffer =
      jni->NewDirectByteBuffer(inputImage._buffer, inputImage._length);
  jni->CallVoidMethod(decoder_, j_push_buffer_, byteBuffer, renderTimeMs);
  jni->DeleteLocalRef(byteBuffer);
  return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
}

int32_t MediaCodecVideoDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoDecoder::Release() {
  // TODO(hellner): this maps nicely to MediaCodecVideoDecoder::dispose().
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoDecoder::Reset() {
  // TODO(hellner): implement. MediaCodec::stop() followed by
  // MediaCodec::start()?
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace webrtc
