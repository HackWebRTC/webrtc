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
 * vie_external_codec_impl.cc
 */

#include "engine_configurations.h"
#include "vie_external_codec_impl.h"
#include "vie_errors.h"
#include "trace.h"
#include "vie_impl.h"
#include "vie_channel.h"
#include "vie_encoder.h"
#include "vie_channel_manager.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViEExternalCodec* ViEExternalCodec::GetInterface(VideoEngine* videoEngine)
{
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViEExternalCodecImpl* vieExternalCodecImpl = vieImpl;
    (*vieExternalCodecImpl)++; // Increase ref count

    return vieExternalCodecImpl;
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

int ViEExternalCodecImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, _instanceId,
               "ViEExternalCodec::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, _instanceId,
                   "ViEExternalCodec release too many times");
        SetLastError(kViEAPIDoesNotExist);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _instanceId,
               "ViEExternalCodec reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
//	RegisterExternalSendCodec
// ----------------------------------------------------------------------------

int ViEExternalCodecImpl::RegisterExternalSendCodec(const int videoChannel,
                                                    const unsigned char plType,
                                                    VideoEncoder* encoder)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s channel %d plType %d encoder 0x%x", __FUNCTION__,
               videoChannel, plType, encoder);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (!vieEncoder)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Invalid argument videoChannel %u. Does it exist?",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidArgument);
        return -1;
    }
    if (!encoder)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Invalid argument Encoder 0x%x.", __FUNCTION__, encoder);
        SetLastError(kViECodecInvalidArgument);
        return -1;
    }

    if (vieEncoder->RegisterExternalEncoder(encoder, plType) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;
}

int ViEExternalCodecImpl::DeRegisterExternalSendCodec(
    const int videoChannel, const unsigned char plType)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s channel %d plType %d", __FUNCTION__, videoChannel, plType);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (!vieEncoder)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Invalid argument videoChannel %u. Does it exist?",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidArgument);
        return -1;
    }

    if (vieEncoder->DeRegisterExternalEncoder(plType) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;

}

int ViEExternalCodecImpl::RegisterExternalReceiveCodec(
    const int videoChannel, const unsigned int plType, VideoDecoder* decoder,
    bool decoderRender /*= false*/, int renderDelay /*= 0*/)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s channel %d plType %d decoder 0x%x, decoderRender %d, "
               "renderDelay %d", __FUNCTION__, videoChannel, plType, decoder,
               decoderRender, renderDelay);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (!vieChannel)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Invalid argument videoChannel %u. Does it exist?",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidArgument);
        return -1;
    }
    if (!decoder)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Invalid argument decoder 0x%x.", __FUNCTION__, decoder);
        SetLastError(kViECodecInvalidArgument);
        return -1;
    }

    if (vieChannel->RegisterExternalDecoder(plType, decoder, decoderRender,
                                            renderDelay) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;
}

int ViEExternalCodecImpl::DeRegisterExternalReceiveCodec(
    const int videoChannel, const unsigned char plType)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s channel %d plType %u", __FUNCTION__, videoChannel, plType);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (!vieChannel)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "%s: Invalid argument videoChannel %u. Does it exist?",
                   __FUNCTION__, videoChannel);
        SetLastError(kViECodecInvalidArgument);
        return -1;
    }
    if (vieChannel->DeRegisterExternalDecoder(plType) != 0)
    {
        SetLastError(kViECodecUnknownError);
        return -1;
    }
    return 0;
}
} // namespace webrtc
