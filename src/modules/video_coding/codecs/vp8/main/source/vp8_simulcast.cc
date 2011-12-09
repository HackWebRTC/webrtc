/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vp8_simulcast.cc
 * WEBRTC VP8 simulcast wrapper interface
 */

#include "vp8_simulcast.h"

#include <string.h>

#include "module_common_types.h"
#include "trace.h"

namespace webrtc {

VP8SimulcastEncoder::VP8SimulcastEncoder() {
  for (int i = 0; i < kMaxSimulcastStreams; i++) {
    encoder_[i] = NULL;
    encode_stream_[i] = false;
    frame_type_[i] = kKeyFrame;
    scaler_[i] = NULL;
  }
}

VP8SimulcastEncoder::~VP8SimulcastEncoder() {
  for (int i = 0; i < kMaxSimulcastStreams; i++) {
    delete encoder_[i];
    delete scaler_[i];
    delete [] video_frame_[i]._buffer;
  }
}

WebRtc_Word32 VP8SimulcastEncoder::Release() {
  for (int i = 0; i < kMaxSimulcastStreams; i++) {
    delete encoder_[i];
    encoder_[i] = NULL;
    delete scaler_[i];
    scaler_[i] = NULL;
    delete [] video_frame_[i]._buffer;
    video_frame_[i]._buffer = NULL;
    video_frame_[i]._size = 0;
  }
  return 0;
}

WebRtc_Word32 VP8SimulcastEncoder::Reset() {
  for (int i = 0; i < kMaxSimulcastStreams; i++) {
    if (encoder_[i]) {
      encoder_[i]->Reset();
    }
  }
  return 0;
}

WebRtc_Word32 VP8SimulcastEncoder::InitEncode(const VideoCodec* codecSettings,
                                              WebRtc_Word32 numberOfCores,
                                              WebRtc_UWord32 maxPayloadSize) {
  // Store a config copy
  memcpy(&video_codec_, codecSettings, sizeof(VideoCodec));

  // local copy
  VideoCodec video_codec;
  memcpy(&video_codec, codecSettings, sizeof(VideoCodec));
  video_codec.numberOfSimulcastStreams = 0;

  WebRtc_UWord32 bitrate_sum = 0;
  WebRtc_Word32 ret_val = 0;
  for (int i = 0; i < codecSettings->numberOfSimulcastStreams; i++) {
    if (encoder_[i] == NULL) {
      encoder_[i] = new VP8Encoder();
    }
    assert(encoder_[i]);

    if (codecSettings->startBitrate > bitrate_sum) {
      frame_type_[i] = kKeyFrame;
      encode_stream_[i] = true;
    } else {
      // no more bits
      encode_stream_[i] = false;
      continue;
    }
    bitrate_sum += codecSettings->simulcastStream[i].maxBitrate;
    if (codecSettings->startBitrate >= bitrate_sum) {
      video_codec.startBitrate = codecSettings->simulcastStream[i].maxBitrate;
    } else {
      // The last stream will get what ever is left of the budget up to its max
      video_codec.startBitrate =
          codecSettings->startBitrate -
          (bitrate_sum -
          codecSettings->simulcastStream[i].maxBitrate);
    }
    video_codec.maxBitrate = codecSettings->simulcastStream[i].maxBitrate;
    video_codec.qpMax = codecSettings->simulcastStream[i].qpMax;
    video_codec.width = codecSettings->simulcastStream[i].width;
    video_codec.height = codecSettings->simulcastStream[i].height;

    WebRtc_Word32 cores = 1;
    if (video_codec.width > 640 &&
        numberOfCores > codecSettings->numberOfSimulcastStreams) {
      cores = 2;
    }
    ret_val = encoder_[i]->InitEncode(&video_codec,
                                      cores,
                                      maxPayloadSize);
    if (ret_val != 0) {
      WEBRTC_TRACE(webrtc::kTraceError,
                   webrtc::kTraceVideoCoding,
                   -1,
                   "Failed to initialize VP8 simulcast idx: %d.",
                   i);
      return ret_val;
    }
    if (codecSettings->width != video_codec.width ||
        codecSettings->height != video_codec.height) {
      if (scaler_[i] == NULL) {
        scaler_[i] = new Scaler();
      }
      scaler_[i]->Set(codecSettings->width, codecSettings->height,
                      video_codec.width, video_codec.height,
                      kI420, kI420, kScaleBox);

      if (video_frame_[i]._size <
          (3u * video_codec.width * video_codec.height / 2u)) {
        video_frame_[i]._size = 3 * video_codec.width * video_codec.height / 2;
        delete video_frame_[i]._buffer;
        video_frame_[i]._buffer = new WebRtc_UWord8[video_frame_[i]._size];
        video_frame_[i]._length = 0;
      }
    }
  }
  return ret_val;
}

WebRtc_Word32  VP8SimulcastEncoder::Encode(
    const RawImage& inputImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const VideoFrameType* requestedFrameTypes) {

  WebRtc_Word32 ret_val = -1;
  // we need a local copy since we modify it
  CodecSpecificInfo info = *codecSpecificInfo;

  const int numberOfStreams = video_codec_.numberOfSimulcastStreams;

  for (int i = 0; i < numberOfStreams; i++) {
    if (encode_stream_[i]) {
       video_frame_[i]._timeStamp = inputImage._timeStamp;
    }
    if (requestedFrameTypes[i] == kKeyFrame) {
      // always do a keyframe if asked to
      frame_type_[i] = kKeyFrame;
    } else if (frame_type_[i] == kKeyFrame) {
        // don't write over a previusly requested keyframe
    } else if (frame_type_[i] == kGoldenFrame) {
      if (requestedFrameTypes[i] == kAltRefFrame) {
        // request for both AltRef and Golden upgrade to keyframe
        frame_type_[i] = kKeyFrame;
      }
    } else if (frame_type_[i] == kAltRefFrame) {
      if (requestedFrameTypes[i] == kGoldenFrame) {
        // request for both AltRef and Golden upgrade to keyframe
        frame_type_[i] = kKeyFrame;
      }
    } else if (frame_type_[i] == kDeltaFrame) {
      // if the current is delta set requested
      frame_type_[i] = requestedFrameTypes[i];
    }
  }

  for (int i = 0; i < numberOfStreams; i++) {
    if (encoder_[i] && encode_stream_[i]) {
      // Need the simulcastIdx to keep track of which encoder encoded the frame.
      info.codecSpecific.VP8.simulcastIdx = i;
      VideoFrameType requested_frame_type = frame_type_[i];
      if (scaler_[i]) {
        int video_frame_size = static_cast<int>(video_frame_[i]._size);
        scaler_[i]->Scale(inputImage._buffer,
                          video_frame_[i]._buffer,
                          video_frame_size);
        video_frame_[i]._length = video_frame_[i]._size = video_frame_size;
        ret_val = encoder_[i]->Encode(video_frame_[i],
                                      &info,
                                      &requested_frame_type);
      } else {
        ret_val = encoder_[i]->Encode(inputImage,
                                      &info,
                                      &requested_frame_type);
      }
      if (ret_val < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding, -1,
                     "Encode error:%d on stream:%d", ret_val, i);
        return ret_val;
      }
      frame_type_[i] = kDeltaFrame;
    }
  }
  return ret_val;
}

