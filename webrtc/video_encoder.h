/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENCODER_H_
#define WEBRTC_VIDEO_ENCODER_H_

#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_frame.h"

namespace webrtc {

class RTPFragmentationHeader;
// TODO(pbos): Expose these through a public (root) header or change these APIs.
struct CodecSpecificInfo;
struct VideoCodec;

class EncodedImageCallback {
 public:
  virtual ~EncodedImageCallback() {}

  // Callback function which is called when an image has been encoded.
  // TODO(pbos): Make encoded_image const or pointer. Remove default arguments.
  virtual int32_t Encoded(
      EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info = NULL,
      const RTPFragmentationHeader* fragmentation = NULL) = 0;
};

class VideoEncoder {
 public:
  enum EncoderType {
    kVp8,
  };

  static VideoEncoder* Create(EncoderType codec_type);

  static VideoCodecVP8 GetDefaultVp8Settings();
  static VideoCodecH264 GetDefaultH264Settings();

  virtual ~VideoEncoder() {}

  virtual int32_t InitEncode(const VideoCodec* codec_settings,
                             int32_t number_of_cores,
                             uint32_t max_payload_size) = 0;
  virtual int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) = 0;
  virtual int32_t Release() = 0;


  virtual int32_t Encode(const I420VideoFrame& frame,
                         const CodecSpecificInfo* codec_specific_info,
                         const std::vector<VideoFrameType>* frame_types) = 0;

  virtual int32_t SetChannelParameters(uint32_t packet_loss, int rtt) = 0;
  virtual int32_t SetRates(uint32_t bitrate, uint32_t framerate) = 0;

  virtual int32_t SetPeriodicKeyFrames(bool enable) { return -1; }
  virtual int32_t CodecConfigParameters(uint8_t* /*buffer*/, int32_t /*size*/) {
    return -1;
  }
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_ENCODER_H_
