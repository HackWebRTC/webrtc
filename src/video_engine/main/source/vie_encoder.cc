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
 * ViEEncoder.cpp
 */

#include "vie_encoder.h"
#include "vie_defines.h"

#include "critical_section_wrapper.h"
#include "process_thread.h"
#include "rtp_rtcp.h"
#include "video_coding.h"
#include "video_coding_defines.h"
#include "video_codec_interface.h"
#include "vie_codec.h"
#include "vie_image_process.h"
#include "tick_util.h"
#include "trace.h"

#include <cassert>
namespace webrtc {

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEEncoder::ViEEncoder(WebRtc_Word32 engineId, WebRtc_Word32 channelId,
                       WebRtc_UWord32 numberOfCores,
                       ProcessThread& moduleProcessThread)
       :
        _engineId(engineId),
        _channelId(channelId),
        _numberOfCores(numberOfCores),
        _vcm(*webrtc::VideoCodingModule::Create(ViEModuleId(engineId,
                                                            channelId))),
        _vpm(*webrtc::VideoProcessingModule::Create(ViEModuleId(engineId,
                                                                channelId))),
        _rtpRtcp(*RtpRtcp::CreateRtpRtcp(ViEModuleId(engineId,
                                                     channelId),
                                                     false)),
        _callbackCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
        _dataCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
        _paused(false), _timeLastIntraRequestMs(0),
        _channelsDroppingDeltaFrames(0), _dropNextFrame(false),
        _fecEnabled(false), _nackEnabled(false), _codecObserver(NULL),
        _effectFilter(NULL), _moduleProcessThread(moduleProcessThread),
        _hasReceivedSLI(false), _pictureIdSLI(0), _hasReceivedRPSI(false),
        _pictureIdRPSI(0), _fileRecorder(channelId)
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo,
                 ViEId(engineId, channelId),
                 "%s(engineId: %d) 0x%p - Constructor", __FUNCTION__, engineId,
                 this);

    _vcm.InitializeSender();
    _vpm.EnableTemporalDecimation(true);

     // Enable/disable content analysis: off by default for now
    _vpm.EnableContentAnalysis(false);

    _moduleProcessThread.RegisterModule(&_vcm);
    _rtpRtcp.InitSender();
    _rtpRtcp.RegisterIncomingVideoCallback(this);
    _rtpRtcp.RegisterIncomingRTCPCallback(this);
    _moduleProcessThread.RegisterModule(&_rtpRtcp);

    //
    _qmCallback = new QMTestVideoSettingsCallback();
    _qmCallback->RegisterVPM(&_vpm);
    _qmCallback->RegisterVCM(&_vcm);
    _qmCallback->SetNumOfCores(_numberOfCores);

#ifdef VIDEOCODEC_VP8
    VideoCodec videoCodec;
    if (_vcm.Codec(webrtc::kVideoCodecVP8, &videoCodec) == VCM_OK)
    {
        _vcm.RegisterSendCodec(&videoCodec, _numberOfCores,
                               _rtpRtcp.MaxDataPayloadLength());
        _rtpRtcp.RegisterSendPayload(videoCodec.plName, videoCodec.plType);
    }
    else
    {
        assert(false);
    }
#else
    VideoCodec videoCodec;
    if (_vcm.Codec(webrtc::kVideoCodecI420, &videoCodec) == VCM_OK)
    {
        _vcm.RegisterSendCodec(&videoCodec, _numberOfCores,
                               _rtpRtcp.MaxDataPayloadLength());
        _rtpRtcp.RegisterSendPayload(videoCodec.plName, videoCodec.plType);
    }
    else
    {
        assert(false);
    }
