/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_coding_impl.h"
#include "common_types.h"
#include "encoded_frame.h"
#include "jitter_buffer.h"
#include "packet.h"
#include "trace.h"
#include "video_codec_interface.h"

namespace webrtc
{

//#define DEBUG_DECODER_BIT_STREAM
//#define DEBUG_ENCODER_INPUT

WebRtc_UWord32
VCMProcessTimer::Period() const
{
    return _periodMs;
}

WebRtc_UWord32
VCMProcessTimer::TimeUntilProcess() const
{
    return static_cast<WebRtc_UWord32>(
        VCM_MAX(static_cast<WebRtc_Word64>(_periodMs) -
                (VCMTickTime::MillisecondTimestamp() - _latestMs), 0));
}

void
VCMProcessTimer::Processed()
{
    _latestMs = VCMTickTime::MillisecondTimestamp();
}

VideoCodingModuleImpl::VideoCodingModuleImpl(const WebRtc_Word32 id)
:
_id(id),
_receiveCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
_receiverInited(false),
_timing(id, 1),
_dualTiming(id, 2, &_timing),
_receiver(_timing, id, 1),
_dualReceiver(_dualTiming, id, 2, false),
_decodedFrameCallback(_timing),
_dualDecodedFrameCallback(_dualTiming),
_frameTypeCallback(NULL),
_frameStorageCallback(NULL),
_receiveStatsCallback(NULL),
_packetRequestCallback(NULL),
_decoder(NULL),
_dualDecoder(NULL),
_bitStreamBeforeDecoder(NULL),
_frameFromFile(),
_keyRequestMode(kKeyOnError),
_scheduleKeyRequest(false),

_sendCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
_encoder(),
_encodedFrameCallback(),
_mediaOpt(id),
_sendCodecType(kVideoCodecUnknown),
_sendStatsCallback(NULL),
_encoderInputFile(NULL),

_codecDataBase(id),
_receiveStatsTimer(1000),
_sendStatsTimer(1000),
_retransmissionTimer(10),
_keyRequestTimer(500)
{
    for (int i = 0; i < kMaxSimulcastStreams; i++)
    {
        _nextFrameType[i] = kVideoFrameDelta;
    }
#ifdef DEBUG_DECODER_BIT_STREAM
    _bitStreamBeforeDecoder = fopen("decoderBitStream.bit", "wb");
#endif
#ifdef DEBUG_ENCODER_INPUT
    _encoderInputFile = fopen("encoderInput.yuv", "wb");
#endif
}

VideoCodingModuleImpl::~VideoCodingModuleImpl()
{
    if (_dualDecoder != NULL)
    {
        _codecDataBase.ReleaseDecoder(_dualDecoder);
    }
    delete &_receiveCritSect;
    delete &_sendCritSect;
#ifdef DEBUG_DECODER_BIT_STREAM
    fclose(_bitStreamBeforeDecoder);
#endif
#ifdef DEBUG_ENCODER_INPUT
    fclose(_encoderInputFile);
#endif
}

VideoCodingModule*
VideoCodingModule::Create(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(id),
                 "VideoCodingModule::Create()");
    return new VideoCodingModuleImpl(id);
}

void
VideoCodingModule::Destroy(VideoCodingModule* module)
{
    if (module != NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceModuleCall,
                     webrtc::kTraceVideoCoding,
                     static_cast<VideoCodingModuleImpl*>(module)->Id(),
                     "VideoCodingModule::Destroy()");
        delete static_cast<VideoCodingModuleImpl*>(module);
    }
}

WebRtc_Word32
VideoCodingModuleImpl::Process()
{
    WebRtc_Word32 returnValue = VCM_OK;

    // Receive-side statistics
    if (_receiveStatsTimer.TimeUntilProcess() == 0)
    {
        _receiveStatsTimer.Processed();
        if (_receiveStatsCallback != NULL)
        {
            WebRtc_UWord32 bitRate;
            WebRtc_UWord32 frameRate;
            const WebRtc_Word32 ret = _receiver.ReceiveStatistics(bitRate,
                                                                  frameRate);
            if (ret == 0)
            {
                _receiveStatsCallback->ReceiveStatistics(bitRate, frameRate);
            }
            else if (returnValue == VCM_OK)
            {
                returnValue = ret;
            }
        }
    }

    // Send-side statistics
    if (_sendStatsTimer.TimeUntilProcess() == 0)
    {
        _sendStatsTimer.Processed();
        if (_sendStatsCallback != NULL)
        {
            WebRtc_UWord32 bitRate;
            WebRtc_UWord32 frameRate;
            {
                CriticalSectionScoped cs(_sendCritSect);
                bitRate = static_cast<WebRtc_UWord32>(
                    _mediaOpt.SentBitRate() + 0.5f);
                frameRate = static_cast<WebRtc_UWord32>(
                    _mediaOpt.SentFrameRate() + 0.5f);
            }
            _sendStatsCallback->SendStatistics(bitRate, frameRate);
        }
    }

    // Packet retransmission requests
    if (_retransmissionTimer.TimeUntilProcess() == 0)
    {
        _retransmissionTimer.Processed();
        if (_packetRequestCallback != NULL)
        {
            WebRtc_UWord16 nackList[kNackHistoryLength];
            WebRtc_UWord16 length = kNackHistoryLength;
            const WebRtc_Word32 ret = NackList(nackList, length);
            if (ret != VCM_OK && returnValue == VCM_OK)
            {
                returnValue = ret;
            }
            if (length > 0)
            {
                _packetRequestCallback->ResendPackets(nackList, length);
            }
        }
    }

    // Key frame requests
    if (_keyRequestTimer.TimeUntilProcess() == 0)
    {
        _keyRequestTimer.Processed();
        if (_scheduleKeyRequest && _frameTypeCallback != NULL)
        {
            const WebRtc_Word32 ret = RequestKeyFrame();
            if (ret != VCM_OK && returnValue == VCM_OK)
            {
                returnValue = ret;
            }
        }
    }

    return returnValue;
}

