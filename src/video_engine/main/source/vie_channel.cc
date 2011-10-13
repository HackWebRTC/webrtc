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
 * ViEChannel.cpp
 */

#include "vie_channel.h"
#include "vie_defines.h"

#include "critical_section_wrapper.h"
#include "rtp_rtcp.h"
#include "udp_transport.h"
#include "video_coding.h"
#include "video_processing.h"
#include "video_render_defines.h"
#ifdef WEBRTC_SRTP
#include "SrtpModule.h"
#endif
#include "process_thread.h"
#include "trace.h"
#include "thread_wrapper.h"
#include "vie_codec.h"
#include "vie_errors.h"
#include "vie_image_process.h"
#include "vie_rtp_rtcp.h"
#include "vie_receiver.h"
#include "vie_sender.h"
#include "vie_sync_module.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEChannel::ViEChannel(WebRtc_Word32 channelId, WebRtc_Word32 engineId,
                       WebRtc_UWord32 numberOfCores,
                       ProcessThread& moduleProcessThread) :
        ViEFrameProviderBase(channelId, engineId),
        _channelId(channelId),
        _engineId(engineId),
        _numberOfCores(numberOfCores),
        _numSocketThreads(kViESocketThreads),
        _callbackCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
        _rtpRtcp(*RtpRtcp::CreateRtpRtcp(
                    ViEModuleId(engineId, channelId), false)),
#ifndef WEBRTC_EXTERNAL_TRANSPORT
        _socketTransport(
            *UdpTransport::Create(
                ViEModuleId(engineId, channelId), _numSocketThreads)),
#endif
        _vcm(*VideoCodingModule::Create(
            ViEModuleId(engineId, channelId))),

        _vieReceiver(*(new ViEReceiver(engineId, channelId, _rtpRtcp, _vcm))),
        _vieSender(*(new ViESender(engineId, channelId))),
        _vieSync(*(new ViESyncModule(ViEId(engineId, channelId), _vcm,
                                     _rtpRtcp))),
        _moduleProcessThread(moduleProcessThread),
        _codecObserver(NULL),
        _doKeyFrameCallbackRequest(false),
        _rtpObserver(NULL),
        _rtcpObserver(NULL),
        _networkObserver(NULL),
        _rtpPacketTimeout(false),
        _usingPacketSpread(false),
        _ptrExternalTransport(NULL),
        _decoderReset(true),
        _waitForKeyFrame(false),
        _ptrDecodeThread(NULL),
        _ptrSrtpModuleEncryption(NULL),
        _ptrSrtpModuleDecryption(NULL),
        _ptrExternalEncryption(NULL),
        _effectFilter(NULL),
        _colorEnhancement(true),
        _vcmRTTReported(TickTime::Now()),
        _fileRecorder(channelId)
{
    WEBRTC_TRACE(
        webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(engineId, channelId),
        "ViEChannel::ViEChannel(channelId: %d, engineId: %d) - Constructor",
        channelId, engineId);
}

WebRtc_Word32 ViEChannel::Init()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: channelId: %d, engineId: %d)", __FUNCTION__, _channelId,
               _engineId);
    // RTP/RTCP initialization
    if (_rtpRtcp.InitSender() != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: RTP::InitSender failure",
                   __FUNCTION__);
        return -1;
    }
    if (_rtpRtcp.SetSendingMediaStatus(false) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP::SetSendingMediaStatus failure", __FUNCTION__);
        return -1;
    }
    if (_rtpRtcp.InitReceiver() != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP::InitReceiver failure", __FUNCTION__);
        return -1;
    }
    if (_rtpRtcp.RegisterIncomingDataCallback((RtpData*) &_vieReceiver)
        != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP::RegisterIncomingDataCallback failure",
                   __FUNCTION__);
        return -1;
    }
    if (_rtpRtcp.RegisterSendTransport((Transport*) &_vieSender) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP::RegisterSendTransport failure", __FUNCTION__);
        return -1;
    }
    if (_moduleProcessThread.RegisterModule(&_rtpRtcp) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP::RegisterModule failure", __FUNCTION__);
        return -1;
    }
    if (_rtpRtcp.SetKeyFrameRequestMethod(kKeyFrameReqFirRtp) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId,_channelId),
                   "%s: RTP::SetKeyFrameRequestMethod failure", __FUNCTION__);
    }
    if (_rtpRtcp.SetRTCPStatus(kRtcpCompound) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP::SetRTCPStatus failure", __FUNCTION__);
    }
    if (_rtpRtcp.RegisterIncomingRTPCallback(this) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP::RegisterIncomingRTPCallback failure",
                   __FUNCTION__);
        return -1;
    }
    if (_rtpRtcp.RegisterIncomingRTCPCallback(this) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP::RegisterIncomingRTCPCallback failure",
                   __FUNCTION__);
        return -1;
    }

    // VCM initialization
    if (_vcm.InitializeReceiver() != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: VCM::InitializeReceiver failure", __FUNCTION__);
        return -1;
    }
    if (_vcm.RegisterReceiveCallback(this) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: VCM::RegisterReceiveCallback failure", __FUNCTION__);
        return -1;
    }
    if (_vcm.RegisterFrameTypeCallback(this) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: VCM::RegisterFrameTypeCallback failure", __FUNCTION__);
    }
    if (_vcm.RegisterReceiveStatisticsCallback(this) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: VCM::RegisterReceiveStatisticsCallback failure",
                   __FUNCTION__);
    }
    if (_vcm.SetRenderDelay(kViEDefaultRenderDelayMs) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: VCM::SetRenderDelay failure", __FUNCTION__);
    }
    if (_moduleProcessThread.RegisterModule(&_vcm) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: VCM::RegisterModule(vcm) failure", __FUNCTION__);
        return -1;
    }
#ifdef VIDEOCODEC_VP8
    VideoCodec videoCodec;
    if (_vcm.Codec(kVideoCodecVP8, &videoCodec) == VCM_OK)
    {
        _rtpRtcp.RegisterSendPayload(videoCodec);
        _rtpRtcp.RegisterReceivePayload(videoCodec);
        _vcm.RegisterReceiveCodec(&videoCodec, _numberOfCores);
        _vcm.RegisterSendCodec(&videoCodec, _numberOfCores,
                               _rtpRtcp.MaxDataPayloadLength());
    }
    else
    {
        assert(false);
    }
#endif

    return 0;
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViEChannel::~ViEChannel()
{
    WEBRTC_TRACE(webrtc::kTraceMemory,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "ViEChannel Destructor, channelId: %d, engineId: %d",
                 _channelId, _engineId);

    // Make sure we don't get more callbacks from the RTP module.
    _rtpRtcp.RegisterIncomingRTPCallback(NULL);
    _rtpRtcp.RegisterSendTransport(NULL);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    _socketTransport.StopReceiving();
#endif
    _moduleProcessThread.DeRegisterModule(&_rtpRtcp);
    _moduleProcessThread.DeRegisterModule(&_vcm);
    _moduleProcessThread.DeRegisterModule(&_vieSync);
    while (_simulcastRtpRtcp.size() > 0)
    {
        std::list<RtpRtcp*>::iterator it = _simulcastRtpRtcp.begin();
        RtpRtcp* rtpRtcp = *it;
        rtpRtcp->RegisterIncomingRTCPCallback(NULL);
        rtpRtcp->RegisterSendTransport(NULL);
        _moduleProcessThread.DeRegisterModule(rtpRtcp);
        RtpRtcp::DestroyRtpRtcp(rtpRtcp);
        _simulcastRtpRtcp.erase(it);
    }
    if (_ptrDecodeThread)
    {
        StopDecodeThread();
    }

    delete &_vieReceiver;
    delete &_vieSender;
    delete &_vieSync;

    delete &_callbackCritsect;

    // Release modules
    RtpRtcp::DestroyRtpRtcp(&_rtpRtcp);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    UdpTransport::Destroy(&_socketTransport);
#endif
    VideoCodingModule::Destroy(&_vcm);
}

// ============================================================================
// Codec
// ============================================================================

