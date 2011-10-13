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
 * vie_file_impl.cc
 */

#include "vie_file_impl.h"

#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
// Defines
#include "vie_defines.h"

// Includes
#include "condition_variable_wrapper.h"
#include "critical_section_wrapper.h"
#include "jpeg.h"
#include "trace.h"
#include "vie_capturer.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"
#include "vie_encoder.h"
#include "vie_errors.h"
#include "vie_file_image.h"
#include "vie_file_player.h"
#include "vie_file_recorder.h"
#include "vie_impl.h"
#include "vie_input_manager.h"
#include "vie_render_manager.h"
#endif

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViEFile* ViEFile::GetInterface(VideoEngine* videoEngine)
{
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViEFileImpl* vieFileImpl = vieImpl;
    (*vieFileImpl)++; // Increase ref count

    return vieFileImpl;
#else
    return NULL;
#endif
}

#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
// ----------------------------------------------------------------------------
// Release
//
// Releases the interface, i.e. reduces the reference counter. The number of
// remaining references is returned, -1 if released too many times.
// ----------------------------------------------------------------------------

int ViEFileImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, _instanceId,
                 "ViEFile::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, _instanceId,
                     "ViEFile release too many times");
        SetLastError(kViEAPIDoesNotExist);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _instanceId,
                 "ViEFile reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEFileImpl::ViEFileImpl()

{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
                 "ViEFileImpl::ViEFileImpl() Ctor");

}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViEFileImpl::~ViEFileImpl()
{

    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
                 "ViEFileImpl::~ViEFileImpl() Dtor");
}

// ----------------------------------------------------------------------------
// StartPlayFile
// ----------------------------------------------------------------------------
// Play file
int ViEFileImpl::StartPlayFile(const char* fileNameUTF8, int& fileId,
                               const bool loop /*= false*/,
                               const webrtc::FileFormats fileFormat
                               /*= webrtc::kFileFormatAviFile*/)
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

    VoiceEngine* voice = _channelManager.GetVoiceEngine();
    const WebRtc_Word32 result = _inputManager.CreateFilePlayer(fileNameUTF8,
                                                                loop,
                                                                fileFormat,
                                                                voice, fileId);
    if (result != 0)
    {
        SetLastError(result);
        return -1;
    }
    return 0;
}

int ViEFileImpl::StopPlayFile(const int fileId)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
                 "%s(fileId: %d)", __FUNCTION__, fileId);

    {
        ViEInputManagerScoped is(_inputManager);
        ViEFilePlayer* ptrViEFilePlayer = is.FilePlayer(fileId);
        if (ptrViEFilePlayer == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                         "%s: File with id %d is not playing.", __FUNCTION__,
                         fileId);
            SetLastError(kViEFileNotPlaying);
            return -1;
        }
    }

    // Destroy the capture device
    return _inputManager.DestroyFilePlayer(fileId);

}

int ViEFileImpl::RegisterObserver(int fileId, ViEFileObserver& observer)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
                 "%s(fileId: %d)", __FUNCTION__, fileId);

    ViEInputManagerScoped is(_inputManager);
    ViEFilePlayer* ptrViEFilePlayer = is.FilePlayer(fileId);
    if (ptrViEFilePlayer == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                     "%s: File with id %d is not playing.", __FUNCTION__,
                     fileId);
        SetLastError(kViEFileNotPlaying);
        return -1;
    }
    if (ptrViEFilePlayer->IsObserverRegistered())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, fileId),
                     "%s: Observer already registered", __FUNCTION__);
        SetLastError(kViEFileObserverAlreadyRegistered);
        return -1;
    }
    if (ptrViEFilePlayer->RegisterObserver(observer) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, fileId),
                     "%s: Failed to register observer", __FUNCTION__, fileId);
        SetLastError(kViEFileUnknownError);
        return -1;
    }
    return 0;

}

