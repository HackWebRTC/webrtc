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
 * vie_channel.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CHANNEL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CHANNEL_H_

// Defines
#include "vie_defines.h"

#include "typedefs.h"
#include "vie_network.h"
#include "rtp_rtcp_defines.h"
#include "udp_transport.h"
#include "video_coding_defines.h"
#ifdef WEBRTC_SRTP
#include "SrtpModule.h"
#endif
#include "tick_util.h"
#include "vie_frame_provider_base.h"
#include "vie_file_recorder.h"

// Forward declarations

class SrtpModule;
class VideoRenderCallback;

namespace webrtc
{
class CriticalSectionWrapper;
class Encryption;
class ProcessThread;
class RtpRtcp;
class ThreadWrapper;
class VideoCodingModule;
class VideoDecoder;
class ViEDecoderObserver;
class ViEEffectFilter;
class ViENetworkObserver;
class ViEReceiver;
class ViERTCPObserver;
class ViERTPObserver;
class ViESender;
class ViESyncModule;
class VoEVideoSync;

class ViEChannel:
    public VCMFrameTypeCallback, // VCM Module
    public VCMReceiveCallback, // VCM Module
    public VCMReceiveStatisticsCallback, // VCM Module
    public VCMPacketRequestCallback, // VCM Module
    public VCMFrameStorageCallback, // VCM Module
    public RtcpFeedback, // RTP/RTCP Module
    public RtpFeedback, // RTP/RTCP Module
    public ViEFrameProviderBase
{
public:
    ViEChannel(WebRtc_Word32 channelId, WebRtc_Word32 engineId,
               WebRtc_UWord32 numberOfCores,
               ProcessThread& moduleProcessThread);
    ~ViEChannel();

    WebRtc_Word32 Init();

    //-----------------------------------------------------------------
    // Codecs
    //-----------------------------------------------------------------

    WebRtc_Word32 SetSendCodec(const VideoCodec& videoCodec,
                               bool newStream = true);

    WebRtc_Word32 SetReceiveCodec(const VideoCodec& videoCodec);

    WebRtc_Word32 GetReceiveCodec(VideoCodec& videoCodec);

    WebRtc_Word32 RegisterCodecObserver(ViEDecoderObserver* observer);

    WebRtc_Word32 RegisterExternalDecoder(const WebRtc_UWord8 plType,
                                          VideoDecoder* decoder,
                                          bool decoderRender,
                                          WebRtc_Word32 renderDelay);

    WebRtc_Word32 DeRegisterExternalDecoder(const WebRtc_UWord8 plType);

    WebRtc_Word32 ReceiveCodecStatistics(WebRtc_UWord32& numKeyFrames,
                                         WebRtc_UWord32& numDeltaFrames);

    WebRtc_Word32 WaitForKeyFrame(bool wait);

    WebRtc_Word32 SetSignalPacketLossStatus(bool enable, bool onlyKeyFrames);

    //-----------------------------------------------------------------
    // RTP/RTCP
    //-----------------------------------------------------------------
    WebRtc_Word32 SetRTCPMode(const RTCPMethod rtcpMode);

    WebRtc_Word32 GetRTCPMode(RTCPMethod& rtcpMode);

    WebRtc_Word32 SetNACKStatus(const bool enable);

    WebRtc_Word32 SetFECStatus(const bool enable,
                               const unsigned char payloadTypeRED,
                               const unsigned char payloadTypeFEC);
    WebRtc_Word32 SetHybridNACKFECStatus(const bool enable,
                                         const unsigned char payloadTypeRED,
                                         const unsigned char payloadTypeFEC);

    WebRtc_Word32
        SetKeyFrameRequestMethod(const KeyFrameRequestMethod method);

    WebRtc_Word32 EnableTMMBR(const bool enable);

    WebRtc_Word32 EnableKeyFrameRequestCallback(const bool enable);

    WebRtc_Word32 SetSSRC(const WebRtc_UWord32 SSRC);

    WebRtc_Word32 GetLocalSSRC(WebRtc_UWord32& SSRC);

    WebRtc_Word32 GetRemoteSSRC(WebRtc_UWord32& SSRC);

    WebRtc_Word32 GetRemoteCSRC(unsigned int CSRCs[kRtpCsrcSize]);

    WebRtc_Word32 SetStartSequenceNumber(WebRtc_UWord16 sequenceNumber);

    WebRtc_Word32 SetRTCPCName(const WebRtc_Word8 rtcpCName[]);

    WebRtc_Word32 GetRTCPCName(WebRtc_Word8 rtcpCName[]);

    WebRtc_Word32 GetRemoteRTCPCName(WebRtc_Word8 rtcpCName[]);

    WebRtc_Word32 RegisterRtpObserver(ViERTPObserver* observer);

    WebRtc_Word32 RegisterRtcpObserver(ViERTCPObserver* observer);

    WebRtc_Word32 SendApplicationDefinedRTCPPacket(
        const WebRtc_UWord8 subType,
        WebRtc_UWord32 name,
        const WebRtc_UWord8* data,
        WebRtc_UWord16 dataLengthInBytes);

    WebRtc_Word32 GetSendRtcpStatistics(WebRtc_UWord16& fractionLost,
                                        WebRtc_UWord32& cumulativeLost,
                                        WebRtc_UWord32& extendedMax,
                                        WebRtc_UWord32& jitterSamples,
                                        WebRtc_Word32& rttMs);

    WebRtc_Word32 GetReceivedRtcpStatistics(WebRtc_UWord16& fractionLost,
                                            WebRtc_UWord32& cumulativeLost,
                                            WebRtc_UWord32& extendedMax,
                                            WebRtc_UWord32& jitterSamples,
                                            WebRtc_Word32& rttMs);

    WebRtc_Word32 GetRtpStatistics(WebRtc_UWord32& bytesSent,
                                   WebRtc_UWord32& packetsSent,
                                   WebRtc_UWord32& bytesReceived,
                                   WebRtc_UWord32& packetsReceived) const;

    WebRtc_Word32 SetKeepAliveStatus(const bool enable,
                                     const WebRtc_Word8 unknownPayloadType,
                                     const WebRtc_UWord16 deltaTransmitTimeMS);

    WebRtc_Word32 GetKeepAliveStatus(bool& enable,
                                     WebRtc_Word8& unknownPayloadType,
                                     WebRtc_UWord16& deltaTransmitTimeMS);

    WebRtc_Word32 StartRTPDump(const char fileNameUTF8[1024],
                               RTPDirections direction);

    WebRtc_Word32 StopRTPDump(RTPDirections direction);

    // Implements RtcpFeedback
    virtual void OnLipSyncUpdate(const WebRtc_Word32 id,
                                 const WebRtc_Word32 audioVideoOffset);

    virtual void OnApplicationDataReceived(const WebRtc_Word32 id,
                                           const WebRtc_UWord8 subType,
                                           const WebRtc_UWord32 name,
                                           const WebRtc_UWord16 length,
                                           const WebRtc_UWord8* data);

    // Implements RtpFeedback
    virtual WebRtc_Word32 OnInitializeDecoder(
        const WebRtc_Word32 id,
        const WebRtc_Word8 payloadType,
        const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
        const int frequency,
        const WebRtc_UWord8 channels,
        const WebRtc_UWord32 rate);

    virtual void OnPacketTimeout(const WebRtc_Word32 id);

    virtual void OnReceivedPacket(const WebRtc_Word32 id,
                                  const RtpRtcpPacketType packetType);

    virtual void OnPeriodicDeadOrAlive(const WebRtc_Word32 id,
                                       const RTPAliveType alive);

    virtual void OnIncomingSSRCChanged(const WebRtc_Word32 id,
                                       const WebRtc_UWord32 SSRC);

    virtual void OnIncomingCSRCChanged(const WebRtc_Word32 id,
                                       const WebRtc_UWord32 CSRC,
                                       const bool added);

    //-----------------------------------------------------------------
    // Network
    //-----------------------------------------------------------------

    // Soure and destination
    WebRtc_Word32 SetLocalReceiver(const WebRtc_UWord16 rtpPort,
                                   const WebRtc_UWord16 rtcpPort,
                                   const WebRtc_Word8* ipAddress);

    WebRtc_Word32 GetLocalReceiver(WebRtc_UWord16& rtpPort,
                                   WebRtc_UWord16& rtcpPort,
                                   WebRtc_Word8* ipAddress) const;

    WebRtc_Word32 SetSendDestination(const WebRtc_Word8* ipAddress,
                                     const WebRtc_UWord16 rtpPort,
                                     const WebRtc_UWord16 rtcpPort,
                                     const WebRtc_UWord16 sourceRtpPort,
                                     const WebRtc_UWord16 sourceRtcpPort);

    WebRtc_Word32 GetSendDestination(WebRtc_Word8* ipAddress,
                                     WebRtc_UWord16& rtpPort,
                                     WebRtc_UWord16& rtcpPort,
                                     WebRtc_UWord16& sourceRtpPort,
                                     WebRtc_UWord16& sourceRtcpPort) const;

    WebRtc_Word32 GetSourceInfo(WebRtc_UWord16& rtpPort,
                                WebRtc_UWord16& rtcpPort,
                                WebRtc_Word8* ipAddress,
                                WebRtc_UWord32 ipAddressLength);

    // Start/Stop Send/Receive
    WebRtc_Word32 StartSend();
    WebRtc_Word32 StopSend();
    bool Sending();
    WebRtc_Word32 StartReceive();
    WebRtc_Word32 StopReceive();
    bool Receiving();

    // External transport
    WebRtc_Word32 RegisterSendTransport(Transport& transport);

    WebRtc_Word32 DeregisterSendTransport();

    WebRtc_Word32 ReceivedRTPPacket(const void* rtpPacket,
                                    const WebRtc_Word32 rtpPacketLength);

    WebRtc_Word32 ReceivedRTCPPacket(const void* rtcpPacket,
                                     const WebRtc_Word32 rtcpPacketLength);

    // IPv6
    WebRtc_Word32 EnableIPv6();

    bool IsIPv6Enabled();

    // Source IP address and port filter
    WebRtc_Word32 SetSourceFilter(const WebRtc_UWord16 rtpPort,
                                  const WebRtc_UWord16 rtcpPort,
                                  const WebRtc_Word8* ipAddress);

    WebRtc_Word32 GetSourceFilter(WebRtc_UWord16& rtpPort,
                                  WebRtc_UWord16& rtcpPort,
                                  WebRtc_Word8* ipAddress) const;

    // ToS
    WebRtc_Word32 SetToS(const WebRtc_Word32 DSCP, const bool useSetSockOpt);

    WebRtc_Word32 GetToS(WebRtc_Word32& DSCP, bool& useSetSockOpt) const;

    // GQoS
    WebRtc_Word32 SetSendGQoS(const bool enable,
                              const WebRtc_Word32 serviceType,
                              const WebRtc_UWord32 maxBitrate,
                              const WebRtc_Word32 overrideDSCP);

    WebRtc_Word32 GetSendGQoS(bool& enabled, WebRtc_Word32& serviceType,
                              WebRtc_Word32& overrideDSCP) const;

    // Network settings
    WebRtc_Word32 SetMTU(WebRtc_UWord16 mtu);

    WebRtc_UWord16 MaxDataPayloadLength() const;

    WebRtc_Word32 SetMaxPacketBurstSize(WebRtc_UWord16 maxNumberOfPackets);

    WebRtc_Word32 SetPacketBurstSpreadState(bool enable,
                                            const WebRtc_UWord16 framePeriodMS);

    // Packet timout notification
    WebRtc_Word32 SetPacketTimeoutNotification(bool enable,
                                               WebRtc_UWord32 timeoutSeconds);

    // Periodic dead-or-alive reports
    WebRtc_Word32 RegisterNetworkObserver(ViENetworkObserver* observer);
    bool NetworkObserverRegistered();

    WebRtc_Word32
        SetPeriodicDeadOrAliveStatus(const bool enable,
                                     const WebRtc_UWord32 sampleTimeSeconds);

    WebRtc_Word32 SendUDPPacket(const WebRtc_Word8* data,
                                const WebRtc_UWord32 length,
                                WebRtc_Word32& transmittedBytes,
                                bool useRtcpSocket);

    //-----------------------------------------------------------------
    // Image processing
    //-----------------------------------------------------------------
    WebRtc_Word32 EnableColorEnhancement(bool enable);

    //-----------------------------------------------------------------
    // Register sender
    //-----------------------------------------------------------------
    WebRtc_Word32
        RegisterSendRtpRtcpModule(RtpRtcp& sendRtpRtcpModule);

    WebRtc_Word32 DeregisterSendRtpRtcpModule();

    // Implements VCM::VCMReceiveCallback, getting decoded frames from
    // VCM.
    virtual WebRtc_Word32 FrameToRender(VideoFrame& videoFrame);

    // Implements VCM::VCMReceiveCallback, getting info about decoded
    // frames from VCM.
    virtual WebRtc_Word32 ReceivedDecodedReferenceFrame(
        const WebRtc_UWord64 pictureId);

    //Implements VCM::VideoFrameStorageCallback
    virtual WebRtc_Word32 StoreReceivedFrame(
        const EncodedVideoData& frameToStore);

    // Implements VCM::VideoReceiveStatisticsCallback
    virtual WebRtc_Word32 ReceiveStatistics(const WebRtc_UWord32 bitRate,
                                            const WebRtc_UWord32 frameRate);

    // Implements VCM::VideoFrameTypeCallback
    virtual WebRtc_Word32 FrameTypeRequest(const FrameType frameType);

    // Implements VCM::VideoFrameTypeCallback
    virtual WebRtc_Word32 SliceLossIndicationRequest(
        const WebRtc_UWord64 pictureId);

    // Implements VCM::VideoPacketRequestCallback
    virtual WebRtc_Word32 ResendPackets(const WebRtc_UWord16* sequenceNumbers,
                                        WebRtc_UWord16 length);

#ifdef WEBRTC_SRTP
    //SRTP
    WebRtc_Word32 EnableSRTPSend(
        const SrtpModule::CipherTypes cipherType,
        const unsigned int cipherKeyLength,
        const SrtpModule::AuthenticationTypes authType,
        const unsigned int authKeyLength,
        const unsigned int authTagLength,
        const SrtpModule::SecurityLevels level,
        const WebRtc_UWord8* key,
        const bool useForRTCP);

    WebRtc_Word32 DisableSRTPSend();

    WebRtc_Word32 EnableSRTPReceive(
        const SrtpModule::CipherTypes cipherType,
        const unsigned int cipherKeyLength,
        const SrtpModule::AuthenticationTypes authType,
        const unsigned int authKeyLength,
        const unsigned int authTagLength,
        const SrtpModule::SecurityLevels level,
        const WebRtc_UWord8* key,
        const bool useForRTCP);
    WebRtc_Word32 DisableSRTPReceive();
#endif

    WebRtc_Word32 RegisterExternalEncryption(Encryption* encryption);
    WebRtc_Word32 DeRegisterExternalEncryption();

    //Voice Engine
    WebRtc_Word32 SetVoiceChannel(WebRtc_Word32 veChannelId,
                                  VoEVideoSync* veSyncInterface);
    WebRtc_Word32 VoiceChannel();

    //ViEFrameProviderBase
	virtual int FrameCallbackChanged(){return -1;}    

    // Effect filter
    WebRtc_Word32 RegisterEffectFilter(ViEEffectFilter* effectFilter);

    WebRtc_Word32 SetInverseH263Logic(const bool enable);

    // File recording
    ViEFileRecorder& GetIncomingFileRecorder();
    void ReleaseIncomingFileRecorder();

protected:
    // Thread function according to ThreadWrapper
    static bool ChannelDecodeThreadFunction(void* obj);
    bool ChannelDecodeProcess();

private:

    WebRtc_Word32 StartDecodeThread();
    WebRtc_Word32 StopDecodeThread();

    // Protection
    WebRtc_Word32 ProcessNACKRequest(const bool enable);

    WebRtc_Word32 ProcessFECRequest(const bool enable,
                                    const unsigned char payloadTypeRED,
                                    const unsigned char payloadTypeFEC);

    // General members
    WebRtc_Word32 _channelId;
    WebRtc_Word32 _engineId;
    WebRtc_UWord32 _numberOfCores;
    WebRtc_UWord8 _numSocketThreads;

    // Critical sections
    // Used for all registered callbacks except rendering.
    CriticalSectionWrapper& _callbackCritsect;
    // Use the same as above instead a seperate?
    CriticalSectionWrapper& _dataCritsect;

    // Owned modules/classes
    RtpRtcp& _rtpRtcp;
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    UdpTransport& _socketTransport;
#endif
    VideoCodingModule& _vcm;
    ViEReceiver& _vieReceiver;
    ViESender& _vieSender;
    ViESyncModule& _vieSync;//Lip syncronization

    //Uses
    ProcessThread& _moduleProcessThread;
    ViEDecoderObserver* _codecObserver;
    bool _doKeyFrameCallbackRequest;
    ViERTPObserver* _rtpObserver;
    ViERTCPObserver* _rtcpObserver;
    ViENetworkObserver* _networkObserver;
    bool _rtpPacketTimeout;
    bool _usingPacketSpread;

    // Registered members
    Transport* _ptrExternalTransport;

    // Codec
    bool _decoderReset;
    bool _waitForKeyFrame;

    // Decoder
    ThreadWrapper* _ptrDecodeThread;

    //SRTP - using seperate pointers for encryption and decryption to support
    // simultaneous operations.
    SrtpModule* _ptrSrtpModuleEncryption;
    SrtpModule* _ptrSrtpModuleDecryption;
    Encryption* _ptrExternalEncryption;

    // Effect filter and color enhancement
    ViEEffectFilter* _effectFilter;
    bool _colorEnhancement;

    // Time when RTT time was last reported to VCM JB.
    TickTime _vcmRTTReported;

    //Recording
    ViEFileRecorder _fileRecorder;
};

} // namespace webrtc

#endif    // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CHANNEL_H_