// ----------------------------------------------------------------------------
// SetSendCodec
//
// videoCodec: encoder settings
// newStream:  the encoder type has changed and we should start a new RTP stream
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetSendCodec(const VideoCodec& videoCodec,
                                       bool newStream)
{
    WEBRTC_TRACE(webrtc::kTraceInfo,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s: codecType: %d",
                 __FUNCTION__,
                 videoCodec.codecType);

    if (videoCodec.codecType == kVideoCodecRED ||
        videoCodec.codecType == kVideoCodecULPFEC)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: codecType: %d is not a valid send codec.",
                   __FUNCTION__, videoCodec.codecType);
        return -1;
    }
    if (kMaxSimulcastStreams < videoCodec.numberOfSimulcastStreams)
    {
        WEBRTC_TRACE(webrtc::kTraceError,
                     webrtc::kTraceVideo,
                     ViEId(_engineId, _channelId),
                     "%s: Too many simulcast streams",
                     __FUNCTION__);
        return -1;
    }
    // Update the RTP module with the settigns
    // Stop and Start the RTP module -> trigger new SSRC
    bool restartRtp = false;
    if (_rtpRtcp.Sending() && newStream)
    {
        restartRtp = true;
        _rtpRtcp.SetSendingStatus(false);
    }
    if (videoCodec.numberOfSimulcastStreams > 0)
    {
        WebRtc_UWord32 startBitrate = videoCodec.startBitrate * 1000;
        WebRtc_UWord32 streamBitrate = std::min(startBitrate, 
            videoCodec.simulcastStream[0].maxBitrate);
        startBitrate -= streamBitrate;
        // set correct bitrate to base layer
        if (_rtpRtcp.SetSendBitrate(
            streamBitrate,
            videoCodec.minBitrate,
            videoCodec.simulcastStream[0].maxBitrate) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                         ViEId(_engineId, _channelId),
                         "%s: could not set send bitrates",
                         __FUNCTION__);
            return -1;
        }
        // Create our simulcast RTP modules
        for (int i = _simulcastRtpRtcp.size();
             i < videoCodec.numberOfSimulcastStreams - 1;
             i++)
        {
            RtpRtcp* rtpRtcp = RtpRtcp::CreateRtpRtcp(
                ViEModuleId(_engineId, _channelId),
                false);
            if (rtpRtcp->RegisterDefaultModule(_defaultRtpRtcp))
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             ViEId(_engineId, _channelId),
                             "%s: could not register default module",
                             __FUNCTION__);
                return -1;
            }
            _simulcastRtpRtcp.push_back(rtpRtcp);
        }
        // Remove last in list if we have too many
        for (int j = _simulcastRtpRtcp.size();
             j > (videoCodec.numberOfSimulcastStreams - 1);
             j--)
        {
            RtpRtcp* rtpRtcp = _simulcastRtpRtcp.back();
            rtpRtcp->RegisterIncomingRTCPCallback(NULL);
            rtpRtcp->RegisterSendTransport(NULL);
            _moduleProcessThread.DeRegisterModule(rtpRtcp);
            RtpRtcp::DestroyRtpRtcp(rtpRtcp);
            _simulcastRtpRtcp.pop_back();
        }
        VideoCodec videoCodec;
        if (_vcm.Codec(kVideoCodecVP8, &videoCodec) != VCM_OK)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning,
                         webrtc::kTraceVideo,
                         ViEId(_engineId, _channelId),
                         "%s: VCM: failure geting default VP8 plType",
                         __FUNCTION__);
            return -1;
        }
        WebRtc_UWord8 idx = 0;
        // Configure all simulcast modules
        for (std::list<RtpRtcp*>::iterator it = _simulcastRtpRtcp.begin();
             it != _simulcastRtpRtcp.end();
             it++)
        {
            idx++;
            RtpRtcp* rtpRtcp = *it;
            if (rtpRtcp->InitSender() != 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError,
                             webrtc::kTraceVideo,
                             ViEId(_engineId, _channelId),
                             "%s: RTP::InitSender failure",
                            __FUNCTION__);
                return -1;
            }
            if (rtpRtcp->InitReceiver() != 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError,
                             webrtc::kTraceVideo,
                             ViEId(_engineId, _channelId),
                             "%s: RTP::InitReceiver failure",
                             __FUNCTION__);
                return -1;
            }
            if (rtpRtcp->RegisterSendTransport((Transport*) &_vieSender) != 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             ViEId(_engineId, _channelId),
                             "%s: RTP::RegisterSendTransport failure",
                             __FUNCTION__);
                return -1;
            }
            if (_moduleProcessThread.RegisterModule(rtpRtcp) != 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             ViEId(_engineId, _channelId),
                             "%s: RTP::RegisterModule failure", __FUNCTION__);
               return -1;
            }
            if (rtpRtcp->SetRTCPStatus(_rtpRtcp.RTCP()) != 0)
            {
                WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                             ViEId(_engineId, _channelId),
                            "%s: RTP::SetRTCPStatus failure", __FUNCTION__);
            }
            rtpRtcp->DeRegisterSendPayload(videoCodec.plType);
            if (rtpRtcp->RegisterSendPayload(videoCodec) != 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             ViEId(_engineId, _channelId),
                             "%s: could not register payload type",
                             __FUNCTION__);
                return -1;
            }
            if (restartRtp)
            {
                rtpRtcp->SetSendingStatus(true);
            }
            // Configure all simulcast streams min and max bitrates
            const WebRtc_UWord32 streamBitrate = std::min(startBitrate, 
                videoCodec.simulcastStream[idx].maxBitrate);
            startBitrate -= streamBitrate;
            if (rtpRtcp->SetSendBitrate(
                streamBitrate,
                videoCodec.minBitrate,
                videoCodec.simulcastStream[idx].maxBitrate) != 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             ViEId(_engineId, _channelId),
                             "%s: could not set send bitrates",
                             __FUNCTION__);
                return -1;
            }
        }
        _vieReceiver.RegisterSimulcastRtpRtcpModules(_simulcastRtpRtcp);
    } else 
    {
        if (!_simulcastRtpRtcp.empty())
        {
            // delete all simulcast rtp modules
            while (!_simulcastRtpRtcp.empty())
            {
                RtpRtcp* rtpRtcp = _simulcastRtpRtcp.back();
                rtpRtcp->RegisterIncomingRTCPCallback(NULL);
                rtpRtcp->RegisterSendTransport(NULL);
                _moduleProcessThread.DeRegisterModule(rtpRtcp);
                RtpRtcp::DestroyRtpRtcp(rtpRtcp);
                _simulcastRtpRtcp.pop_back();
            }
        }
        // Clear any previus modules
        _vieReceiver.RegisterSimulcastRtpRtcpModules(_simulcastRtpRtcp);

        if (_rtpRtcp.SetSendBitrate(videoCodec.startBitrate * 1000,
                                    videoCodec.minBitrate,
                                    videoCodec.maxBitrate) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                         ViEId(_engineId, _channelId),
                         "%s: could not set send bitrates", __FUNCTION__);
            return -1;
        }
    }
    /* TODO Enable this if H264 is available.
     * This sets the wanted packetization mode.
    if(videoCodec.plType==kVideoCodecH264)
    {
        if (videoCodec.codecSpecific.H264.packetization ==  kH264SingleMode)
        {
            _rtpRtcp.SetH264PacketizationMode (H264_SINGLE_NAL_MODE);
        }
        else
        {
            _rtpRtcp.SetH264PacketizationMode(H264_NON_INTERLEAVED_MODE);
        }
        if (videoCodec.codecSpecific.H264.configParametersSize > 0)
        {
            _rtpRtcp.SetH264SendModeNALU_PPS_SPS(true);
        }
    }*/

    // Don't log this error, no way to check in advance if this plType is
    // registered or not...
    _rtpRtcp.DeRegisterSendPayload(videoCodec.plType);
    if (_rtpRtcp.RegisterSendPayload(videoCodec) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: could not register payload type", __FUNCTION__);
        return -1;
    }
    if (restartRtp)
    {
        _rtpRtcp.SetSendingStatus(true);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// SetReceiveCodec
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetReceiveCodec(const VideoCodec& videoCodec)
{
    // We will not receive simulcast streams so no need to hadle that usecase
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    _rtpRtcp.DeRegisterReceivePayload(videoCodec.plType);
    if (_rtpRtcp.RegisterReceivePayload(videoCodec)
        != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not register receive payload type", __FUNCTION__);
        return -1;
    }

    if (videoCodec.codecType != kVideoCodecRED &&
        videoCodec.codecType != kVideoCodecULPFEC)
    { //Register codec type with VCM. But do not register RED or ULPFEC
        if (_vcm.RegisterReceiveCodec(&videoCodec, _numberOfCores,
                                      _waitForKeyFrame) != VCM_OK)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: Could not register decoder", __FUNCTION__);
            return -1;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetReceiveCodec
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetReceiveCodec(VideoCodec& videoCodec)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_vcm.ReceiveCodec(&videoCodec) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not get receive codec", __FUNCTION__);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterCodecObserver
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::RegisterCodecObserver(ViEDecoderObserver* observer)
{
    CriticalSectionScoped cs(_callbackCritsect);
    if (observer)
    {
        if (_codecObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId), "%s: already added",
                       __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer added", __FUNCTION__);
        _codecObserver = observer;
    }
    else
    {
        if (!_codecObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId), "%s: no observer added",
                       __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer removed", __FUNCTION__);
        _codecObserver = NULL;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterExternalDecoder
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::RegisterExternalDecoder(
    const WebRtc_UWord8 plType, VideoDecoder* decoder,
    bool decoderRender, //Decoder also render
    WebRtc_Word32 renderDelay) // Decode and render delay
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    WebRtc_Word32 result = 0;
    result = _vcm.RegisterExternalDecoder(decoder, plType, decoderRender);
    if (decoderRender && result == 0)
    {
        // Let VCM know how long before the actual render time the decoder needs
        // to get a frame for decoding.
        result = _vcm.SetRenderDelay(renderDelay);
    }
    return result;
}

// ----------------------------------------------------------------------------
// DeRegisterExternalDecoder
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::DeRegisterExternalDecoder(const WebRtc_UWord8 plType)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s plType", __FUNCTION__, plType);

    VideoCodec currentReceiveCodec;
    WebRtc_Word32 result = 0;
    result = _vcm.ReceiveCodec(&currentReceiveCodec);
    if (_vcm.RegisterExternalDecoder(NULL, plType, false) != VCM_OK)
    {
        return -1;
    }

    if (result == 0 && currentReceiveCodec.plType == plType)
    {
        result = _vcm.RegisterReceiveCodec(&currentReceiveCodec,
                                           _numberOfCores, _waitForKeyFrame);
    }

    return result;
}

// ----------------------------------------------------------------------------
// ReceiveCodecStatistics
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::ReceiveCodecStatistics(WebRtc_UWord32& numKeyFrames,
                                                 WebRtc_UWord32& numDeltaFrames)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    VCMFrameCount receivedFrames;
    if (_vcm.ReceivedFrameCount(receivedFrames) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not get received frame information", __FUNCTION__);
        return -1;
    }
    numKeyFrames = receivedFrames.numKeyFrames;
    numDeltaFrames = receivedFrames.numDeltaFrames;
    return 0;
}

WebRtc_UWord32 ViEChannel::DiscardedPackets() const {
  WEBRTC_TRACE(webrtc::kTraceInfo,
               webrtc::kTraceVideo,
               ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);
  return _vcm.DiscardedPackets();
}

// ----------------------------------------------------------------------------
// WaitForKeyFrame
//
// Only affects calls to SetReceiveCodec done after this call.
// Default = false
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::WaitForKeyFrame(bool wait)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s(wait: %d)", __FUNCTION__, wait);

    _waitForKeyFrame = wait;
    return 0;
}

// ----------------------------------------------------------------------------
// SetSignalPacketLossStatus
//
// If enabled, a key frame request will be sent as soon as there are lost
// packets. If onlyKeyFrames are set, requests are only sent for loss in key
// frames.
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetSignalPacketLossStatus(bool enable,
                                                    bool onlyKeyFrames)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s(enable: %d)", __FUNCTION__, enable);

    if (enable)
    {
        if (onlyKeyFrames)
        {
            _vcm.SetVideoProtection(kProtectionKeyOnLoss, false);
            if (_vcm.SetVideoProtection(kProtectionKeyOnKeyLoss, true)
                != VCM_OK)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                                 _channelId),
                           "%s failed %d", __FUNCTION__, enable);
                return -1;
            }
        }
        else
        {
            _vcm.SetVideoProtection(kProtectionKeyOnKeyLoss, false);
            if (_vcm.SetVideoProtection(kProtectionKeyOnLoss, true)
                != VCM_OK)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                                 _channelId),
                           "%s failed %d", __FUNCTION__, enable);
                return -1;
            }
        }
    }
    else
    {
        _vcm.SetVideoProtection(kProtectionKeyOnLoss, false);
        _vcm.SetVideoProtection(kProtectionKeyOnKeyLoss, false);
    }
    return 0;
}

