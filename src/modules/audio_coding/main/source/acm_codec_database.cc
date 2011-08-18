/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "trace.h"

// Includes needed to get version info
// and to create the codecs
#include "acm_pcma.h"
#include "acm_pcmu.h"
#include "g711_interface.h"
#include "webrtc_neteq.h"
#include "webrtc_cng.h"
#include "acm_cng.h"
#ifdef WEBRTC_CODEC_AVT
    #include "acm_dtmf_playout.h"
#endif
#ifdef WEBRTC_CODEC_RED
    #include "acm_red.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
    #include "acm_ilbc.h"
    #include "ilbc.h"
#endif
#ifdef WEBRTC_CODEC_ISAC
    #include "acm_isac.h"
    #include "acm_isac_macros.h"
    #include "isac.h"
#endif
#ifdef WEBRTC_CODEC_ISACFX
    #include "acm_isac.h"
    #include "acm_isac_macros.h"
    #include "isacfix.h"
#endif
#ifdef WEBRTC_CODEC_PCM16
    #include "pcm16b.h"
    #include "acm_pcm16b.h"
#endif
#ifdef WEBRTC_CODEC_G722
    #include "acm_g722.h"
    #include "g722_interface.h"
#endif

namespace webrtc
{

#define TEMPORARY_BUFFER_SIZE 500

bool ACMCodecDB::_isInitiated = false;
WebRtc_Word16 ACMCodecDB::_noOfCodecs   = 0;
WebRtc_Word16 ACMCodecDB::_noNetEqDecoders   = 0;
WebRtc_Word16 ACMCodecDB::_noPayloads   = 0;

WebRtc_Word16 ACMCodecDB::isac        = -1;
WebRtc_Word16 ACMCodecDB::isacswb     = -1;
WebRtc_Word16 ACMCodecDB::pcm16b      = -1;
WebRtc_Word16 ACMCodecDB::pcm16bwb    = -1;
WebRtc_Word16 ACMCodecDB::pcm16bswb32 = -1;
WebRtc_Word16 ACMCodecDB::pcm16bswb48 = -1;
WebRtc_Word16 ACMCodecDB::pcmu        = -1;
WebRtc_Word16 ACMCodecDB::pcma        = -1;
WebRtc_Word16 ACMCodecDB::ilbc        = -1;
WebRtc_Word16 ACMCodecDB::gsmAMR      = -1;
WebRtc_Word16 ACMCodecDB::gsmAMRWB    = -1;
WebRtc_Word16 ACMCodecDB::g722        = -1;
WebRtc_Word16 ACMCodecDB::g722_1_32   = -1;
WebRtc_Word16 ACMCodecDB::g722_1_24   = -1;
WebRtc_Word16 ACMCodecDB::g722_1_16   = -1;
WebRtc_Word16 ACMCodecDB::g722_1C_48  = -1;
WebRtc_Word16 ACMCodecDB::g722_1C_32  = -1;
WebRtc_Word16 ACMCodecDB::g722_1C_24  = -1;
WebRtc_Word16 ACMCodecDB::g729        = -1;
WebRtc_Word16 ACMCodecDB::gsmfr       = -1;
WebRtc_Word16 ACMCodecDB::speex8      = -1;
WebRtc_Word16 ACMCodecDB::speex16     = -1;
WebRtc_Word16 ACMCodecDB::cnNB        = -1;
WebRtc_Word16 ACMCodecDB::cnWB        = -1;
WebRtc_Word16 ACMCodecDB::cnSWB       = -1;
WebRtc_Word16 ACMCodecDB::avt         = -1;
WebRtc_Word16 ACMCodecDB::red         = -1;

WebRtc_UWord8      ACMCodecDB::_nrOfAllowedPacSizes[MAX_NR_OF_CODECS];
WebRtc_UWord16     ACMCodecDB::_allowedPacSizesSmpl[MAX_NR_OF_CODECS][MAX_NR_OF_PACSIZES];
CodecInst          ACMCodecDB::_mycodecs[MAX_NR_OF_CODECS];
enum WebRtcNetEQDecoder ACMCodecDB::_netEqDecoders[MAX_NR_OF_CODECS];
WebRtc_Word8       ACMCodecDB::_versions[VERSION_SIZE];
WebRtc_UWord16     ACMCodecDB::_basicCodingBlockSmpl[MAX_NR_OF_CODECS];
WebRtc_UWord16     ACMCodecDB::_channelSupport[MAX_NR_OF_CODECS];
WebRtc_UWord32     ACMCodecDB::_versionStringSize = 0;

// We dynamically allocate some of the dynamic payload types to the defined codecs
// Note! There are a limited number of payload types. If more codecs are defined
// they will receive reserved fixed payload types (values 65-95).
static int kDynamicPayloadtypes[MAX_NR_OF_CODECS] = {
    105, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126,  95,  94,  93,  92,  91,  90,  89,
     88,  87,  86,  85,  84,  83,  82,  81,  80,  79,  78,  77,  76,  75,
     74,  73,  72,  71,  70,  69,  68,  67,  66,  65
};

WebRtc_Word16
ACMCodecDB::Codec(
    WebRtc_Word16 listnr,
    CodecInst*    codec_inst)
{
    // Error check to se that listnr is between 0 and (_noOfCodecs - 1)
    if ((listnr < 0) || (listnr >= _noOfCodecs))
    {
        return -1;
    }

    memcpy(codec_inst,&_mycodecs[listnr],sizeof(CodecInst));
    return 0;
}


WebRtc_Word16
ACMCodecDB::CodecNumber(
    const CodecInst* codecInst,
    WebRtc_Word16&   mirrorID,
    WebRtc_Word8*    errMessage,
    WebRtc_Word16    maxErrMsgLenByte)
{
    WebRtc_Word16 codecID = ACMCodecDB::CodecNumber(codecInst, mirrorID);
    if((codecID < 0) && (errMessage != NULL))
    {
        WebRtc_Word8 myErrMsg[1000];
        if (codecID == -10)
        {
            // Codec not supported
            sprintf(myErrMsg,
                    "Call to ACMCodecDB::CodecNumber failed, plname=%s is \
not a valid codec", codecInst->plname);
        }
        else if (codecID == -20)
        {
            // Sampling frequency doesn't match codec
            sprintf(myErrMsg,
                    "Call to ACMCodecDB::CodecNumber failed, plfreq=%d is \
not a valid frequency for the codec %s", codecInst->plfreq, codecInst->plname);
        }
        else if (codecID == -30)
        {
            // Wrong payload type for Comfort Noise
            sprintf(myErrMsg,
                    "Call to ACMCodecDB::CodecNumber failed, payload \
number %d is out of range for %s", codecInst->pltype, codecInst->plname);
        }
        else if (codecID == -40)
        {
            // Wrong payload type for RED
            sprintf(myErrMsg,
                    "Call to ACMCodecDB::CodecNumber failed, payload \
number %d is out of range for %s", codecInst->pltype, codecInst->plname);
        }
        else if (codecID == -50)
        {
            // Packet size is out of range for the codec
            sprintf(myErrMsg,
                    "Call to ACMCodecDB::CodecNumber failed, Packet \
size is out of range for %s", codecInst->plname);
        }
        else if (codecID == -60)
        {
            // Not a valid rate for the codec
           sprintf(myErrMsg,
                    "Call to ACMCodecDB::CodecNumber failed, rate=%d \
is not a valid rate for %s", codecInst->rate, codecInst->plname);
        }
        else
        {
            // Other error
            sprintf(myErrMsg,
                    "invalid codec parameters to be registered, \
ACMCodecDB::CodecNumber failed");
        }
        strncpy(errMessage, myErrMsg, maxErrMsgLenByte - 1);
        // make sure that the massage is null-terminated.
        errMessage[maxErrMsgLenByte - 1] = '\0';
    }
    return codecID;
}


WebRtc_Word16
ACMCodecDB::CodecNumber(
    const CodecInst* codec_inst,
    WebRtc_Word16&   mirrorID)
{
    WebRtc_Word16 codecNumber = -1;
    WebRtc_Word16 nameMatch = 0;

    // Find a matching payload name and frequency in the codec list
    // Need to check both since some codecs have several codec entries with
    // Different frequencies (like iSAC)
    for(WebRtc_Word16 i = 0; i < _noOfCodecs; i++)
    {
        if(STR_CASE_CMP(_mycodecs[i].plname, codec_inst->plname) == 0)
        {
            //We have found a matching codec name in the list
            nameMatch = 1;

            // Check if frequncy match
            if (codec_inst->plfreq == _mycodecs[i].plfreq)
            {
                codecNumber = i;
                break;
            }
        }
    }

    if(codecNumber == -1)
    {
        if (!nameMatch) {
            // Codec name doesn't match any codec in the list
            return -10;
        } else {
            // Error in payload frequency, doesn't match codec
            return -20;
        }
    }

    // Check the validity of payload type
    if(ValidPayloadType(codec_inst->pltype) < 0)
    {
        // Payload type out of range
        return -40;
    }

    // Comfort Noise is special case, packet-size & rate is not checked
    if(STR_CASE_CMP(_mycodecs[codecNumber].plname, "CN") == 0)
    {
        mirrorID = codecNumber;
        return codecNumber;
    }

    // RED is special case, packet-size & rate is not checked
    if(STR_CASE_CMP(_mycodecs[codecNumber].plname, "red") == 0)
    {
        mirrorID = codecNumber;
        return codecNumber;
    }

    // Check the validity of packet size
    if(_nrOfAllowedPacSizes[codecNumber] > 0)
    {
        // Check that the new packet size is in the valid range.
        bool pacSizeOK = false;
        for(WebRtc_Word32 i=0;i< _nrOfAllowedPacSizes[codecNumber];i++)
        {
            if(codec_inst->pacsize == _allowedPacSizesSmpl[codecNumber][i])
            {
                pacSizeOK = true;
                break;
            }
        }
        if(!pacSizeOK)
        {
            // Packet size is out of range
            return -50;
        }
    }
    if( codec_inst->pacsize < 1)
    {
        // Packet size is out of range
        return -50;
    }

    mirrorID = codecNumber;

    // Check the validity of rate
    if(STR_CASE_CMP("isac", codec_inst->plname) == 0)
    {
        if(IsISACRateValid(codec_inst->rate))
        {
            // Set mirrorID to iSAC WB which is only created
            // once to be used both for iSAC WB and SWB, because
            // they need to share struct
            mirrorID = ACMCodecDB::isac;
            return  codecNumber;
        }
        else
        {
            // Not a valid rate
            return -60;
        }
    }
    else if(STR_CASE_CMP("ilbc", codec_inst->plname) == 0)
    {
        return IsILBCRateValid(codec_inst->rate, codec_inst->pacsize) ? codecNumber : -60;
    }

    return IsRateValid(codecNumber, codec_inst->rate) ? codecNumber : -60;
}


WebRtc_Word16
ACMCodecDB::ReceiverCodecNumber(
    const CodecInst& codecInst,
    WebRtc_Word16&   mirrorID)
{
    WebRtc_Word16 codecNumber = -1;
    WebRtc_Word16 nameMatch = 0;

    // Find a matching payload name and frequency in the codec list
    // Need to check both since some codecs have several codec entries with
    // Different frequencies (like iSAC)
    for(WebRtc_Word16 i = 0; i < _noOfCodecs; i++)
    {
        if(STR_CASE_CMP(_mycodecs[i].plname, codecInst.plname) == 0)
        {
            //We have found a matching codec name in the list
            nameMatch = 1;

            // Check if frequency match
            if (codecInst.plfreq == _mycodecs[i].plfreq)
            {
                codecNumber = i;
                mirrorID = codecNumber;

                // Check if codec is iSAC, set mirrorID to iSAC WB
                // which is only created once to be used both for
                // iSAC WB and SWB, because they need to share struct
                if(STR_CASE_CMP(codecInst.plname, "ISAC") == 0)
                {
                    mirrorID = ACMCodecDB::isac;
                }
                break;
            }
        }
    }

    if(codecNumber == -1)
    {
        return codecNumber;
    }

    return codecNumber;
}


// Return number of codecs in the database
WebRtc_Word16
ACMCodecDB::NoOfCodecs()
{
    return _noOfCodecs;
}


// Return number of NetEQ decoders in the database.
// Note that the number is huigher than _moOfCodecs because some payload names
// are treated as different decoders in NetEQ, like iSAC wb and swb.
WebRtc_Word16
ACMCodecDB::NoNetEqDecoders()
{
    return _noNetEqDecoders;
}


// Return the codec sampling frequency for code number "listnr" in database
WebRtc_Word32
ACMCodecDB::CodecFreq(WebRtc_Word16 listnr)
{
    // Error check to se that listnr is between 0 and (_noOfCodecs - 1)
    if ( listnr < 0 || listnr >= _noOfCodecs)
        return -1;

    return _mycodecs[listnr].plfreq;
}

// Return the codecs basic coding block size in samples
WebRtc_Word16
ACMCodecDB::BasicCodingBlock(
    WebRtc_Word16 listNr)
{
    // Error check to se that listnr is between 0 and (_noOfCodecs - 1)
    if ( listNr < 0 || listNr >= _noOfCodecs)
        return -1;

    return _basicCodingBlockSmpl[listNr];
}

// Return the NetEQ decoder database
enum WebRtcNetEQDecoder*
ACMCodecDB::NetEqDecoders()
{
    return _netEqDecoders;
}

// All version numbers for the codecs in the data base are listed in text.
WebRtc_Word16
ACMCodecDB::CodecsVersion(
    WebRtc_Word8*   version,
    WebRtc_UWord32& remainingBufferInBytes,
    WebRtc_UWord32& position)
{
    WebRtc_UWord32 len = position;
    strncpy(&version[len], _versions, remainingBufferInBytes);
    position = (WebRtc_UWord32)strlen(version);
    remainingBufferInBytes -= (position - len);
    if(remainingBufferInBytes < _versionStringSize)
    {
        return -1;
    }
    return 0;
}

// Get mirror id. The Id is used for codecs sharing struct for settings that
// need different payload types.
WebRtc_Word16
ACMCodecDB::MirrorID(
    const WebRtc_Word16 codecID)
{
    if(STR_CASE_CMP(_mycodecs[codecID].plname, "isac") == 0)
    {
        return ACMCodecDB::isac;
    }
    else
    {
        return codecID;
    }
}



ACMGenericCodec*
ACMCodecDB::CreateCodecInstance(
    const CodecInst* codecInst)
{
    // All we have support for right now
    if(!STR_CASE_CMP(codecInst->plname, "ISAC"))
    {
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
        return new ACMISAC(isac);
#endif
    }
    else if(!STR_CASE_CMP(codecInst->plname, "PCMU"))
    {
        return new ACMPCMU(pcmu);
    }
    else if(!STR_CASE_CMP(codecInst->plname, "PCMA"))
    {
        return new ACMPCMA(pcma);
    }
    else if(!STR_CASE_CMP(codecInst->plname, "ILBC"))
    {
#ifdef WEBRTC_CODEC_ILBC
        return new ACMILBC(ilbc);
#endif
    }
    else if(!STR_CASE_CMP(codecInst->plname, "G722"))
    {
#ifdef WEBRTC_CODEC_G722
        return new ACMG722(g722);
#endif
    }
    else if(!STR_CASE_CMP(codecInst->plname, "CN"))
    {
        // We need to check sampling frequency
        // know what codec to create.
        WebRtc_Word16 codecID;
        switch(codecInst->plfreq)
        {
        case 8000:
            {
                codecID = ACMCodecDB::cnNB;
                break;
            }
        case 16000:
            {
                codecID = ACMCodecDB::cnWB;
                break;
            }
        case 32000:
            {
                codecID = ACMCodecDB::cnSWB;
                break;
            }
        default:
            return NULL;
        }
        return new ACMCNG(codecID);
    }
    else if(!STR_CASE_CMP(codecInst->plname, "L16"))
    {
#ifdef WEBRTC_CODEC_PCM16
        // For this codec we need to check sampling frequency
        // to know what codec to create.
        WebRtc_Word16 codecID;
        switch(codecInst->plfreq)
        {
        case 8000:
            {
                codecID = ACMCodecDB::pcm16b;
                break;
            }
        case 16000:
            {
                codecID = ACMCodecDB::pcm16bwb;
                break;
            }
        case 32000:
            {
                codecID = ACMCodecDB::pcm16bswb32;
                break;
            }
        default:
            return NULL;
        }
        return new ACMPCM16B(codecID);
#endif
    }
    else if(!STR_CASE_CMP(codecInst->plname, "telephone-event"))
    {
#ifdef WEBRTC_CODEC_AVT
        return new ACMDTMFPlayout(avt);
#endif
    }
    else if(!STR_CASE_CMP(codecInst->plname, "red"))
    {
#ifdef WEBRTC_CODEC_RED
        return new ACMRED(red);
#endif
    }
    return NULL;
}


//Here we build the complete database "_mycodecs" of our codecs
void
ACMCodecDB::initACMCodecDB()
{
    if(_isInitiated)
    {
        return;
    }
    else
    {
        _isInitiated = true;
    }
    WebRtc_Word8 versionNrBuff[TEMPORARY_BUFFER_SIZE];
    WebRtc_Word32 remainingSize = VERSION_SIZE;

    _versions[0] = '\0';

    // Init the stereo settings vector
    for (int i=0; i<MAX_NR_OF_CODECS; i++)
    {
        _channelSupport[i] = 1;
    }

#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
    strcpy(_mycodecs[_noOfCodecs].plname,"ISAC");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = ISACWB_DEFAULT_RATE; // Default rate
    _mycodecs[_noOfCodecs].plfreq   = 16000;
    _mycodecs[_noOfCodecs].pltype   = 103;
    _mycodecs[_noOfCodecs].pacsize  = ISACWB_PAC_SIZE; // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 2;
    _allowedPacSizesSmpl[_noOfCodecs][0] = ISACWB_PAC_SIZE;   // 480 sampels equals 30 ms
    _allowedPacSizesSmpl[_noOfCodecs][1] = ISACWB_PAC_SIZE*2;   // 960 sampels equals 60 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 0;

    isac=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderISAC;
    _noOfCodecs++;

    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    ACM_ISAC_VERSION(versionNrBuff);
    strncat(_versions, "ISAC\t\t", remainingSize);
    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, versionNrBuff, remainingSize);
    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, "\n", remainingSize);
