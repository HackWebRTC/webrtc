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
 * vie_file_recorder.cc
 *
 */
#include "vie_file_recorder.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "tick_util.h"
#include "file_player.h"
#include "file_recorder.h"
#include "vie_defines.h"

namespace webrtc {

ViEFileRecorder::ViEFileRecorder(int instanceID)
    :   _ptrCritSec(CriticalSectionWrapper::CreateCriticalSection()),
        _fileRecorder(NULL), _isFirstFrameRecorded(false),
        _isOutStreamStarted(false), _instanceID(instanceID), _frameDelay(0),
        _audioChannel(-1), _audioSource(NO_AUDIO),
        _veFileInterface(NULL)
{
}

ViEFileRecorder::~ViEFileRecorder()
{
    StopRecording();
    delete _ptrCritSec;
}

int ViEFileRecorder::StartRecording(const char* fileNameUTF8,
                                    const VideoCodec& codecInst,
                                    AudioSource audioSource,
                                    int audioChannel,
                                    const webrtc::CodecInst audioCodecInst,
                                    VoiceEngine* vePtr,
                                    const webrtc::FileFormats fileFormat)
{
    CriticalSectionScoped lock(*_ptrCritSec);

    if (_fileRecorder)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceID,
                   "ViEFileRecorder::StartRecording() failed, already recording.");
        return -1;
    }
    _fileRecorder = FileRecorder::CreateFileRecorder(_instanceID, fileFormat);
    if (!_fileRecorder)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceID,
                   "ViEFileRecorder::StartRecording() failed to create file recoder.");
        return -1;
    }

    int error = _fileRecorder->StartRecordingVideoFile(fileNameUTF8,
                                                       audioCodecInst,
                                                       codecInst,
                                                       AMRFileStorage,
                                                       audioSource == NO_AUDIO);
    if (error)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceID,
                   "ViEFileRecorder::StartRecording() failed to StartRecordingVideoFile.");
        FileRecorder::DestroyFileRecorder(_fileRecorder);
        _fileRecorder = NULL;
        return -1;
    }

    _audioSource = audioSource;
    if (vePtr && audioSource != NO_AUDIO) // VeInterface have been provided and we want to record audio
    {
        _veFileInterface = VoEFile::GetInterface(vePtr);
        if (!_veFileInterface)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceID,
                       "ViEFileRecorder::StartRecording() failed to get VEFile interface");
            return -1;
        }
        // always drive VoE in L16
        CodecInst engineAudioCodecInst = { 96, // .pltype
                                            "L16", // .plname
                                            audioCodecInst.plfreq, // .plfreq
                                            audioCodecInst.plfreq / 100, // .pacsize (10ms)
                                            1, // .channels
                                            audioCodecInst.plfreq * 16 // .rate
                                         };

        switch (audioSource)
        {
            case MICROPHONE:
                error
                    = _veFileInterface->StartRecordingMicrophone(
                                                                 this,
                                                                 &engineAudioCodecInst);
                break;
            case PLAYOUT:
                error
                    = _veFileInterface->StartRecordingPlayout(
                                                              audioChannel,
                                                              this,
                                                              &engineAudioCodecInst);
                break;
            case NO_AUDIO:
                break;
            default:
                assert(!"Unknown audioSource");
        }
        if (error != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceID,
                       "ViEFileRecorder::StartRecording() failed to start recording audio");
            FileRecorder::DestroyFileRecorder(_fileRecorder);
            _fileRecorder = NULL;
            return -1;
        }
        _isOutStreamStarted = true;
        _audioChannel = audioChannel;
    }

    _isFirstFrameRecorded = false;
    return 0;
}

