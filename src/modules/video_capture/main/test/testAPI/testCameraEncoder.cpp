/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testCameraEncoder.h"
#include "trace.h"
#include "tick_util.h"


namespace webrtc
{


#ifndef _DEBUG
#undef assert
#define assert(_a) {                 \
    if(!(_a))                       \
    {                               \
        LOG("Failed %s",#_a);  \
    }	                            \
}
#endif

testCameraEncoder::testCameraEncoder(void)
{
    Trace::CreateTrace();
    Trace::SetLevelFilter(webrtc::kTraceAll);    
    Trace::SetTraceFile("testCameraEncoder.txt");
    _captureInfo=VideoCaptureModule::CreateDeviceInfo(5);
#ifdef RENDER_PREVIEW
    _renderer=NULL;
    _videoCoding=webrtc::VideoCodingModule::Createwebrtc::VideoCodingModule(5);
#endif
    
}
 
testCameraEncoder::~testCameraEncoder(void)
{
    delete _captureInfo;

#ifdef RENDER_PREVIEW
    if(_renderer)
        delete _renderer;
    if(_videoCoding)
    {
        webrtc::VideoCodingModule::Destroywebrtc::VideoCodingModule(_videoCoding);
    }

#endif
    Trace::ReturnTrace();
}


int testCameraEncoder::DoTest()
{

#ifdef RENDER_PREVIEW
    if(!_renderer)
    {
        _renderer=new Renderer(true);
    }
    if(_videoCoding)
    {
        webrtc::VideoCodec inst;
        memset(&inst,0,sizeof(inst));
        inst.plType=122;
        inst.width=640;
        inst.height=480;
        inst.codecType=webrtc::kVideoCodecH264;
        strcpy(inst.plName,"H264");                        
        _videoCoding->InitializeReceiver();
        _videoCoding->RegisterReceiveCallback(this);
        assert(_videoCoding->RegisterReceiveCodec(&inst,1,false)==0);
        
    }
        
#endif
    
    // Test one camera at the time
    LOG("\n\nTesting Camera encoder\n");
    for (WebRtc_UWord32 i=0;i<_captureInfo->NumberOfDevices();++i)
    {
        WebRtc_UWord8 name[256];
        WebRtc_UWord8 uniqueID[256];        
        WebRtc_UWord8 productId[256];
        _captureInfo->GetDeviceName(i,name,256,uniqueID,256,productId,256);

        _captureModule= VideoCaptureModule::Create(0,uniqueID);
        _captureModule->RegisterCaptureDataCallback(*this);

        VideoCaptureCapability capability; 
        LOG("Encoder support for device %s",uniqueID);
        for (int capIndex=0;capIndex<
            _captureInfo->NumberOfCapabilities(uniqueID);++capIndex)
        {
            assert(_captureInfo->GetCapability(uniqueID,capIndex,capability)==0);
            if(capability.codecType==webrtc::kVideoCodecH264)
            {
                
                testCapability(capability);
            }
            else if(capability.codecType!=webrtc::kVideoCodecUnknown)
            {
                LOG("type %d width %d, height %d, framerate %d\n",
                    capability.codecType,capability.width,capability.height,capability.maxFPS);
                testCapability(capability);
            }
        }

        VideoCaptureModule::Destroy(_captureModule);
    }
    return 0;
}

int testCameraEncoder::testCapability(VideoCaptureCapability& capability)
{    
    webrtc::VideoCodec codec;
    codec.height=(unsigned short)capability.height;
    codec.width=(unsigned short) capability.width;
    float bitrate=(float)(capability.height*capability.width*3)/1000; //3bits per pixel
    codec.startBitrate=(unsigned int)bitrate;
    codec.maxBitrate=codec.startBitrate*10;
    codec.codecType=webrtc::kVideoCodecH264;
    codec.codecSpecific.H264.profile=webrtc::kProfileBase;

    _encodeInterface=NULL;
    _encodeInterface=_captureModule->GetEncodeInterface(codec);
    if(_encodeInterface)
    assert(_encodeInterface);
    _captureSettings.ResetAll();
    _captureSettings.capability=capability;

    

    assert(capability.width);
    assert(capability.height);
    assert(capability.maxFPS);
    assert(capability.expectedCaptureDelay);            
    _captureSettings.lastRenderTimeMS=0;
    _captureSettings.captureDelay=50;


    
    WebRtc_UWord32 maxPayloadSize=1460;


    LOG("\n\nTesting H264 width %d, height %d, framerate %d bitrate %d\n",
        capability.width,capability.height,capability.maxFPS,codec.startBitrate);
                
    _captureSettings.initStartTime=TickTime::MillisecondTimestamp();    
    assert(_captureModule->StartCapture(capability)==0);

    _captureSettings.startTime=TickTime::MillisecondTimestamp();
    _captureSettings.initStopTime=TickTime::MillisecondTimestamp();

    if(_encodeInterface)
    assert(_encodeInterface->ConfigureEncoder(codec,maxPayloadSize)==0);

    WebRtc_Word32 testTime=10000; 
    while(TickTime::MillisecondTimestamp()-_captureSettings.startTime<testTime
          && _captureSettings.incomingFrames<200)
    {
        SLEEP(200);
    }
    


    int noIncomingFrames=_captureSettings.incomingFrames;
    _captureSettings.bitrateMeasureTime=TickTime::MillisecondTimestamp();
    WebRtc_UWord32 actualbitrate=(_captureSettings.noOfBytes*8)/
        (WebRtc_UWord32)(_captureSettings.bitrateMeasureTime-_captureSettings.firstCapturedFrameTime);
    _captureSettings.noOfBytes=0;

    LOG("Current set bitrate %d, actual bitrate %d\n", codec.startBitrate,actualbitrate);

    for(int bitRateChange=1;bitRateChange< 11;bitRateChange=bitRateChange*2)
    {
        
        float bitrate=(float)(capability.height*
                              capability.width*
                              (bitRateChange))/1000; //3bits per pixel
        codec.startBitrate=(WebRtc_Word32) bitrate;
        LOG("Changing bitrate to %d (%d bits per pixel/s)\n",
            codec.startBitrate,bitRateChange);
        assert(_encodeInterface->SetRates(codec.startBitrate,codec.maxFramerate)==0);

        testTime=2000; 
        while(TickTime::MillisecondTimestamp()-
            _captureSettings.bitrateMeasureTime<testTime)
        {
            SLEEP(200);
        }        

        noIncomingFrames=_captureSettings.incomingFrames;        
        WebRtc_UWord32 actualbitrate=(_captureSettings.noOfBytes*8)/
                (WebRtc_UWord32)(TickTime::MillisecondTimestamp()-
                                _captureSettings.bitrateMeasureTime);
        _captureSettings.bitrateMeasureTime=TickTime::MillisecondTimestamp();
        _captureSettings.noOfBytes=0;
        LOG("Current set bitrate %d, actual bitrate %d\n",
             codec.startBitrate,actualbitrate);
        assert((actualbitrate<(1.2* codec.startBitrate))
               && (actualbitrate>0.8*codec.startBitrate));
    }
    
    _captureSettings.stopTime=TickTime::MillisecondTimestamp();
    _captureSettings.stopStartTime=TickTime::MillisecondTimestamp();
    assert(_captureModule->StopCapture()==0);
    _captureSettings.stopStopTime=TickTime::MillisecondTimestamp();            

    EvaluateTestResult();

    return 0;

}

void testCameraEncoder::OnIncomingCapturedFrame(const WebRtc_Word32 id,                                                
                                         VideoFrame&  videoFrame,
                                         webrtc::VideoCodecType codecType)
{
    _captureSettings.incomingFrames++;
    _captureSettings.noOfBytes+=videoFrame.Length();
    assert(videoFrame.Height()==_captureSettings.capability.height);
    assert(videoFrame.Width()==_captureSettings.capability.width);
    assert(videoFrame.RenderTimeMs()>=(TickTime::MillisecondTimestamp()-30)); // RenderTimstamp should be the time now
    if((videoFrame.RenderTimeMs()>_captureSettings.lastRenderTimeMS
        +(1000*1.2)/_captureSettings.capability.maxFPS
        && _captureSettings.lastRenderTimeMS>0)
        ||
       (videoFrame.RenderTimeMs()<_captureSettings.lastRenderTimeMS+(1000*0.8)
           /_captureSettings.capability.maxFPS && _captureSettings.lastRenderTimeMS>0))
    {                            
        _captureSettings.timingWarnings++;
    }

    if(_captureSettings.lastRenderTimeMS==0)
    {
        _captureSettings.firstCapturedFrameTime=TickTime::MillisecondTimestamp();
        
    }    
    _captureSettings.lastRenderTimeMS=videoFrame.RenderTimeMs();
    if(codecType==webrtc::kVideoCodecH264)
    {
        WebRtc_UWord8* ptrBuffer=videoFrame.Buffer();
        if(ptrBuffer[0]!=0 || ptrBuffer[1]!=0 || ptrBuffer[2]!=0 || ptrBuffer[3]!=1)
        {
            assert(!"frame does not start with NALU header");
        }
        if(ptrBuffer[4]==0x67)
        {            
            _captureSettings.idrFrames++;
            LOG("Got IDR frame frame no %d. total number of IDR frames %d \n",
                _captureSettings.incomingFrames,_captureSettings.idrFrames);
        }
    }

#ifdef RENDER_PREVIEW    
    if(codecType==webrtc::kVideoCodecH264)
    {
     
        VideoEncodedData encodedFrame;
        memset(&encodedFrame,0,sizeof(encodedFrame));
        encodedFrame.codec=webrtc::kVideoCodecH264;
        encodedFrame.encodedHeight=videoFrame.Height();
        encodedFrame.encodedWidth=videoFrame.Width();
        encodedFrame.renderTimeMs=videoFrame.RenderTimeMs();
        encodedFrame.timeStamp=90* (WebRtc_UWord32) videoFrame.RenderTimeMs();
        encodedFrame.payloadData=(WebRtc_UWord8*) malloc(videoFrame.Length());
        memcpy(encodedFrame.payloadData,videoFrame.Buffer(),videoFrame.Length());
        encodedFrame.payloadSize=videoFrame.Length();
        encodedFrame.bufferSize=videoFrame.Length();
        encodedFrame.payloadType=122;
        _videoCoding->DecodeFromStorage(encodedFrame);
    }
    if(codecType==webrtc::kVideoCodecUnknown)
    {
        _renderer->RenderFrame(videoFrame);
    }
#endif

}

void testCameraEncoder::OnCaptureDelayChanged(const WebRtc_Word32 id,
                                       const WebRtc_Word32 delay)
{
    _captureSettings.captureDelay=delay;
}

#ifdef RENDER_PREVIEW
WebRtc_Word32 testCameraEncoder::FrameToRender(VideoFrame& videoFrame)
{
    _renderer->RenderFrame(videoFrame);
    return 0;
}
#endif


void testCameraEncoder::EvaluateTestResult()
{
    CaptureSetting& captureResult=_captureSettings;

    WebRtc_UWord64 timeToFirstFrame=captureResult.firstCapturedFrameTime-captureResult.startTime;
    WebRtc_UWord64 timeToStart=captureResult.initStopTime-captureResult.initStartTime;
    WebRtc_UWord64 timeToStop=captureResult.stopStopTime-captureResult.stopStartTime;

    assert(timeToStart<4000);
    assert(timeToStop<3000);

    
    assert((timeToFirstFrame<3500) && (timeToFirstFrame>0)); // Assert if it takes more than 3500ms to start.
    WebRtc_Word64 expectedNumberOfFrames=((captureResult.stopTime
                                            -captureResult.startTime
                                            -timeToFirstFrame)
                                          *captureResult.capability.maxFPS)/1000;
    assert(captureResult.incomingFrames>0.6*expectedNumberOfFrames); // Make sure at least 60% of the expected frames have been received from the camera    

    LOG(" No Captured %d,expected %d, \n  timingWarnings %d, time to first %lu\n"
        "  time to start %lu, time to stop %lu\n idr frames %u\n",
        captureResult.incomingFrames,(int)(expectedNumberOfFrames),
                                            captureResult.timingWarnings,
                                            (long) timeToFirstFrame,
                                            (long) timeToStart,
                                            (long) timeToStop,
                                            _captureSettings.idrFrames);

    captureResult.ResetSettings();            
}
} // namespace webrtc