int ViEFileImpl::DeregisterObserver(int fileId, ViEFileObserver& observer)
{

    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
                 "%s(fileId: %d)", __FUNCTION__, fileId);

    ViEInputManagerScoped is(_inputManager);
    ViEFilePlayer* ptrViEFilePlayer = is.FilePlayer(fileId);
    if (ptrViEFilePlayer == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                     "%s: File with id %d is not playing.", __FUNCTION__,
                     fileId);
        SetLastError(kViEFileNotPlaying);
        return -1;
    }
    if (!ptrViEFilePlayer->IsObserverRegistered())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, fileId), "%s: No Observer registered",
                     __FUNCTION__);
        SetLastError(kViEFileObserverNotRegistered);
        return -1;
    }
    if (ptrViEFilePlayer->DeRegisterObserver() != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, fileId),
                     "%s: Failed to deregister observer", __FUNCTION__, fileId);
        SetLastError(kViEFileUnknownError);
        return -1;
    }
    return 0;

}

int ViEFileImpl::SendFileOnChannel(const int fileId, const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
                 "%s(fileId: %d)", __FUNCTION__, fileId);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileInvalidChannelId);
        return -1;
    }

    ViEInputManagerScoped is(_inputManager);
    if (is.FrameProvider(ptrViEEncoder) != NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d already connected to a capture device or "
                     "file.", __FUNCTION__, videoChannel);
        SetLastError(kViEFileInputAlreadyConnected);
        return -1;
    }

    ViEFilePlayer* ptrViEFilePlayer = is.FilePlayer(fileId);
    if (ptrViEFilePlayer == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                     "%s: File with id %d is not playing.", __FUNCTION__,
                     fileId);
        SetLastError(kViEFileNotPlaying);
        return -1;
    }

    if (ptrViEFilePlayer->RegisterFrameCallback(videoChannel, ptrViEEncoder)
        != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                     "%s: Failed to register frame callback.", __FUNCTION__,
                     fileId);
        SetLastError(kViEFileUnknownError);
        return -1;
    }
    return 0;
}

int ViEFileImpl::StopSendFileOnChannel(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
                 "%s(videoChannel: %d)", __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileInvalidChannelId);
        return -1;
    }

    ViEInputManagerScoped is(_inputManager);
    ViEFrameProviderBase* frameProvider = is.FrameProvider(ptrViEEncoder);
    if (frameProvider == NULL
        || frameProvider->Id() < kViEFileIdBase
        || frameProvider->Id() > kViEFileIdMax)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: No file connected to Channel %d", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileNotConnected);
        return -1;
    }
    if (frameProvider->DeregisterFrameCallback(ptrViEEncoder) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Failed to deregister file from channel %d",
                     __FUNCTION__, videoChannel);
        SetLastError(kViEFileUnknownError);
    }
    return 0;

}

int ViEFileImpl::StartPlayFileAsMicrophone(const int fileId,
                                           const int audioChannel,
                                           bool mixMicrophone /*= false*/,
                                           float volumeScaling /*= 1*/)
{
    ViEInputManagerScoped is(_inputManager);

    ViEFilePlayer* ptrViEFilePlayer = is.FilePlayer(fileId);
    if (ptrViEFilePlayer == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                     "%s: File with id %d is not playing.", __FUNCTION__,
                     fileId);
        SetLastError(kViEFileNotPlaying);
        return -1;
    }
    if (ptrViEFilePlayer->SendAudioOnChannel(audioChannel, mixMicrophone,
                                             volumeScaling) != 0)
    {
        SetLastError(kViEFileVoEFailure);
        return -1;
    }
    return 0;

}

int ViEFileImpl::StopPlayFileAsMicrophone(const int fileId,
                                          const int audioChannel)
{
    ViEInputManagerScoped is(_inputManager);

    ViEFilePlayer* ptrViEFilePlayer = is.FilePlayer(fileId);
    if (ptrViEFilePlayer == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                     "%s: File with id %d is not playing.", __FUNCTION__,
                     fileId);
        SetLastError(kViEFileNotPlaying);
        return -1;
    }

    if (ptrViEFilePlayer->StopSendAudioOnChannel(audioChannel) != 0)
    {
        SetLastError(kViEFileVoEFailure);
        return -1;
    }
    return 0;
}