#endif

    if (_vcm.RegisterTransportCallback(this) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: VCM::RegisterTransportCallback failure");
    }
    if (_vcm.RegisterSendStatisticsCallback(this) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: VCM::RegisterSendStatisticsCallback failure");
    }

    if (_vcm.RegisterVideoQMCallback(_qmCallback) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "VCM::RegisterQMCallback failure");
    }
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViEEncoder::~ViEEncoder()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "ViEEncoder Destructor 0x%p, engineId: %d", this, _engineId);

    if (_rtpRtcp.NumberChildModules() > 0)
    {
        assert(false);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "Channels still attached %d, leaking memory",
                   _rtpRtcp.NumberChildModules());
        return;
    }
    _moduleProcessThread.DeRegisterModule(&_vcm);
    _moduleProcessThread.DeRegisterModule(&_vpm);
    _moduleProcessThread.DeRegisterModule(&_rtpRtcp);
    delete &_vcm;
    delete &_vpm;
    delete &_rtpRtcp;
    delete &_callbackCritsect;
    delete &_dataCritsect;
}

// ============================================================================
// Start/Stop
// ============================================================================

// ----------------------------------------------------------------------------
// Pause / Retart
//
// Call this to start/stop sending
// ----------------------------------------------------------------------------

void ViEEncoder::Pause()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s", __FUNCTION__);
    CriticalSectionScoped cs(_dataCritsect);
    _paused = true;
}

void ViEEncoder::Restart()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s", __FUNCTION__);
    CriticalSectionScoped cs(_dataCritsect);
    _paused = false;
}

// ----------------------------------------------------------------------------
// DropDeltaAfterKey
//
// Drops the first delta frame after a key frame is encoded.
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::DropDeltaAfterKey(bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s(%d)", __FUNCTION__, enable);
    CriticalSectionScoped cs(_dataCritsect);

    if (enable)
    {
        _channelsDroppingDeltaFrames++;
    } else
    {
        _channelsDroppingDeltaFrames--;
        if (_channelsDroppingDeltaFrames < 0)
        {
            _channelsDroppingDeltaFrames = 0;
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                         ViEId(_engineId, _channelId),
                         "%s: Called too many times", __FUNCTION__, enable);
            return -1;
        }
    }
    return 0;
}

// ============================================================================
// Codec settigns
// ============================================================================

// ----------------------------------------------------------------------------
// NumberOfCodecs
// ----------------------------------------------------------------------------

WebRtc_UWord8 ViEEncoder::NumberOfCodecs()
{
    return _vcm.NumberOfCodecs();
}

// ----------------------------------------------------------------------------
// GetCodec
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::GetCodec(WebRtc_UWord8 listIndex,
                                   webrtc::VideoCodec& videoCodec)
{
    if (_vcm.Codec(listIndex, &videoCodec) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Could not get codec",
                   __FUNCTION__);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// External encoder
// ----------------------------------------------------------------------------
WebRtc_Word32 ViEEncoder::RegisterExternalEncoder(webrtc::VideoEncoder* encoder,
                                                  WebRtc_UWord8 plType)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                  "%s: pltype %u", __FUNCTION__, plType);

    if (encoder == NULL)
        return -1;

    if (_vcm.RegisterExternalEncoder(encoder, plType) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "Could not register external encoder");
        return -1;
    }
    return 0;
}