// Returns version of the module and its components
WebRtc_Word32
VideoCodingModuleImpl::Version(WebRtc_Word8* version,
                                WebRtc_UWord32& remainingBufferInBytes,
                                WebRtc_UWord32& position) const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Version()");
    if (version == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "Invalid buffer pointer in argument to Version()");
        return VCM_PARAMETER_ERROR;
    }
    WebRtc_Word8 ourVersion[] = "VideoCodingModule 1.1.0\n";
    WebRtc_UWord32 ourLength = (WebRtc_UWord32)strlen(ourVersion);
    if (remainingBufferInBytes < ourLength)
    {
        return VCM_MEMORY;
    }
    memcpy(&version[position], ourVersion, ourLength);
    remainingBufferInBytes -= ourLength;
    position += ourLength;

    // Safe to truncate here.
    WebRtc_Word32 ret = _codecDataBase.Version(version,
                                               remainingBufferInBytes,
                                               position);
    if (ret < 0)
    {
        return ret;
    }
    // Ensure the strlen call is safe by terminating at the end of version.
    version[position + remainingBufferInBytes - 1] = '\0';
    ourLength = (WebRtc_UWord32)strlen(&version[position]);
    remainingBufferInBytes -= (ourLength + 1); // include null termination.
    position += (ourLength + 1);

    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::Id() const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Id()");
    CriticalSectionScoped receiveCs(_receiveCritSect);
    {
        CriticalSectionScoped sendCs(_sendCritSect);
        return _id;
    }
}

//  Change the unique identifier of this object
WebRtc_Word32
VideoCodingModuleImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "ChangeUniqueId()");
    CriticalSectionScoped receiveCs(_receiveCritSect);
    {
        CriticalSectionScoped sendCs(_sendCritSect);
        _id = id;
        return VCM_OK;
    }
}

// Returns the number of milliseconds until the module wants a worker thread to
// call Process
WebRtc_Word32
VideoCodingModuleImpl::TimeUntilNextProcess()
{
    WebRtc_UWord32 timeUntilNextProcess = VCM_MIN(
                                    _receiveStatsTimer.TimeUntilProcess(),
                                    _sendStatsTimer.TimeUntilProcess());
    if ((_receiver.NackMode() != kNoNack) ||
        (_dualReceiver.State() != kPassive))
    {
        // We need a Process call more often if we are relying on
        // retransmissions
        timeUntilNextProcess = VCM_MIN(timeUntilNextProcess,
                                       _retransmissionTimer.TimeUntilProcess());
    }
    timeUntilNextProcess = VCM_MIN(timeUntilNextProcess,
                                   _keyRequestTimer.TimeUntilProcess());

    return timeUntilNextProcess;
}

// Get number of supported codecs
WebRtc_UWord8
VideoCodingModule::NumberOfCodecs()
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 -1,
                 "NumberOfCodecs()");
    return VCMCodecDataBase::NumberOfCodecs();
}

// Get supported codec with id
WebRtc_Word32
VideoCodingModule::Codec(WebRtc_UWord8 listId, VideoCodec* codec)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 -1,
                 "Codec()");
    if (codec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    return VCMCodecDataBase::Codec(listId, codec);
}

// Get supported codec with type
WebRtc_Word32
VideoCodingModule::Codec(VideoCodecType codecType, VideoCodec* codec)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 -1,
                 "Codec()");
    if (codec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    return VCMCodecDataBase::Codec(codecType, codec);
}

/*
*   Sender
*/

// Reset send side to initial state - all components
WebRtc_Word32
VideoCodingModuleImpl::InitializeSender()
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "InitializeSender()");
    CriticalSectionScoped cs(_sendCritSect);
    _codecDataBase.ResetSender();
    _encoder = NULL;
    _encodedFrameCallback.SetTransportCallback(NULL);
    // setting default bitRate and frameRate to 0
    _mediaOpt.SetEncodingData(kVideoCodecUnknown, 0, 0, 0, 0, 0);
    _mediaOpt.Reset(); // Resetting frame dropper
    return VCM_OK;
}

