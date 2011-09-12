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
 * vie_capturer.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CAPTURER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CAPTURER_H_

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"
#include "typedefs.h"

#include "video_capture.h"
#include "video_processing.h"
#include "vie_frame_provider_base.h"
#include "video_codec_interface.h"
#include "video_coding.h"
#include "vie_capture.h"
#include "common_types.h"

// Forward declarations
struct ViEPicture;

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class ViEEffectFilter;
class ViEEncoder;
class ProcessThread;

class ViECapturer: public ViEFrameProviderBase,
                  public ViEExternalCapture, // External capture
                  protected VideoCaptureDataCallback,
                  protected VideoEncoder,
                  protected VCMReceiveCallback,
                  protected VideoCaptureFeedBack
{
public:
    static ViECapturer* CreateViECapture(int captureId, int,
                                        VideoCaptureModule& captureModule,
                                        ProcessThread& moduleProcessThread);

    static ViECapturer* CreateViECapture(int captureId, int engineId,
                                        const WebRtc_UWord8* deviceUniqueIdUTF8,
                                        WebRtc_UWord32 deviceUniqueIdUTF8Length,
                                        ProcessThread& moduleProcessThread);
    ~ViECapturer();

    //Override ViEFrameProviderBase
    int FrameCallbackChanged();
    virtual int DeregisterFrameCallback(const ViEFrameCallback* callbackObject);
    bool IsFrameCallbackRegistered(const ViEFrameCallback* callbackObject);

    // Implements ExternalCapture
    virtual int IncomingFrame(unsigned char* videoFrame,
                              unsigned int videoFrameLength,
                              unsigned short width, unsigned short height,
                              RawVideoType videoType,
                              unsigned long long captureTime = 0);

    // Use this capture device as encoder. Returns 0 if the codec is supported by this capture device.
    virtual WebRtc_Word32 PreEncodeToViEEncoder(const VideoCodec& codec,
                                                ViEEncoder& vieEncoder,
                                                WebRtc_Word32 vieEncoderId);

    // Start/Stop
    WebRtc_Word32 Start(const CaptureCapability captureCapability =
                        CaptureCapability());
    WebRtc_Word32 Stop();
    bool Started();

    WebRtc_Word32 SetCaptureDelay(WebRtc_Word32 delayMS);
    WebRtc_Word32 SetRotateCapturedFrames(const RotateCapturedFrame rotation);

    // Effect filter
    WebRtc_Word32 RegisterEffectFilter(ViEEffectFilter* effectFilter);
    WebRtc_Word32 EnableDenoising(bool enable);
    WebRtc_Word32 EnableDeflickering(bool enable);
    WebRtc_Word32 EnableBrightnessAlarm(bool enable);

    // Statistic observer
    WebRtc_Word32 RegisterObserver(ViECaptureObserver& observer);
    WebRtc_Word32 DeRegisterObserver();
    bool IsObserverRegistered();

    //Information
    const WebRtc_UWord8* CurrentDeviceName() const;

    // set device images
    WebRtc_Word32 SetCaptureDeviceImage(const VideoFrame& captureDeviceImage);

protected:
    ViECapturer(int captureId, int engineId,
               ProcessThread& moduleProcessThread);

    WebRtc_Word32 Init(VideoCaptureModule& captureModule);
    WebRtc_Word32 Init(const WebRtc_UWord8* deviceUniqueIdUTF8,
                       const WebRtc_UWord32 deviceUniqueIdUTF8Length);

    // Implements VideoCaptureDataCallback
    virtual void OnIncomingCapturedFrame(const WebRtc_Word32 id,
                                         VideoFrame& videoFrame,
                                         VideoCodecType codecType);
    virtual void OnCaptureDelayChanged(const WebRtc_Word32 id,
                                       const WebRtc_Word32 delay);
    bool EncoderActive();
    bool CaptureCapabilityFixed(); // Returns true if the capture capability has been set in the StartCapture function and may not be changed.
    WebRtc_Word32 IncImageProcRefCount();
    WebRtc_Word32 DecImageProcRefCount();

    // Implements VideoEncoder
    virtual WebRtc_Word32 Version(WebRtc_Word8 *version, WebRtc_Word32 length) const;
    virtual WebRtc_Word32 InitEncode(const VideoCodec* codecSettings,
                                     WebRtc_Word32 numberOfCores,
                                     WebRtc_UWord32 maxPayloadSize);
    virtual WebRtc_Word32 Encode(const RawImage& inputImage,
                                 const CodecSpecificInfo* codecSpecificInfo =
                                     NULL,
                                 VideoFrameType frameType = kDeltaFrame);
    virtual WebRtc_Word32 RegisterEncodeCompleteCallback(
                                                EncodedImageCallback* callback);
    virtual WebRtc_Word32 Release();
    virtual WebRtc_Word32 Reset();
    virtual WebRtc_Word32 SetPacketLoss(WebRtc_UWord32 packetLoss);
    virtual WebRtc_Word32 SetRates(WebRtc_UWord32 newBitRate,
                                   WebRtc_UWord32 frameRate);

    // Implements  VCMReceiveCallback
    virtual WebRtc_Word32 FrameToRender(VideoFrame& videoFrame);
    // Implements VideoCaptureFeedBack
    virtual void OnCaptureFrameRate(const WebRtc_Word32 id,
                                    const WebRtc_UWord32 frameRate);
    virtual void OnNoPictureAlarm(const WebRtc_Word32 id,
                                  const VideoCaptureAlarm alarm);

    // Thread functions for deliver captured frames to receivers
    static bool ViECaptureThreadFunction(void* obj);
    bool ViECaptureProcess();

    void DeliverI420Frame(VideoFrame& videoFrame);
    void DeliverCodedFrame(VideoFrame& videoFrame);

private:
    enum  {kThreadWaitTimeMs = 100};

    CriticalSectionWrapper& _captureCritsect; // Never take this one before deliverCritsect!
    CriticalSectionWrapper& _deliverCritsect;
    VideoCaptureModule* _captureModule;
    VideoCaptureExternal* _externalCaptureModule;
    ProcessThread& _moduleProcessThread;
    const int _captureId;

    // Capture thread
    ThreadWrapper& _vieCaptureThread;
    EventWrapper& _vieCaptureEvent;
    EventWrapper& _vieDeliverEvent;

    VideoFrame _capturedFrame;
    VideoFrame _deliverFrame;
    VideoFrame _encodedFrame;

    // Image processing
    ViEEffectFilter* _effectFilter;
    VideoProcessingModule* _imageProcModule;
    int _imageProcModuleRefCounter;
    VideoProcessingModule::FrameStats* _deflickerFrameStats;
    VideoProcessingModule::FrameStats* _brightnessFrameStats;
    Brightness _currentBrightnessLevel;
    Brightness _reportedBrightnessLevel;
    bool _denoisingEnabled;

    //Statistic observer
    CriticalSectionWrapper& _observerCritsect;
    ViECaptureObserver* _observer;

    // Encoding using encoding capable cameras
    CriticalSectionWrapper& _encodingCritsect;
    VideoCaptureModule::VideoCaptureEncodeInterface* _captureEncoder;
    EncodedImageCallback* _encodeCompleteCallback;
    VideoCodec _codec;
    ViEEncoder* _vieEncoder; //ViEEncoder we are encoding for.
    WebRtc_Word32 _vieEncoderId; //ViEEncoder id we are encoding for.
    VideoCodingModule* _vcm; // Used for decoding preencoded frames
    EncodedVideoData _decodeBuffer; // Used for decoding preencoded frames
    bool _decoderInitialized;
    CaptureCapability _requestedCapability;

    VideoFrame _captureDeviceImage;
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CAPTURER_H_