int ViEFileImpl::StartPlayAudioLocally(const int fileId,
                                       const int audioChannel,
                                       float volumeScaling /*=1*/)
{
    ViEInputManagerScoped is(_inputManager);

    ViEFilePlayer* ptrViEFilePlayer = is.FilePlayer(fileId);
    if (ptrViEFilePlayer == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                     "%s: File with id %d is not playing.", __FUNCTION__,
                     fileId);
        SetLastError(kViEFileNotPlaying);
        return -1;
    }
    if (ptrViEFilePlayer->PlayAudioLocally(audioChannel, volumeScaling) != 0)
    {
        SetLastError(kViEFileVoEFailure);
        return -1;
    }
    return 0;
}

int ViEFileImpl::StopPlayAudioLocally(const int fileId, const int audioChannel)
{
    ViEInputManagerScoped is(_inputManager);

    ViEFilePlayer* ptrViEFilePlayer = is.FilePlayer(fileId);
    if (ptrViEFilePlayer == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                     "%s: File with id %d is not playing.", __FUNCTION__,
                     fileId);
        SetLastError(kViEFileNotPlaying);
        return -1;
    }
    if (ptrViEFilePlayer->StopPlayAudioLocally(audioChannel) != 0)
    {
        SetLastError(kViEFileVoEFailure);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// StartRecordOutgoingVideo
// ----------------------------------------------------------------------------
int ViEFileImpl::StartRecordOutgoingVideo(const int videoChannel,
                                          const char* fileNameUTF8,
                                          AudioSource audioSource,
                                          const webrtc::CodecInst& audioCodec,
                                          const VideoCodec& videoCodec,
                                          const webrtc::FileFormats fileFormat
                                          /*= webrtc::kFileFormatAviFile*/)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s videoChannel: %d)",
                 __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileInvalidChannelId);
        return -1;
    }
    ViEFileRecorder& fileRecorder = ptrViEEncoder->GetOutgoingFileRecorder();
    if (fileRecorder.RecordingStarted())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Already recording outgoing video on channel %d",
                     __FUNCTION__, videoChannel);
        SetLastError(kViEFileAlreadyRecording);
        return -1;
    }

    WebRtc_Word32 veChannelId = -1;
    VoiceEngine* vePtr = NULL;
    if (audioSource != NO_AUDIO)
    {
        ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
        veChannelId = ptrViEChannel->VoiceChannel();
        vePtr = _channelManager.GetVoiceEngine();

        if (!vePtr)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                         ViEId(_instanceId, videoChannel),
                         "%s: Can't access voice engine. Have SetVoiceEngine "
                         "been called?", __FUNCTION__);
            SetLastError(kViEFileVoENotSet);
            return -1;
        }
    }
    if (fileRecorder.StartRecording(fileNameUTF8, videoCodec, audioSource,
                                    veChannelId, audioCodec, vePtr,
                                    fileFormat) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Failed to start recording. Check arguments.",
                     __FUNCTION__);
        SetLastError(kViEFileUnknownError);
        return -1;
    }

    return 0;
}

int ViEFileImpl::StopRecordOutgoingVideo(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s videoChannel: %d)",
                 __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileInvalidChannelId);
        return -1;
    }
    ViEFileRecorder& fileRecorder = ptrViEEncoder->GetOutgoingFileRecorder();
    if (!fileRecorder.RecordingStarted())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d is not recording.", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileNotRecording);
        return -1;
    }
    if (fileRecorder.StopRecording() != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Failed to stop recording of channel %d.",
                     __FUNCTION__, videoChannel);
        SetLastError(kViEFileUnknownError);
        return -1;
    }
    return 0;

}
int ViEFileImpl::StopRecordIncomingVideo(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s videoChannel: %d)",
                 __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileInvalidChannelId);
        return -1;
    }
    ViEFileRecorder& fileRecorder = ptrViEChannel->GetIncomingFileRecorder();
    if (!fileRecorder.RecordingStarted())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d is not recording.", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileNotRecording);
        ptrViEChannel->ReleaseIncomingFileRecorder();

        return -1;
    }
    if (fileRecorder.StopRecording() != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Failed to stop recording of channel %d.",
                     __FUNCTION__, videoChannel);
        SetLastError(kViEFileUnknownError);
        ptrViEChannel->ReleaseIncomingFileRecorder();
        return -1;
    }
    // Let the channel know we are no longer recording
    ptrViEChannel->ReleaseIncomingFileRecorder();
    return 0;

}