#   if (defined(WEBRTC_CODEC_ISAC))
        strcpy(_mycodecs[_noOfCodecs].plname,"ISAC");
        _mycodecs[_noOfCodecs].channels = 1;
        _mycodecs[_noOfCodecs].rate     = ISACSWB_DEFAULT_RATE; // Default rate
        _mycodecs[_noOfCodecs].plfreq   = 32000;
        _mycodecs[_noOfCodecs].pltype   = 104;
        _mycodecs[_noOfCodecs].pacsize  = ISACSWB_PAC_SIZE; // Default packet size

        _nrOfAllowedPacSizes[_noOfCodecs]    = 1;
        _allowedPacSizesSmpl[_noOfCodecs][0] = ISACSWB_PAC_SIZE;   // 960 sampels equals 60 ms
        _basicCodingBlockSmpl[_noOfCodecs]   = 0;

        isacswb = _noOfCodecs;
        _netEqDecoders[_noNetEqDecoders++] = kDecoderISACswb;
        _noOfCodecs++;
#   endif
#endif
#ifdef WEBRTC_CODEC_PCM16
    strcpy(_mycodecs[_noOfCodecs].plname,"L16");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 128000;
    _mycodecs[_noOfCodecs].plfreq   = 8000;
    _mycodecs[_noOfCodecs].pltype   = kDynamicPayloadtypes[_noPayloads++];
    _mycodecs[_noOfCodecs].pacsize  = 80; // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 4;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 80;  //  80 sampels equals 10 ms
    _allowedPacSizesSmpl[_noOfCodecs][1] = 160; // 160 sampels equals 20 ms
    _allowedPacSizesSmpl[_noOfCodecs][2] = 240; // 240 sampels equals 30 ms
    _allowedPacSizesSmpl[_noOfCodecs][3] = 320; // 320 sampels equals 40 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 0;
    _channelSupport[_noOfCodecs] = 2;

    pcm16b=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderPCM16B;
    _noOfCodecs++;


    strcpy(_mycodecs[_noOfCodecs].plname,"L16");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 256000;
    _mycodecs[_noOfCodecs].plfreq   = 16000;
    _mycodecs[_noOfCodecs].pltype   = kDynamicPayloadtypes[_noPayloads++];
    _mycodecs[_noOfCodecs].pacsize  = 160; // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 4;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 160; // 160 sampels equals 10 ms
    _allowedPacSizesSmpl[_noOfCodecs][1] = 320; // 320 sampels equals 20 ms
    _allowedPacSizesSmpl[_noOfCodecs][2] = 480; // 480 sampels equals 30 ms
    _allowedPacSizesSmpl[_noOfCodecs][3] = 640; // 640 sampels equals 40 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 0;
    _channelSupport[_noOfCodecs] = 2;

    pcm16bwb=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderPCM16Bwb;
    _noOfCodecs++;

    strcpy(_mycodecs[_noOfCodecs].plname,"L16");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 512000;
    _mycodecs[_noOfCodecs].plfreq   = 32000;
    _mycodecs[_noOfCodecs].pltype   = kDynamicPayloadtypes[_noPayloads++];
    _mycodecs[_noOfCodecs].pacsize  = 320; // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 2;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 320;  //  320 sampels equals 10 ms
    _allowedPacSizesSmpl[_noOfCodecs][1] = 640;  //  640 sampels equals 20 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 0;
    _channelSupport[_noOfCodecs] = 2;

    pcm16bswb32=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderPCM16Bswb32kHz;
    _noOfCodecs++;

    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, "L16\t\t1.0.0\n", remainingSize);
