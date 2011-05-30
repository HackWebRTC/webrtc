/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "acm_pcma.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

// Codec interface
#include "g711_interface.h"

namespace webrtc
{

ACMPCMA::ACMPCMA(WebRtc_Word16 codecID)
{
    _codecID = codecID;
}


ACMPCMA::~ACMPCMA()
{
    return;
}


WebRtc_Word16 
ACMPCMA::InternalEncode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte)
{
    *bitStreamLenByte = WebRtcG711_EncodeA(NULL, &_inAudio[_inAudioIxRead],
        _frameLenSmpl*_noChannels, (WebRtc_Word16*)bitStream);
    // increment the read index this tell the caller that how far 
    // we have gone forward in reading the audio buffer
    _inAudioIxRead += _frameLenSmpl*_noChannels;
    return *bitStreamLenByte;
}


WebRtc_Word16 
ACMPCMA::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */, 
    WebRtc_Word16* /* audio            */, 
    WebRtc_Word16* /* audioSamples     */, 
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}


WebRtc_Word16 
ACMPCMA::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    // This codec does not need initialization,
    // PCM has no instance
    return 0;    
}


WebRtc_Word16 
ACMPCMA::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    // This codec does not need initialization,
    // PCM has no instance
    return 0;
}


WebRtc_Word32 ACMPCMA::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&  codecInst)
{
    // Fill up the structure by calling 
    // "SET_CODEC_PAR" & "SET_PCMA_FUNCTION."
    // Then call NetEQ to add the codec to it's
    // database.
    SET_CODEC_PAR((codecDef), kDecoderPCMa, codecInst.pltype, NULL, 8000);
    SET_PCMA_FUNCTIONS((codecDef));
    return 0;
}


ACMGenericCodec*
ACMPCMA::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMPCMA::InternalCreateEncoder()
{
    // PCM has no instance
    return 0;
}


WebRtc_Word16
ACMPCMA::InternalCreateDecoder()
{
    // PCM has no instance
    return 0;
}


void 
ACMPCMA::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    // PCM has no instance
    return;
}


void 
ACMPCMA::DestructEncoderSafe()
{
    // PCM has no instance
    return;
}


void 
ACMPCMA::DestructDecoderSafe()
{
    // PCM has no instance
    _decoderInitialized = false;
    _decoderExist = false;
    return;
}


WebRtc_Word16 
ACMPCMA::UnregisterFromNetEqSafe(
    ACMNetEQ*     netEq,
    WebRtc_Word16 payloadType)
{
    if(payloadType != _decoderParams.codecInstant.pltype)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID, 
            "Cannot unregister codec %s given payload-type %d does not match \
the stored payload type", 
            _decoderParams.codecInstant.plname, 
            payloadType, 
            _decoderParams.codecInstant.pltype);
        return -1;
    }

    return netEq->RemoveCodec(kDecoderPCMa);
}

} // namespace webrtc