// ============================================================================
// RTP/RTCP
// ============================================================================

// ----------------------------------------------------------------------------
// SetRTCPMode
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetRTCPMode(const RTCPMethod rtcpMode)
{
    WEBRTC_TRACE(webrtc::kTraceInfo,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s: %d", __FUNCTION__, rtcpMode);

    for (std::list<RtpRtcp*>::iterator it = _simulcastRtpRtcp.begin();
         it != _simulcastRtpRtcp.end();
         it++)
    {
        RtpRtcp* rtpRtcp = *it;
        rtpRtcp->SetRTCPStatus(rtcpMode);
    }
    return _rtpRtcp.SetRTCPStatus(rtcpMode);
}

// ----------------------------------------------------------------------------
// GetRTCPMode
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetRTCPMode(RTCPMethod& rtcpMode)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    rtcpMode = _rtpRtcp.RTCP();
    return 0;
}

// ----------------------------------------------------------------------------
// EnableNACKStatus
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetNACKStatus(const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s(enable: %d)", __FUNCTION__, enable);

    // Update the decoding VCM
    if (_vcm.SetVideoProtection(kProtectionNack, enable) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_engineId, _channelId),
                     "%s: Could not set VCM NACK protection: %d", __FUNCTION__,
                     enable);
        return -1;
    }
    if (enable)
    {
        // Disable possible FEC
        SetFECStatus(false, 0, 0);
    }
    // Update the decoding VCM
    if (_vcm.SetVideoProtection(kProtectionNack, enable) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_engineId, _channelId),
                     "%s: Could not set VCM NACK protection: %d", __FUNCTION__,
                     enable);
        return -1;
   }
    return ProcessNACKRequest(enable);
}

WebRtc_Word32 ViEChannel::ProcessNACKRequest(const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s(enable: %d)", __FUNCTION__, enable);

    if (enable)
    {
        // Turn on NACK,
        NACKMethod nackMethod = kNackRtcp;
        if (_rtpRtcp.RTCP() == kRtcpOff)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: Could not enable NACK, RTPC not on ", __FUNCTION__);
            return -1;
        }
        if (_rtpRtcp.SetNACKStatus(nackMethod) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: Could not set NACK method %d", __FUNCTION__,
                       nackMethod);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                     ViEId(_engineId, _channelId),
                      "%s: Using NACK method %d", __FUNCTION__, nackMethod);
        _rtpRtcp.SetStorePacketsStatus(true, kNackHistorySize);

        _vcm.RegisterPacketRequestCallback(this);

        for (std::list<RtpRtcp*>::iterator it = _simulcastRtpRtcp.begin();
             it != _simulcastRtpRtcp.end();
             it++)
        {
            RtpRtcp* rtpRtcp = *it;
            rtpRtcp->SetStorePacketsStatus(true, kNackHistorySize);
        }
    }
    else
    {
        for (std::list<RtpRtcp*>::iterator it = _simulcastRtpRtcp.begin();
             it != _simulcastRtpRtcp.end();
             it++)
        {
            RtpRtcp* rtpRtcp = *it;
            rtpRtcp->SetStorePacketsStatus(false);
        }
        _rtpRtcp.SetStorePacketsStatus(false);
        _vcm.RegisterPacketRequestCallback(NULL);
        if (_rtpRtcp.SetNACKStatus(kNackOff) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideo,
                         ViEId(_engineId, _channelId),
                         "%s: Could not turn off NACK", __FUNCTION__);
            return -1;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// SetFECStatus
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetFECStatus(const bool enable,
                                       const unsigned char payloadTypeRED,
                                       const unsigned char payloadTypeFEC)
{
    // Disable possible NACK
    if (enable)
    {
        SetNACKStatus(false);
    }

    return ProcessFECRequest(enable, payloadTypeRED, payloadTypeFEC);

}
WebRtc_Word32
ViEChannel::ProcessFECRequest(const bool enable,
                              const unsigned char payloadTypeRED,
                              const unsigned char payloadTypeFEC)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s(enable: %d, payloadTypeRED: %u, payloadTypeFEC: %u)",
                 __FUNCTION__, enable, payloadTypeRED, payloadTypeFEC);

    if (_rtpRtcp.SetGenericFECStatus(enable, payloadTypeRED, payloadTypeFEC)
        != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not change FEC status to %d", __FUNCTION__,
                   enable);
        return -1;
    }
    for (std::list<RtpRtcp*>::iterator it = _simulcastRtpRtcp.begin();
         it != _simulcastRtpRtcp.end();
         it++)
    {
        RtpRtcp* rtpRtcp = *it;
        rtpRtcp->SetGenericFECStatus(enable, payloadTypeRED, payloadTypeFEC);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// EnableNACKFECStatus
// ----------------------------------------------------------------------------

WebRtc_Word32
ViEChannel::SetHybridNACKFECStatus(const bool enable,
                                   const unsigned char payloadTypeRED,
                                   const unsigned char payloadTypeFEC)
{
    // Update the decoding VCM with hybrid mode
    if (_vcm.SetVideoProtection(kProtectionNackFEC, enable) != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not set VCM NACK protection: %d", __FUNCTION__,
                   enable);
        return -1;
    }

    WebRtc_Word32 retVal = 0;
    retVal = ProcessNACKRequest(enable);
    if (retVal < 0)
    {
        return retVal;
    }
    return ProcessFECRequest(enable, payloadTypeRED, payloadTypeFEC);
}

// ----------------------------------------------------------------------------
// KeyFrameRequestMethod
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetKeyFrameRequestMethod(
    const KeyFrameRequestMethod method)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: %d", __FUNCTION__, method);

    return _rtpRtcp.SetKeyFrameRequestMethod(method);
}

// ----------------------------------------------------------------------------
// EnableTMMBR
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::EnableTMMBR(const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: %d", __FUNCTION__, enable);

    return _rtpRtcp.SetTMMBRStatus(enable);
}

// ----------------------------------------------------------------------------
// EnableKeyFrameRequestCallback
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::EnableKeyFrameRequestCallback(const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: %d", __FUNCTION__, enable);

    CriticalSectionScoped cs(_callbackCritsect);
    if (enable && _codecObserver == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: No ViECodecObserver set", __FUNCTION__, enable);
        return -1;
    }
    _doKeyFrameCallbackRequest = enable;
    return 0;

}

// ----------------------------------------------------------------------------
// SetSSRC
//
// Sets SSRC for outgoing stream
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetSSRC(const WebRtc_UWord32 SSRC,
                                  const StreamType /*usage*/,
                                  const unsigned char simulcastIdx)
{
    // TODO(pwestin) add support for streamType when we add RTX
    WEBRTC_TRACE(webrtc::kTraceInfo,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s(SSRC: %u, idx:%u)",
                 __FUNCTION__, SSRC, simulcastIdx);

    if (simulcastIdx  == 0)
    {
        return _rtpRtcp.SetSSRC(SSRC);
    }
    std::list<RtpRtcp*>::const_iterator it = _simulcastRtpRtcp.begin();
    for (int i = 1; i < simulcastIdx; i++)
    {
       it++;
       if (it == _simulcastRtpRtcp.end())
       {
           return -1;
       }
    }
    RtpRtcp* rtpRtcp = *it;
    return rtpRtcp->SetSSRC(SSRC);
}

// ----------------------------------------------------------------------------
// SetSSRC
//
// Gets SSRC for outgoing stream
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetLocalSSRC(WebRtc_UWord32& SSRC)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    SSRC = _rtpRtcp.SSRC();
    return 0;
}

// ----------------------------------------------------------------------------
// GetRemoteSSRC
//
// Gets SSRC for the incoming stream
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetRemoteSSRC(WebRtc_UWord32& SSRC)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    SSRC = _rtpRtcp.RemoteSSRC();
    return 0;
}

// ----------------------------------------------------------------------------
// GetRemoteCSRC
//
// Gets the CSRC for the incoming stream
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetRemoteCSRC(unsigned int CSRCs[kRtpCsrcSize])
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    WebRtc_UWord32 arrayCSRC[kRtpCsrcSize];
    memset(arrayCSRC, 0, sizeof(arrayCSRC));

    WebRtc_Word32 numCSRCs = _rtpRtcp.RemoteCSRCs(arrayCSRC);
    if (numCSRCs > 0)
    {
        memcpy(CSRCs, arrayCSRC, numCSRCs * sizeof(WebRtc_UWord32));
        for (int idx = 0; idx < numCSRCs; idx++)
        {
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId), "\tCSRC[%d] = %lu", idx,
                       CSRCs[idx]);
        }
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: CSRC list is empty", __FUNCTION__);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// SetStartSequenceNumber
//
// Sets the starting sequence number, must be called before StartSend.
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetStartSequenceNumber(WebRtc_UWord16 sequenceNumber)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_rtpRtcp.Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: already sending",
                   __FUNCTION__);
        return -1;
    }
    return _rtpRtcp.SetSequenceNumber(sequenceNumber);
}

// ----------------------------------------------------------------------------
// SetRTCPCName
//
// Sets the CName for the outgoing stream on the channel
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetRTCPCName(const WebRtc_Word8 rtcpCName[])
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_rtpRtcp.Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: already sending",
                   __FUNCTION__);
        return -1;
    }
    return _rtpRtcp.SetCNAME(rtcpCName);
}

// ----------------------------------------------------------------------------
// GetRTCPCName
//
// Gets the CName for the outgoing stream on the channel
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetRTCPCName(WebRtc_Word8 rtcpCName[])
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    return _rtpRtcp.CNAME(rtcpCName);
}

// ----------------------------------------------------------------------------
// GetRemoteRTCPCName
//
// Gets the CName of the incoming stream
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetRemoteRTCPCName(WebRtc_Word8 rtcpCName[])
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    WebRtc_UWord32 remoteSSRC = _rtpRtcp.RemoteSSRC();
    return _rtpRtcp.RemoteCNAME(remoteSSRC, rtcpCName);
}

