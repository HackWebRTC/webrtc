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
 * vie_file_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_IMPL_H_

#include "typedefs.h"
#include "vie_defines.h"
#include "vie_file.h"
#include "vie_frame_provider_base.h"
#include "vie_ref_count.h"
#include "vie_shared_data.h"

namespace webrtc
{
class ConditionVariableWrapper;

// ----------------------------------------------------------------------------
//	ViECaptureSnapshot
// ----------------------------------------------------------------------------

class ViECaptureSnapshot: public ViEFrameCallback
{
public:
    ViECaptureSnapshot();
    ~ViECaptureSnapshot();

    bool GetSnapshot(VideoFrame& videoFrame, unsigned int maxWaitTime);

    // From ViEFrameCallback
    virtual void DeliverFrame(int id, VideoFrame& videoFrame, int numCSRCs = 0,
                              const WebRtc_UWord32 CSRC[kRtpCsrcSize] = NULL);

    virtual void DelayChanged(int id, int frameDelay) {}

    virtual int GetPreferedFrameSettings(int &width, int &height,
                                         int &frameRate)
    {
        return -1;
    }

    virtual void ProviderDestroyed(int id) {}

private:
    CriticalSectionWrapper& _crit;
    ConditionVariableWrapper& _conditionVaraible;
    VideoFrame* _ptrVideoFrame;
};

// ----------------------------------------------------------------------------
//	VideoFileImpl
// ----------------------------------------------------------------------------

class ViEFileImpl: public virtual ViESharedData,
                   public ViEFile,
                   public ViERefCount

{
public:
    virtual int Release();

    // Play file
    virtual int StartPlayFile(const char* fileNameUTF8, int& fileId,
                              const bool loop = false,
                              const webrtc::FileFormats fileFormat =
                                  webrtc::kFileFormatAviFile);

    virtual int StopPlayFile(const int fileId);

    virtual int RegisterObserver(int fileId, ViEFileObserver& observer);

    virtual int DeregisterObserver(int fileId, ViEFileObserver& observer);

    virtual int SendFileOnChannel(const int fileId, const int videoChannel);

    virtual int StopSendFileOnChannel(const int videoChannel);

    virtual int StartPlayFileAsMicrophone(const int fileId,
                                          const int audioChannel,
                                          bool mixMicrophone = false,
                                          float volumeScaling = 1);

    virtual int StopPlayFileAsMicrophone(const int fileId,
                                         const int audioChannel);

    virtual int StartPlayAudioLocally(const int fileId, const int audioChannel,
                                      float volumeScaling = 1);

    virtual int StopPlayAudioLocally(const int fileId, const int audioChannel);

    virtual int StartRecordOutgoingVideo(const int videoChannel,
                                         const char* fileNameUTF8,
                                         AudioSource audioSource,
                                         const webrtc::CodecInst& audioCodec,
                                         const VideoCodec& videoCodec,
                                         const webrtc::FileFormats fileFormat =
                                             webrtc::kFileFormatAviFile);

    virtual int StartRecordIncomingVideo(const int videoChannel,
                                         const char* fileNameUTF8,
                                         AudioSource audioSource,
                                         const webrtc::CodecInst& audioCodec,
                                         const VideoCodec& videoCodec,
                                         const webrtc::FileFormats fileFormat =
                                             webrtc::kFileFormatAviFile);

    virtual int StopRecordOutgoingVideo(const int videoChannel);

    virtual int StopRecordIncomingVideo(const int videoChannel);

    // File information
    virtual int GetFileInformation(const char* fileName,
                                   VideoCodec& videoCodec,
                                   webrtc::CodecInst& audioCodec,
                                   const webrtc::FileFormats fileFormat =
                                       webrtc::kFileFormatAviFile);

    // Snapshot
    virtual int GetRenderSnapshot(const int videoChannel,
                                  const char* fileNameUTF8);

    virtual int GetRenderSnapshot(const int videoChannel, ViEPicture& picture);

    virtual int FreePicture(ViEPicture& picture);

    virtual int GetCaptureDeviceSnapshot(const int captureId,
                                         const char* fileNameUTF8);

    virtual int GetCaptureDeviceSnapshot(const int captureId,
                                         ViEPicture& picture);

    // Capture device images
    virtual int SetCaptureDeviceImage(const int captureId,
                                      const char* fileNameUTF8);

    virtual int SetCaptureDeviceImage(const int captureId,
                                      const ViEPicture& picture);
    // Render images
    virtual int SetRenderStartImage(const int videoChannel,
                                    const char* fileNameUTF8);

    virtual int SetRenderStartImage(const int videoChannel,
                                    const ViEPicture& picture);

    // Timeout image
    virtual int SetRenderTimeoutImage(const int videoChannel,
                                      const char* fileNameUTF8,
                                      const unsigned int timeoutMs);

    virtual int SetRenderTimeoutImage(const int videoChannel,
                                      const ViEPicture& picture,
                                      const unsigned int timeoutMs);

protected:
    ViEFileImpl();
    virtual ~ViEFileImpl();

private:
    WebRtc_Word32 GetNextCapturedFrame(WebRtc_Word32 captureId,
                                       VideoFrame& videoFrame);

};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_IMPL_H_
