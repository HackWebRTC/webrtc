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
 * vie_file_player.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_PLAYER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_PLAYER_H_

#include "typedefs.h"
#include "common_types.h"          // webrtc::OutStream
#include "file_player.h"
#include "media_file_defines.h"
#include "vie_file.h"
#include "voe_file.h"
#include "voe_video_sync.h"
#include "list_wrapper.h"
#include "vie_frame_provider_base.h"
#include "file_wrapper.h"

namespace webrtc
{
class EventWrapper;
class ThreadWrapper;
class ViEInputManager;
class ViEFilePlayer: public ViEFrameProviderBase,
                     protected webrtc::FileCallback,
                     protected webrtc::InStream // for audio
{
public:
    static ViEFilePlayer *CreateViEFilePlayer(int fileId, int engineId,
                                              const char* fileNameUTF8,
                                              const bool loop,
                                              const webrtc::FileFormats fileFormat,
                                              ViEInputManager& inputManager,
                                              VoiceEngine* vePtr);

    static int GetFileInformation(const int engineId,
                                  const char* fileName,
                                  webrtc::VideoCodec& videoCodec,
                                  webrtc::CodecInst& audioCodec,
                                  const webrtc::FileFormats fileFormat);
    ~ViEFilePlayer();

    bool IsObserverRegistered();
    int RegisterObserver(ViEFileObserver& observer);
    int DeRegisterObserver();
    int SendAudioOnChannel(const int audioChannel, bool mixMicrophone,
                           float volumeScaling);
    int StopSendAudioOnChannel(const int audioChannel);
    int PlayAudioLocally(const int audioChannel, float volumeScaling);
    int StopPlayAudioLocally(const int audioChannel);

    //Implement ViEFrameProviderBase
    virtual int FrameCallbackChanged();

protected:
    ViEFilePlayer(int Id, int engineId, ViEInputManager& inputManager);
    int Init(const WebRtc_Word8* fileNameUTF8, const bool loop,
             const webrtc::FileFormats fileFormat, VoiceEngine* vePtr);
    int StopPlay();
    int StopPlayAudio();

    // File play decode function.
    static bool FilePlayDecodeThreadFunction(void* obj);
    bool FilePlayDecodeProcess();
    bool NeedsAudioFromFile(void* buf);

    // From webrtc::InStream
    virtual int Read(void *buf, int len);
    virtual int Rewind() { return 0;}

    // From FileCallback
    virtual void PlayNotification(const WebRtc_Word32 /*id*/,
                                  const WebRtc_UWord32 /*notificationMs*/){}
    virtual void RecordNotification(const WebRtc_Word32 id,
                            const WebRtc_UWord32 notificationMs){}
    virtual void PlayFileEnded(const WebRtc_Word32 id);
    virtual void RecordFileEnded(const WebRtc_Word32 id) { }


private:
    enum   { kThreadWaitTimeMs = 100 };

    bool _playBackStarted;
    ViEInputManager& _inputManager;

    CriticalSectionWrapper* _ptrFeedBackCritSect;
    CriticalSectionWrapper* _ptrAudioCritSect;

    webrtc::FilePlayer* _filePlayer;
    bool _audioStream;

    int _videoClients; // Number of active video clients
    int _audioClients; //No of audio channels sending this audio.
    int _localAudioChannel; //Local audio channel playing this video. Sync video against this.

    ViEFileObserver* _observer;
    WebRtc_Word8 _fileName[FileWrapper::kMaxFileNameSize];

    // VE Interface
    VoEFile* _veFileInterface;
    VoEVideoSync* _veVideoSync;
    // Thread for decoding video (and audio if no audio clients connected)
    ThreadWrapper* _ptrDecodeThread;
    EventWrapper* _ptrDecodeEvent;
    WebRtc_Word16 _decodedAudio[320];
    WebRtc_UWord32 _decodedAudioLength;

    ListWrapper _audioChannelBuffers; //trick - list containing VE buffer reading this file. Used if multiple audio channels are sending.
    MapWrapper _audioChannelsSending; // AudioChannels sending audio from this file
    VideoFrame _decodedVideo; // Frame receiving decoded video from file.
};
} // namespace webrtc

#endif // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_PLAYER_H_