WebRtc_Word32 ViEEncoder::DeRegisterExternalEncoder(WebRtc_UWord8 plType)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s: pltype %u", __FUNCTION__, plType);

    webrtc::VideoCodec currentSendCodec;
    if (_vcm.SendCodec(&currentSendCodec) == VCM_OK)
    {
        currentSendCodec.startBitrate = _vcm.Bitrate();
    }

    if (_vcm.RegisterExternalEncoder(NULL, plType) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "Could not deregister external encoder");
        return -1;
    }

    // If the external encoder is the current send codec use vcm internal encoder
    if (currentSendCodec.plType == plType)
    {
        WebRtc_UWord16 maxDataPayloadLength = _rtpRtcp.MaxDataPayloadLength();
        if (_vcm.RegisterSendCodec(&currentSendCodec, _numberOfCores,
                                   maxDataPayloadLength) != VCM_OK)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                         ViEId(_engineId, _channelId),
                         "Could not use internal encoder");
            return -1;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// SetEncoder
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::SetEncoder(const webrtc::VideoCodec& videoCodec)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s: CodecType: %d, width: %u, height: %u, maxPayloadLength: %u",
               __FUNCTION__, videoCodec.codecType, videoCodec.width,
               videoCodec.height);

    // Multiply startBitrate by 1000 because RTP module changed in API.
    if (_rtpRtcp.SetSendBitrate(videoCodec.startBitrate * 1000,
                                videoCodec.minBitrate, videoCodec.maxBitrate) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "Could not set RTP module bitrates");
        return -1;
    }

    // Setting target width and height for VPM
    if (_vpm.SetTargetResolution(videoCodec.width, videoCodec.height, videoCodec.maxFramerate) != VPM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "Could not set VPM target dimensions");
        return -1;

    }

    if (_rtpRtcp.RegisterSendPayload(videoCodec.plName, videoCodec.plType) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "Could register RTP module video payload");
        return -1;
    }

    WebRtc_UWord16 maxDataPayloadLength = _rtpRtcp.MaxDataPayloadLength();

    // update QM with MaxDataPayloadLength
    _qmCallback->SetMaxPayloadLength(maxDataPayloadLength);

    if (_vcm.RegisterSendCodec(&videoCodec, _numberOfCores,
                               maxDataPayloadLength) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "Could not register send codec");
        return -1;
    }
    _dataCritsect.Enter();
    memcpy(&_sendCodec, &videoCodec, sizeof(_sendCodec)); // Copy current send codec
    _dataCritsect.Leave();

    // Set this module as sending right away, let the
    // slave module in the channel start and stop sending...
    if (_rtpRtcp.Sending() == false)
    {
        if (_rtpRtcp.SetSendingStatus(true) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                             _channelId),
                       "Could start RTP module sending");
            return -1;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetSendCodec
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::GetEncoder(webrtc::VideoCodec& videoCodec)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_vcm.SendCodec(&videoCodec) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "Could not get VCM send codec");
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetCodecConfigParameters
//
// Only valid for H.264 and MPEG-4
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::GetCodecConfigParameters(
                                                   unsigned char configParameters[kConfigParameterSize],
                                                   unsigned char& configParametersSize)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    WebRtc_Word32 numParameters =
        _vcm.CodecConfigParameters(configParameters, kConfigParameterSize);
    if (numParameters <= 0)
    {
        configParametersSize = 0;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "Could not get config parameters");
        return -1;
    }
    configParametersSize = (unsigned char) numParameters;
    return 0;
}

// ----------------------------------------------------------------------------
// ScaleInputImage
//
// The input image will be scaled if the codec resolution differs from the
// image resolution of the input image, otherwise will the image be
// cropped/padded. Default: crop/pad.
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::ScaleInputImage(bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s(enable %d)", __FUNCTION__, enable);

    VideoFrameResampling resamplingMode = kFastRescaling;
    if (enable == true)
    {
        // Currently not supported.
        //resamplingMode = kInterpolation;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_engineId, _channelId), "%s not supported",
                     __FUNCTION__, enable);
        return -1;
    }
    _vpm.SetInputFrameResampleMode(resamplingMode);

    return 0;
}

//=============================================================================
// RTP settings
//=============================================================================

// ----------------------------------------------------------------------------
// GetRtpRtcpModule
// ----------------------------------------------------------------------------

RtpRtcp* ViEEncoder::SendRtpRtcpModule()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    return &_rtpRtcp;
}

//=============================================================================
// Data flow
//=============================================================================


// ----------------------------------------------------------------------------
// DeliverFrame
// Implements ViEFrameCallback::DeliverFrame
// Receive videoFrame to be encoded from a provider (capture or file)
// ----------------------------------------------------------------------------