WebRtc_Word32 VP8SimulcastEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  WebRtc_Word32 ret_val = 0;
  for (int i = 0; i < kMaxSimulcastStreams; i++) {
    if (encoder_[i]) {
      ret_val = encoder_[i]->RegisterEncodeCompleteCallback(callback);
      if (ret_val < 0) {
        WEBRTC_TRACE(webrtc::kTraceError,
                     webrtc::kTraceVideoCoding,
                     -1,
                     "RegisterEncodeCompleteCallback error:%d on stream:%d",
                     ret_val,
                     i);
        return ret_val;
      }
    }
  }
  return ret_val;
}

WebRtc_Word32 VP8SimulcastEncoder::SetChannelParameters(
    WebRtc_UWord32 packetLoss,
    int rtt) {
  WebRtc_Word32 ret_val = 0;
  for (int i = 0; i < kMaxSimulcastStreams; i++) {
    if (encoder_[i]) {
      ret_val = encoder_[i]->SetChannelParameters(packetLoss, rtt);
      if (ret_val < 0) {
        WEBRTC_TRACE(webrtc::kTraceError,
                     webrtc::kTraceVideoCoding,
                     -1,
                     "SetPacketLoss error:%d on stream:%d",
                     ret_val,
                     i);
        return ret_val;
      }
    }
  }
  return ret_val;
}

WebRtc_Word32 VP8SimulcastEncoder::SetRates(WebRtc_UWord32 new_bitrate,
                                            WebRtc_UWord32 frame_rate) {
  WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideoCoding, -1,
               "VP8 simulcast SetRates(%d,%d)", new_bitrate, frame_rate);

  WebRtc_UWord32 bitrate_sum = 0;
  WebRtc_Word32 ret_val = 0;
  for (int i = 0; i < video_codec_.numberOfSimulcastStreams; i++) {
    if (new_bitrate > bitrate_sum) {
      if (!encode_stream_[i]) {
        frame_type_[i] = kKeyFrame;
        encode_stream_[i] = true;
      }
    } else {
      // no more bits
      encode_stream_[i] = false;
      continue;
    }
    WebRtc_UWord32 stream_bitrate = 0;
    bitrate_sum += video_codec_.simulcastStream[i].maxBitrate;
    if (new_bitrate >= bitrate_sum) {
      stream_bitrate = video_codec_.simulcastStream[i].maxBitrate;
    } else {
      stream_bitrate =
          new_bitrate -
          (bitrate_sum -
          video_codec_.simulcastStream[i].maxBitrate);
    }
    ret_val = encoder_[i]->SetRates(stream_bitrate, frame_rate);
    if (ret_val < 0) {
      WEBRTC_TRACE(webrtc::kTraceError,
                   webrtc::kTraceVideoCoding,
                   -1,
                   "VP8 error stream:%d SetRates(%d,%d)",
                   i, stream_bitrate, frame_rate);
    } else {
      WEBRTC_TRACE(webrtc::kTraceStateInfo,
                   webrtc::kTraceVideoCoding,
                   -1,
                   "VP8 stream:%d SetRates(%d,%d)",
                   i, stream_bitrate, frame_rate);
    }
  }
  return ret_val;
}

WebRtc_Word32 VP8SimulcastEncoder::VersionStatic(WebRtc_Word8 *version,
                                                 WebRtc_Word32 length) {
  const WebRtc_Word8* str = "WebM/VP8 simulcast version 1.0.0\n";
  WebRtc_Word32 verLen = (WebRtc_Word32)strlen(str);
  if (verLen > length) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  strncpy(version, str, length);
  return verLen;
}

WebRtc_Word32 VP8SimulcastEncoder::Version(WebRtc_Word8 *version,
                                           WebRtc_Word32 length) const {
  return VersionStatic(version, length);
}
}  // namespace webrtc
