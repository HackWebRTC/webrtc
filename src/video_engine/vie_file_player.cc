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
 * vie_file_player.cc
 *
 */

#include "critical_section_wrapper.h"
#include "trace.h"
#include "vie_file_player.h"
#include "tick_util.h"
#include "thread_wrapper.h"
#include "event_wrapper.h"
#include "vie_input_manager.h"
namespace webrtc {
ViEFilePlayer* ViEFilePlayer::CreateViEFilePlayer(int fileId,
                                                  int engineId,
                                                  const char* fileNameUTF8,
                                                  const bool loop,
                                                  const webrtc::FileFormats fileFormat,
                                                  ViEInputManager& inputManager,
                                                  VoiceEngine* vePtr)
{
    ViEFilePlayer* self = new ViEFilePlayer(fileId, engineId, inputManager);
    if (!self || self->Init(fileNameUTF8, loop, fileFormat, vePtr) != 0)
    {
        delete self;
        self = NULL;
    }
    return self;
}

ViEFilePlayer::ViEFilePlayer(int Id, int engineId,
                             ViEInputManager& inputManager)
    : ViEFrameProviderBase(Id, engineId), _playBackStarted(false),
      _inputManager(inputManager), _ptrFeedBackCritSect(NULL),
      _ptrAudioCritSect(NULL), _filePlayer(NULL), _audioStream(false),
      _videoClients(0), _audioClients(0), _localAudioChannel(-1), _observer(NULL),
      _veFileInterface(NULL), _veVideoSync(NULL), _ptrDecodeThread(NULL),
      _ptrDecodeEvent(NULL), _decodedAudioLength(0), _audioChannelBuffers(),
      _decodedVideo()
{
}

ViEFilePlayer::~ViEFilePlayer()
{
    StopPlay();
    delete _ptrDecodeEvent;
    delete _ptrAudioCritSect;
    delete _ptrFeedBackCritSect;
}

int ViEFilePlayer::Init(const char* fileNameUTF8, const bool loop,
                        const webrtc::FileFormats fileFormat,
                        VoiceEngine* vePtr)
{

    _ptrFeedBackCritSect = CriticalSectionWrapper::CreateCriticalSection();
    if (!_ptrFeedBackCritSect)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StartPlay() failed to allocate critsect");
        return -1;
    }

    _ptrAudioCritSect = CriticalSectionWrapper::CreateCriticalSection();
    if (!_ptrAudioCritSect)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StartPlay() failed to allocate critsect");
        return -1;
    }

    _ptrDecodeEvent = EventWrapper::Create();
    if (!_ptrDecodeEvent)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StartPlay() failed to allocate event");
        return -1;

    }
    if (strlen(fileNameUTF8) > FileWrapper::kMaxFileNameSize)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StartPlay() To long filename");
        return -1;
    }
    strncpy(_fileName, fileNameUTF8, strlen(fileNameUTF8) + 1);

    _filePlayer = FilePlayer::CreateFilePlayer(ViEId(engine_id_, id_),
                                               fileFormat);
    if (!_filePlayer)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StartPlay() failed to create file player");
        return -1;
    }
    if (_filePlayer->RegisterModuleFileCallback(this) == -1)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StartPlay() failed to RegisterModuleFileCallback");
        _filePlayer = NULL;
        return -1;
    }
    _ptrDecodeThread = ThreadWrapper::CreateThread(FilePlayDecodeThreadFunction,
                                                this, kHighestPriority,
                                                "ViEFilePlayThread");
    if (!_ptrDecodeThread)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StartPlay() failed to start decode thread.");
        _filePlayer = NULL;
        return -1;

    }

    // Always try to open with Audio since we don't know on what channels the audio should be played on.
    WebRtc_Word32 error = _filePlayer->StartPlayingVideoFile(_fileName, loop,
                                                             false);
    if (error) // Failed to open the file with audio. Try without
    {
        error = _filePlayer->StartPlayingVideoFile(_fileName, loop, true);
        _audioStream = false;
        if (error)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                       "ViEFilePlayer::StartPlay() failed to Start play video file");
            return -1;
        }

    } else
    {
        _audioStream = true;
    }

    if (_audioStream) // The file contain an audiostream
    {
        if (vePtr) // && localAudioChannel!=-1) // VeInterface have been provided and we want to play audio on local channel.
        {
            _veFileInterface = VoEFile::GetInterface(vePtr);
            if (!_veFileInterface)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                           ViEId(engine_id_, id_),
                           "ViEFilePlayer::StartPlay() failed to get VEFile interface");
                return -1;
            }
            _veVideoSync = VoEVideoSync::GetInterface(vePtr);
            if (!_veVideoSync)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                           ViEId(engine_id_, id_),
                           "ViEFilePlayer::StartPlay() failed to get "
                               "VoEVideoSync interface");
                return -1;
            }
        }
    }

    _ptrDecodeEvent->StartTimer(true, 10); // Read audio /(or just video) every 10ms.

    return 0;
}
/*
 //Implements ViEFrameProviderBase
 // Starts the decode thread when someone cares.
 */