int ViEFileRecorder::StopRecording()
{

    int error = 0;
    // Stop recording audio
    // Note - we can not hold the _ptrCritSect while accessing VE functions. It might cause deadlock in Write
    if (_veFileInterface)
    {
        switch (_audioSource)
        {
            case MICROPHONE:
                error = _veFileInterface->StopRecordingMicrophone();
                break;
            case PLAYOUT:
                error = _veFileInterface->StopRecordingPlayout(_audioChannel);
                break;
            case NO_AUDIO:
                break;
            default:
                assert(!"Unknown audioSource");
        }
        if (error != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceID,
                       "ViEFileRecorder::StopRecording() failed to stop recording audio");
        }
    }
    CriticalSectionScoped lock(*_ptrCritSec);
    if (_veFileInterface)
    {
        _veFileInterface->Release();
        _veFileInterface = NULL;
    }

    if (_fileRecorder)
    {
        if (_fileRecorder->IsRecording())
        {
            int error = _fileRecorder->StopRecording();
            if (error)
            {
                return -1;
            }
        }
        FileRecorder::DestroyFileRecorder(_fileRecorder);
        _fileRecorder = NULL;
    }
    _isFirstFrameRecorded = false;
    _isOutStreamStarted = false;
    return 0;
}

void ViEFileRecorder::SetFrameDelay(int frameDelay)
{
    CriticalSectionScoped lock(*_ptrCritSec);
    _frameDelay = frameDelay;
}

bool ViEFileRecorder::RecordingStarted()
{
    CriticalSectionScoped lock(*_ptrCritSec);
    return _fileRecorder && _fileRecorder->IsRecording();
}

bool ViEFileRecorder::FirstFrameRecorded()
{
    CriticalSectionScoped lock(*_ptrCritSec);
    return _isFirstFrameRecorded;
}

bool ViEFileRecorder::IsRecordingFileFormat(const webrtc::FileFormats fileFormat)
{
    CriticalSectionScoped lock(*_ptrCritSec);
    return (_fileRecorder->RecordingFileFormat() == fileFormat) ? true : false;
}

/*******************************************************************************
 * void RecordVideoFrame()
 *
 * Records incoming decoded video frame to AVI-file.
 *
 */
void ViEFileRecorder::RecordVideoFrame(const VideoFrame& videoFrame)
{
    CriticalSectionScoped lock(*_ptrCritSec);

    if (_fileRecorder && _fileRecorder->IsRecording())
    {
        if (!IsRecordingFileFormat(webrtc::kFileFormatAviFile))
        {
            return;
        }

        //Compensate for frame delay in order to get audiosync when recording local video.
        const WebRtc_UWord32 timeStamp = videoFrame.TimeStamp();
        const WebRtc_Word64 renderTimeStamp = videoFrame.RenderTimeMs();
        VideoFrame& unconstVideoFrame =
            const_cast<VideoFrame&> (videoFrame);
        unconstVideoFrame.SetTimeStamp(timeStamp - 90 * _frameDelay);
        unconstVideoFrame.SetRenderTime(renderTimeStamp - _frameDelay);

        _fileRecorder->RecordVideoToFile(unconstVideoFrame);

        unconstVideoFrame.SetRenderTime(renderTimeStamp);
        unconstVideoFrame.SetTimeStamp(timeStamp);
    }
}

// ---------------------
// From OutStream
// ---------------------
// 10 ms block of PCM 16
bool ViEFileRecorder::Write(const void* buf, int len)
{
    if (!_isOutStreamStarted)
        return true;

    // always L16 from VoCE
    if (len % (2 * 80)) // 2 bytes 80 samples
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _audioChannel,
                   "Audio length not supported: %d.", len);
        return true;
    }
    AudioFrame audioFrame;
    WebRtc_UWord16 lengthInSamples = len / 2;

    audioFrame.UpdateFrame(_audioChannel, 0, (const WebRtc_Word16*) buf,
                           lengthInSamples, lengthInSamples * 100,
                           AudioFrame::kUndefined,
                           AudioFrame::kVadUnknown);

    CriticalSectionScoped lock(*_ptrCritSec);

    if (_fileRecorder && _fileRecorder->IsRecording())
    {
        TickTime tickTime = TickTime::Now();
        _fileRecorder->RecordAudioToFile(audioFrame, &tickTime);
    }
    return true; // Always return true!
}

int ViEFileRecorder::Rewind()
{
    // Not supported!
    return -1;
}
} // namespace webrtc