#endif
    strcpy(_mycodecs[_noOfCodecs].plname,"PCMU");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 64000;
    _mycodecs[_noOfCodecs].plfreq   = 8000;
    _mycodecs[_noOfCodecs].pltype   = 0;
    _mycodecs[_noOfCodecs].pacsize  = 160; // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 6;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 80;  // 80  sampels equals 10 ms
    _allowedPacSizesSmpl[_noOfCodecs][1] = 160; // 160 sampels equals 20 ms
    _allowedPacSizesSmpl[_noOfCodecs][2] = 240; // 240 sampels equals 30 ms
    _allowedPacSizesSmpl[_noOfCodecs][3] = 320; // 320 sampels equals 40 ms
    _allowedPacSizesSmpl[_noOfCodecs][4] = 400; // 400 sampels equals 50 ms
    _allowedPacSizesSmpl[_noOfCodecs][5] = 480; // 480 sampels equals 60 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 0;   /* 0 indicates all allowed
                                                packetsizes can be used as
                                                basic coding block   */
    _channelSupport[_noOfCodecs] = 2;

    pcmu=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderPCMu;
    _noOfCodecs++;

    strcpy(_mycodecs[_noOfCodecs].plname,"PCMA");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 64000;
    _mycodecs[_noOfCodecs].plfreq   = 8000;
    _mycodecs[_noOfCodecs].pltype   = 8;
    _mycodecs[_noOfCodecs].pacsize  = 160; // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 6;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 80;  // 80  sampels equals 10 ms
    _allowedPacSizesSmpl[_noOfCodecs][1] = 160; // 160 sampels equals 20 ms
    _allowedPacSizesSmpl[_noOfCodecs][2] = 240; // 240 sampels equals 30 ms
    _allowedPacSizesSmpl[_noOfCodecs][3] = 320; // 320 sampels equals 40 ms
    _allowedPacSizesSmpl[_noOfCodecs][4] = 400; // 400 sampels equals 50 ms
    _allowedPacSizesSmpl[_noOfCodecs][5] = 480; // 480 sampels equals 60 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 0;   /* 0 indicates all allowed
                                                 packetsizes can be used as
                                                 basic coding block       */
    _channelSupport[_noOfCodecs] = 2;

    pcma=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderPCMa;
    _noOfCodecs++;

    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    WebRtcG711_Version(versionNrBuff, TEMPORARY_BUFFER_SIZE);
    strncat(_versions, "G.711\t\t", remainingSize);
    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, versionNrBuff, remainingSize);
    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, "\n", remainingSize);

