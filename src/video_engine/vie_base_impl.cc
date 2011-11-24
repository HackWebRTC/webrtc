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
 * vie_base_impl.cc
 */

#include "vie_base_impl.h"

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"

#include "critical_section_wrapper.h"
#include "trace.h"
#include "vie_errors.h"
#include "vie_impl.h"
#include "vie_shared_data.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"
#include "vie_encoder.h"
#include "vie_input_manager.h"
#include "vie_performance_monitor.h"
#include "rtp_rtcp.h"
#include "video_render.h"
#include "video_coding.h"
#include "video_processing.h"
#include "stdio.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViEBase* ViEBase::GetInterface(VideoEngine* videoEngine)
{
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViEBaseImpl* vieBaseImpl = vieImpl;
    (*vieBaseImpl)++; // Increase ref count

    return vieBaseImpl;
}

// ----------------------------------------------------------------------------
// Release
//
// Releases the interface, i.e. reduces the reference counter. The number of
// remaining references is returned, -1 if released too many times.
// ----------------------------------------------------------------------------
int ViEBaseImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, instance_id_,
               "ViEBase::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, instance_id_,
                   "ViEBase release too many times");
        SetLastError(kViEAPIDoesNotExist);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, instance_id_,
               "ViEBase reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEBaseImpl::ViEBaseImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
               "ViEBaseImpl::ViEBaseImpl() Ctor");
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViEBaseImpl::~ViEBaseImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
               "ViEBaseImpl::ViEBaseImpl() Dtor");
}

// ----------------------------------------------------------------------------
// Init
//
// Must be called before any other API is called.
// This API should also reset the state of the enigne to the original state.
// ----------------------------------------------------------------------------

int ViEBaseImpl::Init()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, instance_id_, "Init");
    if (Initialized())
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, instance_id_,
                   "Init called twice");
        return 0;
    }

    SetInitialized();
    return 0;
}

// ----------------------------------------------------------------------------
// SetVoiceEngine
//
// Connects ViE to a VoE instance.
// ----------------------------------------------------------------------------

int ViEBaseImpl::SetVoiceEngine(VoiceEngine* ptrVoiceEngine)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_), "%s",
               __FUNCTION__);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   instance_id_);
        return -1;
    }
    if (channel_manager_.SetVoiceEngine(ptrVoiceEngine) != 0)
    {
        SetLastError(kViEBaseVoEFailure);
        return -1;
    }
    return 0;

}

// ============================================================================
// Channel functions
// ============================================================================

// ----------------------------------------------------------------------------
// CreateChannel
//
// Creates a new ViE channel
// ----------------------------------------------------------------------------

int ViEBaseImpl::CreateChannel(int& videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_), "%s",
               __FUNCTION__);

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   instance_id_);
        return -1;
    }

    if (channel_manager_.CreateChannel(videoChannel) == -1)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s: Could not create channel", __FUNCTION__);
        videoChannel = -1;
        SetLastError(kViEBaseChannelCreationFailed);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s: channel created: %d", __FUNCTION__, videoChannel);
    return 0;
}

// ----------------------------------------------------------------------------
// CreateChannel
//
// Creates a new channel using the same capture device and encoder as
// the original channel.
// ----------------------------------------------------------------------------

int ViEBaseImpl::CreateChannel(int& videoChannel, int originalChannel)
{

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   instance_id_);
        return -1;
    }

    ViEChannelManagerScoped cs(channel_manager_);

    if (!cs.Channel(originalChannel))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s - originalChannel does not exist.", __FUNCTION__,
                   instance_id_);
        SetLastError(kViEBaseInvalidChannelId);
        return -1;
    }
    if (channel_manager_.CreateChannel(videoChannel,
                                      originalChannel) == -1)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s: Could not create channel", __FUNCTION__);
        videoChannel = -1;
        SetLastError(kViEBaseChannelCreationFailed);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s: channel created: %d", __FUNCTION__, videoChannel);
    return 0;
}

// ----------------------------------------------------------------------------
// DeleteChannel
//
// Deleted a ViE channel
// ----------------------------------------------------------------------------