// Makes sure the encoder is in its initial state.
WebRtc_Word32
VideoCodingModuleImpl::ResetEncoder()
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "ResetEncoder()");
    CriticalSectionScoped cs(_sendCritSect);
    if (_encoder != NULL)
    {
        return _encoder->Reset();
    }
    return VCM_OK;
}

// Register the send codec to be used.
WebRtc_Word32
VideoCodingModuleImpl::RegisterSendCodec(const VideoCodec* sendCodec,
                                         WebRtc_UWord32 numberOfCores,
                                         WebRtc_UWord32 maxPayloadSize)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterSendCodec()");
    CriticalSectionScoped cs(_sendCritSect);
    if (sendCodec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    WebRtc_Word32 ret = _codecDataBase.RegisterSendCodec(sendCodec,
                                                         numberOfCores,
                                                         maxPayloadSize);
    if (ret < 0)
    {
        return ret;
    }

    _encoder = _codecDataBase.SetEncoder(sendCodec, &_encodedFrameCallback);
    if (_encoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "Failed to initialize encoder");
        return VCM_CODEC_ERROR;
    }
    _sendCodecType = sendCodec->codecType;
    _mediaOpt.SetEncodingData(_sendCodecType,
                              sendCodec->maxBitrate,
                              sendCodec->maxFramerate,
                              sendCodec->startBitrate,
                              sendCodec->width,
                              sendCodec->height);
    _mediaOpt.SetMtu(maxPayloadSize);

    return VCM_OK;
}

// Get current send codec
WebRtc_Word32
VideoCodingModuleImpl::SendCodec(VideoCodec* currentSendCodec) const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "SendCodec()");
    CriticalSectionScoped cs(_sendCritSect);

    if (currentSendCodec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    return _codecDataBase.SendCodec(currentSendCodec);
}

// Get the current send codec type
VideoCodecType
VideoCodingModuleImpl::SendCodec() const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "SendCodec()");
    CriticalSectionScoped cs(_sendCritSect);

    return _codecDataBase.SendCodec();
}

// Register an external decoder object.
// This can not be used together with external decoder callbacks.
WebRtc_Word32
VideoCodingModuleImpl::RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                               WebRtc_UWord8 payloadType,
                                               bool internalSource /*= false*/)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterExternalEncoder()");
    CriticalSectionScoped cs(_sendCritSect);

    if (externalEncoder == NULL)
    {
        bool wasSendCodec = false;
        const WebRtc_Word32 ret = _codecDataBase.DeRegisterExternalEncoder(
                                                                  payloadType,
                                                                  wasSendCodec);
        if (wasSendCodec)
        {
            // Make sure the VCM doesn't use the de-registered codec
            _encoder = NULL;
        }
        return ret;
    }
    return _codecDataBase.RegisterExternalEncoder(externalEncoder,
                                                  payloadType,
                                                  internalSource);
}

// Get codec config parameters
WebRtc_Word32
VideoCodingModuleImpl::CodecConfigParameters(WebRtc_UWord8* buffer,
                                             WebRtc_Word32 size)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
               "CodecConfigParameters()");
    CriticalSectionScoped cs(_sendCritSect);
    if (_encoder != NULL)
    {
        return _encoder->CodecConfigParameters(buffer, size);
    }
    return VCM_UNINITIALIZED;
}

// Get encode bitrate
WebRtc_UWord32
VideoCodingModuleImpl::Bitrate() const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Bitrate()");
    CriticalSectionScoped cs(_sendCritSect);
    // return the bit rate which the encoder is set to
    if (_encoder != NULL)
    {
        return _encoder->BitRate();
    }
    return VCM_UNINITIALIZED;
}

// Get encode frame rate
WebRtc_UWord32
VideoCodingModuleImpl::FrameRate() const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "FrameRate()");
    CriticalSectionScoped cs(_sendCritSect);
    // input frame rate, not compensated
    if (_encoder != NULL)
    {
        return _encoder->FrameRate();
    }
    return VCM_UNINITIALIZED;
}

// Set channel parameters
WebRtc_Word32
VideoCodingModuleImpl::SetChannelParameters(WebRtc_UWord32 availableBandWidth,
                                            WebRtc_UWord8 lossRate,
                                            WebRtc_UWord32 RTT)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "SetChannelParameters()");
    WebRtc_Word32 ret = 0;
    {
        CriticalSectionScoped sendCs(_sendCritSect);
        WebRtc_UWord32 targetRate = _mediaOpt.SetTargetRates(availableBandWidth,
                                                             lossRate,
                                                             RTT);
        if (_encoder != NULL)
        {
            ret = _encoder->SetPacketLoss(lossRate);
            if (ret < 0 )
            {
                return ret;
            }
            ret = (WebRtc_Word32)_encoder->SetRates(targetRate,
                                                    _mediaOpt.InputFrameRate());
            if (ret < 0)
            {
                return ret;
            }
        }
        else
        {
            return VCM_UNINITIALIZED;
        } // encoder
    }// send side
    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::SetReceiveChannelParameters(WebRtc_UWord32 RTT)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "SetReceiveChannelParameters()");
    CriticalSectionScoped receiveCs(_receiveCritSect);
    _receiver.UpdateRtt(RTT);
    return 0;
}