#ifdef WEBRTC_CODEC_ILBC
    strcpy(_mycodecs[_noOfCodecs].plname,"iLBC");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 13300;
    _mycodecs[_noOfCodecs].plfreq   = 8000;
    _mycodecs[_noOfCodecs].pltype   = 102;
    _mycodecs[_noOfCodecs].pacsize  = 240; // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 4;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 160; // 160 sampels equals 20 ms
    _allowedPacSizesSmpl[_noOfCodecs][1] = 240; // 240 sampels equals 30 ms
    _allowedPacSizesSmpl[_noOfCodecs][2] = 320; // 320 sampels equals 40 ms
    _allowedPacSizesSmpl[_noOfCodecs][3] = 480; // 480 sampels equals 60 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 0;   /* 0 indicates all allowed
                                                 packetsizes can be used as
                                                 basic coding block       */

    ilbc=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderILBC;
    _noOfCodecs++;

    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    WebRtcIlbcfix_version(versionNrBuff);
    strncat(_versions, "ILBC\t\t", remainingSize);
    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, versionNrBuff, remainingSize);
    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, "\n", remainingSize);
#endif
#ifdef WEBRTC_CODEC_G722
    strcpy(_mycodecs[_noOfCodecs].plname,"G722");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 64000;
    _mycodecs[_noOfCodecs].plfreq   = 16000;
    _mycodecs[_noOfCodecs].pltype   = 9;
    _mycodecs[_noOfCodecs].pacsize  = 320; // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 6;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 160; // 160 sampels equals 10 ms
    _allowedPacSizesSmpl[_noOfCodecs][1] = 320; // 320 sampels equals 20 ms
    _allowedPacSizesSmpl[_noOfCodecs][2] = 480; // 480 sampels equals 30 ms
    _allowedPacSizesSmpl[_noOfCodecs][3] = 640; // 640 sampels equals 40 ms
    _allowedPacSizesSmpl[_noOfCodecs][4] = 800; // 480 sampels equals 50 ms
    _allowedPacSizesSmpl[_noOfCodecs][5] = 960; // 640 sampels equals 60 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 0;
    _channelSupport[_noOfCodecs] = 2;

    g722=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderG722;
    _noOfCodecs++;

    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    WebRtcG722_Version(versionNrBuff, TEMPORARY_BUFFER_SIZE);
    strncat(_versions, "G.722\t\t", remainingSize);
    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, versionNrBuff, remainingSize);