int ViEFilePlayer::FrameCallbackChanged()
{
    if (ViEFrameProviderBase::NumberOfRegisteredFrameCallbacks() > _videoClients)
    {
        if (!_playBackStarted)
        {
            _playBackStarted = true;
            unsigned int threadId;
            if (_ptrDecodeThread->Start(threadId))
            {
                WEBRTC_TRACE(
                           webrtc::kTraceStateInfo,
                           webrtc::kTraceVideo,
                           ViEId(engine_id_, id_),
                           "ViEFilePlayer::FrameCallbackChanged() Started filedecode thread %u",
                           threadId);
            } else
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                           ViEId(engine_id_, id_),
                           "ViEFilePlayer::FrameCallbackChanged() Failed to start file decode thread.");
            }
        } else if (!_filePlayer->IsPlayingFile())
        {
            if (_filePlayer->StartPlayingVideoFile(_fileName, false,
                                                   !_audioStream) != 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                           ViEId(engine_id_, id_),
                           "ViEFilePlayer::FrameCallbackChanged(), Failed to restart the file player.");

            }

        }
    }
    _videoClients = ViEFrameProviderBase::NumberOfRegisteredFrameCallbacks();
    return 0;

}

// File play decode function.
bool ViEFilePlayer::FilePlayDecodeThreadFunction(void* obj)
{
    return static_cast<ViEFilePlayer*> (obj)->FilePlayDecodeProcess();
}
bool ViEFilePlayer::FilePlayDecodeProcess()
{

    if (_ptrDecodeEvent->Wait(kThreadWaitTimeMs) == kEventSignaled)
    {
        if (_audioStream && _audioClients == 0) // If there is audio but no one cares- read the audio self
        {
            Read(NULL, 0);
        }
        if (_filePlayer->TimeUntilNextVideoFrame() < 10) // Less than 10ms to next videoframe
        {
            if (_filePlayer->GetVideoFromFile(_decodedVideo) != 0)
            {
            }
        }
        if (_decodedVideo.Length() > 0)
        {

            if (_localAudioChannel != -1 && _veVideoSync) // We are playing audio locally
            {
                int audioDelay = 0;
                if (_veVideoSync->GetPlayoutBufferSize(audioDelay) == 0)
                {
                    _decodedVideo.SetRenderTime(_decodedVideo.RenderTimeMs()
                        + audioDelay);
                }
            }
            DeliverFrame(_decodedVideo);
            _decodedVideo.SetLength(0);
        }

    }
    return true;
}