// Register a transport callback which will be called to deliver the encoded
// buffers
WebRtc_Word32
VideoCodingModuleImpl::RegisterTransportCallback(
    VCMPacketizationCallback* transport)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterTransportCallback()");
    CriticalSectionScoped cs(_sendCritSect);
    _encodedFrameCallback.SetMediaOpt(&_mediaOpt);
    _encodedFrameCallback.SetTransportCallback(transport);
    return VCM_OK;
}

// Register video output information callback which will be called to deliver
// information about the video stream produced by the encoder, for instance the
// average frame rate and bit rate.
WebRtc_Word32
VideoCodingModuleImpl::RegisterSendStatisticsCallback(
    VCMSendStatisticsCallback* sendStats)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterSendStatisticsCallback()");
    CriticalSectionScoped cs(_sendCritSect);
    _sendStatsCallback = sendStats;
    return VCM_OK;
}

// Register a video quality settings callback which will be called when frame
// rate/dimensions need to be updated for video quality optimization
WebRtc_Word32
VideoCodingModuleImpl::RegisterVideoQMCallback(
    VCMQMSettingsCallback* videoQMSettings)
{
    CriticalSectionScoped cs(_sendCritSect);
    return _mediaOpt.RegisterVideoQMCallback(videoQMSettings);
}


// Register a video protection callback which will be called to deliver the
// requested FEC rate and NACK status (on/off).
WebRtc_Word32
VideoCodingModuleImpl::RegisterProtectionCallback(
    VCMProtectionCallback* protection)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterProtectionCallback()");
    CriticalSectionScoped cs(_sendCritSect);
    _mediaOpt.RegisterProtectionCallback(protection);
    return VCM_OK;
}

// Enable or disable a video protection method.
WebRtc_Word32
VideoCodingModuleImpl::SetVideoProtection(VCMVideoProtection videoProtection,
                                          bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "SetVideoProtection()");

    switch (videoProtection)
    {

    case kProtectionNack:
        {
            // Both send-side and receive-side
            SetVideoProtection(kProtectionNackSender, enable);
            SetVideoProtection(kProtectionNackReceiver, enable);
            break;
        }

    case kProtectionNackSender:
        {
            CriticalSectionScoped cs(_sendCritSect);
            _mediaOpt.EnableProtectionMethod(enable, kNack);
            break;
        }

    case kProtectionNackReceiver:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            if (enable)
            {
                _receiver.SetNackMode(kNackInfinite);
            }
            else
            {
                _receiver.SetNackMode(kNoNack);
            }
            break;
        }

    case kProtectionDualDecoder:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            if (enable)
            {
                _receiver.SetNackMode(kNoNack);
                _dualReceiver.SetNackMode(kNackInfinite);
            }
            else
            {
                _dualReceiver.SetNackMode(kNoNack);
            }
            break;
        }

    case kProtectionKeyOnLoss:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            if (enable)
            {
                _keyRequestMode = kKeyOnLoss;
            }
            else if (_keyRequestMode == kKeyOnLoss)
            {
                _keyRequestMode = kKeyOnError; // default mode
            }
            else
            {
                return VCM_PARAMETER_ERROR;
            }
            break;
        }

    case kProtectionKeyOnKeyLoss:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            if (enable)
            {
                _keyRequestMode = kKeyOnKeyLoss;
            }
            else if (_keyRequestMode == kKeyOnKeyLoss)
            {
                _keyRequestMode = kKeyOnError; // default mode
            }
            else
            {
                return VCM_PARAMETER_ERROR;
            }
            break;
        }

    case kProtectionNackFEC:
        {
            {
              // Receive side
                CriticalSectionScoped cs(_receiveCritSect);
                if (enable)
                {
                    _receiver.SetNackMode(kNackHybrid);
                }
                else
                {
                    _receiver.SetNackMode(kNoNack);
                }
            }
            // Send Side
            {
                CriticalSectionScoped cs(_sendCritSect);
                _mediaOpt.EnableProtectionMethod(enable, kNackFec);
            }
            break;
        }

    case kProtectionFEC:
        {
            CriticalSectionScoped cs(_sendCritSect);
            _mediaOpt.EnableProtectionMethod(enable, kFec);
            break;
        }

    case kProtectionPeriodicKeyFrames:
        {
            CriticalSectionScoped cs(_sendCritSect);
            return _codecDataBase.SetPeriodicKeyFrames(enable);
            break;
        }

    default:
        return VCM_PARAMETER_ERROR;
    }
    return VCM_OK;
}