void ViEEncoder::DeliverFrame(int id, webrtc::VideoFrame& videoFrame,
                              int numCSRCs,
                              const WebRtc_UWord32 CSRC[kRtpCsrcSize])
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: %llu", __FUNCTION__, videoFrame.TimeStamp());

    {
        CriticalSectionScoped cs(_dataCritsect);
        if (_paused || _rtpRtcp.SendingMedia() == false)
        {
            // We've passed or we have no channels attached, don't encode
            return;
        }
        if (_dropNextFrame)
        {
            // Drop this frame
            WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId,
                                                              _channelId),
                       "%s: Dropping frame %llu after a key fame",
                       __FUNCTION__, videoFrame.TimeStamp());
            _dropNextFrame = false;
            return;
        }
    }
    // Set the frame timestamp
    const WebRtc_UWord32 timeStamp = 90 * (WebRtc_UWord32) videoFrame.RenderTimeMs();
    videoFrame.SetTimeStamp(timeStamp);
    {
        // Send to effect filter, if registered by user.
        CriticalSectionScoped cs(_callbackCritsect);
        if (_effectFilter)
        {
            _effectFilter->Transform(videoFrame.Length(), videoFrame.Buffer(),
                                     videoFrame.TimeStamp(),
                                     videoFrame.Width(), videoFrame.Height());
        }
    }
    // Record un-encoded frame.
    _fileRecorder.RecordVideoFrame(videoFrame);
    // Make sure the CSRC list is correct.
    if (numCSRCs > 0)
    {
        WebRtc_UWord32 tempCSRC[kRtpCsrcSize];
        for (int i = 0; i < numCSRCs; i++)
        {
            if (CSRC[i] == 1)
            {
                tempCSRC[i] = _rtpRtcp.SSRC();
            }
            else
            {
                tempCSRC[i] = CSRC[i];
            }
        }
        _rtpRtcp.SetCSRCs(tempCSRC, (WebRtc_UWord8) numCSRCs);
    }

#ifdef VIDEOCODEC_VP8
    if (_vcm.SendCodec() == webrtc::kVideoCodecVP8)
    {
        webrtc::CodecSpecificInfo codecSpecificInfo;
        codecSpecificInfo.codecType = webrtc::kVideoCodecUnknown;

        if (_hasReceivedSLI || _hasReceivedRPSI)
        {
            webrtc::VideoCodec currentSendCodec;
            _vcm.SendCodec(&currentSendCodec);
            if (currentSendCodec.codecType == webrtc::kVideoCodecVP8)
            {
                codecSpecificInfo.codecType = webrtc::kVideoCodecVP8;
                codecSpecificInfo.codecSpecific.VP8.hasReceivedRPSI = _hasReceivedRPSI;
                codecSpecificInfo.codecSpecific.VP8.hasReceivedSLI = _hasReceivedSLI;
                codecSpecificInfo.codecSpecific.VP8.pictureIdRPSI = _pictureIdRPSI;
                codecSpecificInfo.codecSpecific.VP8.pictureIdSLI  = _pictureIdSLI;
            }
            _hasReceivedSLI = false;
            _hasReceivedRPSI = false;
        }
        // Pass frame via preprocessor
        VideoFrame *decimatedFrame = NULL;
        const int ret = _vpm.PreprocessFrame(&videoFrame, &decimatedFrame);
        if (ret == 1)
        {
            // Drop this frame
            return;
        }
        else if (ret != VPM_OK)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                       "%s: Error preprocessing frame %u", __FUNCTION__,
                       videoFrame.TimeStamp());
            return;
        }

        VideoContentMetrics* contentMetrics = NULL;
        contentMetrics = _vpm.ContentMetrics();

        if (_vcm.AddVideoFrame
            (*decimatedFrame, contentMetrics, &codecSpecificInfo) != VCM_OK)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                       "%s: Error encoding frame %u", __FUNCTION__,
                       videoFrame.TimeStamp());
        }
        return;
    }
#endif
    // Pass frame via preprocessor
    VideoFrame *decimatedFrame = NULL;
    const int ret = _vpm.PreprocessFrame(&videoFrame, &decimatedFrame);
    if (ret == 1)
    {
        // Drop this frame
        return;
    }
    else if (ret != VPM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                  "%s: Error preprocessing frame %u", __FUNCTION__, videoFrame.TimeStamp());
        return;
    }
    if (_vcm.AddVideoFrame(*decimatedFrame) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Error encoding frame %u",
                   __FUNCTION__, videoFrame.TimeStamp());
    }
}
// ----------------------------------------------------------------------------
// DeliverFrame
// Implements ViEFrameCallback::DelayChanged
// ----------------------------------------------------------------------------
void ViEEncoder::DelayChanged(int id, int frameDelay)

