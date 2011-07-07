/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ACM_PCM16B_H
#define ACM_PCM16B_H

#include "acm_generic_codec.h"

namespace webrtc
{

class ACMPCM16B : public ACMGenericCodec
{
public:
    ACMPCM16B(WebRtc_Word16 codecID);
    ~ACMPCM16B();
    // for FEC
    ACMGenericCodec* CreateInstance(void);

    WebRtc_Word16 InternalEncode(
        WebRtc_UWord8* bitstream,
        WebRtc_Word16* bitStreamLenByte);

    WebRtc_Word16 InternalInitEncoder(
        WebRtcACMCodecParams *codecParams);

    WebRtc_Word16 InternalInitDecoder(
        WebRtcACMCodecParams *codecParams);

protected:
    WebRtc_Word16 DecodeSafe(
        WebRtc_UWord8* bitStream,
        WebRtc_Word16  bitStreamLenByte, 
        WebRtc_Word16* audio, 
        WebRtc_Word16* audioSamples, 
        WebRtc_Word8*  speechType);

    WebRtc_Word32 CodecDef(
        WebRtcNetEQ_CodecDef& codecDef, 
        const CodecInst&      codecInst);

    void DestructEncoderSafe();
    
    void DestructDecoderSafe();
    
    WebRtc_Word16 InternalCreateEncoder();
    
    WebRtc_Word16 InternalCreateDecoder();
    
    WebRtc_Word16 UnregisterFromNetEqSafe(
        ACMNetEQ*       netEq,
        WebRtc_Word16   payloadType);

    void InternalDestructEncoderInst(
        void* ptrInst);

    WebRtc_Word32 _samplingFreqHz;
};

} // namespace webrtc

#endif //ACM_PCM16B_H

