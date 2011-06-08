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
 * vp8.cc
 *
 * This file contains the WEBRTC VP8 wrapper implementation
 *
 */
#include "vp8.h"
#include "tick_util.h"

#include "vpx/vpx_encoder.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8cx.h"
#include "vpx/vp8dx.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "module_common_types.h"

#define VP8_FREQ_HZ 90000
//#define DEV_PIC_LOSS

namespace webrtc
{

VP8Encoder::VP8Encoder():
    _encodedImage(),
    _encodedCompleteCallback(NULL),
    _width(0),
    _height(0),
    _maxBitRateKbit(0),
    _inited(false),
    _pictureID(0),
    _pictureLossIndicationOn(false),
    _feedbackModeOn(false),
    _nextRefIsGolden(true),
    _lastAcknowledgedIsGolden(true),
    _haveReceivedAcknowledgement(false),
    _pictureIDLastSentRef(0),
    _pictureIDLastAcknowledgedRef(0),
    _cpuSpeed(-6), // default value
    _encoder(NULL),
    _cfg(NULL),
    _raw(NULL)
{
    srand((WebRtc_UWord32)TickTime::MillisecondTimestamp());
}

VP8Encoder::~VP8Encoder()
{
    Release();
}

WebRtc_Word32
VP8Encoder::VersionStatic(WebRtc_Word8* version, WebRtc_Word32 length)
{
    const WebRtc_Word8* str = "WebM/VP8 version 1.0.0\n"; // Bali
    WebRtc_Word32 verLen = (WebRtc_Word32)strlen(str);
    if (verLen > length)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    strncpy(version, str, length);
    return verLen;
}

WebRtc_Word32
VP8Encoder::Version(WebRtc_Word8 *version, WebRtc_Word32 length) const
{
    return VersionStatic(version, length);
}

WebRtc_Word32
VP8Encoder::Release()
{
    if (_encodedImage._buffer != NULL)
    {
        delete [] _encodedImage._buffer;
        _encodedImage._buffer = NULL;
    }
    if (_encoder != NULL)
    {
        if (vpx_codec_destroy(_encoder))
        {
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
        delete _encoder;
        _encoder = NULL;
    }
    if (_cfg != NULL)
    {
        delete _cfg;
        _cfg = NULL;
    }
    if (_raw != NULL)
    {
        vpx_img_free(_raw);
        delete _raw;
        _raw = NULL;
    }
    _inited = false;

    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
VP8Encoder::Reset()
{
    if (!_inited)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    if (_encoder != NULL)
    {
        if (vpx_codec_destroy(_encoder))
        {
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
        delete _encoder;
        _encoder = NULL;
    }

    _encoder = new vpx_codec_ctx_t;

    return InitAndSetSpeed();
}

WebRtc_Word32
VP8Encoder::SetRates(WebRtc_UWord32 newBitRateKbit, WebRtc_UWord32 newFrameRate)
{
    if (!_inited)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (_encoder->err)
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (newFrameRate < 1)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }

    // update bit rate
    if (_maxBitRateKbit > 0 &&
        newBitRateKbit > static_cast<WebRtc_UWord32>(_maxBitRateKbit))
    {
        newBitRateKbit = _maxBitRateKbit;
    }
    _cfg->rc_target_bitrate = newBitRateKbit; // in kbit/s

    // update frame rate
    if (newFrameRate != _maxFrameRate)
    {
        _maxFrameRate = static_cast<int>(newFrameRate);
        _cfg->g_timebase.num = 1;
        _cfg->g_timebase.den = _maxFrameRate;//VP8_FREQ_HZ;
    }

    // update encoder context
    if (vpx_codec_enc_config_set(_encoder, _cfg))
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
VP8Encoder::InitEncode(const VideoCodec* inst,
                       WebRtc_Word32 numberOfCores,
                       WebRtc_UWord32 /*maxPayloadSize */)
{
    if (inst == NULL)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (inst->maxFramerate < 1)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (inst->startBitrate < 0 || inst->maxBitrate < 0)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    // allow zero to represent an unspecified maxBitRate
    if (inst->maxBitrate > 0 && inst->startBitrate > inst->maxBitrate)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (inst->width < 1 || inst->height < 1)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (numberOfCores < 1)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
#ifdef DEV_PIC_LOSS
    // we need to know if we use feedback
    _feedbackModeOn = inst->codecSpecific.VP8.feedbackModeOn;
    _pictureLossIndicationOn = inst->codecSpecific.VP8.pictureLossIndicationOn;
#endif

    WebRtc_Word32 retVal = Release();
    if (retVal < 0)
    {
        return retVal;
    }
    if (_encoder == NULL)
    {
        _encoder = new vpx_codec_ctx_t;
    }
    if (_cfg == NULL)
    {
        _cfg = new vpx_codec_enc_cfg_t;
    }
    if (_raw == NULL)
    {
        _raw = new vpx_image_t;
    }

    _maxBitRateKbit = inst->maxBitrate;
    _maxFrameRate = inst->maxFramerate;
    _width = inst->width;
    _height = inst->height;

    // random start 16 bits is enough
    _pictureID = (WebRtc_UWord16)rand();

    // allocate memory for encoded image
    if (_encodedImage._buffer != NULL)
    {
        delete [] _encodedImage._buffer;
    }
    _encodedImage._size = (3 * inst->width * inst->height) >> 1;
    _encodedImage._buffer = new WebRtc_UWord8[_encodedImage._size];
    if (_encodedImage._buffer == NULL)
    {
        return WEBRTC_VIDEO_CODEC_MEMORY;
    }

    vpx_img_alloc(_raw, IMG_FMT_I420, inst->width, inst->height, 1);
    // populate encoder configuration with default values
    if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), _cfg, 0))
    {
         return WEBRTC_VIDEO_CODEC_ERROR;
    }

    _cfg->g_w = inst->width;
    _cfg->g_h = inst->height;
    if (_maxBitRateKbit > 0 && inst->startBitrate > static_cast<unsigned int>(_maxBitRateKbit))
    {
        _cfg->rc_target_bitrate = _maxBitRateKbit;
    }
    else
    {
      _cfg->rc_target_bitrate = inst->startBitrate;  // in kbit/s
    }

    // setting the time base of the codec
    _cfg->g_timebase.num = 1;
    _cfg->g_timebase.den = _maxFrameRate;

    _cfg->g_error_resilient = 1;  //enabled
    _cfg->g_lag_in_frames = 0; // 0- no frame lagging

    _cfg->g_threads = numberOfCores;

    // rate control settings
    _cfg->rc_dropframe_thresh = 0;
    _cfg->rc_end_usage = VPX_CBR;
    _cfg->g_pass = VPX_RC_ONE_PASS;
    _cfg->rc_resize_allowed = 0;
    _cfg->rc_min_quantizer = 4;
    _cfg->rc_max_quantizer = 56;
    _cfg->rc_undershoot_pct = 98;
    _cfg->rc_buf_initial_sz = 500;
    _cfg->rc_buf_optimal_sz = 600;
    _cfg->rc_buf_sz = 1000;


#ifdef DEV_PIC_LOSS
    // this can only be off if we know we use feedback
    if (_pictureLossIndicationOn)
    {
        _cfg->kf_mode = VPX_KF_DISABLED; // don't generate key frame unless we tell you
    }
    else
#endif
    {
        _cfg->kf_mode = VPX_KF_AUTO;
        _cfg->kf_max_dist = 300;
    }

    switch (inst->codecSpecific.VP8.complexity)
    {
        case kComplexityHigh:
        {
            _cpuSpeed = -5;
            break;
        }
        case kComplexityHigher:
        {
            _cpuSpeed = -4;
            break;
        }
        case kComplexityMax:
        {
            _cpuSpeed = -3;
            break;
        }
        default:
        {
            _cpuSpeed = -6;
            break;
        }
    }

    return InitAndSetSpeed();
}

WebRtc_Word32
VP8Encoder::InitAndSetSpeed()
{
    // construct encoder context
    vpx_codec_enc_cfg_t cfg_copy = *_cfg;
    if (vpx_codec_enc_init(_encoder, vpx_codec_vp8_cx(), _cfg, 0))
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    vpx_codec_control(_encoder, VP8E_SET_CPUUSED, _cpuSpeed);

    *_cfg = cfg_copy;

    _inited = true;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
VP8Encoder::Encode(const RawImage& inputImage,
                             const void* codecSpecificInfo,
                             VideoFrameType frameTypes)
{
    if (!_inited)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (inputImage._buffer == NULL)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (_encodedCompleteCallback == NULL)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    vpx_codec_iter_t iter = NULL;

    // image in vpx_image_t format
    _raw->planes[PLANE_Y] =  inputImage._buffer;
    _raw->planes[PLANE_U] =  &inputImage._buffer[_height * _width];
    _raw->planes[PLANE_V] =  &inputImage._buffer[_height * _width * 5 >> 2];

    int flags = 0;
    if (frameTypes == kKeyFrame)
    {
        flags |= VPX_EFLAG_FORCE_KF; // will update both golden and altref
        _encodedImage._frameType = kKeyFrame;
        _pictureIDLastSentRef = _pictureID;
    }
    else
    {
#ifdef DEV_PIC_LOSS
        if (_feedbackModeOn && codecSpecificInfo)
        {
            const CodecSpecificInfo* info = static_cast<const CodecSpecificInfo*>(codecSpecificInfo);
            if (info->codecType == kVideoCodecVP8)
            {
                // codecSpecificInfo will contain received RPSI and SLI picture IDs
                // this will help us decide on when to switch type of reference frame

                // if we receive SLI
                // force using an old golden or altref as a reference

                if (info->codecSpecific.VP8.hasReceivedSLI)
                {
                    // if this is older than my last acked ref we can ignore it
                    // info->codecSpecific.VP8.pictureIdSLI valid 6 bits => 64 frames

                    // since picture id can wrap check if in between our last sent and last acked

                    bool sendRefresh = false;
                    // check for a wrap in picture ID
                    if ((_pictureIDLastAcknowledgedRef & 0x3f) > (_pictureID & 0x3f))
                    {
                        // we have a wrap
                        if ( info->codecSpecific.VP8.pictureIdSLI > (_pictureIDLastAcknowledgedRef&0x3f)||
                            info->codecSpecific.VP8.pictureIdSLI < (_pictureID & 0x3f))
                        {
                            sendRefresh = true;
                        }
                    }
                    else if (info->codecSpecific.VP8.pictureIdSLI > (_pictureIDLastAcknowledgedRef&0x3f)&&
                             info->codecSpecific.VP8.pictureIdSLI < (_pictureID & 0x3f))
                    {
                        sendRefresh = true;
                    }

                    // right now we could also ignore it if it's older than our last sent ref since
                    // last sent ref only refers back to last acked
                    // _pictureIDLastSentRef;
                    if (sendRefresh)
                    {
                        flags |= VP8_EFLAG_NO_REF_LAST; // Don't reference the last frame

                        if (_haveReceivedAcknowledgement)
                        {
                            // we cant set this if we refer to a key frame
                            if (_lastAcknowledgedIsGolden)
                            {
                                flags |= VP8_EFLAG_NO_REF_ARF; // Don't reference the alternate reference frame
                            }
                            else
                            {
                                flags |= VP8_EFLAG_NO_REF_GF; // Don't reference the golden frame
                            }
                        }
                    }
                }
                if (info->codecSpecific.VP8.hasReceivedRPSI)
                {
                    if ((info->codecSpecific.VP8.pictureIdRPSI & 0x3fff) == (_pictureIDLastSentRef & 0x3fff)) // compare 14 bits
                    {
                        // remote peer have received our last reference frame
                        // switch frame type
                        _haveReceivedAcknowledgement = true;
                        _nextRefIsGolden = !_nextRefIsGolden;
                        _pictureIDLastAcknowledgedRef = _pictureIDLastSentRef;
                    }
                }
            }
            const WebRtc_UWord16 periodX = 64; // we need a period X to decide on the distance between golden and altref
            if (_pictureID % periodX == 0)
            {
                // only required if we have had a loss
                // however we don't acknowledge a SLI so if that is lost it's no good
                flags |= VP8_EFLAG_NO_REF_LAST; // Don't reference the last frame

                if (_nextRefIsGolden)
                {
                    flags |= VP8_EFLAG_FORCE_GF; // force a golden
                    flags |= VP8_EFLAG_NO_UPD_ARF; // don't update altref
                    if (_haveReceivedAcknowledgement)
                    {
                        // we can't set this if we refer to a key frame
                        // pw temporary as proof of concept
                        flags |= VP8_EFLAG_NO_REF_GF; // Don't reference the golden frame
                    }
                }
                else
                {
                    flags |= VP8_EFLAG_FORCE_ARF; // force an altref
                    flags |= VP8_EFLAG_NO_UPD_GF; // Don't update golden
                    if (_haveReceivedAcknowledgement)
                    {
                        // we can't set this if we refer to a key frame
                        // pw temporary as proof of concept
                        flags |= VP8_EFLAG_NO_REF_ARF; // Don't reference the alternate reference frame
                    }
                }
                // remember our last reference frame
                _pictureIDLastSentRef = _pictureID;
            }
            else
            {
                flags |= VP8_EFLAG_NO_UPD_GF;  // don't update golden
                flags |= VP8_EFLAG_NO_UPD_ARF; // don't update altref
            }
        }
#endif
        _encodedImage._frameType = kDeltaFrame;
    }

    if (vpx_codec_encode(_encoder, _raw, _maxFrameRate * inputImage._timeStamp / VP8_FREQ_HZ, 1, flags, VPX_DL_REALTIME))
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    const vpx_codec_cx_pkt_t *pkt= vpx_codec_get_cx_data(_encoder, &iter); // no lagging => 1 frame at a time
    if (pkt == NULL && !_encoder->err)
    {
        // dropped frame
        return WEBRTC_VIDEO_CODEC_OK;
    }
    else if (pkt->kind == VPX_CODEC_CX_FRAME_PKT)
    {
        // attach Picture ID
        // we use 14 bits generating 1 or 2 bytes
        // TODO(hlundin): update to follow latest RTP spec
        WebRtc_UWord8 pictureIdSize = 2;
        // TODO(hlundin): we should refactor this so that the pictureID is
        // signaled through a codec specific struct and added in the RTP module.
        if (_pictureID > 0x7f)
        {
            // more than 7 bits
            _encodedImage._buffer[0] = 0x80 | (WebRtc_UWord8)(_pictureID >> 7);
            _encodedImage._buffer[1] = (WebRtc_UWord8)(_pictureID & 0x7f);
        }
        else
        {
            _encodedImage._buffer[0] = (WebRtc_UWord8)_pictureID;
            pictureIdSize = 1;
        }

        memcpy(_encodedImage._buffer+pictureIdSize, pkt->data.frame.buf, pkt->data.frame.sz);
        _encodedImage._length = WebRtc_UWord32(pkt->data.frame.sz) + pictureIdSize;
        _encodedImage._encodedHeight = _raw->h;
        _encodedImage._encodedWidth = _raw->w;

        // check if encoded frame is a key frame
        if (pkt->data.frame.flags & VPX_FRAME_IS_KEY)
        {
            _encodedImage._frameType = kKeyFrame;
        }

        if (_encodedImage._length > 0)
        {
            _encodedImage._timeStamp = inputImage._timeStamp;

            // Figure out where partition boundaries are located.
            RTPFragmentationHeader fragInfo;
            fragInfo.VerifyAndAllocateFragmentationHeader(2); // two partitions: 1st and 2nd

            // First partition
            fragInfo.fragmentationOffset[0] = 0;
            WebRtc_UWord8 *firstByte = &_encodedImage._buffer[pictureIdSize];
            WebRtc_UWord32 tmpSize = (firstByte[2] << 16) | (firstByte[1] << 8)
                | firstByte[0];
            fragInfo.fragmentationLength[0] = (tmpSize >> 5) & 0x7FFFF;
            // Let the PictureID belong to the first partition.
            fragInfo.fragmentationLength[0] += pictureIdSize;
            fragInfo.fragmentationPlType[0] = 0; // not known here
            fragInfo.fragmentationTimeDiff[0] = 0;

            // Second partition
            fragInfo.fragmentationOffset[1] = fragInfo.fragmentationLength[0];
            fragInfo.fragmentationLength[1] = _encodedImage._length -
                fragInfo.fragmentationLength[0];
            fragInfo.fragmentationPlType[1] = 0; // not known here
            fragInfo.fragmentationTimeDiff[1] = 0;

            _encodedCompleteCallback->Encoded(_encodedImage, NULL, &fragInfo);
        }

        _pictureID++; // prepare next
        return WEBRTC_VIDEO_CODEC_OK;
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
}

WebRtc_Word32
VP8Encoder::SetPacketLoss(WebRtc_UWord32 packetLoss)
{
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
VP8Encoder::RegisterEncodeCompleteCallback(EncodedImageCallback* callback)
{
    _encodedCompleteCallback = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

VP8Decoder::VP8Decoder():
    _inited(false),
    _feedbackModeOn(false),
    _decoder(NULL)
{
}

VP8Decoder::~VP8Decoder()
{
    _inited = true; // in order to do the actual release
    Release();
}

WebRtc_Word32
VP8Decoder::Reset()
{
    if (!_inited)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    InitDecode(NULL, 1);
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
VP8Decoder::InitDecode(const VideoCodec* inst,
                                 WebRtc_Word32 numberOfCores)
{
    vp8_postproc_cfg_t  ppcfg;
    WebRtc_Word32 retVal = Release();
    if (retVal < 0 )
    {
        return retVal;
    }
    if (_decoder == NULL)
    {
        _decoder = new vpx_dec_ctx_t;
    }
#ifdef DEV_PIC_LOSS
    if(inst && inst->codecType == kVideoCodecVP8)
    {
        _feedbackModeOn = inst->codecSpecific.VP8.feedbackModeOn;
    }
#endif

    vpx_codec_dec_cfg_t  cfg;
    cfg.threads = numberOfCores;
    cfg.h = cfg.w = 0; // set after decode

    if(vpx_codec_dec_init(_decoder, vpx_codec_vp8_dx(), NULL, 0))
    {
        return WEBRTC_VIDEO_CODEC_MEMORY;
    }
    // config post-processing settings for decoder
    ppcfg.post_proc_flag   = VP8_DEBLOCK;
    ppcfg.deblocking_level = 5; //Strength of deblocking filter. Valid range:[0,16]
    //ppcfg.NoiseLevel     = 1; //Noise intensity. Valid range: [0,7]
    vpx_codec_control(_decoder, VP8_SET_POSTPROC, &ppcfg);

    _inited = true;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
VP8Decoder::Decode(const EncodedImage& inputImage,
                             bool missingFrames,
                             const void* /*codecSpecificInfo*/,
                             WebRtc_Word64 /*renderTimeMs*/)
 {
    if (!_inited)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (inputImage._buffer == NULL)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (_decodeCompleteCallback == NULL)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (inputImage._length <= 0)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (inputImage._completeFrame == false)
    {
        // future improvement
        // we can't decode this frame
        if (_feedbackModeOn)
        {
            return WEBRTC_VIDEO_CODEC_ERR_REQUEST_SLI;
        }
        else
        {
            return WEBRTC_VIDEO_CODEC_ERROR;
        }
    }
    vpx_dec_iter_t _iter = NULL;
    vpx_image_t* img;
    WebRtc_UWord64 pictureID = 0;

    // scan for number of bytes used for picture ID
    WebRtc_UWord8 numberOfBytes;
    for (numberOfBytes = 0;(inputImage._buffer[numberOfBytes] & 0x80 )&& numberOfBytes < 8; numberOfBytes++)
    {
        pictureID += inputImage._buffer[numberOfBytes] & 0x7f;
        pictureID <<= 7;
    }
    pictureID += inputImage._buffer[numberOfBytes] & 0x7f;
    numberOfBytes++;

    // check for missing frames
    if (missingFrames)
    {
        // call decoder with zero data length to signal missing frames
        if (vpx_codec_decode(_decoder, NULL, 0, 0, VPX_DL_REALTIME))
        {
            return WEBRTC_VIDEO_CODEC_ERROR;
        }
    }

    // we remove the picture ID here
    if (vpx_codec_decode(_decoder,
                         inputImage._buffer+numberOfBytes,
                         inputImage._length-numberOfBytes,
                         0,
                         VPX_DL_REALTIME))
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    int lastRefUpdates = 0;
#ifdef DEV_PIC_LOSS
    if (vpx_codec_control(_decoder, VP8D_GET_LAST_REF_UPDATES, &lastRefUpdates))
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
    int corrupted = 0;
    if (vpx_codec_control(_decoder, VP8D_GET_FRAME_CORRUPTED, &corrupted))
    {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
#endif

    img = vpx_codec_get_frame(_decoder, &_iter);

    // Allocate memory for decoded image
    WebRtc_UWord32 requiredSize = (3*img->h*img->w) >> 1;
    if (_decodedImage._buffer != NULL)
    {
        delete [] _decodedImage._buffer;
        _decodedImage._buffer = NULL;
    }
    if (_decodedImage._buffer == NULL)
    {
        _decodedImage._size = requiredSize;
        _decodedImage._buffer = new WebRtc_UWord8[_decodedImage._size];
        if (_decodedImage._buffer == NULL)
        {
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
    }

    WebRtc_UWord8* buf;
    WebRtc_UWord32 locCnt = 0;
    WebRtc_UWord32 plane, y;

    for (plane = 0; plane < 3; plane++)
    {
        buf = img->planes[plane];
        WebRtc_UWord32 shiftFactor = plane ? 1 : 0;
        for(y = 0; y < img->d_h >> shiftFactor; y++)
        {
            memcpy(&_decodedImage._buffer[locCnt], buf, img->d_w >> shiftFactor);
            locCnt += img->d_w >> shiftFactor;
            buf += img->stride[plane];
        }
    }

    // Set image parameters
    _decodedImage._height = img->d_h;
    _decodedImage._width = img->d_w;
    _decodedImage._length = (3 * img->d_h * img->d_w) >> 1;
    _decodedImage._timeStamp = inputImage._timeStamp;
    _decodeCompleteCallback->Decoded(_decodedImage);

    // we need to communicate that we should send a RPSI with a specific picture ID

    // TODO(pw): how do we know it's a golden or alt reference frame? On2 will provide an API
    // for now I added it temporarily
    if((lastRefUpdates & VP8_GOLD_FRAME) || (lastRefUpdates & VP8_ALTR_FRAME))
    {
        if (!missingFrames && (inputImage._completeFrame == true))
        //if (!corrupted) // TODO(pw): Can we engage this line intead of the above?
        {
            _decodeCompleteCallback->ReceivedDecodedReferenceFrame(pictureID);
        }
    }
    _decodeCompleteCallback->ReceivedDecodedFrame(pictureID);

#ifdef DEV_PIC_LOSS
    if (corrupted)
    {
        // we can decode but with artifacts
        return WEBRTC_VIDEO_CODEC_REQUEST_SLI;
    }
#endif
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
VP8Decoder::RegisterDecodeCompleteCallback(DecodedImageCallback* callback)
{
    _decodeCompleteCallback = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
VP8Decoder::Release()
{
    if (_decodedImage._buffer != NULL)
    {
        delete [] _decodedImage._buffer;
        _decodedImage._buffer = NULL;
    }
    if (_decoder != NULL)
    {
        if(vpx_codec_destroy(_decoder))
        {
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
        delete _decoder;
        _decoder = NULL;
    }

    _inited = false;
    return WEBRTC_VIDEO_CODEC_OK;
}

}