{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: %u", __FUNCTION__, frameDelay);

    _rtpRtcp.SetCameraDelay(frameDelay);
    _fileRecorder.SetFrameDelay(frameDelay);
}
// ----------------------------------------------------------------------------
// GetPreferedFrameSettings
// Implements ViEFrameCallback::GetPreferedFrameSettings
// Fetch the widh, height and frame rate prefered by this encoder.
// ----------------------------------------------------------------------------

int ViEEncoder::GetPreferedFrameSettings(int &width, int &height,
                                         int &frameRate)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    webrtc::VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(videoCodec));
    if (_vcm.SendCodec(&videoCodec) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "Could not get VCM send codec");
        return -1;
    }

    width = videoCodec.width;
    height = videoCodec.height;
    frameRate = videoCodec.maxFramerate;
    return 0;

}
// ----------------------------------------------------------------------------
// SendKeyFrame
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::SendKeyFrame()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    return _vcm.FrameTypeRequest(kVideoFrameKey);
}

// ----------------------------------------------------------------------------
// SendCodecStatistics
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::SendCodecStatistics(WebRtc_UWord32& numKeyFrames,
                                              WebRtc_UWord32& numDeltaFrames)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    webrtc::VCMFrameCount sentFrames;
    if (_vcm.SentFrameCount(sentFrames) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not get sent frame information", __FUNCTION__);
        return -1;
    }
    numKeyFrames = sentFrames.numKeyFrames;
    numDeltaFrames = sentFrames.numDeltaFrames;
    return 0;

}

//=============================================================================
// Loss protection
//=============================================================================

// ----------------------------------------------------------------------------
// UpdateProtectionMethod
//
// Updated protection method to VCM to get correct packetization sizes
// FEC has larger overhead than NACK -> set FEC if used
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::UpdateProtectionMethod()
{
    // Get FEC status
    bool fecEnabled = false;
    WebRtc_UWord8 dummyPTypeRed = 0;
    WebRtc_UWord8 dummyPTypeFEC = 0;

    // check if fec is enabled
    WebRtc_Word32 error = _rtpRtcp.GenericFECStatus(fecEnabled, dummyPTypeRed,
                                                    dummyPTypeFEC);
    if (error)
    {
        return -1;
    }

    // check if nack is enabled
    bool nackEnabled = (_rtpRtcp.NACK() == kNackOff) ? false : true;
    if (_fecEnabled == fecEnabled && _nackEnabled == nackEnabled)
    {
        // no change to current state
        return 0;
    }
    _fecEnabled = fecEnabled;
    _nackEnabled = nackEnabled;

    // Set Video Protection for VCM
    if (fecEnabled && nackEnabled)
    {
        _vcm.SetVideoProtection(webrtc::kProtectionNackFEC, true);
    }
    else
    {
        _vcm.SetVideoProtection(webrtc::kProtectionFEC, _fecEnabled);
        _vcm.SetVideoProtection(webrtc::kProtectionNack, _nackEnabled);
        _vcm.SetVideoProtection(webrtc::kProtectionNackFEC, false);
    }

    // If nack and/or fec is enalbed, the following should be triggered
    if (fecEnabled || nackEnabled)
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                     ViEId(_engineId, _channelId),
                     "%s: FEC status ", __FUNCTION__, fecEnabled);
        _vcm.RegisterProtectionCallback(this);
        // Need to reregister the send codec in order to set the new MTU
        webrtc::VideoCodec codec;
        if (_vcm.SendCodec(&codec) == 0)
        {
            WebRtc_UWord16 maxPayLoad = _rtpRtcp.MaxDataPayloadLength();
            codec.startBitrate = _vcm.Bitrate();
            if (_vcm.RegisterSendCodec(&codec, _numberOfCores, maxPayLoad) != 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             ViEId(_engineId, _channelId),
                             "%s: Failed to update Sendcodec when enabling FEC",
                             __FUNCTION__, fecEnabled);
                return -1;
            }
        }
        return 0;
    }
    else
    {
        // FEC and NACK are disabled
        _vcm.RegisterProtectionCallback(NULL);
    }
    return 0;
}