// ----------------------------------------------------------------------------
// RegisterRtpObserver
//
// Registers an RTP observer
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::RegisterRtpObserver(ViERTPObserver* observer)
{
    CriticalSectionScoped cs(_callbackCritsect);
    if (observer)
    {
        if (_rtpObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: observer alread added", __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer added", __FUNCTION__);
        _rtpObserver = observer;
    }
    else
    {
        if (!_rtpObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId), "%s: no observer added",
                       __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer removed", __FUNCTION__);
        _rtpObserver = NULL;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterRtcpObserver
//
// Registers an RTPC observer
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::RegisterRtcpObserver(ViERTCPObserver* observer)
{
    CriticalSectionScoped cs(_callbackCritsect);
    if (observer)
    {
        if (_rtcpObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: observer alread added", __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer added", __FUNCTION__);
        _rtcpObserver = observer;
    }
    else
    {
        if (!_rtcpObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId), "%s: no observer added",
                       __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer removed", __FUNCTION__);
        _rtcpObserver = NULL;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// SendApplicationDefinedRTCPPacket
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SendApplicationDefinedRTCPPacket(
    const WebRtc_UWord8 subType, WebRtc_UWord32 name, const WebRtc_UWord8* data,
    WebRtc_UWord16 dataLengthInBytes)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);
    // Sanity checks
    if (!_rtpRtcp.Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: not sending",
                   __FUNCTION__);
        return -1;
    }
    if (data == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: no input argument",
                   __FUNCTION__);
        return -1;
    }
    if (dataLengthInBytes % 4 != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: input length error",
                   __FUNCTION__);
        return -1;
    }
    RTCPMethod rtcpMethod = _rtpRtcp.RTCP();
    if (rtcpMethod == kRtcpOff)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: RTCP not enabled",
                   __FUNCTION__);
        return -1;
    }
    // Create and send packet
    if (_rtpRtcp.SetRTCPApplicationSpecificData(subType, name, data,
                                                dataLengthInBytes) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not send RTCP application data", __FUNCTION__);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetSendRtcpStatistics
//
// Gets statistics sent in RTCP packets to remote side
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetSendRtcpStatistics(WebRtc_UWord16& fractionLost,
                                                WebRtc_UWord32& cumulativeLost,
                                                WebRtc_UWord32& extendedMax,
                                                WebRtc_UWord32& jitterSamples,
                                                WebRtc_Word32& rttMs)
{
    WEBRTC_TRACE(webrtc::kTraceInfo,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s", __FUNCTION__);

    /*
        TODO(pwestin) how do we do this for simulcast? average for all 
        except cumulativeLost that is the sum?
    for (std::list<RtpRtcp*>::const_iterator it = _simulcastRtpRtcp.begin();
         it != _simulcastRtpRtcp.end();
         it++)
    {
        RtpRtcp* rtpRtcp = *it;
    }
    */
    WebRtc_UWord32 remoteSSRC = _rtpRtcp.RemoteSSRC();

    RTCPReportBlock remoteStat;
    if (_rtpRtcp.RemoteRTCPStat(remoteSSRC, &remoteStat) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not get remote stats", __FUNCTION__);
        return -1;
    }
    fractionLost = remoteStat.fractionLost;
    cumulativeLost = remoteStat.cumulativeLost;
    extendedMax = remoteStat.extendedHighSeqNum;
    jitterSamples = remoteStat.jitter;

    WebRtc_UWord16 dummy;
    WebRtc_UWord16 rtt = 0;
    if (_rtpRtcp.RTT(remoteSSRC, &rtt, &dummy, &dummy, &dummy) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Could not get RTT",
                   __FUNCTION__);
        return -1;
    }
    rttMs = rtt;
    return 0;
}

// ----------------------------------------------------------------------------
// GetSendRtcpStatistics
//
// Gets statistics received in RTCP packets from remote side
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetReceivedRtcpStatistics(
    WebRtc_UWord16& fractionLost, WebRtc_UWord32& cumulativeLost,
    WebRtc_UWord32& extendedMax, WebRtc_UWord32& jitterSamples,
    WebRtc_Word32& rttMs)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    WebRtc_UWord8 fracLost = 0;
    if (_rtpRtcp.StatisticsRTP(&fracLost, &cumulativeLost, &extendedMax,
                               &jitterSamples) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not get received RTP statistics", __FUNCTION__);
        return -1;
    }
    fractionLost = fracLost;

    WebRtc_UWord32 remoteSSRC = _rtpRtcp.RemoteSSRC();
    WebRtc_UWord16 dummy = 0;
    WebRtc_UWord16 rtt = 0;
    if (_rtpRtcp.RTT(remoteSSRC, &rtt, &dummy, &dummy, &dummy) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Could not get RTT",
                   __FUNCTION__);
        return -1;
    }
    rttMs = rtt;
    return 0;
}

// ----------------------------------------------------------------------------
// GetRtpStatistics
//
// Gets sent/received packets statistics
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetRtpStatistics(
    WebRtc_UWord32& bytesSent,
    WebRtc_UWord32& packetsSent,
    WebRtc_UWord32& bytesReceived,
    WebRtc_UWord32& packetsReceived) const
{
    WEBRTC_TRACE(webrtc::kTraceInfo,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                  "%s", __FUNCTION__);

    if (_rtpRtcp.DataCountersRTP(&bytesSent,
                                 &packetsSent,
                                 &bytesReceived,
                                 &packetsReceived) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_engineId, _channelId),
                     "%s: Could not get counters", __FUNCTION__);
        return -1;
    }
    for (std::list<RtpRtcp*>::const_iterator it = _simulcastRtpRtcp.begin();
         it != _simulcastRtpRtcp.end();
         it++)
    {
        WebRtc_UWord32 bytesSentTemp = 0;
        WebRtc_UWord32 packetsSentTemp = 0;
        RtpRtcp* rtpRtcp = *it;
        rtpRtcp->DataCountersRTP(&bytesSentTemp, &packetsSentTemp, NULL, NULL);
        bytesSent += bytesSentTemp;
        packetsSent += packetsSentTemp;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// SetKeepAliveStatus
//
// Enables/disbles RTP keeoalive
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetKeepAliveStatus(
    const bool enable, const WebRtc_Word8 unknownPayloadType,
    const WebRtc_UWord16 deltaTransmitTimeMS)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (enable && _rtpRtcp.RTPKeepalive())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP keepalive already enabled", __FUNCTION__);
        return -1;
    }
    else if (!enable && !_rtpRtcp.RTPKeepalive())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: RTP keepalive already disabled", __FUNCTION__);
        return -1;
    }

    if (_rtpRtcp.SetRTPKeepaliveStatus(enable, unknownPayloadType,
                                       deltaTransmitTimeMS) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not set RTP keepalive status %d", __FUNCTION__,
                   enable);
        if (enable == false && !_rtpRtcp.DefaultModuleRegistered())
        {
            // Not sending media and we try to disable keep alive
            _rtpRtcp.ResetSendDataCountersRTP();
            _rtpRtcp.SetSendingStatus(false);
        }
        return -1;
    }

    if (enable && !_rtpRtcp.Sending())
    {
        // Enable sending to start sending Sender reports instead of receive
        // reports
        if (_rtpRtcp.SetSendingStatus(true) != 0)
        {
            _rtpRtcp.SetRTPKeepaliveStatus(false, 0, 0);
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: Could not start sending", __FUNCTION__);
            return -1;
        }
    }
    else if (!enable && !_rtpRtcp.SendingMedia())
    {
        // Not sending media and we're disabling keep alive
        _rtpRtcp.ResetSendDataCountersRTP();
        if (_rtpRtcp.SetSendingStatus(false) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: Could not stop sending", __FUNCTION__);
            return -1;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetKeepAliveStatus
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetKeepAliveStatus(
    bool& enabled, WebRtc_Word8& unknownPayloadType,
    WebRtc_UWord16& deltaTransmitTimeMs)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);
    if (_rtpRtcp.RTPKeepaliveStatus(&enabled, &unknownPayloadType,
                                    &deltaTransmitTimeMs) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not get RTP keepalive status", __FUNCTION__);
        return -1;
    }
    WEBRTC_TRACE(
        webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
        "%s: enabled = %d, unknownPayloadType = %d, deltaTransmitTimeMs = %ul",
        __FUNCTION__, enabled, (WebRtc_Word32) unknownPayloadType,
        deltaTransmitTimeMs);

    return 0;
}

// ----------------------------------------------------------------------------
// StartRTPDump
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::StartRTPDump(const char fileNameUTF8[1024],
                                       RTPDirections direction)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (direction != kRtpIncoming && direction != kRtpOutgoing)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: invalid input",
                   __FUNCTION__);
        return -1;
    }

    if (direction == kRtpIncoming)
    {
        return _vieReceiver.StartRTPDump(fileNameUTF8);
    }
    else
    {
        return _vieSender.StartRTPDump(fileNameUTF8);
    }
}

// ----------------------------------------------------------------------------
// StopRTPDump
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::StopRTPDump(RTPDirections direction)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (direction != kRtpIncoming && direction != kRtpOutgoing)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: invalid input",
                   __FUNCTION__);
        return -1;
    }

    if (direction == kRtpIncoming)
    {
        return _vieReceiver.StopRTPDump();
    }
    else
    {
        return _vieSender.StopRTPDump();
    }
}

// ============================================================================
// Network
// ============================================================================