#endif

    // Comfort Noise is always included in the build, no #ifdef needed
    strcpy(_mycodecs[_noOfCodecs].plname,"CN");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 0;
    _mycodecs[_noOfCodecs].plfreq   = 8000;
    _mycodecs[_noOfCodecs].pltype   = 13;
    _mycodecs[_noOfCodecs].pacsize  = 240;   // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 1;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 240; // 240 samples equals 30 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 240;

     cnNB=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderCNG;
    _noOfCodecs++;

     strcpy(_mycodecs[_noOfCodecs].plname,"CN");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 0;
    _mycodecs[_noOfCodecs].plfreq   = 16000;
    _mycodecs[_noOfCodecs].pltype   = 98;
    _mycodecs[_noOfCodecs].pacsize  = 480;   // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 1;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 480; // 480 samples equals 30 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 480;

     cnWB=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderCNG;
    _noOfCodecs++;

     strcpy(_mycodecs[_noOfCodecs].plname,"CN");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 0;
    _mycodecs[_noOfCodecs].plfreq   = 32000;
    _mycodecs[_noOfCodecs].pltype   = 99;
    _mycodecs[_noOfCodecs].pacsize  = 960;   // Default packet size

    _nrOfAllowedPacSizes[_noOfCodecs]    = 1;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 960; // 960 samples equals 30 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 960;

     cnSWB=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderCNG;
    _noOfCodecs++;

    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    WebRtcCng_Version(versionNrBuff);
    strncat(_versions, "CNG\t\t", remainingSize);
    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, versionNrBuff, remainingSize);