int ViEFileImpl::StartRecordIncomingVideo(const int videoChannel,
                                          const char* fileNameUTF8,
                                          AudioSource audioSource,
                                          const webrtc::CodecInst& audioCodec,
                                          const VideoCodec& videoCodec,
                                          const webrtc::FileFormats fileFormat
                                          /*= webrtc::kFileFormatAviFile*/)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s videoChannel: %d)",
                 __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileInvalidChannelId);
        return -1;
    }
    ViEFileRecorder& fileRecorder = ptrViEChannel->GetIncomingFileRecorder();
    if (fileRecorder.RecordingStarted())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Already recording outgoing video on channel %d",
                     __FUNCTION__, videoChannel);
        SetLastError(kViEFileAlreadyRecording);
        return -1;
    }

    WebRtc_Word32 veChannelId = -1;
    VoiceEngine* vePtr = NULL;
    if (audioSource != NO_AUDIO)
    {
        veChannelId = ptrViEChannel->VoiceChannel();
        vePtr = _channelManager.GetVoiceEngine();

        if (!vePtr)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                         ViEId(_instanceId, videoChannel),
                         "%s: Can't access voice engine. Have SetVoiceEngine "
                         "been called?", __FUNCTION__);
            SetLastError(kViEFileVoENotSet);
            return -1;
        }
    }
    if (fileRecorder.StartRecording(fileNameUTF8, videoCodec, audioSource,
                                    veChannelId, audioCodec, vePtr, fileFormat)
        != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Failed to start recording. Check arguments.",
                     __FUNCTION__);
        SetLastError(kViEFileUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// File information
// ============================================================================

// ----------------------------------------------------------------------------
// GetFileInformation
//
//
// ----------------------------------------------------------------------------

int ViEFileImpl::GetFileInformation(const char* fileName,
                                    VideoCodec& videoCodec,
                                    webrtc::CodecInst& audioCodec,
                                    const webrtc::FileFormats fileFormat
                                    /*= webrtc::kFileFormatAviFile*/)
{
    return ViEFilePlayer::GetFileInformation(
        _instanceId, (WebRtc_Word8*) fileName,
        videoCodec, audioCodec, fileFormat);
}

// ============================================================================
// Snapshot
// ============================================================================
// ----------------------------------------------------------------------------

int ViEFileImpl::GetRenderSnapshot(const int videoChannel,
                                   const char* fileNameUTF8)
{
    // gain access to the renderer for the specified channel and get it's
    // current frame
    ViERenderManagerScoped rs(_renderManager);
    ViERenderer* ptrRender = rs.Renderer(videoChannel);
    if (!ptrRender)
    {
        return -1;
    }

    VideoFrame videoFrame;
    if (-1 == ptrRender->GetLastRenderedFrame(videoChannel, videoFrame))
    {
        return -1;
    }

    const int JPEG_FORMAT = 0;
    int format = JPEG_FORMAT;

    switch (format)
    {
        case JPEG_FORMAT:
        {
            // *** JPEGEncoder writes the jpeg file for you (no control
            // over it) and does not return you the buffer
            // *** Thusly, we are not going to be writing to the disk here

            JpegEncoder jpegEncoder;
            RawImage inputImage;

            if (-1 == jpegEncoder.SetFileName(fileNameUTF8))
            {
                // could not set filename for whatever reason
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             _instanceId,
                             "\tCould not open output file '%s' for writing!",
                             fileNameUTF8);
                return -1;
            }

            inputImage._width = videoFrame.Width();
            inputImage._height = videoFrame.Height();
            videoFrame.Swap(inputImage._buffer, inputImage._length,
                            inputImage._size);

            if (-1 == jpegEncoder.Encode(inputImage))
            {
                // could not encode i420->jpeg
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             _instanceId,
                             "\tCould not encode i420 -> jpeg file '%s' for "
                             "writing!", fileNameUTF8);
                if (inputImage._buffer)
                {
                    delete [] inputImage._buffer;
                }
                return -1;
            }

            delete [] inputImage._buffer;
            inputImage._buffer = NULL;

            break;
        }
        default:
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceFile, _instanceId,
                         "\tUnsupported file format for %s", __FUNCTION__);
            return -1;
            break;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