// Add one raw video frame to the encoder, blocking.
WebRtc_Word32
VideoCodingModuleImpl::AddVideoFrame(const VideoFrame& videoFrame,
                                     const VideoContentMetrics* contentMetrics,
                                     const CodecSpecificInfo* codecSpecificInfo)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "AddVideoFrame()");
    CriticalSectionScoped cs(_sendCritSect);

    if (_encoder == NULL)
    {
        return VCM_UNINITIALIZED;
    }
    if (_nextFrameType[0] == kFrameEmpty)
    {
        return VCM_OK;
    }
    _mediaOpt.UpdateIncomingFrameRate();

    if (_mediaOpt.DropFrame())
    {
        WEBRTC_TRACE(webrtc::kTraceStream,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "Drop frame due to bitrate");
    }
    else
    {
        _mediaOpt.updateContentData(contentMetrics);
        WebRtc_Word32 ret = _encoder->Encode(videoFrame,
                                             codecSpecificInfo,
                                             _nextFrameType);
#ifdef DEBUG_ENCODER_INPUT
        if (_encoderInputFile != NULL)
        {
            fwrite(videoFrame.Buffer(), 1, videoFrame.Length(),
                   _encoderInputFile);
        }
#endif
        if (ret < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Encode error: %d", ret);
            return ret;
        }
        for (int i = 0; i < kMaxSimulcastStreams; i++)
        {
            _nextFrameType[i] = kVideoFrameDelta; // default frame type
        }
    }
    return VCM_OK;
}

// Next frame encoded should be of the type frameType
// Good for only one frame
WebRtc_Word32
VideoCodingModuleImpl::FrameTypeRequest(FrameType frameType, 
                                        WebRtc_UWord8 simulcastIdx)
{
    assert(simulcastIdx < kMaxSimulcastStreams);

    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "FrameTypeRequest()");

    CriticalSectionScoped cs(_sendCritSect);
    _nextFrameType[simulcastIdx] = frameType;
    if (_encoder != NULL && _encoder->InternalSource())
    {
        // Try to request the frame if we have an external encoder with
        // internal source since AddVideoFrame never will be called.
        if (_encoder->RequestFrame(_nextFrameType) == WEBRTC_VIDEO_CODEC_OK)
        {
            _nextFrameType[simulcastIdx] = kVideoFrameDelta;
        }
    }
    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::EnableFrameDropper(bool enable)
{
    CriticalSectionScoped cs(_sendCritSect);
    _mediaOpt.EnableFrameDropper(enable);
    return VCM_OK;
}


WebRtc_Word32
VideoCodingModuleImpl::SentFrameCount(VCMFrameCount &frameCount) const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "SentFrameCount()");
    CriticalSectionScoped cs(_sendCritSect);
    return _mediaOpt.SentFrameCount(frameCount);
}

// Initialize receiver, resets codec database etc
WebRtc_Word32
VideoCodingModuleImpl::InitializeReceiver()
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "InitializeReceiver()");
    CriticalSectionScoped cs(_receiveCritSect);
    WebRtc_Word32 ret = _receiver.Initialize();
    if (ret < 0)
    {
        return ret;
    }

    ret = _dualReceiver.Initialize();
    if (ret < 0)
    {
        return ret;
    }
    _codecDataBase.ResetReceiver();
    _timing.Reset();

    _decoder = NULL;
    _decodedFrameCallback.SetUserReceiveCallback(NULL);
    _receiverInited = true;
    _frameTypeCallback = NULL;
    _frameStorageCallback = NULL;
    _receiveStatsCallback = NULL;
    _packetRequestCallback = NULL;
    _keyRequestMode = kKeyOnError;
    _scheduleKeyRequest = false;

    return VCM_OK;
}

// Register a receive callback. Will be called whenever there is a new frame
// ready for rendering.
WebRtc_Word32
VideoCodingModuleImpl::RegisterReceiveCallback(
    VCMReceiveCallback* receiveCallback)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterReceiveCallback()");
    CriticalSectionScoped cs(_receiveCritSect);
    _decodedFrameCallback.SetUserReceiveCallback(receiveCallback);
    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::RegisterReceiveStatisticsCallback(
                                     VCMReceiveStatisticsCallback* receiveStats)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterReceiveStatisticsCallback()");
    CriticalSectionScoped cs(_receiveCritSect);
    _receiveStatsCallback = receiveStats;
    return VCM_OK;
}

// Register an externally defined decoder/render object.
// Can be a decoder only or a decoder coupled with a renderer.
WebRtc_Word32
VideoCodingModuleImpl::RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                               WebRtc_UWord8 payloadType,
                                               bool internalRenderTiming)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
               webrtc::kTraceVideoCoding,
               VCMId(_id),
               "RegisterExternalDecoder()");
    CriticalSectionScoped cs(_receiveCritSect);
    if (externalDecoder == NULL)
    {
        // Make sure the VCM updates the decoder next time it decodes.
        _decoder = NULL;
        return _codecDataBase.DeRegisterExternalDecoder(payloadType);
    }
    else
    {
        return _codecDataBase.RegisterExternalDecoder(externalDecoder,
                                                      payloadType,
                                                      internalRenderTiming);
    }
}