#ifdef WEBRTC_CODEC_AVT
    strcpy(_mycodecs[_noOfCodecs].plname,"telephone-event");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].plfreq   = 8000;
    _mycodecs[_noOfCodecs].rate     = 0;
    _mycodecs[_noOfCodecs].pltype   = 106;
    _mycodecs[_noOfCodecs].pacsize  = 240;    // Default packet size 240ms

    _nrOfAllowedPacSizes[_noOfCodecs]    = 1;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 240; // 240 samples equals 30 ms
    _basicCodingBlockSmpl[_noOfCodecs]   = 240;

    avt=_noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderAVT;
    _noOfCodecs++;

    // Currently tone generation doesn't have a getVersion-function
    remainingSize = (WebRtc_Word32)(VERSION_SIZE - strlen(_versions));
    strncat(_versions, "Tone Generation\t1.0.0\n", remainingSize);
#endif
#ifdef WEBRTC_CODEC_RED
    strcpy(_mycodecs[_noOfCodecs].plname,"red");
    _mycodecs[_noOfCodecs].channels = 1;
    _mycodecs[_noOfCodecs].rate     = 0;
    _mycodecs[_noOfCodecs].plfreq   = 8000;
    _mycodecs[_noOfCodecs].pltype   = 127;
    _mycodecs[_noOfCodecs].pacsize  = 0;

    _nrOfAllowedPacSizes[_noOfCodecs]    = 1;
    _allowedPacSizesSmpl[_noOfCodecs][0] = 0;
    _basicCodingBlockSmpl[_noOfCodecs]   = 0;

    red = _noOfCodecs;
    _netEqDecoders[_noNetEqDecoders++] = kDecoderRED;
    _noOfCodecs++;
