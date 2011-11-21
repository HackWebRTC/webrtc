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
 * vie_encoder.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_ENCODER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_ENCODER_H_

#include "vie_defines.h"
#include "typedefs.h"
#include "vie_frame_provider_base.h"
#include "vie_file_recorder.h"
#include "rtp_rtcp_defines.h"
#include "video_coding_defines.h"
#include "video_processing.h"
#include "common_types.h"

namespace webrtc {
class CriticalSectionWrapper;
class ProcessThread;
class RtpRtcp;
class ViEEffectFilter;
class VideoCodingModule;
class ViEEncoderObserver;

class ViEEncoder:   public ViEFrameCallback, // New frame delivery
                    public RtpVideoFeedback, // Feedback from RTP module
                    public RtcpFeedback, // RTP/RTCP Module
                    public VCMPacketizationCallback, // Callback from VCM
                    public VCMProtectionCallback, // Callback from VCM
                    public VCMSendStatisticsCallback // Callback from VCM
{
public:
    ViEEncoder(WebRtc_Word32 engineId, WebRtc_Word32 channelId,
               WebRtc_UWord32 numberOfCores,
               ProcessThread& moduleProcessThread);
    ~ViEEncoder();

    // Drops incoming packets
    void Pause();
    void Restart();

    WebRtc_Word32 DropDeltaAfterKey(bool enable);

    // Codec settings
    WebRtc_UWord8 NumberOfCodecs();
    WebRtc_Word32 GetCodec(WebRtc_UWord8 listIndex, VideoCodec& videoCodec);
    WebRtc_Word32 RegisterExternalEncoder(VideoEncoder* encoder,
                                          WebRtc_UWord8 plType);
    WebRtc_Word32 DeRegisterExternalEncoder(WebRtc_UWord8 plType);
    WebRtc_Word32 SetEncoder(const VideoCodec& videoCodec);
    WebRtc_Word32 GetEncoder(VideoCodec& videoCodec);

    WebRtc_Word32 GetCodecConfigParameters(
        unsigned char configParameters[kConfigParameterSize],
        unsigned char& configParametersSize);

    // Scale or crop/pad image
    WebRtc_Word32 ScaleInputImage(bool enable);

    // RTP settings
    RtpRtcp* SendRtpRtcpModule();

    // Implementing ViEFrameCallback
    virtual void DeliverFrame(int id, VideoFrame& videoFrame, int numCSRCs = 0,
                              const WebRtc_UWord32 CSRC[kRtpCsrcSize] = NULL);
    virtual void DelayChanged(int id, int frameDelay);
    virtual int GetPreferedFrameSettings(int &width, int &height,
                                         int &frameRate);

    virtual void ProviderDestroyed(int id) { return; }

    WebRtc_Word32 EncodeFrame(VideoFrame& videoFrame);
    WebRtc_Word32 SendKeyFrame();
    WebRtc_Word32 SendCodecStatistics(WebRtc_UWord32& numKeyFrames,
                                      WebRtc_UWord32& numDeltaFrames);
    // Loss protection
    WebRtc_Word32 UpdateProtectionMethod();
    // Implements VCMPacketizationCallback
    virtual WebRtc_Word32 SendData(
        const FrameType frameType,
        const WebRtc_UWord8 payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const RTPFragmentationHeader& fragmentationHeader,
        const RTPVideoHeader* rtpVideoHdr);
    // Implements VideoProtectionCallback
    virtual WebRtc_Word32 ProtectionRequest(const WebRtc_UWord8 deltaFECRate,
                                            const WebRtc_UWord8 keyFECRate,
                                            const bool deltaUseUepProtection,
                                            const bool keyUseUepProtection,
                                            const bool nack);

    // Implements VideoSendStatisticsCallback
    virtual WebRtc_Word32 SendStatistics(const WebRtc_UWord32 bitRate,
                                         const WebRtc_UWord32 frameRate);
    WebRtc_Word32 RegisterCodecObserver(ViEEncoderObserver* observer);
    // Implements RtcpFeedback
    virtual void OnSLIReceived(const WebRtc_Word32 id,
                               const WebRtc_UWord8 pictureId);
    virtual void OnRPSIReceived(const WebRtc_Word32 id,
                                const WebRtc_UWord64 pictureId);

    // Implements RtpVideoFeedback
    virtual void OnReceivedIntraFrameRequest(const WebRtc_Word32 id,
                                             const FrameType type,
                                             const WebRtc_UWord8 streamIdx);

    virtual void OnNetworkChanged(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 bitrateBps,
                                  const WebRtc_UWord8 fractionLost,
                                  const WebRtc_UWord16 roundTripTimeMs);
    // Effect filter
    WebRtc_Word32 RegisterEffectFilter(ViEEffectFilter* effectFilter);
    //Recording
    ViEFileRecorder& GetOutgoingFileRecorder();

private:
    WebRtc_Word32 _engineId;

    class QMTestVideoSettingsCallback : public VCMQMSettingsCallback
    {
    public:
        QMTestVideoSettingsCallback();
        // update VPM with QM (quality modes: frame size & frame rate) settings
        WebRtc_Word32 SetVideoQMSettings(const WebRtc_UWord32 frameRate,
                                         const WebRtc_UWord32 width,
                                         const WebRtc_UWord32 height);
        // register VPM and VCM
        void RegisterVPM(VideoProcessingModule* vpm);
        void RegisterVCM(VideoCodingModule* vcm);
        void SetNumOfCores(WebRtc_Word32 numOfCores)
            {_numOfCores = numOfCores;};
        void SetMaxPayloadLength(WebRtc_Word32 maxPayloadLength)
            {_maxPayloadLength = maxPayloadLength;};
    private:
        VideoProcessingModule*         _vpm;
        VideoCodingModule*             _vcm;
        WebRtc_Word32                  _numOfCores;
        WebRtc_Word32                  _maxPayloadLength;
    };

    WebRtc_Word32 _channelId;
    const WebRtc_UWord32 _numberOfCores;

    VideoCodingModule& _vcm;
    VideoProcessingModule& _vpm;
    RtpRtcp& _defaultRtpRtcp;
    CriticalSectionWrapper& _callbackCritsect;
    CriticalSectionWrapper& _dataCritsect;
    VideoCodec _sendCodec;

    bool _paused;
    WebRtc_Word64 _timeLastIntraRequestMs[kMaxSimulcastStreams];
    WebRtc_Word32 _channelsDroppingDeltaFrames;
    bool _dropNextFrame;
    //Loss protection
    bool _fecEnabled;
    bool _nackEnabled;
    // Uses
    ViEEncoderObserver* _codecObserver;
    ViEEffectFilter* _effectFilter;
    ProcessThread& _moduleProcessThread;

    bool _hasReceivedSLI;
    WebRtc_UWord8 _pictureIdSLI;
    bool _hasReceivedRPSI;
    WebRtc_UWord64 _pictureIdRPSI;

    //Recording
    ViEFileRecorder _fileRecorder;

    // Quality modes callback
    QMTestVideoSettingsCallback* _qmCallback;

};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_ENCODER_H_
