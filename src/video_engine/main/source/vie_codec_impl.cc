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
 * vie_codec_impl.cc
 */

#include "vie_codec_impl.h"

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"

#include "video_coding.h"
#include "trace.h"
#include "vie_errors.h"
#include "vie_impl.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"
#include "vie_encoder.h"
#include "vie_input_manager.h"
#include "vie_capturer.h"

#include <string.h>

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViECodec* ViECodec::GetInterface(VideoEngine* videoEngine)
{
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViECodecImpl* vieCodecImpl = vieImpl;
    (*vieCodecImpl)++; // Increase ref count

    return vieCodecImpl;
#else
    return NULL;
#endif
}

// ----------------------------------------------------------------------------
// Release
//
// Releases the interface, i.e. reduces the reference counter. The number of
// remaining references is returned, -1 if released too many times.
// ----------------------------------------------------------------------------

int ViECodecImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, _instanceId,
               "ViECodecImpl::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, _instanceId,
                   "ViECodec released too many times");
        SetLastError(kViEAPIDoesNotExist);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _instanceId,
               "ViECodec reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViECodecImpl::ViECodecImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
               "ViECodecImpl::ViECodecImpl() Ctor");
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViECodecImpl::~ViECodecImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
               "ViECodecImpl::~ViECodecImpl() Dtor");
}

// Available codecs
// ----------------------------------------------------------------------------
// NumberOfCodecs
//
// Returns the number of available codecs
// ----------------------------------------------------------------------------

int ViECodecImpl::NumberOfCodecs() const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId), "%s",
               __FUNCTION__);

    if (!IsInitialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   _instanceId);
        return -1;
    }
    // +2 because of FEC(RED and ULPFEC)
    return (int) (VideoCodingModule::NumberOfCodecs() + 2);
}

// ----------------------------------------------------------------------------
// GetCodec
//
// Return the video codec with listNumber
// ----------------------------------------------------------------------------

int ViECodecImpl::GetCodec(const unsigned char listNumber,
                           VideoCodec& videoCodec) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(listNumber: %d, codecType: %d)", __FUNCTION__, listNumber);
    if (!IsInitialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   _instanceId);
        return -1;
    }
    if (listNumber == VideoCodingModule::NumberOfCodecs())
    {
        memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
        strcpy(videoCodec.plName, "RED");
        videoCodec.codecType = kVideoCodecRED;
        videoCodec.plType = VCM_RED_PAYLOAD_TYPE;
    }
    else if (listNumber == VideoCodingModule::NumberOfCodecs() + 1)
    {
        memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
        strcpy(videoCodec.plName, "ULPFEC");
        videoCodec.codecType = kVideoCodecULPFEC;
        videoCodec.plType = VCM_ULPFEC_PAYLOAD_TYPE;
    }
    else if (VideoCodingModule::Codec(listNumber, &videoCodec)
        != VCM_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Could not get codec for listNumber: %u", __FUNCTION__,
                   listNumber);
        SetLastError(kViECodecInvalidArgument);
        return -1;
    }
    return 0;
}

// Codec settings
// ----------------------------------------------------------------------------
// SetSendCodec
//
// Sets the send codec for videoChannel
// This call will affect all channels using the same encoder
// ----------------------------------------------------------------------------