#endif


    _versionStringSize = (WebRtc_UWord32)strlen(_versions);
}


// Check if the bitrate is valid for the codec
bool
ACMCodecDB::IsRateValid(
    const WebRtc_Word16 listNr,
    const WebRtc_Word32 rate)
{
    if(_mycodecs[listNr].rate == rate)
    {
        return true;
    }
    else
    {
        return false;
    }
}


// Check if the bitrate is valid for iSAC
bool
ACMCodecDB::IsISACRateValid(
#if (!defined(WEBRTC_CODEC_ISAC) && !defined(WEBRTC_CODEC_ISACFX))
    const WebRtc_Word32 /* rate */)
{
    return false;
#else
    const WebRtc_Word32 rate)
{
    if((rate == -1) ||
        ((rate <= 56000) && (rate >= 10000)))
    {
        return true;
    }
    else
    {
        return false;
    }
#endif
}

// Check if the bitrate is valid for iLBC
bool
ACMCodecDB::IsILBCRateValid(
#ifndef WEBRTC_CODEC_ILBC
    const WebRtc_Word32 /* rate             */,
    const WebRtc_Word16 /* frameSizeSamples */)
{
    return false;
#else
    const WebRtc_Word32 rate,
    const WebRtc_Word16 frameSizeSamples)
{
    if(((frameSizeSamples == 240) || (frameSizeSamples == 480)) &&
        (rate == 13300))
    {
        return true;
    }
    else if(((frameSizeSamples == 160) || (frameSizeSamples == 320)) &&
        (rate == 15200))
    {
        return true;
    }
    else
    {
        return false;
    }
#endif
}

// Check if the payload type is valid
WebRtc_Word16
ACMCodecDB::ValidPayloadType(
    const int    payloadType)
{
    if((payloadType < 0) || (payloadType > 127))
    {
        return -1;
    }
    return 0;
}

} // namespace webrtc