int ViEFilePlayer::StopPlay() //Only called from destructor.
{
    if (_ptrDecodeThread)
    {
        _ptrDecodeThread->SetNotAlive();
        if (_ptrDecodeThread->Stop())
        {

            delete _ptrDecodeThread;
        } else
        {
            assert(!"ViEFilePlayer::StopPlay() Failed to stop decode thread");
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                       "ViEFilePlayer::StartPlay() Failed to stop file decode thread.");
        }
    }

    _ptrDecodeThread = NULL;
    if (_ptrDecodeEvent)
    {
        _ptrDecodeEvent->StopTimer();
    }

    StopPlayAudio();

    if (_veFileInterface)
    {
        _veFileInterface->Release();
        _veFileInterface = NULL;
    }
    if (_veVideoSync)
    {
        _veVideoSync->Release();
        _veVideoSync = NULL;
    }

    if (_filePlayer)
    {
        _filePlayer->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_filePlayer);
        _filePlayer = NULL;
    }

    return 0;
}
int ViEFilePlayer::StopPlayAudio()
{
    // Stop sending audio
    while (MapItem* audioItem = _audioChannelsSending.First())
    {
        StopSendAudioOnChannel(audioItem->GetId());
    }

    // Stop local audio playback
    if (_localAudioChannel != -1)
    {
        StopPlayAudioLocally(_localAudioChannel);
    }
    _localAudioChannel = -1;
    while (_audioChannelBuffers.PopFront() != -1) {}
    while (_audioChannelsSending.Erase(_audioChannelsSending.First()) != -1) {}
    _audioClients = 0;
    return 0;
}

// From webrtc::InStream
int ViEFilePlayer::Read(void *buf, int len)
{
    CriticalSectionScoped lock(*_ptrAudioCritSect); // Protect from simultaneouse reading from multiple channels
    if (NeedsAudioFromFile(buf))
    {
        if (_filePlayer->Get10msAudioFromFile(_decodedAudio,
                                              _decodedAudioLength, 16000) != 0) // we will run the VE in 16KHz
        {
            // No data
            _decodedAudioLength = 0;
            return 0;
        }
        _decodedAudioLength *= 2; // 2 bytes per sample
        if (buf != 0)
        {
            _audioChannelBuffers.PushBack(buf);
        }
    } else
    {
        // No need for new audiobuffer from file. Ie the buffer read from file has not been played on this channel.
    }
    if (buf)
    {
        memcpy(buf, _decodedAudio, _decodedAudioLength);
    }
    return _decodedAudioLength;

}
bool ViEFilePlayer::NeedsAudioFromFile(void* buf)
{
    bool needsNewAudio = false;
    if (_audioChannelBuffers.GetSize() == 0)
    {
        return true;
    }

    //Check if we the buf already have read the current audio.
    for (ListItem* item = _audioChannelBuffers.First(); item != NULL; item
        = _audioChannelBuffers.Next(item))
    {
        if (item->GetItem() == buf)
        {
            needsNewAudio = true;
            _audioChannelBuffers.Erase(item);
            break;
        }
    }
    return needsNewAudio;
}

// From FileCallback
void ViEFilePlayer::PlayFileEnded(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_, id),
               "%s: fileId %d", __FUNCTION__, id_);

    _filePlayer->StopPlayingFile();

    CriticalSectionScoped lock(*_ptrFeedBackCritSect);
    if (_observer)
    {
        _observer->PlayFileEnded(id_);
    }
}

bool ViEFilePlayer::IsObserverRegistered()
{
    CriticalSectionScoped lock(*_ptrFeedBackCritSect);
    return _observer != NULL;

}
int ViEFilePlayer::RegisterObserver(ViEFileObserver& observer)
{
    CriticalSectionScoped lock(*_ptrFeedBackCritSect);
    if (_observer)
        return -1;
    _observer = &observer;
    return 0;
}
int ViEFilePlayer::DeRegisterObserver()
{
    CriticalSectionScoped lock(*_ptrFeedBackCritSect);
    _observer = NULL;
    return 0;
}

// ----------------------------------------------------------------------------
// SendAudioOnChannel
// Order the voice engine to send the audio on a channel
// ----------------------------------------------------------------------------
int ViEFilePlayer::SendAudioOnChannel(const int audioChannel,
                                      bool mixMicrophone, float volumeScaling)
{

    if (!_veFileInterface)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "%s No VEFile interface.", __FUNCTION__);
        return -1;
    }
    if (_veFileInterface->StartPlayingFileAsMicrophone(audioChannel,
                                                       this,
                                                       mixMicrophone,
                                                       kFileFormatPcm16kHzFile,
                                                       volumeScaling) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::SendAudioOnChannel() VE_StartPlayingFileAsMicrophone failed. audioChannel %d, mixMicrophone %d, volumeScaling %.2f",
                   audioChannel, mixMicrophone, volumeScaling);
        return -1;
    }
    _audioChannelsSending.Insert(audioChannel, NULL);

    CriticalSectionScoped lock(*_ptrAudioCritSect);
    _audioClients++; // Increase the number of audioClients;

    return 0;
}