// ----------------------------------------------------------------------------
// SetLocalReceiver
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetLocalReceiver(const WebRtc_UWord16 rtpPort,
                                           const WebRtc_UWord16 rtcpPort,
                                           const WebRtc_Word8* ipAddress)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    _callbackCritsect.Enter();
    if (_ptrExternalTransport)
    {
        _callbackCritsect.Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: external transport registered", __FUNCTION__);
        return -1;
    }
    _callbackCritsect.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.Receiving())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: already receiving",
                   __FUNCTION__);
        return -1;
    }

    const WebRtc_Word8* multicastIpAddress = NULL;
    if (_socketTransport.InitializeReceiveSockets(&_vieReceiver, rtpPort,
                                                  ipAddress,
                                                  multicastIpAddress, rtcpPort)
        != 0)
    {
        WebRtc_Word32 socketError = _socketTransport.LastError();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: could not initialize receive sockets. Socket error: %d",
                   __FUNCTION__, socketError);
        return -1;
    }

    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// GetLocalReceiver
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetLocalReceiver(WebRtc_UWord16& rtpPort,
                                           WebRtc_UWord16& rtcpPort,
                                           WebRtc_Word8* ipAddress) const
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    _callbackCritsect.Enter();
    if (_ptrExternalTransport)
    {
        _callbackCritsect.Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: external transport registered", __FUNCTION__);
        return -1;
    }
    _callbackCritsect.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.ReceiveSocketsInitialized() == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: receive sockets not initialized", __FUNCTION__);
        return -1;
    }

    WebRtc_Word8 multicastIpAddress[UdpTransport::
                                    kIpAddressVersion6Length];
    if (_socketTransport.ReceiveSocketInformation(ipAddress, rtpPort, rtcpPort,
                                                  multicastIpAddress) != 0)
    {
        WebRtc_Word32 socketError = _socketTransport.LastError();
        WEBRTC_TRACE(
            webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
            "%s: could not get receive socket information. Socket error: %d",
            __FUNCTION__, socketError);
        return -1;
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// SetSendDestination
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetSendDestination(
    const WebRtc_Word8* ipAddress, const WebRtc_UWord16 rtpPort,
    const WebRtc_UWord16 rtcpPort, const WebRtc_UWord16 sourceRtpPort,
    const WebRtc_UWord16 sourceRtcpPort)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    _callbackCritsect.Enter();
    if (_ptrExternalTransport)
    {
        _callbackCritsect.Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: external transport registered", __FUNCTION__);
        return -1;
    }
    _callbackCritsect.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    const bool isIPv6 = _socketTransport.IpV6Enabled();
    if (UdpTransport::IsIpAddressValid(ipAddress, isIPv6) == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Not a valid IP address: %s", __FUNCTION__, ipAddress);
        return -1;
    }
    if (_socketTransport.InitializeSendSockets(ipAddress, rtpPort, rtcpPort)
        != 0)
    {
        WebRtc_Word32 socketError = _socketTransport.LastError();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: could not initialize send socket. Socket error: %d",
                   __FUNCTION__, socketError);
        return -1;
    }

    if (sourceRtpPort != 0)
    {
        WebRtc_UWord16 receiveRtpPort = 0;
        WebRtc_UWord16 receiveRtcpPort = 0;
        if (_socketTransport.ReceiveSocketInformation(NULL, receiveRtpPort,
                                                      receiveRtcpPort, NULL)
            != 0)
        {
            WebRtc_Word32 socketError = _socketTransport.LastError();
            WEBRTC_TRACE(
                webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                "%s: could not get receive port information. Socket error: %d",
                __FUNCTION__, socketError);
            return -1;
        }
        // Initialize an extra socket only if send port differs from receive
        // port
        if (sourceRtpPort != receiveRtpPort)
        {
            if (_socketTransport.InitializeSourcePorts(sourceRtpPort,
                                                       sourceRtcpPort) != 0)
            {
                WebRtc_Word32 socketError = _socketTransport.LastError();
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                           ViEId(_engineId, _channelId),
                           "%s: could not set source ports. Socket error: %d",
                           __FUNCTION__, socketError);
                return -1;
            }
        }
    }
    _vieSender.RegisterSendTransport(&_socketTransport);

    // Workaround to avoid SSRC colision detection in loppback tests
    if (!isIPv6)
    {
        WebRtc_UWord32 localHostAddress = 0;
        const WebRtc_UWord32 currentIpAddress =
            UdpTransport::InetAddrIPV4(ipAddress);

        if ((UdpTransport::LocalHostAddress(localHostAddress) == 0
            && localHostAddress == currentIpAddress) ||
            strncmp("127.0.0.1", ipAddress, 9) == 0)
        {
            _rtpRtcp.SetSSRC(0xFFFFFFFF);
            WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "Running in loopback. Forcing fixed SSRC");
        }
    }
    else
    {
        WebRtc_UWord8 localHostAddress[16];
        WebRtc_UWord8 currentIpAddress[16];

        WebRtc_Word32 convResult =
            UdpTransport::LocalHostAddressIPV6(localHostAddress);
        convResult
            += _socketTransport.InetPresentationToNumeric(23, ipAddress,
                                                          currentIpAddress);
        if (convResult == 0)
        {
            bool localHost = true;
            for (WebRtc_Word32 i = 0; i < 16; i++)
            {
                if (localHostAddress[i] != currentIpAddress[i])
                {
                    localHost = false;
                    break;
                }
            }
            if (!localHost)
            {
                localHost = true;
                for (WebRtc_Word32 i = 0; i < 15; i++)
                {
                    if (currentIpAddress[i] != 0)
                    {
                        localHost = false;
                        break;
                    }
                }
                if (localHost == true && currentIpAddress[15] != 1)
                {
                    localHost = false;
                }
            }
            if (localHost)
            {
                _rtpRtcp.SetSSRC(0xFFFFFFFF);
                WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
                           ViEId(_engineId, _channelId),
                           "Running in loopback. Forcing fixed SSRC");
            }
        }
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// GetSendDestination
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetSendDestination(
    WebRtc_Word8* ipAddress, WebRtc_UWord16& rtpPort, WebRtc_UWord16& rtcpPort,
    WebRtc_UWord16& sourceRtpPort, WebRtc_UWord16& sourceRtcpPort) const
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    _callbackCritsect.Enter();
    if (_ptrExternalTransport)
    {
        _callbackCritsect.Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: external transport registered", __FUNCTION__);
        return -1;
    }
    _callbackCritsect.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.SendSocketsInitialized() == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: send sockets not initialized", __FUNCTION__);
        return -1;
    }
    if (_socketTransport.SendSocketInformation(ipAddress, rtpPort, rtcpPort)
        != 0)
    {
        WebRtc_Word32 socketError = _socketTransport.LastError();
        WEBRTC_TRACE(
            webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
            "%s: could not get send socket information. Socket error: %d",
            __FUNCTION__, socketError);
        return -1;
    }
    sourceRtpPort = 0;
    sourceRtcpPort = 0;
    if (_socketTransport.SourcePortsInitialized())
    {
        _socketTransport.SourcePorts(sourceRtpPort, sourceRtcpPort);
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// StartSend
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::StartSend()
{
    CriticalSectionScoped cs(_callbackCritsect);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_ptrExternalTransport)
    {
        if (_socketTransport.SendSocketsInitialized() == false)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId,_channelId),
                       "%s: send sockets not initialized", __FUNCTION__);
            return -1;
        }
    }
#endif
    _rtpRtcp.SetSendingMediaStatus(true);

    if (_rtpRtcp.Sending() && !_rtpRtcp.RTPKeepalive())
    {
        if (_rtpRtcp.RTPKeepalive())
        {
            // Sending Keep alive, don't trigger an error
            return 0;
        }
        // Already sending
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Already sending",
                   __FUNCTION__);
        return kViEBaseAlreadySending;
    }
    if (_rtpRtcp.SetSendingStatus(true) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not start sending RTP", __FUNCTION__);
        return -1;
    }
    for (std::list<RtpRtcp*>::const_iterator it = _simulcastRtpRtcp.begin();
         it != _simulcastRtpRtcp.end();
         it++)
    {
        RtpRtcp* rtpRtcp = *it;
        rtpRtcp->SetSendingMediaStatus(true);
        rtpRtcp->SetSendingStatus(true);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// StopSend
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::StopSend()
{
    WEBRTC_TRACE(webrtc::kTraceInfo,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s", __FUNCTION__);

    _rtpRtcp.SetSendingMediaStatus(false);
    for (std::list<RtpRtcp*>::iterator it = _simulcastRtpRtcp.begin();
         it != _simulcastRtpRtcp.end();
         it++)
    {
        RtpRtcp* rtpRtcp = *it;
        rtpRtcp->SetSendingMediaStatus(false);
    }
    if (_rtpRtcp.RTPKeepalive())
    {
        // Don't turn off sending since we'll send keep alive packets
        return 0;
    }
    if (!_rtpRtcp.Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Not sending",
                   __FUNCTION__);
        return kViEBaseNotSending;
    }
    // Reset
    _rtpRtcp.ResetSendDataCountersRTP();
    if (_rtpRtcp.SetSendingStatus(false) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: could not stop RTP sending", __FUNCTION__);
        return -1;
    }
    for (std::list<RtpRtcp*>::iterator it = _simulcastRtpRtcp.begin();
         it != _simulcastRtpRtcp.end();
         it++)
    {
        RtpRtcp* rtpRtcp = *it;
        rtpRtcp->ResetSendDataCountersRTP();
        rtpRtcp->SetSendingStatus(false);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Sending
// ----------------------------------------------------------------------------

bool ViEChannel::Sending()
{
    return _rtpRtcp.Sending();
}

// ----------------------------------------------------------------------------
// StartReceive
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::StartReceive()
{
    CriticalSectionScoped cs(_callbackCritsect);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_ptrExternalTransport)
    {
        if (_socketTransport.Receiving())
        {
            // Warning, don't return error
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId), "%s: already receiving",
                       __FUNCTION__);
            return 0;
        }
        if (_socketTransport.ReceiveSocketsInitialized() == false)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: receive sockets not initialized", __FUNCTION__);
            return -1;
        }
        if (_socketTransport.StartReceiving(kViENumReceiveSocketBuffers)
            != 0)
        {
            WebRtc_Word32 socketError = _socketTransport.LastError();
            WEBRTC_TRACE(
                webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                "%s: could not get receive socket information. Socket error:%d",
                __FUNCTION__, socketError);
            return -1;
        }
    }
#endif
    if (StartDecodeThread() != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: could not start decoder thread", __FUNCTION__);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
        _socketTransport.StopReceiving();
#endif
        _vieReceiver.StopReceive();
        return -1;
    }
    _vieReceiver.StartReceive();

    return 0;
}

// ----------------------------------------------------------------------------
// StopReceive
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::StopReceive()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    _vieReceiver.StopReceive();
    StopDecodeThread();
    _vcm.ResetDecoder();
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (_ptrExternalTransport)
        {
            return 0;
        }
    }

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.Receiving() == false)
    {
        // Warning, don't return error
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: not receiving",
                   __FUNCTION__);
        return 0;
    }
    if (_socketTransport.StopReceiving() != 0)
    {
        WebRtc_Word32 socketError = _socketTransport.LastError();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: Socket error: %d", __FUNCTION__, socketError);
        return -1;
    }
#endif

    return 0;
}

