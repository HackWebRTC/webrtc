/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ACM_CODEC_DATABASE_H
#define ACM_CODEC_DATABASE_H

#include "acm_generic_codec.h"
#include "common_types.h"
#include "typedefs.h"
#include "webrtc_neteq.h"

namespace webrtc
{

// These might need to be increased if adding a new codec to
// the database
#define MAX_NR_OF_CODECS      52
#define MAX_NR_OF_PACSIZES     6
#define VERSION_SIZE        1000

class ACMCodecDB
{
public:
    static WebRtc_Word16 Codec(
        WebRtc_Word16 listnr,
        CodecInst*    codec_inst);

    
    static WebRtc_Word16 CodecNumber(
        const CodecInst* codec_inst,
        WebRtc_Word16&   mirrorID,
        WebRtc_Word8*    errMessage,
        WebRtc_Word16    maxErrMsgLenByte);

    static WebRtc_Word16 CodecNumber(
        const CodecInst* codec_inst,
        WebRtc_Word16&   mirrorID);

    static WebRtc_Word16 ReceiverCodecNumber(
        const CodecInst& codecInst,
        WebRtc_Word16&   mirrorID);

    static WebRtc_Word16 NoOfCodecs();
    
    static WebRtc_Word16 NoNetEqDecoders();
    
    static WebRtc_Word32 CodecFreq(
        WebRtc_Word16 listnr);

    static WebRtc_Word16 BasicCodingBlock(
        WebRtc_Word16 listnr);

    static enum WebRtcNetEQDecoder* NetEqDecoders();

    static WebRtc_Word16 CodecsVersion(
        WebRtc_Word8*   version,
        WebRtc_UWord32& remainingBufferInBytes,
        WebRtc_UWord32& position);

    static WebRtc_Word16 MirrorID(
        const WebRtc_Word16 codecID);

    static ACMGenericCodec* CreateCodecInstance(
        const CodecInst* codecInst);

    static void initACMCodecDB();

    static bool IsRateValid(
        const WebRtc_Word16 listNr,
        const WebRtc_Word32 rate);

   static bool IsISACRateValid(
        const WebRtc_Word32 rate);

    static bool IsILBCRateValid(
        const WebRtc_Word32 rate,
        const WebRtc_Word16 frameSizeSamples);

    static WebRtc_Word16 ValidPayloadType(
        const int payloadType);

    static WebRtc_Word16
        pcm16b,
        pcm16bwb,
        pcm16bswb32, 
        pcm16bswb48,
        pcmu,
        pcma,
        ilbc,
        gsmAMR,
        gsmAMRWB,
        g722,
        g722_1_32,
        g722_1_24,
        g722_1_16,
        g722_1C_48,
        g722_1C_32,
        g722_1C_24,
        g729,
        isac,
        isacswb,
        gsmfr,
        speex8,
        speex16,
        cnNB,
        cnWB,
        cnSWB,
        avt,
        red;

    static WebRtc_Word16  _noOfCodecs;
    static WebRtc_Word16  _noNetEqDecoders;
    static WebRtc_Word16  _noPayloads;

    // Information about the supported codecs
    static CodecInst      _mycodecs[MAX_NR_OF_CODECS];
    static enum WebRtcNetEQDecoder _netEqDecoders[MAX_NR_OF_CODECS];
    static WebRtc_UWord16 _allowedPacSizesSmpl[MAX_NR_OF_CODECS][MAX_NR_OF_PACSIZES];
    static WebRtc_UWord8  _nrOfAllowedPacSizes[MAX_NR_OF_CODECS];
    static WebRtc_UWord16 _basicCodingBlockSmpl[MAX_NR_OF_CODECS];
    static WebRtc_UWord16 _channelSupport[MAX_NR_OF_CODECS];

private:
    static bool           _isInitiated;
    static WebRtc_Word8   _versions[VERSION_SIZE];
    static WebRtc_UWord32 _versionStringSize;
};

} // namespace webrtc

#endif //ACM_CODEC_DATABASE_H
