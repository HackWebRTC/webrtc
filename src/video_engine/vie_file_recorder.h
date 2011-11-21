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
 * vie_file_recorder.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_RECORDER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_RECORDER_H_

#include "typedefs.h"
#include "file_recorder.h"
#include "vie_file.h"
#include "voe_file.h"

namespace webrtc {
class CriticalSectionWrapper;

class ViEFileRecorder: protected webrtc::OutStream // for audio
{
public:
    ViEFileRecorder(int channelId);
    ~ViEFileRecorder();

    int StartRecording(const char* fileNameUTF8,
                       const webrtc::VideoCodec& codecInst,
                       AudioSource audioSource, int audioChannel,
                       const webrtc::CodecInst audioCodecInst,
                       VoiceEngine* vePtr,
                       const webrtc::FileFormats fileFormat = webrtc::kFileFormatAviFile);
    int StopRecording();
    void SetFrameDelay(int frameDelay);
    bool RecordingStarted();
    void RecordVideoFrame(const VideoFrame& videoFrame);

protected:
    bool FirstFrameRecorded();
    bool IsRecordingFileFormat(const webrtc::FileFormats fileFormat);
    // From webrtc::OutStream
    bool Write(const void* buf, int len);
    int Rewind();

private:
    CriticalSectionWrapper* _ptrCritSec;

    FileRecorder* _fileRecorder;
    bool _isFirstFrameRecorded;
    bool _isOutStreamStarted;
    int _instanceID;
    int _frameDelay;
    int _audioChannel;
    AudioSource _audioSource;
    VoEFile* _veFileInterface;
};

} // namespace webrtc
#endif // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_RECORDER_H_