bool ViEChannel::Receiving()
{
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    return _socketTransport.Receiving();
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------
// GetSourceInfo
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetSourceInfo(WebRtc_UWord16& rtpPort,
                                        WebRtc_UWord16& rtcpPort,
                                        WebRtc_Word8* ipAddress,
                                        WebRtc_UWord32 ipAddressLength)
{
    {
        CriticalSectionScoped cs(_callbackCritsect);
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s", __FUNCTION__);

        if (_ptrExternalTransport)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: external transport registered", __FUNCTION__);
            return -1;
        }
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT

    if (_socketTransport.IpV6Enabled() && ipAddressLength
        < UdpTransport::kIpAddressVersion6Length)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: IP address length is too small for IPv6", __FUNCTION__);
        return -1;
    }
    else if (ipAddressLength <
             UdpTransport::kIpAddressVersion4Length)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: IP address length is too small for IPv4", __FUNCTION__);
        return -1;
    }

    if (_socketTransport.RemoteSocketInformation(ipAddress, rtpPort, rtcpPort)
        != 0)
    {
        WebRtc_Word32 socketError = _socketTransport.LastError();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Error getting source ports. Socket error: %d",
                   __FUNCTION__, socketError);
        return -1;
    }

    return 0;
#else

    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
               ViEId(_engineId, _channelId),
               "%s: not available for external transport",
               __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// RegisterSendTransport
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::RegisterSendTransport(Transport& transport)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.SendSocketsInitialized()
        || _socketTransport.ReceiveSocketsInitialized())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s:  socket transport already initialized",
                   __FUNCTION__);
        return -1;
    }
#endif

    if (_rtpRtcp.Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Sending", __FUNCTION__);
        return -1;
    }

    CriticalSectionScoped cs(_callbackCritsect);
    if (_ptrExternalTransport)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: transport already registered", __FUNCTION__);
        return -1;
    }
    _ptrExternalTransport = &transport;
    _vieSender.RegisterSendTransport(&transport);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: Transport registered: 0x%p", __FUNCTION__,
               &_ptrExternalTransport);

    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterSendTransport
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::DeregisterSendTransport()
{
    CriticalSectionScoped cs(_callbackCritsect);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_ptrExternalTransport == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: no transport registered", __FUNCTION__);
        return -1;
    }
    if (_rtpRtcp.Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Sending", __FUNCTION__);
        return -1;
    }
    _ptrExternalTransport = NULL;
    _vieSender.DeregisterSendTransport();
    return 0;
}

// ----------------------------------------------------------------------------
// ReceivedRTPPacket
//
// Incoming packet from external transport
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::ReceivedRTPPacket(const void* rtpPacket,
                                            const WebRtc_Word32 rtpPacketLength)
{
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (!_ptrExternalTransport)
        {
            return -1;
        }
    }
    return _vieReceiver.ReceivedRTPPacket(rtpPacket, rtpPacketLength);
}

// ----------------------------------------------------------------------------
// ReceivedRTPPacket
//
// Incoming packet from external transport
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::ReceivedRTCPPacket(
    const void* rtcpPacket, const WebRtc_Word32 rtcpPacketLength)
{
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (!_ptrExternalTransport)
        {
            return -1;
        }
    }
    return _vieReceiver.ReceivedRTCPPacket(rtcpPacket, rtcpPacketLength);
}

// ----------------------------------------------------------------------------
// EnableIPv6
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::EnableIPv6()
{
    _callbackCritsect.Enter();
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_ptrExternalTransport)
    {
        _callbackCritsect.Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: External transport registered", __FUNCTION__);
        return -1;
    }
    _callbackCritsect.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.IpV6Enabled())
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(_engineId,
                                                           _channelId),
                   "%s: IPv6 already enabled", __FUNCTION__);
        return -1;
    }

    if (_socketTransport.EnableIpV6() != 0)
    {
        WebRtc_Word32 socketError = _socketTransport.LastError();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: could not enable IPv6. Socket error: %d", __FUNCTION__,
                   socketError);
        return -1;
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
               ViEId(_engineId, _channelId),
               "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif

}

// ----------------------------------------------------------------------------
// IsIPv6Enabled
// ----------------------------------------------------------------------------

bool ViEChannel::IsIPv6Enabled()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (_ptrExternalTransport)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: External transport registered", __FUNCTION__);
            return false;
        }
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    return _socketTransport.IpV6Enabled();
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return false;
#endif
}

// ----------------------------------------------------------------------------
// SetSourceFilter
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetSourceFilter(const WebRtc_UWord16 rtpPort,
                                          const WebRtc_UWord16 rtcpPort,
                                          const WebRtc_Word8* ipAddress)
{
    _callbackCritsect.Enter();
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_ptrExternalTransport)
    {
        _callbackCritsect.Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: External transport registered", __FUNCTION__);
        return -1;
    }
    _callbackCritsect.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.SetFilterIP(ipAddress) != 0)
    {
        // Logging done in module
        return -1;
    }
    if (_socketTransport.SetFilterPorts(rtpPort, rtcpPort) != 0)
    {
        // Logging done
        return -1;
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
               ViEId(_engineId, _channelId),
               "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// GetSourceFilter
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetSourceFilter(WebRtc_UWord16& rtpPort,
                                          WebRtc_UWord16& rtcpPort,
                                          WebRtc_Word8* ipAddress) const
{
    _callbackCritsect.Enter();
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_ptrExternalTransport)
    {
        _callbackCritsect.Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: External transport registered", __FUNCTION__);
        return -1;
    }
    _callbackCritsect.Leave();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.FilterIP(ipAddress) != 0)
    {
        // Logging done in module
        return -1;
    }
    if (_socketTransport.FilterPorts(rtpPort, rtcpPort) != 0)
    {
        // Logging done in module
        return -1;
    }

    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// SetToS
// ----------------------------------------------------------------------------

// ToS
WebRtc_Word32 ViEChannel::SetToS(const WebRtc_Word32 DSCP,
                                 const bool useSetSockOpt)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (_ptrExternalTransport)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: External transport registered", __FUNCTION__);
            return -1;
        }
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.SetToS(DSCP, useSetSockOpt) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Socket error: %d",
                   __FUNCTION__, _socketTransport.LastError());
        return -1;
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// GetToS
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetToS(WebRtc_Word32& DSCP, bool& useSetSockOpt) const
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (_ptrExternalTransport)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: External transport registered", __FUNCTION__);
            return -1;
        }
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.ToS(DSCP, useSetSockOpt) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Socket error: %d",
                   __FUNCTION__, _socketTransport.LastError());
        return -1;
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// SetSendGQoS
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetSendGQoS(const bool enable,
                                      const WebRtc_Word32 serviceType,
                                      const WebRtc_UWord32 maxBitrate,
                                      const WebRtc_Word32 overrideDSCP)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (_ptrExternalTransport)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: External transport registered", __FUNCTION__);
            return -1;
        }
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.SetQoS(enable, serviceType, maxBitrate, overrideDSCP,
                                false) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Socket error: %d",
                   __FUNCTION__, _socketTransport.LastError());
        return -1;
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// GetSendGQoS
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::GetSendGQoS(bool& enabled,
                                      WebRtc_Word32& serviceType,
                                      WebRtc_Word32& overrideDSCP) const
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (_ptrExternalTransport)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: External transport registered", __FUNCTION__);
            return -1;
        }
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_socketTransport.QoS(enabled, serviceType, overrideDSCP) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Socket error: %d",
                   __FUNCTION__, _socketTransport.LastError());
        return -1;
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// SetMTU
//
// Sets the maximum transfer unit size for the network link, i.e. including
// IP, UDP[/TCP] and RTP headers,
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetMTU(WebRtc_UWord16 mtu)
{
    WEBRTC_TRACE(webrtc::kTraceInfo,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s", __FUNCTION__);

    if (_rtpRtcp.SetMaxTransferUnit(mtu) != 0)
    {
        // Logging done
        return -1;
    }
    for (std::list<RtpRtcp*>::iterator it = _simulcastRtpRtcp.begin();
         it != _simulcastRtpRtcp.end();
         it++)
    {
        RtpRtcp* rtpRtcp = *it;
        rtpRtcp->SetMaxTransferUnit(mtu);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetMaxDataPayloadLength
//
// maxDataPayloadLength: maximum allowed payload size, i.e. the maximum
// allowed size of encoded data in each packet.
// ----------------------------------------------------------------------------

WebRtc_UWord16 ViEChannel::MaxDataPayloadLength() const
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    return _rtpRtcp.MaxDataPayloadLength();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// SetPacketTimeoutNotification
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::SetPacketTimeoutNotification(
                                                       bool enable,
                                                       WebRtc_UWord32 timeoutSeconds)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (enable)
    {
        WebRtc_UWord32 timeoutMs = 1000 * timeoutSeconds;
        if (_rtpRtcp.SetPacketTimeout(timeoutMs, 0) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId), "%s",
                       __FUNCTION__);
            return -1;
        }
    }
    else
    {
        if (_rtpRtcp.SetPacketTimeout(0, 0) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId,_channelId), "%s",
                       __FUNCTION__);
            return -1;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterObserver
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::RegisterNetworkObserver(
    ViENetworkObserver* observer)
{
    CriticalSectionScoped cs(_callbackCritsect);
    if (observer)
    {
        if (_networkObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: observer alread added", __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer added", __FUNCTION__);
        _networkObserver = observer;
    }
    else
    {
        if (!_networkObserver)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId), "%s: no observer added",
                       __FUNCTION__);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: observer removed", __FUNCTION__);
        _networkObserver = NULL;
    }
    return 0;
}
bool ViEChannel::NetworkObserverRegistered()
{
    CriticalSectionScoped cs(_callbackCritsect);
    return _networkObserver != NULL;
}

// ----------------------------------------------------------------------------
// SetPeriodicDeadOrAliveStatus
// ----------------------------------------------------------------------------
WebRtc_Word32 ViEChannel::SetPeriodicDeadOrAliveStatus(
    const bool enable, const WebRtc_UWord32 sampleTimeSeconds)
{
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    CriticalSectionScoped cs(_callbackCritsect);
    if (_networkObserver == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: no observer added", __FUNCTION__);
        return -1;
    }

    bool enabled = false;
    WebRtc_UWord8 currentSampletimeSeconds = 0;

    // Get old settings
    _rtpRtcp.PeriodicDeadOrAliveStatus(enabled, currentSampletimeSeconds);
    // Set new settings
    if (_rtpRtcp.SetPeriodicDeadOrAliveStatus(
            enable, (WebRtc_UWord8) sampleTimeSeconds) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Could not set periodic dead-or-alive status",
                   __FUNCTION__);
        return -1;
    }
    if (!enable)
    {
        // Restore last utilized sample time.
        // Without this trick, the sample time would always be reset to default
        // (2 sec), each time dead-or-alive was disabled without sample-time
        // parameter.
        _rtpRtcp.SetPeriodicDeadOrAliveStatus(enable, currentSampletimeSeconds);
    }

    return 0;
}

