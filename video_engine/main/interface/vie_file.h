/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//  - File recording and playing.
//  - Snapshots.
//  - Background images.


#ifndef WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_FILE_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_FILE_H_

#include "common_types.h"

namespace webrtc
{
class VideoEngine;
struct VideoCodec;

// This structure contains picture data and describes the picture type.
struct ViEPicture
{
    unsigned char* data;
    unsigned int size;
    unsigned int width;
    unsigned int height;
    RawVideoType type;

    ViEPicture()
    {
        data = NULL;
        size = 0;
        width = 0;
        height = 0;
        type = kVideoI420;
    }

    //call FreePicture to free data
    ~ViEPicture()
    {
        data = NULL;
        size = 0;
        width = 0;
        height = 0;
        type = kVideoUnknown;
    }
};

// This enumerator tells which audio source to use for media files.
enum AudioSource
{
    NO_AUDIO,
    MICROPHONE,
    PLAYOUT,
    VOICECALL
};

// This class declares an abstract interface for a user defined observer. It is
// up to the VideoEngine user to implement a derived class which implements the
// observer class. The observer is registered using RegisterObserver() and
// deregistered using DeregisterObserver().
class WEBRTC_DLLEXPORT ViEFileObserver
{
public:
    // This method is called when the end is reached of a played file.
    virtual void PlayFileEnded(const WebRtc_Word32 fileId) = 0;

protected:
    virtual ~ViEFileObserver() {};
};

// ----------------------------------------------------------------------------
//	ViEFile
// ----------------------------------------------------------------------------

class WEBRTC_DLLEXPORT ViEFile
{
public:
    // Factory for the ViEFile sub‐API and increases an internal reference
    // counter if successful. Returns NULL if the API is not supported or if
    // construction fails.
    static ViEFile* GetInterface(VideoEngine* videoEngine);

    // Releases the ViEFile sub-API and decreases an internal reference counter.
    // Returns the new reference count. This value should be zero
    // for all sub-API:s before the VideoEngine object can be safely deleted.
    virtual int Release() = 0;

    // Starts playing a video file.
    virtual int StartPlayFile(const char* fileNameUTF8, int& fileId,
                              const bool loop = false,
                              const FileFormats fileFormat =
                                  kFileFormatAviFile) = 0;

    // Stops a file from being played.
    virtual int StopPlayFile(const int fileId) = 0;

    // Registers an instance of a user implementation of the ViEFileObserver.
    virtual int RegisterObserver(int fileId, ViEFileObserver& observer) = 0;

    // Removes an already registered instance of ViEFileObserver.
    virtual int DeregisterObserver(int fileId, ViEFileObserver& observer) = 0;

    // This function tells which channel, if any, the file should be sent on.
    virtual int SendFileOnChannel(const int fileId, const int videoChannel) = 0;

    // Stops a file from being sent on a a channel.
    virtual int StopSendFileOnChannel(const int videoChannel) = 0;

    // Starts playing the file audio as microphone input for the specified voice
    // channel.
    virtual int StartPlayFileAsMicrophone(const int fileId,
                                          const int audioChannel,
                                          bool mixMicrophone = false,
                                          float volumeScaling = 1) = 0;

    // The function stop the audio from being played on a VoiceEngine channel.
    virtual int StopPlayFileAsMicrophone(const int fileId,
                                         const int audioChannel) = 0;

    // The function plays and mixes the file audio with the local speaker signal
    // for playout.
    virtual int StartPlayAudioLocally(const int fileId, const int audioChannel,
                                      float volumeScaling = 1) = 0;

    // Stops the audio from a file from being played locally.
    virtual int StopPlayAudioLocally(const int fileId,
                                     const int audioChannel) = 0;

    // This function starts recording the video transmitted to another endpoint.
    virtual int StartRecordOutgoingVideo(const int videoChannel,
                                         const char* fileNameUTF8,
                                         AudioSource audioSource,
                                         const CodecInst& audioCodec,
                                         const VideoCodec& videoCodec,
                                         const FileFormats fileFormat =
                                             kFileFormatAviFile) =0;

    // This function starts recording the incoming video stream on a channel.
    virtual int StartRecordIncomingVideo(const int videoChannel,
                                         const char* fileNameUTF8,
                                         AudioSource audioSource,
                                         const CodecInst& audioCodec,
                                         const VideoCodec& videoCodec,
                                         const FileFormats fileFormat =
                                             kFileFormatAviFile) = 0;

    // Stops the file recording of the outgoing stream.
    virtual int StopRecordOutgoingVideo(const int videoChannel) = 0;

    // Stops the file recording of the incoming stream.
    virtual int StopRecordIncomingVideo(const int videoChannel) = 0;

    // Gets the audio codec, video codec and file format of a recorded file.
    virtual int GetFileInformation(const char* fileName,
                                   VideoCodec& videoCodec,
                                   CodecInst& audioCodec,
                                   const FileFormats fileFormat =
                                       kFileFormatAviFile) = 0;

    // The function takes a snapshot of the last rendered image for a video
    // channel.
    virtual int GetRenderSnapshot(const int videoChannel,
                                  const char* fileNameUTF8) = 0;

    // The function takes a snapshot of the last rendered image for a video
    // channel
    virtual int GetRenderSnapshot(const int videoChannel,
                                  ViEPicture& picture) = 0;

    // The function takes a snapshot of the last captured image by a specified
    // capture device.
    virtual int GetCaptureDeviceSnapshot(const int captureId,
                                         const char* fileNameUTF8) = 0;

    // The function takes a snapshot of the last captured image by a specified
    // capture device.
    virtual int GetCaptureDeviceSnapshot(const int captureId,
                                         ViEPicture& picture) = 0;

    // This function sets a jpg image to show before the first frame is captured
    // by the capture device. This frame will be encoded and transmitted to a
    // possible receiver
    virtual int SetCaptureDeviceImage(const int captureId,
                                      const char* fileNameUTF8) = 0;

    // This function sets an image to show before the first frame is captured by
    // the capture device. This frame will be encoded and transmitted to a
    // possible receiver
    virtual int SetCaptureDeviceImage(const int captureId,
                                      const ViEPicture& picture) = 0;

    virtual int FreePicture(ViEPicture& picture) = 0;

    // This function sets a jpg image to render before the first received video
    // frame is decoded for a specified channel.
    virtual int SetRenderStartImage(const int videoChannel,
                                    const char* fileNameUTF8) = 0;

    // This function sets an image to render before the first received video
    // frame is decoded for a specified channel.
    virtual int SetRenderStartImage(const int videoChannel,
                                    const ViEPicture& picture) = 0;

    // This function sets a jpg image to render if no frame is decoded for a
    // specified time interval.
    virtual int SetRenderTimeoutImage(const int videoChannel,
                                      const char* fileNameUTF8,
                                      const unsigned int timeoutMs = 1000) = 0;

    // This function sets an image to render if no frame is decoded for a
    // specified time interval.
    virtual int SetRenderTimeoutImage(const int videoChannel,
                                      const ViEPicture& picture,
                                      const unsigned int timeoutMs) = 0;

protected:
    ViEFile() {};
    virtual ~ViEFile() {};
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_FILE_H_
