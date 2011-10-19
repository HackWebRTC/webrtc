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
 * tb_I420_codec.h
 *
 * This file contains the interface to I420 "codec"
 * This is a dummy wrapper to allow VCM deal with raw I420 sequences
 *
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_I420_CODEC_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_I420_CODEC_H_

#include "video_codec_interface.h"

class tbI420Encoder: public webrtc::VideoEncoder
{
public:

    tbI420Encoder();

    virtual ~tbI420Encoder();

    static WebRtc_Word32 VersionStatic(WebRtc_Word8* version,
                                       WebRtc_Word32 length);
    virtual WebRtc_Word32  Version(WebRtc_Word8 *version,
                                   WebRtc_Word32 length) const;

    virtual WebRtc_Word32 InitEncode(const webrtc::VideoCodec* codecSettings,
                                     WebRtc_Word32 numberOfCores,
                                     WebRtc_UWord32 maxPayloadSize);

    virtual WebRtc_Word32 Encode(
        const webrtc::RawImage& inputImage,
        const webrtc::CodecSpecificInfo* codecSpecificInfo,
        const webrtc::VideoFrameType* frameType);

    virtual WebRtc_Word32 RegisterEncodeCompleteCallback(
        webrtc::EncodedImageCallback* callback);

    virtual WebRtc_Word32 Release();

    virtual WebRtc_Word32 Reset();

    virtual WebRtc_Word32 SetPacketLoss(WebRtc_UWord32 packetLoss);

    virtual WebRtc_Word32 SetRates(WebRtc_UWord32 newBitRate,
                                   WebRtc_UWord32 frameRate);

    virtual WebRtc_Word32 SetPeriodicKeyFrames(bool enable);

    virtual WebRtc_Word32 CodecConfigParameters(WebRtc_UWord8* /*buffer*/,
                                                WebRtc_Word32 /*size*/);

    struct FunctionCalls
    {
        WebRtc_Word32 InitEncode;
        WebRtc_Word32 Encode;
        WebRtc_Word32 RegisterEncodeCompleteCallback;
        WebRtc_Word32 Release;
        WebRtc_Word32 Reset;
        WebRtc_Word32 SetRates;
        WebRtc_Word32 SetPacketLoss;
        WebRtc_Word32 SetPeriodicKeyFrames;
        WebRtc_Word32 CodecConfigParameters;

    };

    FunctionCalls GetFunctionCalls();
private:
    bool _inited;
    webrtc::EncodedImage _encodedImage;
    FunctionCalls _functionCalls;
    webrtc::EncodedImageCallback* _encodedCompleteCallback;

}; // end of tbI420Encoder class


/***************************/
/* tbI420Decoder class */
/***************************/

class tbI420Decoder: public webrtc::VideoDecoder
{
public:

    tbI420Decoder();

    virtual ~tbI420Decoder();

    virtual WebRtc_Word32 InitDecode(const webrtc::VideoCodec* inst,
                                     WebRtc_Word32 numberOfCores);
    virtual WebRtc_Word32 Decode(
        const webrtc::EncodedImage& inputImage,
        bool missingFrames,
        const webrtc::RTPFragmentationHeader* fragmentation,
        const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
        WebRtc_Word64 renderTimeMs = -1);

    virtual WebRtc_Word32
        RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback);
    virtual WebRtc_Word32 Release();
    virtual WebRtc_Word32 Reset();

    struct FunctionCalls
    {
        WebRtc_Word32 InitDecode;
        WebRtc_Word32 Decode;
        WebRtc_Word32 RegisterDecodeCompleteCallback;
        WebRtc_Word32 Release;
        WebRtc_Word32 Reset;
    };

    FunctionCalls GetFunctionCalls();

private:

    webrtc::RawImage _decodedImage;
    WebRtc_Word32 _width;
    WebRtc_Word32 _height;
    bool _inited;
    FunctionCalls _functionCalls;
    webrtc::DecodedImageCallback* _decodeCompleteCallback;

}; // end of tbI420Decoder class

#endif // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_I420_CODEC_H_
