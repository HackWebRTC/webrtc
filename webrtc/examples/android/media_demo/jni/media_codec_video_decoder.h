/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_EXAMPLES_ANDROID_MEDIA_DEMO_JNI_MEDIA_CODEC_VIDEO_DECODER_H_
#define WEBRTC_EXAMPLES_ANDROID_MEDIA_DEMO_JNI_MEDIA_CODEC_VIDEO_DECODER_H_

#include <jni.h>

#include "webrtc/examples/android/media_demo/jni/jni_helpers.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

namespace webrtc {

class MediaCodecVideoDecoder : public VideoDecoder {
 public:
  MediaCodecVideoDecoder(JavaVM* vm, jobject decoder);
  virtual ~MediaCodecVideoDecoder();

  virtual int32_t InitDecode(const VideoCodec* codecSettings,
                             int32_t numberOfCores);

  virtual int32_t Decode(const EncodedImage& inputImage, bool missingFrames,
                         const RTPFragmentationHeader* fragmentation,
                         const CodecSpecificInfo* codecSpecificInfo,
                         int64_t renderTimeMs);

  virtual int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback);

  virtual int32_t Release();

  virtual int32_t Reset();

  virtual int32_t SetCodecConfigParameters(const uint8_t* /*buffer*/,
                                           int32_t /*size*/) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  virtual VideoDecoder* Copy() {
    CHECK(0, "Not implemented");
    return NULL;
  }

 private:
  JavaVM* vm_;
  // Global reference to a (Java) MediaCodecVideoDecoder object.
  jobject decoder_;
  jmethodID j_start_;
  jmethodID j_push_buffer_;
};

}  // namespace webrtc

#endif  // WEBRTC_EXAMPLES_ANDROID_MEDIA_DEMO_JNI_MEDIA_CODEC_VIDEO_DECODER_H_