//
// GetRenderSnapshot
// ----------------------------------------------------------------------------

int ViEFileImpl::GetRenderSnapshot(const int videoChannel, ViEPicture& picture)
{

    // gain access to the renderer for the specified channel and get it's
    // current frame
    ViERenderManagerScoped rs(_renderManager);
    ViERenderer* ptrRender = rs.Renderer(videoChannel);
    if (!ptrRender)
    {
        return -1;
    }

    VideoFrame videoFrame;
    if (-1 == ptrRender->GetLastRenderedFrame(videoChannel, videoFrame))
    {
        return -1;
    }

    // copy from VideoFrame class to ViEPicture struct
    int bufferLength = (int) (videoFrame.Width() * videoFrame.Height() * 1.5);
    picture.data
        = (WebRtc_UWord8*) malloc(bufferLength * sizeof(WebRtc_UWord8));
    memcpy(picture.data, videoFrame.Buffer(), bufferLength);
    picture.size = bufferLength;
    picture.width = videoFrame.Width();
    picture.height = videoFrame.Height();
    picture.type = kVideoI420;

    return 0;
}

// ----------------------------------------------------------------------------
//
//
// GetCaptureDeviceSnapshot
// ----------------------------------------------------------------------------

int ViEFileImpl::GetCaptureDeviceSnapshot(const int captureId,
                                          const char* fileNameUTF8)
{
    ViEInputManagerScoped is(_inputManager);
    ViECapturer* ptrCapture = is.Capture(captureId);
    if (!ptrCapture)
    {
        return -1;
    }

    VideoFrame videoFrame;
    if (GetNextCapturedFrame(captureId, videoFrame) == -1)
    {
        // Failed to get a snapshot...
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceId,
                     "Could not gain acces to capture device %d video frame "
                     "%s:%d", captureId, __FUNCTION__);
        return -1;
    }

    const int JPEG_FORMAT = 0;
    int format = JPEG_FORMAT;

    switch (format)
    {
        case JPEG_FORMAT:
        {
            // *** JPEGEncoder writes the jpeg file for you (no control
            // over it) and does not return you the buffer
            // *** Thusly, we are not going to be writing to the disk here

            JpegEncoder jpegEncoder;
            RawImage inputImage;

            inputImage._width = videoFrame.Width();
            inputImage._height = videoFrame.Height();
            videoFrame.Swap(inputImage._buffer, inputImage._length,
                            inputImage._size);

            if (-1 == jpegEncoder.SetFileName(fileNameUTF8))
            {
                // could not set filename for whatever reason
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             _instanceId,
                             "\tCould not open output file '%s' for writing!",
                             fileNameUTF8);

                if (inputImage._buffer)
                {
                    delete [] inputImage._buffer;
                }
                return -1;
            }

            if (-1 == jpegEncoder.Encode(inputImage))
            {
                // could not encode i420->jpeg
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                             _instanceId,
                             "\tCould not encode i420 -> jpeg file '%s' for "
                             "writing!", fileNameUTF8);

                if (inputImage._buffer)
                {
                    delete [] inputImage._buffer;
                }
                return -1;
            }

            delete [] inputImage._buffer;
            inputImage._buffer = NULL;
            break;
        }
        default:
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceFile, _instanceId,
                         "\tUnsupported file format for %s", __FUNCTION__);
            return -1;
            break;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
//
//
// GetCaptureDeviceSnapshot
// ----------------------------------------------------------------------------