int ViEBaseImpl::DeleteChannel(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_), "%s(%d)",
               __FUNCTION__, videoChannel);

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   instance_id_);
        return -1;
    }

    {
        ViEChannelManagerScoped cs(channel_manager_);
        ViEChannel* vieChannel = cs.Channel(videoChannel);
        if (vieChannel == NULL)
        {
            // No such channel
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                       "%s: channel %d doesn't exist", __FUNCTION__,
                       videoChannel);
            SetLastError(kViEBaseInvalidChannelId);
            return -1;
        }

        // Deregister the ViEEncoder if no other channel is using it.
        ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
        if (cs.ChannelUsingViEEncoder(videoChannel) == false)
        {
            // No other channels using this ViEEncoder.
            // Disconnect the channel encoder from possible input.
            // capture or file.
            ViEInputManagerScoped is(input_manager_);
            ViEFrameProviderBase* provider = is.FrameProvider(ptrViEEncoder);
            if (provider)
            {
                provider->DeregisterFrameCallback(ptrViEEncoder);
            }
        }
    }
    if (channel_manager_.DeleteChannel(videoChannel) == -1)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s: Could not delete channel %d", __FUNCTION__,
                   videoChannel);
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s: channel deleted: %d", __FUNCTION__, videoChannel);
    return 0;
}

// ----------------------------------------------------------------------------
// ConnectAudioChannel
//
// Connects a ViE channel with a VoE channel
// ----------------------------------------------------------------------------

int ViEBaseImpl::ConnectAudioChannel(const int videoChannel,
                                     const int audioChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_), "%s(%d)",
               __FUNCTION__, videoChannel);

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   instance_id_);
        return -1;
    }

    ViEChannelManagerScoped cs(channel_manager_);
    if (cs.Channel(videoChannel) == NULL)
    {
        // No such channel
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s: channel %d doesn't exist", __FUNCTION__, videoChannel);
        SetLastError(kViEBaseInvalidChannelId);
        return -1;
    }

    if (channel_manager_.ConnectVoiceChannel(videoChannel, audioChannel) != 0)
    {
        SetLastError(kViEBaseVoEFailure);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DisconnectAudioChannel
//
// Disconnects a previously connected ViE and VoE channel pair
// ----------------------------------------------------------------------------

int ViEBaseImpl::DisconnectAudioChannel(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_), "%s(%d)",
               __FUNCTION__, videoChannel);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   instance_id_);
        return -1;
    }
    ViEChannelManagerScoped cs(channel_manager_);
    if (cs.Channel(videoChannel) == NULL)
    {
        // No such channel
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                   "%s: channel %d doesn't exist", __FUNCTION__, videoChannel);
        SetLastError(kViEBaseInvalidChannelId);
        return -1;
    }

    if (channel_manager_.DisconnectVoiceChannel(videoChannel) != 0)
    {
        SetLastError(kViEBaseVoEFailure);
        return -1;
    }
    return 0;
}

// ============================================================================
// Start and stop
// ============================================================================

// ----------------------------------------------------------------------------
// StartSend
//
// Starts sending on videoChannel and also starts the encoder.
// ----------------------------------------------------------------------------
int ViEBaseImpl::StartSend(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_,
                                                       videoChannel),
               "%s(channel: %d)", __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(instance_id_, videoChannel),
                   "%s: Channel %d does not exist", __FUNCTION__, videoChannel);
        SetLastError(kViEBaseInvalidChannelId);
        return -1;
    }
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        assert(false);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(instance_id_, videoChannel),
                   "%s: Could not find encoder for channel %d", __FUNCTION__,
                   videoChannel);
        return -1;
    }

    // Make sure we start with a key frame...
    ptrViEEncoder->Pause();
    WebRtc_Word32 error = ptrViEChannel->StartSend();
    if (error != 0)
    {
        // Restart the encoder, if it was stopped
        ptrViEEncoder->Restart();

        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(instance_id_, videoChannel),
                   "%s: Could not start sending on channel %d", __FUNCTION__,
                   videoChannel);
        if (error == kViEBaseAlreadySending)
        {
            SetLastError(kViEBaseAlreadySending);
        }
        SetLastError(kViEBaseUnknownError);
        return -1;
    }

    // Trigger the key frame and restart
    ptrViEEncoder->SendKeyFrame();
    ptrViEEncoder->Restart();
    return 0;
}