// ----------------------------------------------------------------------------
// SendUDPPacket
// ----------------------------------------------------------------------------
WebRtc_Word32 ViEChannel::SendUDPPacket(const WebRtc_Word8* data,
                                        const WebRtc_UWord32 length,
                                        WebRtc_Word32& transmittedBytes,
                                        bool useRtcpSocket)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (_ptrExternalTransport)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: External transport registered", __FUNCTION__);
            return -1;
        }
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    transmittedBytes = _socketTransport.SendRaw(data, length, useRtcpSocket);
    if (transmittedBytes == -1)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s", __FUNCTION__);
        return -1;
    }
    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
        ViEId(_engineId, _channelId),
        "%s: not available for external transport", __FUNCTION__);
    return -1;
#endif
}

// ============================================================================
// Color enahncement
// ============================================================================

// ----------------------------------------------------------------------------
// EnableColorEnhancement
//
// Enables/disables color enhancement for all decoded frames
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::EnableColorEnhancement(bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s(enable: %d)", __FUNCTION__, enable);

    CriticalSectionScoped cs(_callbackCritsect);
    if (enable && _colorEnhancement)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: Already enabled",
                   __FUNCTION__);
        return -1;
    }
    else if (enable == false && _colorEnhancement == false)

    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: not enabled",
                   __FUNCTION__);
        return -1;
    }
    _colorEnhancement = enable;
    return 0;
}

// ============================================================================
// Register sender
// ============================================================================

// ----------------------------------------------------------------------------
// RegisterSendRtpRtcpModule
//
// Register send RTP RTCP module, which will deliver the frames to send
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::RegisterSendRtpRtcpModule(
    RtpRtcp& sendRtpRtcpModule)
{
    WEBRTC_TRACE(webrtc::kTraceInfo,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s", __FUNCTION__);

    WebRtc_Word32 retVal = _rtpRtcp.RegisterDefaultModule(&sendRtpRtcpModule);
    if (retVal == 0)
    {
        // we need to store this for the SetSendCodec call
        _defaultRtpRtcp = &sendRtpRtcpModule;
    }
    return retVal;
}

// ----------------------------------------------------------------------------
// RegisterSendDeregisterSendRtpRtcpModuleRtpRtcpModule
//
// Deregisters the send RTP RTCP module, which will stop the encoder input to
// the channel
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::DeregisterSendRtpRtcpModule()
{
    WEBRTC_TRACE(webrtc::kTraceInfo,
                 webrtc::kTraceVideo,
                 ViEId(_engineId, _channelId),
                 "%s", __FUNCTION__);

    _defaultRtpRtcp = NULL;

    for (std::list<RtpRtcp*>::const_iterator it = _simulcastRtpRtcp.begin();
         it != _simulcastRtpRtcp.end();
         it++)
    {
        RtpRtcp* rtpRtcp = *it;
        rtpRtcp->DeRegisterDefaultModule();
    }
    return _rtpRtcp.DeRegisterDefaultModule();
}

// ============================================================================
// FrameToRender
// Called by VCM when a frame have been decoded.
// ============================================================================

WebRtc_Word32 ViEChannel::FrameToRender(VideoFrame& videoFrame)
{
    CriticalSectionScoped cs(_callbackCritsect);

    if (_decoderReset)
    {
        // Trigger a callback to the user if the incoming codec has changed.
        if (_codecObserver)
        {
            VideoCodec decoder;
            memset(&decoder, 0, sizeof(decoder));
            if (_vcm.ReceiveCodec(&decoder) == VCM_OK)
            {
                // VCM::ReceiveCodec returns the codec set by
                // RegisterReceiveCodec, which might not be the size we're
                // actually decoding
                decoder.width = (unsigned short) videoFrame.Width();
                decoder.height = (unsigned short) videoFrame.Height();
                _codecObserver->IncomingCodecChanged(_channelId, decoder);
            }
            else
            {
                assert(false);
                WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                           ViEId(_engineId, _channelId),
                           "%s: Could not get receive codec", __FUNCTION__);
            }
        }
        _decoderReset = false;
    }
    if (_effectFilter)
    {
        _effectFilter->Transform(videoFrame.Length(), videoFrame.Buffer(),
                                 videoFrame.TimeStamp(), videoFrame.Width(),
                                 videoFrame.Height());
    }
    if (_colorEnhancement)
    {
        VideoProcessingModule::ColorEnhancement(videoFrame);
    }

    // Record videoframe
    _fileRecorder.RecordVideoFrame(videoFrame);

    WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize];
    WebRtc_Word32 noOfCSRCs = _rtpRtcp.RemoteCSRCs(arrOfCSRC);
    if (noOfCSRCs <= 0)
    {
        arrOfCSRC[0] = _rtpRtcp.RemoteSSRC();
        noOfCSRCs = 1;
    }

    DeliverFrame(videoFrame, noOfCSRCs, arrOfCSRC);

    return 0;
}

WebRtc_Word32 ViEChannel::ReceivedDecodedReferenceFrame(
    const WebRtc_UWord64 pictureId)
{
    return _rtpRtcp.SendRTCPReferencePictureSelection(pictureId);
}

// ============================================================================
// StoreReceivedFrame
// Called by VCM before a frame have been decoded. could be used for recording
// incoming video.
// ============================================================================

WebRtc_Word32 ViEChannel::StoreReceivedFrame(
    const EncodedVideoData& frameToStore)
{
    return 0;
}

// ----------------------------------------------------------------------------
// ReceiveStatistics
//
// Called by VCM with information about received video stream
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::ReceiveStatistics(const WebRtc_UWord32 bitRate,
                                            const WebRtc_UWord32 frameRate)
{
    CriticalSectionScoped cs(_callbackCritsect);
    if (_codecObserver)
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: bitrate %u, framerate %u", __FUNCTION__, bitRate,
                   frameRate);
        _codecObserver->IncomingRate(_channelId, frameRate, bitRate);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// FrameTypeRequest
//
// Called by VCM when a certain frame type is needed to continue decoding
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::FrameTypeRequest(const FrameType frameType)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s(frameType: %d)", __FUNCTION__, frameType);

    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (_codecObserver && _doKeyFrameCallbackRequest)
        {
            _codecObserver->RequestNewKeyFrame(_channelId);
        }
    }
    return _rtpRtcp.RequestKeyFrame(frameType);
}

WebRtc_Word32 ViEChannel::SliceLossIndicationRequest(
    const WebRtc_UWord64 pictureId)
{
    return _rtpRtcp.SendRTCPSliceLossIndication((WebRtc_UWord8) pictureId);
}

// ----------------------------------------------------------------------------
// ResendPackets
//
// Called by VCM when VCM wants to request resend of packets (NACK)
// ----------------------------------------------------------------------------
WebRtc_Word32 ViEChannel::ResendPackets(const WebRtc_UWord16* sequenceNumbers,
                                        WebRtc_UWord16 length)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s(length: %d)", __FUNCTION__, length);

    return _rtpRtcp.SendNACK(sequenceNumbers, length);
}

// ============================================================================
// Protected methods
// ============================================================================

// ----------------------------------------------------------------------------
// ChannelDecodeThreadFunction
// ----------------------------------------------------------------------------

bool ViEChannel::ChannelDecodeThreadFunction(void* obj)
{
    return static_cast<ViEChannel*> (obj)->ChannelDecodeProcess();
}

// ----------------------------------------------------------------------------
// ChannelDecodeThreadFunction
// ----------------------------------------------------------------------------

bool ViEChannel::ChannelDecodeProcess()
{
    // Decode is blocking, but sleep some time anyway to not get a spin
    _vcm.Decode(50);

    if ((TickTime::Now() - _vcmRTTReported).Milliseconds() > 1000)
    {
        WebRtc_UWord16 RTT;
        WebRtc_UWord16 avgRTT;
        WebRtc_UWord16 minRTT;
        WebRtc_UWord16 maxRTT;

        if (_rtpRtcp.RTT(_rtpRtcp.RemoteSSRC(), &RTT, &avgRTT, &minRTT, &maxRTT)
            == 0)
        {
            _vcm.SetReceiveChannelParameters(RTT);
        }
        _vcmRTTReported = TickTime::Now();
    }
    return true;
}

// ============================================================================
// Private methods
// ============================================================================

// ----------------------------------------------------------------------------
// StartDecodeThread
//
// Assumed to be critsect protected if needed
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::StartDecodeThread()
{
    // Start the decode thread
    if (_ptrDecodeThread)
    {
        // Already started
        return 0;
    }
    _ptrDecodeThread = ThreadWrapper::CreateThread(ChannelDecodeThreadFunction,
                                                this, kHighestPriority,
                                                "DecodingThread");
    if (_ptrDecodeThread == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: could not create decode thread", __FUNCTION__);
        return -1;
    }

    unsigned int threadId;
    if (_ptrDecodeThread->Start(threadId) == false)
    {
        delete _ptrDecodeThread;
        _ptrDecodeThread = NULL;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: could not start decode thread", __FUNCTION__);
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: decode thread with id %u started", __FUNCTION__);
    return 0;
}

// ----------------------------------------------------------------------------
// StopDecodeThread
//
// Assumed to be critsect protected if needed
// ----------------------------------------------------------------------------

WebRtc_Word32 ViEChannel::StopDecodeThread()
{
    if (_ptrDecodeThread == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: decode thread not running", __FUNCTION__);
        return 0;
    }

    _ptrDecodeThread->SetNotAlive();
    if (_ptrDecodeThread->Stop())
    {
        delete _ptrDecodeThread;
    }
    else
    {
        // Couldn't stop the thread, leak instead of crash...
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: could not stop decode thread", __FUNCTION__);
        assert(!"could not stop decode thread");
    }
    _ptrDecodeThread = NULL;
    return 0;
}

#ifdef WEBRTC_SRTP

WebRtc_Word32
ViEChannel::EnableSRTPSend(const SrtpModule::CipherTypes cipherType,
    const unsigned int cipherKeyLength,
    const SrtpModule::AuthenticationTypes authType,
    const unsigned int authKeyLength, const unsigned int authTagLength,
    const SrtpModule::SecurityLevels level, const WebRtc_UWord8* key,
    const bool useForRTCP)
{

    _callbackCritsect.Enter();
    if (_ptrExternalEncryption)
    {
        _callbackCritsect.Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: external encryption already registered", __FUNCTION__);
        return -1;
    }
    _callbackCritsect.Leave();

    if (!_ptrSrtpModuleEncryption)
    {
        _ptrSrtpModuleEncryption =
            SrtpModule::CreateSrtpModule(
                ViEModuleId(_engineId, _channelId));
        if(!_ptrSrtpModuleEncryption)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "Failed to create SRTP module");
            return -1;
        }
    }

    const WebRtc_Word32 result =
        _ptrSrtpModuleEncryption->EnableSRTPEncrypt(
            !useForRTCP, cipherType, cipherKeyLength, authType, authKeyLength,
            authTagLength,level,key);
    if (0 != result)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "EnableSRTPEncrypt result %d, Apply To RTCP %d", result,
                   useForRTCP);
    }
    else
    {
        _vieSender.RegisterSRTPModule(_ptrSrtpModuleEncryption);
        if (useForRTCP)
        {
            _vieSender.RegisterSRTCPModule(_ptrSrtpModuleEncryption);
        }
    }
    return result;
}