int ViECodecImpl::SetSendCodec(const int videoChannel,
                               const VideoCodec& videoCodec)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId,videoChannel),
               "%s(videoChannel: %d, codecType: %d)", __FUNCTION__,
               videoChannel, videoCodec.codecType);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_instanceId, videoChannel),
               "%s: codec: %d, plType: %d, width: %d, height: %d, bitrate: %d"
               "maxBr: %d, minBr: %d, frameRate: %d)", __FUNCTION__,
               videoCodec.codecType, videoCodec.plType, videoCodec.width,
               videoCodec.height, videoCodec.startBitrate,
               videoCodec.maxBitrate, videoCodec.minBitrate,
               videoCodec.maxFramerate);

    if (CodecValid(videoCodec) == false)
    {
        // Error logged
        SetLastError(kViECodecInvalidCodec);
        return -1;
    }

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }

    // Set a maxBitrate if the user hasn't...
    VideoCodec videoCodecInternal;
    memcpy(&videoCodecInternal, &videoCodec, sizeof(webrtc::VideoCodec));
    if (videoCodecInternal.maxBitrate == 0)
    {
        // Max is one bit per pixel ...
        videoCodecInternal.maxBitrate = (videoCodecInternal.width
            * videoCodecInternal.height * videoCodecInternal.maxFramerate)
            / 1000;
        if (videoCodecInternal.startBitrate > videoCodecInternal.maxBitrate)
        {
            // ... but should'nt limit the set start bitrate.
            videoCodecInternal.maxBitrate = videoCodecInternal.startBitrate;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_instanceId,
                                                        videoChannel),
                   "%s: New max bitrate set to %d kbps", __FUNCTION__,
                   videoCodecInternal.maxBitrate);
    }

    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        assert(false);
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: No encoder found for channel %d", __FUNCTION__);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }

    // We need to check if the codec settings changed,
    // then we need a new SSRC
    bool newRtpStream = false;

    VideoCodec encoder;
    vieEncoder->GetEncoder(encoder);
    if (encoder.codecType != videoCodecInternal.codecType ||
        encoder.width != videoCodecInternal.width ||
        encoder.height != videoCodecInternal.height)
    {
        if (cs.ChannelUsingViEEncoder(videoChannel))
        {
            // We don't allow changing codec type or size when several
            // channels share encoder.
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                       ViEId(_instanceId, videoChannel),
                       "%s: Settings differs from other channels using encoder",
                       __FUNCTION__);
            SetLastError(kViECodecInUse);
            return -1;
        }
        newRtpStream = true;
    }

    ViEInputManagerScoped is(_inputManager);
    ViEFrameProviderBase* frameProvider = NULL;

    // Stop the media flow while reconfiguring
    vieEncoder->Pause();

    // Check if we have a frame provider that is a camera and can provide this
    // codec for us.
    bool useCaptureDeviceAsEncoder = false;
    frameProvider = is.FrameProvider(vieEncoder);
    if (frameProvider)
    {
        ViECapturer* vieCapture = static_cast<ViECapturer *> (frameProvider);
        // Try to get preencoded. Nothing to do if it is not supported.
        if (vieCapture && vieCapture->PreEncodeToViEEncoder(videoCodecInternal,
                                                            *vieEncoder,
                                                            videoChannel) == 0)
        {
            useCaptureDeviceAsEncoder = true;
        }
    }

    // Update the encoder settings if we are not using a capture device capable
    // of this codec.
    if (!useCaptureDeviceAsEncoder
        && vieEncoder->SetEncoder(videoCodecInternal) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Could not change encoder for channel %d", __FUNCTION__,
                   videoChannel);
        SetLastError(kViECodecUnknownError);
        return -1;
    }

    // Give the channel the new information
    if (vieChannel->SetSendCodec(videoCodecInternal, newRtpStream) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Could not set send codec for channel %d", __FUNCTION__,
                   videoChannel);
        SetLastError(kViECodecUnknownError);
        return -1;
    }

    // Update the protection mode, we might be switching NACK/FEC
    vieEncoder->UpdateProtectionMethod();
    // Get new best format for frame provider
    if (frameProvider)
    {
        frameProvider->FrameCallbackChanged();
    }
    // Restart the media flow
    if (newRtpStream)
    {
        // Stream settings changed, make sure we get a key frame
        vieEncoder->SendKeyFrame();
    }
    vieEncoder->Restart();

    return 0;
}

// ----------------------------------------------------------------------------
// GetSendCodec
//
// Gets the current send codec
// ----------------------------------------------------------------------------

int ViECodecImpl::GetSendCodec(const int videoChannel,
                               VideoCodec& videoCodec) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel), "%s(videoChannel: %d)",
               __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: No encoder for channel %d", __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }

    return vieEncoder->GetEncoder(videoCodec);
}

// ----------------------------------------------------------------------------
// SetReceiveCodec
//
// Registers a possible receive codec
// ----------------------------------------------------------------------------

int ViECodecImpl::SetReceiveCodec(const int videoChannel,
                                  const VideoCodec& videoCodec)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "%s(videoChannel: %d, codecType: %d)", __FUNCTION__,
               videoChannel, videoCodec.codecType);
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_instanceId, videoChannel),
               "%s: codec: %d, plType: %d, width: %d, height: %d, bitrate: %d,"
               "maxBr: %d, minBr: %d, frameRate: %d", __FUNCTION__,
               videoCodec.codecType, videoCodec.plType, videoCodec.width,
               videoCodec.height, videoCodec.startBitrate,
               videoCodec.maxBitrate, videoCodec.minBitrate,
               videoCodec.maxFramerate);

    if (CodecValid(videoCodec) == false)
    {
        // Error logged
        SetLastError(kViECodecInvalidCodec);
        return -1;
    }

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }

    if (vieChannel->SetReceiveCodec(videoCodec) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId,
                                                         videoChannel),
                   "%s: Could not set receive codec for channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecUnknownError);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// GetReceiveCodec
//
// Gets the current receive codec
// ----------------------------------------------------------------------------