// ----------------------------------------------------------------------------
// StopSend
//
// Stops sending on the channel. This will also stop the encoder for the
// channel, if not shared with still active channels.
// ----------------------------------------------------------------------------
int ViEBaseImpl::StopSend(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(instance_id_, videoChannel), "%s(channel: %d)",
               __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(instance_id_, videoChannel),
                   "%s: Channel %d does not exist", __FUNCTION__, videoChannel);
        SetLastError(kViEBaseInvalidChannelId);
        return -1;
    }

    WebRtc_Word32 error = ptrViEChannel->StopSend();
    if (error != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(instance_id_, videoChannel),
                   "%s: Could not stop sending on channel %d", __FUNCTION__,
                   videoChannel);
        if (error == kViEBaseNotSending)
        {
            SetLastError(kViEBaseNotSending);
        }
        else
        {
            SetLastError(kViEBaseUnknownError);
        }
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// StartReceive
//
// Stops receiving on the channel. This will also start the decoder.
// ----------------------------------------------------------------------------
int ViEBaseImpl::StartReceive(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(instance_id_, videoChannel), "%s(channel: %d)",
               __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(instance_id_, videoChannel),
                   "%s: Channel %d does not exist", __FUNCTION__, videoChannel);
        SetLastError(kViEBaseInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->Receiving())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(instance_id_, videoChannel),
                   "%s: Channel %d already receive.", __FUNCTION__,
                   videoChannel);
        SetLastError(kViEBaseAlreadyReceiving);
        return -1;
    }
    if (ptrViEChannel->StartReceive() != 0)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// StopReceive
//
// Stops receiving on the channel. No decoding will be done.
// ----------------------------------------------------------------------------
int ViEBaseImpl::StopReceive(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(instance_id_, videoChannel), "%s(channel: %d)",
               __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(instance_id_, videoChannel),
                   "%s: Channel %d does not exist", __FUNCTION__, videoChannel);
        SetLastError(kViEBaseInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->StopReceive() != 0)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// Channel functions
// ============================================================================

// ----------------------------------------------------------------------------
// RegisterObserver
//
// Registers a customer implemented ViE observer
// ----------------------------------------------------------------------------

int ViEBaseImpl::RegisterObserver(ViEBaseObserver& observer)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s", __FUNCTION__);
    if (vie_performance_monitor_.ViEBaseObserverRegistered())
    {
        SetLastError(kViEBaseObserverAlreadyRegistered);
        return -1;
    }
    return vie_performance_monitor_.Init(&observer);
}

// ----------------------------------------------------------------------------
// DeregisterObserver
//
// Deregisters an observer
// ----------------------------------------------------------------------------

int ViEBaseImpl::DeregisterObserver()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s", __FUNCTION__);

    if (!vie_performance_monitor_.ViEBaseObserverRegistered())
    {
        SetLastError(kViEBaseObserverNotRegistered);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, instance_id_,
                   "%s No observer registered.", __FUNCTION__);
        return -1;
    }
    vie_performance_monitor_.Terminate();
    return 0;
}

// ============================================================================
// Info functions
// ============================================================================

// ----------------------------------------------------------------------------
// GetVersion
//
// Writes version information in 'version'
// ----------------------------------------------------------------------------

int ViEBaseImpl::GetVersion(char version[1024])
{

    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
               "GetVersion(version=?)");
    assert(kViEVersionMaxMessageSize == 1024);

    if (version == NULL)
    {
        SetLastError(kViEBaseInvalidArgument);
        return (-1);
    }

    char versionBuf[kViEVersionMaxMessageSize];
    char* versionPtr = versionBuf;

    WebRtc_Word32 len = 0; // does not include terminating NULL
    WebRtc_Word32 accLen = 0;

    len = AddViEVersion(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);

    len = AddBuildInfo(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);