WebRtc_Word32 ViEChannel::DisableSRTPSend()
{
    WebRtc_Word32 result = -1;
    if (_ptrSrtpModuleEncryption)
    {
        result = _ptrSrtpModuleEncryption->DisableSRTPEncrypt();
        _vieSender.DeregisterSRTPModule();
        _vieSender.DeregisterSRTCPModule();
    }

}

WebRtc_Word32
ViEChannel::EnableSRTPReceive(const SrtpModule::CipherTypes cipherType,
    const unsigned int cipherKeyLength,
    const SrtpModule::AuthenticationTypes authType,
    const unsigned int authKeyLength, const unsigned int authTagLength,
    const SrtpModule::SecurityLevels level, const WebRtc_UWord8* key,
    const bool useForRTCP)
{
    _callbackCritsect.Enter();
    if (_ptrExternalEncryption)
    {
        _callbackCritsect.Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: external encryption already registered", __FUNCTION__);
        return -1;
    }
    _callbackCritsect.Leave();

    if (!_ptrSrtpModuleDecryption)
    {
        _ptrSrtpModuleDecryption =
            SrtpModule::CreateSrtpModule(
                ViEModuleId(_engineId, _channelId));
        if(!_ptrSrtpModuleDecryption)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "Failed to create SRTP module");
            return -1;
        }
    }

    const WebRtc_Word32 result =
        _ptrSrtpModuleDecryption->EnableSRTPDecrypt(
            !useForRTCP, cipherType, cipherKeyLength, authType, authKeyLength,
            authTagLength,level,key);
    if (0 != result)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "EnableSRTPEncrypt result %d, Apply To RTCP %d", result,
                   useForRTCP);
    }
    else
    {
        _vieReceiver.RegisterSRTPModule(_ptrSrtpModuleDecryption);
        if (useForRTCP)
        {
            _vieReceiver.RegisterSRTCPModule(_ptrSrtpModuleDecryption);
        }
    }
    return result;
}

WebRtc_Word32 ViEChannel::DisableSRTPReceive()
{
    WebRtc_Word32 result = -1;
    if (_ptrSrtpModuleDecryption)
    {
        result = _ptrSrtpModuleDecryption->DisableSRTPDecrypt();
        _vieReceiver.DeregisterSRTPModule();
        _vieReceiver.DeregisterSRTPModule();
    }
    return result;
}
#endif

WebRtc_Word32 ViEChannel::RegisterExternalEncryption(
    Encryption* encryption)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    CriticalSectionScoped cs(_callbackCritsect);
    if (_ptrExternalEncryption)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: external encryption already registered", __FUNCTION__);
        return -1;
    }

    _ptrExternalEncryption = encryption;

    _vieReceiver.RegisterExternalDecryption(encryption);
    _vieSender.RegisterExternalEncryption(encryption);

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", "external encryption object registerd with channel=%d",
               _channelId);
    return 0;
}

WebRtc_Word32 ViEChannel::DeRegisterExternalEncryption()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    CriticalSectionScoped cs(_callbackCritsect);
    if (!_ptrExternalEncryption)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: external encryption is not registered", __FUNCTION__);
        return -1;
    }

    _ptrExternalTransport = NULL;
    _vieReceiver.DeregisterExternalDecryption();
    _vieSender.DeregisterExternalEncryption();
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s external encryption object de-registerd with channel=%d",
               __FUNCTION__, _channelId);
    return 0;
}

WebRtc_Word32 ViEChannel::SetVoiceChannel(WebRtc_Word32 veChannelId,
                                          VoEVideoSync* veSyncInterface)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s, audio channel %d, video channel %d", __FUNCTION__,
               veChannelId, _channelId);

    if (veSyncInterface)
    {
        // Register lip sync
        _moduleProcessThread.RegisterModule(&_vieSync);
    }
    else
    {
        _moduleProcessThread.DeRegisterModule(&_vieSync);
    }
    return _vieSync.SetVoiceChannel(veChannelId, veSyncInterface);
}

WebRtc_Word32 ViEChannel::VoiceChannel()
{
    return _vieSync.VoiceChannel();
}

WebRtc_Word32 ViEChannel::RegisterEffectFilter(ViEEffectFilter* effectFilter)
{
    CriticalSectionScoped cs(_callbackCritsect);
    if (effectFilter == NULL)
    {
        if (_effectFilter == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: no effect filter added for channel %d",
                       __FUNCTION__, _channelId);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: deregister effect filter for device %d", __FUNCTION__,
                   _channelId);
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s: register effect filter for device %d", __FUNCTION__,
                   _channelId);
        if (_effectFilter)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                       ViEId(_engineId, _channelId),
                       "%s: effect filter already added for channel %d",
                       __FUNCTION__, _channelId);
            return -1;
        }
    }
    _effectFilter = effectFilter;
    return 0;
}

ViEFileRecorder& ViEChannel::GetIncomingFileRecorder()
{
    // Start getting callback of all frames before they are decoded
    _vcm.RegisterFrameStorageCallback(this);
    return _fileRecorder;
}

void ViEChannel::ReleaseIncomingFileRecorder()
{
    // Stop getting callback of all frames before they are decoded
    _vcm.RegisterFrameStorageCallback(NULL);
}

void ViEChannel::OnLipSyncUpdate(const WebRtc_Word32 id,
                                 const WebRtc_Word32 audioVideoOffset)
{
    if (_channelId != ChannelId(id))
    {
        WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s, incorrect id",
                   __FUNCTION__, id);
        return;
    }
    _vieSync.SetNetworkDelay(audioVideoOffset);
}

void ViEChannel::OnApplicationDataReceived(const WebRtc_Word32 id,
                                           const WebRtc_UWord8 subType,
                                           const WebRtc_UWord32 name,
                                           const WebRtc_UWord16 length,
                                           const WebRtc_UWord8* data)
{
    if (_channelId != ChannelId(id))
    {
        WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s, incorrect id",
                   __FUNCTION__, id);
        return;
    }
    CriticalSectionScoped cs(_callbackCritsect);
    {
        if (_rtcpObserver)
        {
            _rtcpObserver->OnApplicationDataReceived(_channelId, subType, name,
                                                     (const char*) data, length);
        }
    }
}

WebRtc_Word32 ViEChannel::OnInitializeDecoder(
    const WebRtc_Word32 id, const WebRtc_Word8 payloadType,
    const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
    const int frequency, const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: payloadType %d, payloadName %s", __FUNCTION__, payloadType,
               payloadName);

    _vcm.ResetDecoder();

    _callbackCritsect.Enter();
    _decoderReset = true;
    _callbackCritsect.Leave();

    return 0;
}

void ViEChannel::OnPacketTimeout(const WebRtc_Word32 id)
{
    assert(ChannelId(id) == _channelId);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    CriticalSectionScoped cs(_callbackCritsect);
    if (_networkObserver)
    {
#ifndef WEBRTC_EXTERNAL_TRANSPORT
        if (_socketTransport.Receiving() || _ptrExternalTransport)
#else
        if (_ptrExternalTransport)
#endif
        {
            _networkObserver->PacketTimeout(_channelId, NoPacket);
            _rtpPacketTimeout = true;
        }
    }
}

//
// Called by the rtp module when the first packet is received and
// when first packet after a timeout is received.
//
void ViEChannel::OnReceivedPacket(const WebRtc_Word32 id,
                                  const RtpRtcpPacketType packetType)
{
    assert(ChannelId(id) == _channelId);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s", __FUNCTION__);

    if (_rtpPacketTimeout && packetType == kPacketRtp)
    {
        CriticalSectionScoped cs(_callbackCritsect);
        if (_networkObserver)
        {
            _networkObserver->PacketTimeout(_channelId, PacketReceived);
        }
        // Reset even if no observer set, might have been removed during timeout
        _rtpPacketTimeout = false;
    }
}

void ViEChannel::OnPeriodicDeadOrAlive(const WebRtc_Word32 id,
                                       const RTPAliveType alive)
{
    assert(ChannelId(id) == _channelId);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s(id=%d, alive=%d)", __FUNCTION__, id, alive);

    CriticalSectionScoped cs(_callbackCritsect);
    if (!_networkObserver)
    {
        return;
    }
    bool isAlive = true;
    if (alive == kRtpDead)
    {
        isAlive = false;
    }
    _networkObserver->OnPeriodicDeadOrAlive(_channelId, isAlive);
    return;
}

void ViEChannel::OnIncomingSSRCChanged(const WebRtc_Word32 id,
                                       const WebRtc_UWord32 SSRC)
{
    if (_channelId != ChannelId(id))
    {
        assert(false);
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s, incorrect id", __FUNCTION__, id);
        return;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: %u", __FUNCTION__, SSRC);

    CriticalSectionScoped cs(_callbackCritsect);
    {
        if (_rtpObserver)
        {
            _rtpObserver->IncomingSSRCChanged(_channelId, SSRC);
        }
    }
}

void ViEChannel::OnIncomingCSRCChanged(const WebRtc_Word32 id,
                                       const WebRtc_UWord32 CSRC,
                                       const bool added)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: %u added: %d", __FUNCTION__, CSRC, added);

    if (_channelId != ChannelId(id))
    {
        assert(false);
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                   "%s, incorrect id", __FUNCTION__, id);
        return;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
               "%s: %u", __FUNCTION__, CSRC);

    CriticalSectionScoped cs(_callbackCritsect);
    {
        if (_rtpObserver)
        {
            _rtpObserver->IncomingCSRCChanged(_channelId, CSRC, added);
        }
    }
}

WebRtc_Word32 ViEChannel::SetInverseH263Logic(const bool enable)
{
    return _rtpRtcp.SetH263InverseLogic(enable);
}

} // namespace webrtc