int ViECodecImpl::GetReceiveCodec(const int videoChannel,
                                  VideoCodec& videoCodec) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "%s(videoChannel: %d, codecType: %d)", __FUNCTION__,
               videoChannel, videoCodec.codecType);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }

    if (vieChannel->GetReceiveCodec(videoCodec) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetCodecConfigParameters
//
// Gets the codec config parameters to be sent out-of-band.
// ----------------------------------------------------------------------------

int ViECodecImpl::GetCodecConfigParameters(
    const int videoChannel,
    unsigned char configParameters[kConfigParameterSize],
    unsigned char& configParametersSize) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel), "%s(videoChannel: %d)",
               __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: No encoder for channel %d", __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }

    if (vieEncoder->GetCodecConfigParameters(configParameters,
                                             configParametersSize) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// SetImageScaleStatus
//
// Enables scaling of the encoded image instead of padding black border or
// cropping
// ----------------------------------------------------------------------------

int ViECodecImpl::SetImageScaleStatus(const int videoChannel, const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "%s(videoChannel: %d, enable: %d)", __FUNCTION__, videoChannel,
               enable);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }

    if (vieEncoder->ScaleInputImage(enable) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;
}

// Codec statistics
// ----------------------------------------------------------------------------
// GetSendCodecStastistics
//
// Get codec statistics for outgoing stream
// ----------------------------------------------------------------------------


int ViECodecImpl::GetSendCodecStastistics(const int videoChannel,
                                          unsigned int& keyFrames,
                                          unsigned int& deltaFrames) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel), "%s(videoChannel %d)",
               __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: No send codec for channel %d", __FUNCTION__,
                   videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }

    if (vieEncoder->SendCodecStatistics((WebRtc_UWord32&) keyFrames,
                                        (WebRtc_UWord32&) deltaFrames) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetReceiveCodecStastistics
//
// Get codec statistics for incoming stream
// ----------------------------------------------------------------------------

int ViECodecImpl::GetReceiveCodecStastistics(const int videoChannel,
                                             unsigned int& keyFrames,
                                             unsigned int& deltaFrames) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "%s(videoChannel: %d, codecType: %d)", __FUNCTION__,
               videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieChannel->ReceiveCodecStatistics((WebRtc_UWord32&) keyFrames,
                                           (WebRtc_UWord32&) deltaFrames) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;

}

// Callbacks
// ----------------------------------------------------------------------------
// SetKeyFrameRequestCallbackStatus
//
// Enables a kecallback for keyframe request instead of using RTCP
// ----------------------------------------------------------------------------

int ViECodecImpl::SetKeyFrameRequestCallbackStatus(const int videoChannel,
                                                   const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel), "%s(videoChannel: %d)",
               __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieChannel->EnableKeyFrameRequestCallback(enable) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;

}

// ----------------------------------------------------------------------------
// SetSignalKeyPacketLossStatus
//
// Triggers a key frame request when there is packet loss in a received key
// frame
// ----------------------------------------------------------------------------

int ViECodecImpl::SetSignalKeyPacketLossStatus(const int videoChannel,
                                               const bool enable,
                                               const bool onlyKeyFrames)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "%s(videoChannel: %d, enable: %d, onlyKeyFrames: %d)",
               __FUNCTION__, videoChannel, enable);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieChannel->SetSignalPacketLossStatus(enable, onlyKeyFrames) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterEncoderObserver
// ----------------------------------------------------------------------------

int ViECodecImpl::RegisterEncoderObserver(const int videoChannel,
                                          ViEEncoderObserver& observer)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId), "%s",
               __FUNCTION__);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: No encoder for channel %d", __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieEncoder->RegisterCodecObserver(&observer) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Could not register codec observer at channel",
                   __FUNCTION__);
        SetLastError(kViECodecObserverAlreadyRegistered);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterEncoderObserver
// ----------------------------------------------------------------------------

int ViECodecImpl::DeregisterEncoderObserver(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId), "%s",
               __FUNCTION__);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: No encoder for channel %d", __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieEncoder->RegisterCodecObserver(NULL) != 0)
    {
        SetLastError(kViECodecObserverNotRegistered);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterDecoderObserver
// ----------------------------------------------------------------------------

int ViECodecImpl::RegisterDecoderObserver(const int videoChannel,
                                          ViEDecoderObserver& observer)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId), "%s",
               __FUNCTION__);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieChannel->RegisterCodecObserver(&observer) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Could not register codec observer at channel",
                   __FUNCTION__);
        SetLastError(kViECodecObserverAlreadyRegistered);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterDecoderObserver
// ----------------------------------------------------------------------------

int ViECodecImpl::DeregisterDecoderObserver(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId), "%s",
               __FUNCTION__);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieChannel->RegisterCodecObserver(NULL) != 0)
    {
        SetLastError(kViECodecObserverNotRegistered);
        return -1;
    }

    return 0;
}