//=============================================================================
// Implementation of VideoPacketizationCallback from VCM
//=============================================================================

// ----------------------------------------------------------------------------
// SendData
// ----------------------------------------------------------------------------
WebRtc_Word32
ViEEncoder::SendData(const FrameType frameType,
                     const WebRtc_UWord8 payloadType,
                     const WebRtc_UWord32 timeStamp,
                     const WebRtc_UWord8* payloadData,
                     const WebRtc_UWord32 payloadSize,
                     const webrtc::RTPFragmentationHeader& fragmentationHeader,
                     const RTPVideoTypeHeader* rtpTypeHdr)
{
    {
        CriticalSectionScoped cs(_dataCritsect);
        if (_paused)
        {
            // Paused, don't send this packet
            return 0;
        }
        if (_channelsDroppingDeltaFrames && frameType == webrtc::kVideoFrameKey)
        {
            WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId,
                                                              _channelId),
                       "%s: Sending key frame, drop next frame", __FUNCTION__);
            _dropNextFrame = true;
        }
    }
    // New encoded data, hand over to the rtp module
    WebRtc_Word32 retVal = _rtpRtcp.SendOutgoingData(frameType, payloadType,
                                                     timeStamp, payloadData,
                                                     payloadSize,
                                                     &fragmentationHeader,
                                                     rtpTypeHdr);
    return retVal;
}

//=============================================================================
// Implementation of VideoProtectionCallback from VCM
//=============================================================================

// ----------------------------------------------------------------------------
// ProtectionRequest
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::ProtectionRequest(const WebRtc_UWord8 deltaFECRate,
                                            const WebRtc_UWord8 keyFECRate,
                                            const bool nack)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: deltaFECRate: %u, keyFECRate: %u, nack: %d", __FUNCTION__,
               deltaFECRate, keyFECRate, nack);

    if (_rtpRtcp.SetFECCodeRate(keyFECRate, deltaFECRate) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not update FEC code rate", __FUNCTION__);
    }
    return 0;
}

//=============================================================================
// Implementation of VideoSendStatisticsCallback from VCM
//=============================================================================

// ----------------------------------------------------------------------------
// SendStatistics
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::SendStatistics(const WebRtc_UWord32 bitRate,
                                         const WebRtc_UWord32 frameRate)
{
    CriticalSectionScoped cs(_callbackCritsect);
    if (_codecObserver)
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: bitrate %u, framerate %u", __FUNCTION__, bitRate,
                   frameRate);
        _codecObserver->OutgoingRate(_channelId, frameRate, bitRate);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterCodecObserver
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEEncoder::RegisterCodecObserver(ViEEncoderObserver* observer)
{
    CriticalSectionScoped cs(_callbackCritsect);
    if (observer)
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer added", __FUNCTION__);
        if (_codecObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                             _channelId),
                       "%s: observer already set.", __FUNCTION__);
            return -1;
        }
        _codecObserver = observer;
    } else
    {
        if (_codecObserver == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId,
                                                            _channelId),
                       "%s: observer does not exist.", __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer removed", __FUNCTION__);
        _codecObserver = NULL;
    }
    return 0;
}

//=============================================================================
// Implementation of RtcpFeedback
//=============================================================================

void ViEEncoder::OnSLIReceived(const WebRtc_Word32 id,
                               const WebRtc_UWord8 pictureId)
{
    _pictureIdSLI = pictureId;
    _hasReceivedSLI = true;
}

void ViEEncoder::OnRPSIReceived(const WebRtc_Word32 id,
                                const WebRtc_UWord64 pictureId)
{
    _pictureIdRPSI = pictureId;
    _hasReceivedRPSI = true;
}

//=============================================================================
// Implementation of RtpVideoFeedback
//=============================================================================

// ----------------------------------------------------------------------------
// OnReceivedIntraFrameRequest
// ----------------------------------------------------------------------------