int ViEFileImpl::GetCaptureDeviceSnapshot(const int captureId,
                                          ViEPicture& picture)
{
    VideoFrame videoFrame;
    ViEInputManagerScoped is(_inputManager);
    ViECapturer* ptrCapture = is.Capture(captureId);
    if (!ptrCapture)
    {
        return -1;
    }

    if (GetNextCapturedFrame(captureId, videoFrame) == -1)
    {
        // Failed to get a snapshot...
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceId,
                     "Could not gain acces to capture device %d video frame "
                     "%s:%d", captureId, __FUNCTION__);
        return -1;
    }

    // copy from VideoFrame class to ViEPicture struct
    int bufferLength = (int) (videoFrame.Width() * videoFrame.Height() * 1.5);
    picture.data
        = (WebRtc_UWord8*) malloc(bufferLength * sizeof(WebRtc_UWord8));
    memcpy(picture.data, videoFrame.Buffer(), bufferLength);
    picture.size = bufferLength;
    picture.width = videoFrame.Width();
    picture.height = videoFrame.Height();
    picture.type = kVideoI420;

    return 0;
}

// ----------------------------------------------------------------------------
//
//
// FreePicture
// ----------------------------------------------------------------------------

int ViEFileImpl::FreePicture(ViEPicture& picture)
{
    if (picture.data)
        free(picture.data);

    picture.data = NULL;
    picture.size = 0;
    picture.width = 0;
    picture.height = 0;
    picture.type = kVideoUnknown;

    return 0;
}

// ============================================================================
// Capture device images
// ============================================================================

// ----------------------------------------------------------------------------
//
//
// SetCaptureDeviceImage
// ----------------------------------------------------------------------------

int ViEFileImpl::SetCaptureDeviceImage(const int captureId,
                                       const char* fileNameUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, _instanceId,
                 "%s(captureId: %d)", __FUNCTION__, captureId);

    ViEInputManagerScoped is(_inputManager);
    ViECapturer* ptrCapture = is.Capture(captureId);
    if (!ptrCapture)
    {
        SetLastError(kViEFileInvalidCaptureId);
        return -1;
    }

    VideoFrame captureImage;
    if (ViEFileImage::ConvertJPEGToVideoFrame(
        ViEId(_instanceId, captureId), fileNameUTF8, captureImage) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, captureId),
                     "%s(captureId: %d) Failed to open file.", __FUNCTION__,
                     captureId);
        SetLastError(kViEFileInvalidFile);
        return -1;
    }
    if (ptrCapture->SetCaptureDeviceImage(captureImage))
    {
        SetLastError(kViEFileSetCaptureImageError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
//
//
// SetCaptureDeviceImage
// ----------------------------------------------------------------------------

int ViEFileImpl::SetCaptureDeviceImage(const int captureId,
                                       const ViEPicture& picture)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, _instanceId,
                 "%s(captureId: %d)", __FUNCTION__, captureId);

    if (picture.type != kVideoI420)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, captureId),
                     "%s(captureId: %d) Not a valid picture type.",
                     __FUNCTION__, captureId);
        SetLastError(kViEFileInvalidArgument);
        return -1;
    }
    ViEInputManagerScoped is(_inputManager);
    ViECapturer* ptrCapture = is.Capture(captureId);
    if (!ptrCapture)
    {
        SetLastError(kViEFileSetCaptureImageError);
        return -1;
    }

    VideoFrame captureImage;
    if (ViEFileImage::ConvertPictureToVideoFrame(
            ViEId(_instanceId,captureId), picture, captureImage) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, captureId),
                     "%s(captureId: %d) Failed to use picture.", __FUNCTION__,
                     captureId);
        SetLastError(kViEFileInvalidFile);
        return -1;
    }
    if (ptrCapture->SetCaptureDeviceImage(captureImage))
    {
        SetLastError(kViEFileInvalidCapture);
        return -1;
    }
    return 0;
}

// ============================================================================
// Render images
// ============================================================================

// ----------------------------------------------------------------------------
//
//
// SetRenderStartImage
// ----------------------------------------------------------------------------