// ----------------------------------------------------------------------------
// StopSendAudioOnChannel
// Order the voice engine to stop send the audio on a channel
// ----------------------------------------------------------------------------
int ViEFilePlayer::StopSendAudioOnChannel(const int audioChannel)
{
    int result = 0;
    MapItem* audioItem = _audioChannelsSending.Find(audioChannel);
    if (!audioItem)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "_s AudioChannel %d not sending", __FUNCTION__, audioChannel);
        return -1;
    }
    result = _veFileInterface->StopPlayingFileAsMicrophone(audioChannel);
    if (result != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StopSendAudioOnChannel() VE_StopPlayingFileAsMicrophone failed. audioChannel %d",
                   audioChannel);
    }
    _audioChannelsSending.Erase(audioItem);
    CriticalSectionScoped lock(*_ptrAudioCritSect);
    _audioClients--; // Decrease the number of audioClients;
    assert(_audioClients>=0);
    return 0;

}
int ViEFilePlayer::PlayAudioLocally(const int audioChannel, float volumeScaling)
{
    if (!_veFileInterface)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "%s No VEFile interface.", __FUNCTION__);
        return -1;
    }
    if (_veFileInterface->StartPlayingFileLocally(
                                                  audioChannel,
                                                  this,
                                                  kFileFormatPcm16kHzFile,
                                                  volumeScaling) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "%s  VE_StartPlayingFileAsMicrophone failed. audioChannel %d, mixMicrophone %d, volumeScaling %.2f",
                   __FUNCTION__, audioChannel, volumeScaling);
        return -1;
    }

    CriticalSectionScoped lock(*_ptrAudioCritSect);
    _localAudioChannel = audioChannel;
    _audioClients++; // Increase the number of audioClients;

    return 0;

}

int ViEFilePlayer::StopPlayAudioLocally(const int audioChannel)
{
    if (!_veFileInterface)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "%s No VEFile interface.", __FUNCTION__);
        return -1;
    }
    if (_veFileInterface->StopPlayingFileLocally(audioChannel) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_, id_),
                   "%s VE_StopPlayingFileLocally failed. audioChannel %d.",
                   __FUNCTION__, audioChannel);
        return -1;
    }

    CriticalSectionScoped lock(*_ptrAudioCritSect);
    _localAudioChannel = -1;
    _audioClients--; // Decrease the number of audioClients;

    return 0;

}

//static
int ViEFilePlayer::GetFileInformation(int engineId, const char* fileName,
                                      VideoCodec& videoCodec,
                                      webrtc::CodecInst& audioCodec,
                                      const webrtc::FileFormats fileFormat)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, engineId, "%s ", __FUNCTION__);

    FilePlayer* filePlayer = FilePlayer::CreateFilePlayer(engineId, fileFormat);
    if (!filePlayer)
    {
        return -1;
    }

    bool videoOnly = false;

    memset(&videoCodec, 0, sizeof(videoCodec));
    memset(&audioCodec, 0, sizeof(audioCodec));

    if (filePlayer->StartPlayingVideoFile(fileName, false, false) != 0)
    {
        videoOnly = true;
        if (filePlayer->StartPlayingVideoFile(fileName, false, true) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, engineId,
                       "%s Failed to open file.", __FUNCTION__);
            FilePlayer::DestroyFilePlayer(filePlayer);
            return -1;
        }
    }

    if (!videoOnly && filePlayer->AudioCodec(audioCodec) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, engineId,
                   "%s Failed to get audio codec.", __FUNCTION__);
        FilePlayer::DestroyFilePlayer(filePlayer);
        return -1;
    }
    if (filePlayer->video_codec_info(videoCodec) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, engineId,
                   "%s Failed to get video codec.", __FUNCTION__);
        FilePlayer::DestroyFilePlayer(filePlayer);
        return -1;
    }
    FilePlayer::DestroyFilePlayer(filePlayer);
    return 0;
}
} // namespace webrtc