void ViEEncoder::OnReceivedIntraFrameRequest(const WebRtc_Word32 id,
                                             const WebRtc_UWord8 message)
{
    // Key frame request from other side, signal to VCM
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_timeLastIntraRequestMs + kViEMinKeyRequestIntervalMs
                                 > TickTime::MillisecondTimestamp())
    {
        WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Not not encoding new intra due to timing", __FUNCTION__);
        return;
    }
    // Default message == 0...
    if (message == 0)
    {
        _vcm.FrameTypeRequest(kVideoFrameKey);
    } else
    {
        _vcm.FrameTypeRequest((FrameType) message);
    }
    _timeLastIntraRequestMs = TickTime::MillisecondTimestamp();
    return;
}

// ----------------------------------------------------------------------------
// OnNetworkChanged
// ----------------------------------------------------------------------------
void ViEEncoder::OnNetworkChanged(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 minBitrateBps,
                                  const WebRtc_UWord32 maxBitrateBps,
                                  const WebRtc_UWord8 fractionLost,
                                  const WebRtc_UWord16 roundTripTimeMs,
                                  const WebRtc_UWord16 bwEstimateKbitMin,
                                  const WebRtc_UWord16 bwEstimateKbitMax)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s(minBitrateBps: %u, maxBitrateBps: %u,fractionLost: %u, rttMs: %u, bwEstMinKbit: %u, bwEstMaxKbit: %u",
               __FUNCTION__, minBitrateBps, maxBitrateBps, fractionLost,
               roundTripTimeMs, bwEstimateKbitMin, bwEstimateKbitMax);
    _vcm.SetChannelParameters(minBitrateBps / 1000, fractionLost, roundTripTimeMs);
    return;
}

WebRtc_Word32 ViEEncoder::RegisterEffectFilter(ViEEffectFilter* effectFilter)
{
    CriticalSectionScoped cs(_callbackCritsect);
    if (effectFilter == NULL)
    {
        if (_effectFilter == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                             _channelId),
                       "%s: no effect filter added", __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: deregister effect filter", __FUNCTION__);
    } else
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: register effect", __FUNCTION__);
        if (_effectFilter)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                             _channelId),
                       "%s: effect filter already added ", __FUNCTION__);
            return -1;
        }
    }
    _effectFilter = effectFilter;
    return 0;
}

ViEFileRecorder& ViEEncoder::GetOutgoingFileRecorder()
{
    return _fileRecorder;
}

//=============================================================================
// Implementation of Video QM settings callback:
// Callback to be called from VCM to update VPM of frame rate and size
//=============================================================================

ViEEncoder::QMTestVideoSettingsCallback::QMTestVideoSettingsCallback():
_vpm(NULL),
_vcm(NULL)
{

}

void ViEEncoder::QMTestVideoSettingsCallback::
                 RegisterVPM(VideoProcessingModule *vpm)
{
    _vpm = vpm;
}

void ViEEncoder::QMTestVideoSettingsCallback::
                 RegisterVCM(VideoCodingModule *vcm)
{
    _vcm = vcm;
}

WebRtc_Word32 ViEEncoder::QMTestVideoSettingsCallback::SetVideoQMSettings
                              (const WebRtc_UWord32 frameRate,
                               const WebRtc_UWord32 width,
                               const WebRtc_UWord32 height)
{

    WebRtc_Word32 retVal = 0;
    retVal = _vpm->SetTargetResolution(width, height, frameRate);
    //Initialize codec with new values
    if (!retVal)
    {
        // first get current settings
        VideoCodec currentCodec;
        _vcm->SendCodec(&currentCodec);

        WebRtc_UWord32 currentBitRate = _vcm->Bitrate();

        // now set new values:
        currentCodec.height = (WebRtc_UWord16)height;
        currentCodec.width = (WebRtc_UWord16)width;
        currentCodec.maxFramerate = (WebRtc_UWord8)frameRate;
        currentCodec.startBitrate = currentBitRate;

        // re-register encoder
        retVal = _vcm->RegisterSendCodec(&currentCodec,_numOfCores,_maxPayloadLength);
    }

    return retVal;
}

/////////////////////


} // namespace webrtc