int ViEFileImpl::SetRenderStartImage(const int videoChannel,
                                     const char* fileNameUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(videoChannel: %d)",
                 __FUNCTION__, videoChannel);

    ViERenderManagerScoped rs(_renderManager);
    ViERenderer* ptrRender = rs.Renderer(videoChannel);
    if (!ptrRender)
    {
        SetLastError(kViEFileInvalidRenderId);
        return -1;
    }

    VideoFrame startImage;
    if (ViEFileImage::ConvertJPEGToVideoFrame(
            ViEId(_instanceId, videoChannel), fileNameUTF8, startImage) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Failed to open file.", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileInvalidFile);
        return -1;
    }
    if (ptrRender->SetRenderStartImage(startImage) != 0)
    {
        SetLastError(kViEFileSetStartImageError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
//
//
// SetRenderStartImage
// ----------------------------------------------------------------------------

int ViEFileImpl::SetRenderStartImage(const int videoChannel,
                                     const ViEPicture& picture)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(videoChannel: %d)",
                 __FUNCTION__, videoChannel);

    if (picture.type != kVideoI420)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Not a valid picture type.",
                     __FUNCTION__, videoChannel);
        SetLastError(kViEFileInvalidArgument);
        return -1;
    }

    ViERenderManagerScoped rs(_renderManager);
    ViERenderer* ptrRender = rs.Renderer(videoChannel);
    if (!ptrRender)
    {
        SetLastError(kViEFileInvalidRenderId);
        return -1;
    }

    VideoFrame startImage;
    if (ViEFileImage::ConvertPictureToVideoFrame(
            ViEId(_instanceId, videoChannel), picture, startImage) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Failed to use picture.",
                     __FUNCTION__, videoChannel);
        SetLastError(kViEFileInvalidCapture);
        return -1;
    }
    if (ptrRender->SetRenderStartImage(startImage) != 0)
    {
        SetLastError(kViEFileSetStartImageError);
        return -1;
    }
    return 0;
}

// ============================================================================
// Timeout image
// ============================================================================

// ----------------------------------------------------------------------------
//
//
// SetRenderTimeoutImage
// ----------------------------------------------------------------------------

int ViEFileImpl::SetRenderTimeoutImage(const int videoChannel,
                                       const char* fileNameUTF8,
                                       const unsigned int timeoutMs)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(videoChannel: %d)",
                 __FUNCTION__, videoChannel);

    ViERenderManagerScoped rs(_renderManager);
    ViERenderer* ptrRender = rs.Renderer(videoChannel);
    if (!ptrRender)
    {
        SetLastError(kViEFileInvalidRenderId);
        return -1;
    }
    VideoFrame timeoutImage;
    if (ViEFileImage::ConvertJPEGToVideoFrame(
            ViEId(_instanceId,videoChannel), fileNameUTF8, timeoutImage) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Failed to open file.", __FUNCTION__,
                     videoChannel);
        SetLastError(kViEFileInvalidFile);
        return -1;
    }
    WebRtc_Word32 timeoutTime = timeoutMs;
    if (timeoutMs < kViEMinRenderTimeoutTimeMs)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Invalid timeoutMs, using %d.",
                     __FUNCTION__, videoChannel, kViEMinRenderTimeoutTimeMs);
        timeoutTime = kViEMinRenderTimeoutTimeMs;
    }
    if (timeoutMs > kViEMaxRenderTimeoutTimeMs)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Invalid timeoutMs, using %d.",
                     __FUNCTION__, videoChannel, kViEMaxRenderTimeoutTimeMs);
        timeoutTime = kViEMaxRenderTimeoutTimeMs;
    }
    if (ptrRender->SetTimeoutImage(timeoutImage, timeoutTime) != 0)
    {
        SetLastError(kViEFileSetRenderTimeoutError);
        return -1;
    }
    return 0;
}