// Force a key frame
// ----------------------------------------------------------------------------
// SendKeyFrame
//
// Force the next frame to be a key frame
// ----------------------------------------------------------------------------

int ViECodecImpl::SendKeyFrame(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(videoChannel: %d)", __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieEncoder->SendKeyFrame() != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;

}

// ----------------------------------------------------------------------------
// WaitForFirstKeyFrame
//
// Forc the next frame to be a key frame
// ----------------------------------------------------------------------------

int ViECodecImpl::WaitForFirstKeyFrame(const int videoChannel, const bool wait)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(videoChannel: %d, wait: %d)", __FUNCTION__, videoChannel,
               wait);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieChannel->WaitForKeyFrame(wait) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;
}

// H263 Specific
// ----------------------------------------------------------------------------
// SetInverseH263Logic
//
// Used to interoperate with old MS H.263 where key frames are marked as delta
// and the oposite.
// ----------------------------------------------------------------------------

int ViECodecImpl::SetInverseH263Logic(int videoChannel, bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(videoChannel: %d)", __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId,videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidChannelId);
        return -1;
    }
    if (vieChannel->SetInverseH263Logic(enable) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// CodecValid
// ----------------------------------------------------------------------------

bool ViECodecImpl::CodecValid(const VideoCodec& videoCodec)
{
    // Check plName matches codecType
    if (videoCodec.codecType == kVideoCodecRED)
    {
#if defined(WIN32)
        if (_strnicmp(videoCodec.plName, "red", 3) == 0)
#elif defined(WEBRTC_MAC_INTEL) || defined(WEBRTC_LINUX)
        if (strncasecmp(videoCodec.plName, "red",3) == 0)
#endif
        {
            // We only care about the type and name for red
            return true;
        }
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, -1,
                   "Codec type doesn't match plName", videoCodec.plType);
        return false;
    }
    else if (videoCodec.codecType == kVideoCodecULPFEC)
    {
#if defined(WIN32)
        if (_strnicmp(videoCodec.plName, "ULPFEC", 6) == 0)
#elif defined(WEBRTC_MAC_INTEL)|| defined(WEBRTC_LINUX)
        if (strncasecmp(videoCodec.plName, "ULPFEC",6) == 0)
#endif
        {
            // We only care about the type and name for ULPFEC
            return true;
        }
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, -1,
                   "Codec type doesn't match plName", videoCodec.plType);
        return false;
    }
    else if ((videoCodec.codecType == kVideoCodecH263 &&
             strncmp(videoCodec.plName, "H263", 4) == 0)
            || (videoCodec.codecType == kVideoCodecH263
                && strncmp(videoCodec.plName, "H263-1998", 9) == 0)
            || (videoCodec.codecType == kVideoCodecVP8
                && strncmp(videoCodec.plName, "VP8", 4) == 0)
            || (videoCodec.codecType == kVideoCodecI420
                && strncmp(videoCodec.plName, "I420", 4) == 0)
            || (videoCodec.codecType == kVideoCodecH264
                && strncmp(videoCodec.plName, "H264", 4) == 0))
            // || (videoCodec.codecType == kVideoCodecMPEG4
            //  && strncmp(videoCodec.plName, "MP4V-ES", 7) == 0)
    {
        // ok
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, -1,
                   "Codec type doesn't match plName", videoCodec.plType);
        return false;
    }

    // pltype
    if (videoCodec.plType == 0 && videoCodec.plType > 127)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, -1,
                   "Invalid codec payload type: %d", videoCodec.plType);
        return false;
    }

    // Size
    if (videoCodec.width > kViEMaxCodecWidth || videoCodec.height
        > kViEMaxCodecHeight)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, -1,
                   "Invalid codec size: %u x %u", videoCodec.width,
                   videoCodec.height);
        return false;
    }

    if (videoCodec.startBitrate < kViEMinCodecBitrate)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, -1,
                   "Invalid startBitrate: %u", videoCodec.startBitrate);
        return false;
    }
    if (videoCodec.minBitrate < kViEMinCodecBitrate)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, -1,
                   "Invalid minBitrate: %u", videoCodec.minBitrate);
        return false;
    }
    if (videoCodec.startBitrate < kViEMinCodecBitrate)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, -1,
                   "Invalid minBitrate: %u", videoCodec.minBitrate);
        return false;
    }

    if (videoCodec.codecType == kVideoCodecH263)
    {
        if ((videoCodec.width == 704 && videoCodec.height == 576)
            || (videoCodec.width == 352 && videoCodec.height == 288)
            || (videoCodec.width == 176 && videoCodec.height == 144)
            || (videoCodec.width == 128 && videoCodec.height == 96))
        {
            // ok
        }
        else
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, -1,
                       "Invalid size for H.263");
            return false;
        }
    }
    return true;
}
} // namespace webrtc