#ifdef WEBRTC_EXTERNAL_TRANSPORT
    len = AddExternalTransportBuild(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);
#endif

    len = AddVCMVersion(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    len = AddSocketModuleVersion(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);
#endif

#ifdef WEBRTC_SRTP
    len = AddSRTPModuleVersion(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);
#endif

    len = AddRtpRtcpModuleVersion(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);

    len = AddVideoCaptureVersion(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);

    len = AddRenderVersion(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);

    len = AddVideoProcessingVersion(versionPtr);
    if (len == -1)
    {
        SetLastError(kViEBaseUnknownError);
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kViEVersionMaxMessageSize);

    memcpy(version, versionBuf, accLen);
    version[accLen] = '\0';

    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo, ViEId(instance_id_),
               "GetVersion() => %s", version);
    return 0;
}

WebRtc_Word32 ViEBaseImpl::AddBuildInfo(char* str) const
{
    return sprintf(str, "Build: %s\n", BUILDINFO);
}

WebRtc_Word32 ViEBaseImpl::AddViEVersion(char* str) const
{
    return sprintf(str, "VideoEngine 3.1.0\n");
}

#ifdef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32 ViEBaseImpl::AddExternalTransportBuild(char* str) const
{
    return sprintf(str, "External transport build\n");
}
#endif

WebRtc_Word32 ViEBaseImpl::AddModuleVersion(webrtc::Module* module,
                                            char* str) const
{
    WebRtc_Word8 version[kViEMaxModuleVersionSize];
    WebRtc_UWord32 remainingBufferInBytes(kViEMaxModuleVersionSize);
    WebRtc_UWord32 position(0);
    if (module && module->Version(version, remainingBufferInBytes, position)
        == 0)
    {
        return sprintf(str, "%s\n", version);
    }
    return -1;
}

WebRtc_Word32 ViEBaseImpl::AddVCMVersion(char* str) const
{
    webrtc::VideoCodingModule* vcmPtr =
        webrtc::VideoCodingModule::Create(instance_id_);
    int len = AddModuleVersion(vcmPtr, str);
    webrtc::VideoCodingModule::Destroy(vcmPtr);
    return len;
}

WebRtc_Word32 ViEBaseImpl::AddVideoCaptureVersion(char* str) const
{
    return 0;
}

WebRtc_Word32 ViEBaseImpl::AddVideoProcessingVersion(char* str) const
{
    webrtc::VideoProcessingModule* videoPtr =
        webrtc::VideoProcessingModule::Create(instance_id_);
    int len = AddModuleVersion(videoPtr, str);
    webrtc::VideoProcessingModule::Destroy(videoPtr);
    return len;
}
WebRtc_Word32 ViEBaseImpl::AddRenderVersion(char* str) const
{

    return 0;
}

#ifndef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32 ViEBaseImpl::AddSocketModuleVersion(char* str) const
{
    WebRtc_UWord8 numSockThreads(1);
    UdpTransport* socketPtr =
        UdpTransport::Create(
            instance_id_, numSockThreads);
    int len = AddModuleVersion(socketPtr, str);
    UdpTransport::Destroy(socketPtr);
    return len;
}
#endif

#ifdef WEBRTC_SRTP
WebRtc_Word32 ViEBaseImpl::AddSRTPModuleVersion(char* str) const
{
    SrtpModule* srtpPtr = SrtpModule::CreateSrtpModule(-1);
    int len = AddModuleVersion(srtpPtr, str);
    SrtpModule::DestroySrtpModule(srtpPtr);
    return len;
}
#endif

WebRtc_Word32 ViEBaseImpl::AddRtpRtcpModuleVersion(char* str) const
{
    RtpRtcp* rtpRtcpPtr =
        RtpRtcp::CreateRtpRtcp(-1, true);
    int len = AddModuleVersion(rtpRtcpPtr, str);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpPtr);
    return len;
}

// ----------------------------------------------------------------------------
// LastError
//
// Returns the last set error in this ViE instance
// ----------------------------------------------------------------------------

int ViEBaseImpl::LastError()
{
    return LastErrorInternal();
}

} // namespace webrtc