// Register a frame type request callback.
WebRtc_Word32
VideoCodingModuleImpl::RegisterFrameTypeCallback(
    VCMFrameTypeCallback* frameTypeCallback)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterFrameTypeCallback()");
    CriticalSectionScoped cs(_receiveCritSect);
    _frameTypeCallback = frameTypeCallback;
    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::RegisterFrameStorageCallback(
    VCMFrameStorageCallback* frameStorageCallback)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterFrameStorageCallback()");
    CriticalSectionScoped cs(_receiveCritSect);
    _frameStorageCallback = frameStorageCallback;
    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::RegisterPacketRequestCallback(
    VCMPacketRequestCallback* callback)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterPacketRequestCallback()");
    CriticalSectionScoped cs(_receiveCritSect);
    _packetRequestCallback = callback;
    return VCM_OK;
}

// Decode next frame, blocking.
// Should be called as often as possible to get the most out of the decoder.
WebRtc_Word32
VideoCodingModuleImpl::Decode(WebRtc_UWord16 maxWaitTimeMs)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Decode()");
    WebRtc_Word64 nextRenderTimeMs;
    {
        CriticalSectionScoped cs(_receiveCritSect);
        if (!_receiverInited)
        {
            return VCM_UNINITIALIZED;
        }
        if (!_codecDataBase.DecoderRegistered())
        {
            return VCM_NO_CODEC_REGISTERED;
        }
    }

    const bool dualReceiverEnabledNotReceiving =
        (_dualReceiver.State() != kReceiving &&
         _dualReceiver.NackMode() == kNackInfinite);

    VCMEncodedFrame* frame = _receiver.FrameForDecoding(
                                                  maxWaitTimeMs,
                                                  nextRenderTimeMs,
                                                  _codecDataBase.RenderTiming(),
                                                  &_dualReceiver);

    if (dualReceiverEnabledNotReceiving && _dualReceiver.State() == kReceiving)
    {
        // Dual receiver is enabled (kNACK enabled), but was not receiving
        // before the call to FrameForDecoding(). After the call the state
        // changed to receiving, and therefore we must copy the primary decoder
        // state to the dual decoder to make it possible for the dual decoder to
        // start decoding retransmitted frames and recover.
        CriticalSectionScoped cs(_receiveCritSect);
        if (_dualDecoder != NULL)
        {
            _codecDataBase.ReleaseDecoder(_dualDecoder);
        }
        _dualDecoder = _codecDataBase.CreateDecoderCopy();
        if (_dualDecoder != NULL)
        {
            _dualDecoder->RegisterDecodeCompleteCallback(
                &_dualDecodedFrameCallback);
        }
        else
        {
            _dualReceiver.Reset();
        }
    }

    if (frame == NULL)
      return VCM_FRAME_NOT_READY;
    else
    {
        CriticalSectionScoped cs(_receiveCritSect);

        // If this frame was too late, we should adjust the delay accordingly
        _timing.UpdateCurrentDelay(frame->RenderTimeMs(),
                                   VCMTickTime::MillisecondTimestamp());

#ifdef DEBUG_DECODER_BIT_STREAM
        if (_bitStreamBeforeDecoder != NULL)
        {
            // Write bit stream to file for debugging purposes
            fwrite(frame->Buffer(), 1, frame->Length(),
                   _bitStreamBeforeDecoder);
        }
#endif
        if (_frameStorageCallback != NULL)
        {
            WebRtc_Word32 ret = frame->Store(*_frameStorageCallback);
            if (ret < 0)
            {
                return ret;
            }
        }

        const WebRtc_Word32 ret = Decode(*frame);
        _receiver.ReleaseFrame(frame);
        frame = NULL;
        if (ret != VCM_OK)
        {
            return ret;
        }
    }
    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::RequestSliceLossIndication(
    const WebRtc_UWord64 pictureID) const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterSliceLossIndication()");
    if (_frameTypeCallback != NULL)
    {
        const WebRtc_Word32 ret =
            _frameTypeCallback->SliceLossIndicationRequest(pictureID);
        if (ret < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to request key frame");
            return ret;
        }
    } else
    {
        WEBRTC_TRACE(webrtc::kTraceWarning,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "No frame type request callback registered");
        return VCM_MISSING_CALLBACK;
    }
    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::RequestKeyFrame()
{
    if (_frameTypeCallback != NULL)
    {
        const WebRtc_Word32 ret = _frameTypeCallback->FrameTypeRequest(
                                                                kVideoFrameKey);
        if (ret < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to request key frame");
            return ret;
        }
        _scheduleKeyRequest = false;
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceWarning,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "No frame type request callback registered");
        return VCM_MISSING_CALLBACK;
    }
    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::DecodeDualFrame(WebRtc_UWord16 maxWaitTimeMs)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "DecodeDualFrame()");
    CriticalSectionScoped cs(_receiveCritSect);
    if (_dualReceiver.State() != kReceiving ||
        _dualReceiver.NackMode() != kNackInfinite)
    {
        // The dual receiver is currently not receiving or
        // dual decoder mode is disabled.
        return VCM_OK;
    }
    WebRtc_Word64 dummyRenderTime;
    WebRtc_Word32 decodeCount = 0;
    VCMEncodedFrame* dualFrame = _dualReceiver.FrameForDecoding(
                                                            maxWaitTimeMs,
                                                            dummyRenderTime);
    if (dualFrame != NULL && _dualDecoder != NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceStream,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "Decoding frame %u with dual decoder",
                     dualFrame->TimeStamp());
        // Decode dualFrame and try to catch up
        WebRtc_Word32 ret = _dualDecoder->Decode(*dualFrame);
        if (ret != WEBRTC_VIDEO_CODEC_OK)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to decode frame with dual decoder");
            _dualReceiver.ReleaseFrame(dualFrame);
            return VCM_CODEC_ERROR;
        }
        if (_receiver.DualDecoderCaughtUp(dualFrame, _dualReceiver))
        {
            // Copy the complete decoder state of the dual decoder
            // to the primary decoder.
            WEBRTC_TRACE(webrtc::kTraceStream,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Dual decoder caught up");
            _codecDataBase.CopyDecoder(*_dualDecoder);
            _codecDataBase.ReleaseDecoder(_dualDecoder);
            _dualDecoder = NULL;
        }
        decodeCount++;
    }
    _dualReceiver.ReleaseFrame(dualFrame);
    return decodeCount;
}


