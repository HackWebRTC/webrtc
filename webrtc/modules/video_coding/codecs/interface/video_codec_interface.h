/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_INTERFACE_VIDEO_CODEC_INTERFACE_H
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_INTERFACE_VIDEO_CODEC_INTERFACE_H

#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/codecs/interface/video_error_codes.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_encoder.h"

namespace webrtc
{

class RTPFragmentationHeader; // forward declaration

// Note: if any pointers are added to this struct, it must be fitted
// with a copy-constructor. See below.
struct CodecSpecificInfoVP8 {
  bool hasReceivedSLI;
  uint8_t pictureIdSLI;
  bool hasReceivedRPSI;
  uint64_t pictureIdRPSI;
  int16_t pictureId;  // Negative value to skip pictureId.
  bool nonReference;
  uint8_t simulcastIdx;
  uint8_t temporalIdx;
  bool layerSync;
  int tl0PicIdx;  // Negative value to skip tl0PicIdx.
  int8_t keyIdx;  // Negative value to skip keyIdx.
};

struct CodecSpecificInfoGeneric {
  uint8_t simulcast_idx;
};

struct CodecSpecificInfoH264 {};

union CodecSpecificInfoUnion {
  CodecSpecificInfoGeneric generic;
  CodecSpecificInfoVP8 VP8;
  CodecSpecificInfoH264 H264;
};

// Note: if any pointers are added to this struct or its sub-structs, it
// must be fitted with a copy-constructor. This is because it is copied
// in the copy-constructor of VCMEncodedFrame.
struct CodecSpecificInfo
{
    VideoCodecType   codecType;
    CodecSpecificInfoUnion codecSpecific;
};

class DecodedImageCallback
{
public:
    virtual ~DecodedImageCallback() {};

    // Callback function which is called when an image has been decoded.
    //
    // Input:
    //          - decodedImage         : The decoded image.
    //
    // Return value                    : 0 if OK, < 0 otherwise.
    virtual int32_t Decoded(I420VideoFrame& decodedImage) = 0;

    virtual int32_t ReceivedDecodedReferenceFrame(const uint64_t pictureId) {return -1;}

    virtual int32_t ReceivedDecodedFrame(const uint64_t pictureId) {return -1;}
};

class VideoDecoder
{
public:
    virtual ~VideoDecoder() {};

    // Initialize the decoder with the information from the VideoCodec.
    //
    // Input:
    //          - inst              : Codec settings
    //          - numberOfCores     : Number of cores available for the decoder
    //
    // Return value                 : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual int32_t InitDecode(const VideoCodec* codecSettings, int32_t numberOfCores) = 0;

    // Decode encoded image (as a part of a video stream). The decoded image
    // will be returned to the user through the decode complete callback.
    //
    // Input:
    //          - inputImage        : Encoded image to be decoded
    //          - missingFrames     : True if one or more frames have been lost
    //                                since the previous decode call.
    //          - fragmentation     : Specifies where the encoded frame can be
    //                                split into separate fragments. The meaning
    //                                of fragment is codec specific, but often
    //                                means that each fragment is decodable by
    //                                itself.
    //          - codecSpecificInfo : Pointer to codec specific data
    //          - renderTimeMs      : System time to render in milliseconds. Only
    //                                used by decoders with internal rendering.
    //
    // Return value                 : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual int32_t
    Decode(const EncodedImage& inputImage,
           bool missingFrames,
           const RTPFragmentationHeader* fragmentation,
           const CodecSpecificInfo* codecSpecificInfo = NULL,
           int64_t renderTimeMs = -1) = 0;

    // Register an decode complete callback object.
    //
    // Input:
    //          - callback         : Callback object which handles decoded images.
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual int32_t RegisterDecodeCompleteCallback(DecodedImageCallback* callback) = 0;

    // Free decoder memory.
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual int32_t Release() = 0;

    // Reset decoder state and prepare for a new call.
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual int32_t Reset() = 0;

    // Codec configuration data sent out-of-band, i.e. in SIP call setup
    //
    // Input/Output:
    //          - buffer           : Buffer pointer to the configuration data
    //          - size             : The size of the configuration data in
    //                               bytes
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual int32_t SetCodecConfigParameters(const uint8_t* /*buffer*/, int32_t /*size*/) { return WEBRTC_VIDEO_CODEC_ERROR; }

    // Create a copy of the codec and its internal state.
    //
    // Return value                : A copy of the instance if OK, NULL otherwise.
    virtual VideoDecoder* Copy() { return NULL; }
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_INTERFACE_VIDEO_CODEC_INTERFACE_H