int ViEFileImpl::SetRenderTimeoutImage(const int videoChannel,
                                       const ViEPicture& picture,
                                       const unsigned int timeoutMs)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(videoChannel: %d)",
                 __FUNCTION__, videoChannel);

    if (picture.type != kVideoI420)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Not a valid picture type.",
                     __FUNCTION__, videoChannel);
        SetLastError(kViEFileInvalidArgument);
        return -1;
    }

    ViERenderManagerScoped rs(_renderManager);
    ViERenderer* ptrRender = rs.Renderer(videoChannel);
    if (!ptrRender)
    {
        SetLastError(kViEFileSetRenderTimeoutError);
        return -1;
    }
    VideoFrame timeoutImage;
    if (ViEFileImage::ConvertPictureToVideoFrame(
            ViEId(_instanceId, videoChannel), picture, timeoutImage) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Failed to use picture.",
                     __FUNCTION__, videoChannel);
        SetLastError(kViEFileInvalidCapture);
        return -1;
    }
    WebRtc_Word32 timeoutTime = timeoutMs;
    if (timeoutMs < kViEMinRenderTimeoutTimeMs)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Invalid timeoutMs, using %d.",
                     __FUNCTION__, videoChannel, kViEMinRenderTimeoutTimeMs);
        timeoutTime = kViEMinRenderTimeoutTimeMs;
    }
    if (timeoutMs > kViEMaxRenderTimeoutTimeMs)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s(videoChannel: %d) Invalid timeoutMs, using %d.",
                     __FUNCTION__, videoChannel, kViEMaxRenderTimeoutTimeMs);
        timeoutTime = kViEMaxRenderTimeoutTimeMs;
    }
    if (ptrRender->SetTimeoutImage(timeoutImage, timeoutTime) != 0)
    {
        SetLastError(kViEFileSetRenderTimeoutError);
        return -1;
    }
    return 0;
}

WebRtc_Word32 ViEFileImpl::GetNextCapturedFrame(WebRtc_Word32 captureId,
                                                VideoFrame& videoFrame)
{
    ViEInputManagerScoped is(_inputManager);
    ViECapturer* ptrCapture = is.Capture(captureId);
    if (!ptrCapture)
    {
        return -1;
    }

    ViECaptureSnapshot* snapShot = new ViECaptureSnapshot();
    ptrCapture->RegisterFrameCallback(-1, snapShot);
    bool snapshotTaken =
        snapShot->GetSnapshot(videoFrame, kViECaptureMaxSnapshotWaitTimeMs);

    // Check once again if it has been destroyed...
    ptrCapture->DeregisterFrameCallback(snapShot);
    delete snapShot;
    snapShot = NULL;

    if (snapshotTaken)
    {
        return 0;
    }
    return -1;
}

ViECaptureSnapshot::ViECaptureSnapshot() :
        _crit(*CriticalSectionWrapper::CreateCriticalSection()),
        _conditionVaraible(*ConditionVariableWrapper::CreateConditionVariable()),
        _ptrVideoFrame(NULL)
{
}

ViECaptureSnapshot::~ViECaptureSnapshot()
{
    _crit.Enter();
    _crit.Leave();
    delete &_crit;
    if (_ptrVideoFrame)
    {
        delete _ptrVideoFrame;
        _ptrVideoFrame = NULL;
    }
}

bool ViECaptureSnapshot::GetSnapshot(VideoFrame& videoFrame,
                                     unsigned int maxWaitTime)
{
    _crit.Enter();
    _ptrVideoFrame = new VideoFrame();
    if (_conditionVaraible.SleepCS(_crit, maxWaitTime))
    {
        // Snapshot taken
        videoFrame.SwapFrame(*_ptrVideoFrame);
        delete _ptrVideoFrame;
        _ptrVideoFrame = NULL;
        _crit.Leave();
        return true;
    }
    return false;
}

void ViECaptureSnapshot::DeliverFrame(int id, VideoFrame& videoFrame,
                                      int numCSRCs,
                                      const WebRtc_UWord32 CSRC[kRtpCsrcSize])
{
    CriticalSectionScoped cs(_crit);
    if (!_ptrVideoFrame)
    {
        return;
    }
    _ptrVideoFrame->SwapFrame(videoFrame);
    _conditionVaraible.WakeAll();
    return;
}
#endif
} // namespace webrtc