// Must be called from inside the receive side critical section.
WebRtc_Word32
VideoCodingModuleImpl::Decode(const VCMEncodedFrame& frame)
{
    // Change decoder if payload type has changed
    const bool renderTimingBefore = _codecDataBase.RenderTiming();
    _decoder = _codecDataBase.SetDecoder(frame.PayloadType(),
                                         _decodedFrameCallback);
    if (renderTimingBefore != _codecDataBase.RenderTiming())
    {
        // Make sure we reset the decode time estimate since it will
        // be zero for codecs without render timing.
        _timing.ResetDecodeTime();
    }
    if (_decoder == NULL)
    {
        return VCM_NO_CODEC_REGISTERED;
    }
    // Decode a frame
    WebRtc_Word32 ret = _decoder->Decode(frame);

    // Check for failed decoding, run frame type request callback if needed.
    if (ret < 0)
    {
        if (ret == VCM_ERROR_REQUEST_SLI)
        {
            return RequestSliceLossIndication(
                    _decodedFrameCallback.LastReceivedPictureID() + 1);
        }
        else
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to decode frame %u, requesting key frame",
                         frame.TimeStamp());
            ret = RequestKeyFrame();
        }
    }
    else if (ret == VCM_REQUEST_SLI)
    {
        ret = RequestSliceLossIndication(
            _decodedFrameCallback.LastReceivedPictureID() + 1);
    }
    if (!frame.Complete() || frame.MissingFrame())
    {
        switch (_keyRequestMode)
        {
            case kKeyOnKeyLoss:
            {
                if (frame.FrameType() == kVideoFrameKey)
                {
                    _scheduleKeyRequest = true;
                    return VCM_OK;
                }
                break;
            }
            case kKeyOnLoss:
            {
                _scheduleKeyRequest = true;
                return VCM_OK;
            }
            default:
                break;
        }
    }
    return ret;
}

WebRtc_Word32
VideoCodingModuleImpl::DecodeFromStorage(
    const EncodedVideoData& frameFromStorage)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "DecodeFromStorage()");
    CriticalSectionScoped cs(_receiveCritSect);
    WebRtc_Word32 ret = _frameFromFile.ExtractFromStorage(frameFromStorage);
    if (ret < 0)
    {
        return ret;
    }
    return Decode(_frameFromFile);
}

// Reset the decoder state
WebRtc_Word32
VideoCodingModuleImpl::ResetDecoder()
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "ResetDecoder()");
    CriticalSectionScoped cs(_receiveCritSect);
    if (_decoder != NULL)
    {
        _receiver.Initialize();
        _timing.Reset();
        return _decoder->Reset();
        _scheduleKeyRequest = false;
    }
    if (_dualReceiver.State() != kPassive)
    {
        _dualReceiver.Initialize();
    }
    if (_dualDecoder != NULL)
    {
        _codecDataBase.ReleaseDecoder(_dualDecoder);
        _dualDecoder = NULL;
    }
    return VCM_OK;
}

// Register possible receive codecs, can be called multiple times
WebRtc_Word32
VideoCodingModuleImpl::RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                                WebRtc_Word32 numberOfCores,
                                                bool requireKeyFrame)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "RegisterReceiveCodec()");
    CriticalSectionScoped cs(_receiveCritSect);
    if (receiveCodec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    return _codecDataBase.RegisterReceiveCodec(receiveCodec, numberOfCores,
                                               requireKeyFrame);
}

// Get current received codec
WebRtc_Word32
VideoCodingModuleImpl::ReceiveCodec(VideoCodec* currentReceiveCodec) const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "ReceiveCodec()");
    CriticalSectionScoped cs(_receiveCritSect);
    if (currentReceiveCodec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    return _codecDataBase.ReceiveCodec(currentReceiveCodec);
}

// Get current received codec
VideoCodecType
VideoCodingModuleImpl::ReceiveCodec() const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "ReceiveCodec()");
    CriticalSectionScoped cs(_receiveCritSect);
    return _codecDataBase.ReceiveCodec();
}

// Incoming packet from network parsed and ready for decode, non blocking.
WebRtc_Word32
VideoCodingModuleImpl::IncomingPacket(const WebRtc_UWord8* incomingPayload,
                                    WebRtc_UWord32 payloadLength,
                                    const WebRtcRTPHeader& rtpInfo)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "IncomingPacket()");
    const VCMPacket packet(incomingPayload, payloadLength, rtpInfo);
    WebRtc_Word32 ret;
    if (_dualReceiver.State() != kPassive)
    {
        ret = _dualReceiver.InsertPacket(packet,
                                         rtpInfo.type.Video.width,
                                         rtpInfo.type.Video.height);
        if (ret == VCM_FLUSH_INDICATOR) {
          RequestKeyFrame();
          ResetDecoder();
        } else if (ret < 0) {
          return ret;
        }
    }
    ret = _receiver.InsertPacket(packet, rtpInfo.type.Video.width,
                                 rtpInfo.type.Video.height);
    if (ret == VCM_FLUSH_INDICATOR) {
      RequestKeyFrame();
      ResetDecoder();
    } else if (ret < 0) {
      return ret;
    }
    return VCM_OK;
}

// Set codec config parameters
WebRtc_Word32
VideoCodingModuleImpl::SetCodecConfigParameters(WebRtc_UWord8 payloadType,
                                                const WebRtc_UWord8* buffer,
                                                WebRtc_Word32 length)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "SetCodecConfigParameters()");
    CriticalSectionScoped cs(_receiveCritSect);

    WebRtc_Word32 ret = _codecDataBase.SetCodecConfigParameters(payloadType,
                                                                buffer,
                                                                length);
    if (ret < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "SetCodecConfigParameters() failed, %d", ret);
        return ret;
    }
    return VCM_OK;
}

// Minimum playout delay (used for lip-sync). This is the minimum delay required
// to sync with audio. Not included in  VideoCodingModule::Delay()
// Defaults to 0 ms.
WebRtc_Word32
VideoCodingModuleImpl::SetMinimumPlayoutDelay(WebRtc_UWord32 minPlayoutDelayMs)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "SetMininumPlayoutDelay(%u)", minPlayoutDelayMs);
    _timing.SetMinimumTotalDelay(minPlayoutDelayMs);
    return VCM_OK;
}

// The estimated delay caused by rendering, defaults to
// kDefaultRenderDelayMs = 10 ms
WebRtc_Word32
VideoCodingModuleImpl::SetRenderDelay(WebRtc_UWord32 timeMS)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "SetRenderDelay(%u)", timeMS);
    _timing.SetRenderDelay(timeMS);
    return VCM_OK;
}

// Current video delay
WebRtc_Word32
VideoCodingModuleImpl::Delay() const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Delay()");
    return _timing.TargetVideoDelay();
}

// Nack list
WebRtc_Word32
VideoCodingModuleImpl::NackList(WebRtc_UWord16* nackList, WebRtc_UWord16& size)
{
    VCMNackStatus nackStatus = kNackOk;
    // Collect sequence numbers from the default receiver
    // if in normal nack mode. Otherwise collect them from
    // the dual receiver if the dual receiver is receiving.
    if (_receiver.NackMode() != kNoNack)
    {
        nackStatus = _receiver.NackList(nackList, size);
    }
    else if (_dualReceiver.State() != kPassive)
    {
        nackStatus = _dualReceiver.NackList(nackList, size);
    }
    else
    {
        size = 0;
    }

    switch (nackStatus)
    {
    case kNackNeedMoreMemory:
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Out of memory");
            return VCM_MEMORY;
        }
    case kNackKeyFrameRequest:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            WEBRTC_TRACE(webrtc::kTraceWarning,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to get NACK list, requesting key frame");
            return RequestKeyFrame();
        }
    default:
        break;
    }
    return VCM_OK;
}

WebRtc_Word32
VideoCodingModuleImpl::ReceivedFrameCount(VCMFrameCount& frameCount) const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "ReceivedFrameCount()");
    return _receiver.ReceivedFrameCount(frameCount);
}

WebRtc_UWord32 VideoCodingModuleImpl::DiscardedPackets() const {
  WEBRTC_TRACE(webrtc::kTraceModuleCall,
               webrtc::kTraceVideoCoding,
               VCMId(_id),
               "DiscardedPackets()");
  return _receiver.DiscardedPackets();
}

}  // namespace webrtc
