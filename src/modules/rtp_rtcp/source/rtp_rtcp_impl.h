/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_

#include <list>

#include "bandwidth_management.h"
#include "rtcp_receiver.h"
#include "rtcp_sender.h"
#include "rtp_receiver.h"
#include "rtp_rtcp.h"
#include "rtp_sender.h"

#ifdef MATLAB
class MatlabPlot;
#endif

namespace webrtc {

class ModuleRtpRtcpImpl : public RtpRtcp, private TMMBRHelp
{
public:
    ModuleRtpRtcpImpl(const WebRtc_Word32 id,
                      const bool audio,
                      RtpRtcpClock* clock);

    virtual ~ModuleRtpRtcpImpl();

    // get Module ID
    WebRtc_Word32 Id()   {return _id;}

    // Get Module version
    WebRtc_Word32 Version(WebRtc_Word8* version,
                        WebRtc_UWord32& remainingBufferInBytes,
                        WebRtc_UWord32& position) const;

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    // De-muxing functionality for
    virtual WebRtc_Word32 RegisterDefaultModule(RtpRtcp* module);
    virtual WebRtc_Word32 DeRegisterDefaultModule();
    virtual bool DefaultModuleRegistered();

    virtual WebRtc_UWord32 NumberChildModules();

    // Lip-sync between voice-video
    virtual WebRtc_Word32 RegisterSyncModule(RtpRtcp* module);
    virtual WebRtc_Word32 DeRegisterSyncModule();

    virtual WebRtc_Word32 RegisterVideoModule(RtpRtcp* videoModule);
    virtual void DeRegisterVideoModule();

    // returns the number of milliseconds until the module want a worker thread to call Process
    virtual WebRtc_Word32 TimeUntilNextProcess();

    // Process any pending tasks such as timeouts
    virtual WebRtc_Word32 Process();

    /**
    *   Receiver
    */
    virtual WebRtc_Word32 InitReceiver();

    // configure a timeout value
    virtual WebRtc_Word32 SetPacketTimeout(const WebRtc_UWord32 RTPtimeoutMS,
                                           const WebRtc_UWord32 RTCPtimeoutMS);

    // Set periodic dead or alive notification
    virtual WebRtc_Word32 SetPeriodicDeadOrAliveStatus(
        const bool enable,
        const WebRtc_UWord8 sampleTimeSeconds);

    // Get periodic dead or alive notification status
    virtual WebRtc_Word32 PeriodicDeadOrAliveStatus(
        bool &enable,
        WebRtc_UWord8 &sampleTimeSeconds);

    virtual WebRtc_Word32 RegisterReceivePayload(const CodecInst& voiceCodec);

    virtual WebRtc_Word32 RegisterReceivePayload(const VideoCodec& videoCodec);

    virtual WebRtc_Word32 ReceivePayloadType(const CodecInst& voiceCodec,
                                             WebRtc_Word8* plType);

    virtual WebRtc_Word32 ReceivePayloadType(const VideoCodec& videoCodec,
                                             WebRtc_Word8* plType);

    virtual WebRtc_Word32 DeRegisterReceivePayload(
        const WebRtc_Word8 payloadType);

    // get the currently configured SSRC filter
    virtual WebRtc_Word32 SSRCFilter(WebRtc_UWord32& allowedSSRC) const;

    // set a SSRC to be used as a filter for incoming RTP streams
    virtual WebRtc_Word32 SetSSRCFilter(const bool enable, const WebRtc_UWord32 allowedSSRC);

    // Get last received remote timestamp
    virtual WebRtc_UWord32 RemoteTimestamp() const;

    // Get the current estimated remote timestamp
    virtual WebRtc_Word32 EstimatedRemoteTimeStamp(WebRtc_UWord32& timestamp) const;

    // Get incoming SSRC
    virtual WebRtc_UWord32 RemoteSSRC() const;

    // Get remote CSRC
    virtual WebRtc_Word32 RemoteCSRCs( WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const ;

    // called by the network module when we receive a packet
    virtual WebRtc_Word32 IncomingPacket( const WebRtc_UWord8* incomingPacket,
                                        const WebRtc_UWord16 packetLength);

    virtual WebRtc_Word32 IncomingAudioNTP(const WebRtc_UWord32 audioReceivedNTPsecs,
                                         const WebRtc_UWord32 audioReceivedNTPfrac,
                                         const WebRtc_UWord32 audioRTCPArrivalTimeSecs,
                                         const WebRtc_UWord32 audioRTCPArrivalTimeFrac);

    // Used by the module to deliver the incoming data to the codec module
    virtual WebRtc_Word32 RegisterIncomingDataCallback(RtpData* incomingDataCallback);

    // Used by the module to deliver messages to the codec module/appliation
    virtual WebRtc_Word32 RegisterIncomingRTPCallback(RtpFeedback* incomingMessagesCallback);

    virtual WebRtc_Word32 RegisterIncomingRTCPCallback(RtcpFeedback* incomingMessagesCallback);

    virtual WebRtc_Word32 RegisterIncomingVideoCallback(RtpVideoFeedback* incomingMessagesCallback);

    virtual WebRtc_Word32 RegisterAudioCallback(RtpAudioFeedback* messagesCallback);

    /**
    *   Sender
    */
    virtual WebRtc_Word32 InitSender();

    virtual WebRtc_Word32 SetRTPKeepaliveStatus(const bool enable,
                                              const WebRtc_Word8 unknownPayloadType,
                                              const WebRtc_UWord16 deltaTransmitTimeMS);

    virtual WebRtc_Word32 RTPKeepaliveStatus(bool* enable,
                                           WebRtc_Word8* unknownPayloadType,
                                           WebRtc_UWord16* deltaTransmitTimeMS) const;

    virtual bool RTPKeepalive() const;

    virtual WebRtc_Word32 RegisterSendPayload(const CodecInst& voiceCodec);

    virtual WebRtc_Word32 RegisterSendPayload(const VideoCodec& videoCodec);

    // set codec name and payload type
    WebRtc_Word32 RegisterSendPayload(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                            const WebRtc_Word8 payloadType,
                                            const WebRtc_UWord32 frequency,
                                            const WebRtc_UWord8 channels,
                                            const WebRtc_UWord32 rate);

    virtual WebRtc_Word32 DeRegisterSendPayload(const WebRtc_Word8 payloadType);

    virtual WebRtc_Word8 SendPayloadType() const;

    // get start timestamp
    virtual WebRtc_UWord32 StartTimestamp() const;

    // configure start timestamp, default is a random number
    virtual WebRtc_Word32 SetStartTimestamp(const WebRtc_UWord32 timestamp);

    // Get SequenceNumber
    virtual WebRtc_UWord16 SequenceNumber() const;

    // Set SequenceNumber, default is a random number
    virtual WebRtc_Word32 SetSequenceNumber(const WebRtc_UWord16 seq);

    // Get SSRC
    virtual WebRtc_UWord32 SSRC() const;

    // configure SSRC, default is a random number
    virtual WebRtc_Word32 SetSSRC(const WebRtc_UWord32 ssrc);

    // Get CSRC
    virtual WebRtc_Word32 CSRCs( WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const ;

    // Set CSRC
    virtual WebRtc_Word32 SetCSRCs( const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
                                  const WebRtc_UWord8 arrLength);

    virtual WebRtc_Word32 SetCSRCStatus(const bool include);

    virtual WebRtc_UWord32 PacketCountSent() const;

    virtual int CurrentSendFrequencyHz() const;

    virtual WebRtc_UWord32 ByteCountSent() const;

    // sends kRtcpByeCode when going from true to false
    virtual WebRtc_Word32 SetSendingStatus(const bool sending);

    // get send status
    virtual bool Sending() const;

    // Drops or relays media packets
    virtual WebRtc_Word32 SetSendingMediaStatus(const bool sending);

    // Send media status
    virtual bool SendingMedia() const;

    // Used by the module to send RTP and RTCP packet to the network module
    virtual WebRtc_Word32 RegisterSendTransport(Transport* outgoingTransport);

    // Used by the codec module to deliver a video or audio frame for packetization
    virtual WebRtc_Word32
    SendOutgoingData(const FrameType frameType,
                     const WebRtc_Word8 payloadType,
                     const WebRtc_UWord32 timeStamp,
                     const WebRtc_UWord8* payloadData,
                     const WebRtc_UWord32 payloadSize,
                     const RTPFragmentationHeader* fragmentation = NULL,
                     const RTPVideoHeader* rtpVideoHdr = NULL);

    /*
    *   RTCP
    */

    // Get RTCP status
    virtual RTCPMethod RTCP() const;

    // configure RTCP status i.e on/off
    virtual WebRtc_Word32 SetRTCPStatus(const RTCPMethod method);

    // Set RTCP CName
    virtual WebRtc_Word32 SetCNAME(const WebRtc_Word8 cName[RTCP_CNAME_SIZE]);

    // Get RTCP CName
    virtual WebRtc_Word32 CNAME(WebRtc_Word8 cName[RTCP_CNAME_SIZE]);

    // Get remote CName
    virtual WebRtc_Word32 RemoteCNAME(const WebRtc_UWord32 remoteSSRC,
                                    WebRtc_Word8 cName[RTCP_CNAME_SIZE]) const;

    // Get remote NTP
    virtual WebRtc_Word32 RemoteNTP(WebRtc_UWord32 *ReceivedNTPsecs,
                                  WebRtc_UWord32 *ReceivedNTPfrac,
                                  WebRtc_UWord32 *RTCPArrivalTimeSecs,
                                  WebRtc_UWord32 *RTCPArrivalTimeFrac) const ;

    virtual WebRtc_Word32 AddMixedCNAME(const WebRtc_UWord32 SSRC,
                                      const WebRtc_Word8 cName[RTCP_CNAME_SIZE]);

    virtual WebRtc_Word32 RemoveMixedCNAME(const WebRtc_UWord32 SSRC);

    // Get RoundTripTime
    virtual WebRtc_Word32 RTT(const WebRtc_UWord32 remoteSSRC,
                            WebRtc_UWord16* RTT,
                            WebRtc_UWord16* avgRTT,
                            WebRtc_UWord16* minRTT,
                            WebRtc_UWord16* maxRTT) const;

    // Reset RoundTripTime statistics
    virtual WebRtc_Word32 ResetRTT(const WebRtc_UWord32 remoteSSRC);

    // Force a send of an RTCP packet
    // normal SR and RR are triggered via the process function
    virtual WebRtc_Word32 SendRTCP(WebRtc_UWord32 rtcpPacketType = kRtcpReport);

    // statistics of our localy created statistics of the received RTP stream
    virtual WebRtc_Word32 StatisticsRTP(WebRtc_UWord8  *fraction_lost,
                                      WebRtc_UWord32 *cum_lost,
                                      WebRtc_UWord32 *ext_max,
                                      WebRtc_UWord32 *jitter,
                                      WebRtc_UWord32 *max_jitter = NULL) const;

    // Reset RTP statistics
    virtual WebRtc_Word32 ResetStatisticsRTP();

    virtual WebRtc_Word32 ResetReceiveDataCountersRTP();

    virtual WebRtc_Word32 ResetSendDataCountersRTP();

    // statistics of the amount of data sent and received
    virtual WebRtc_Word32 DataCountersRTP(WebRtc_UWord32 *bytesSent,
                                          WebRtc_UWord32 *packetsSent,
                                          WebRtc_UWord32 *bytesReceived,
                                          WebRtc_UWord32 *packetsReceived) const;

    virtual WebRtc_Word32 ReportBlockStatistics(WebRtc_UWord8 *fraction_lost,
                                                WebRtc_UWord32 *cum_lost,
                                                WebRtc_UWord32 *ext_max,
                                                WebRtc_UWord32 *jitter);

    // Get received RTCP report, sender info
    virtual WebRtc_Word32 RemoteRTCPStat( RTCPSenderInfo* senderInfo);

    // Get received RTCP report, report block
    virtual WebRtc_Word32 RemoteRTCPStat( const WebRtc_UWord32 remoteSSRC,
                                        RTCPReportBlock* receiveBlock);

    // Set received RTCP report block
    virtual WebRtc_Word32 AddRTCPReportBlock(const WebRtc_UWord32 SSRC,
                                           const RTCPReportBlock* receiveBlock);

    virtual WebRtc_Word32 RemoveRTCPReportBlock(const WebRtc_UWord32 SSRC);

    /*
    *  (REMB) Receiver Estimated Max Bitrate
    */
    virtual bool REMB() const;

    virtual WebRtc_Word32 SetREMBStatus(const bool enable);

    virtual WebRtc_Word32 SetREMBData(const WebRtc_UWord32 bitrate,
                                      const WebRtc_UWord8 numberOfSSRC,
                                      const WebRtc_UWord32* SSRC);
    /*
    *   (TMMBR) Temporary Max Media Bit Rate
    */
    virtual bool TMMBR() const ;

    virtual WebRtc_Word32 SetTMMBRStatus(const bool enable);

    virtual WebRtc_Word32 TMMBRReceived(const WebRtc_UWord32 size,
                                      const WebRtc_UWord32 accNumCandidates,
                                      TMMBRSet* candidateSet) const;

    virtual WebRtc_Word32 SetTMMBN(const TMMBRSet* boundingSet,
                                 const WebRtc_UWord32 maxBitrateKbit);

    virtual WebRtc_Word32 RequestTMMBR(const WebRtc_UWord32 estimatedBW,
                                     const WebRtc_UWord32 packetOH);

    virtual WebRtc_UWord16 MaxPayloadLength() const;

    virtual WebRtc_UWord16 MaxDataPayloadLength() const;

    virtual WebRtc_Word32 SetMaxTransferUnit(const WebRtc_UWord16 size);

    virtual WebRtc_Word32 SetTransportOverhead(const bool TCP,
                                             const bool IPV6,
                                             const WebRtc_UWord8 authenticationOverhead = 0);

    /*
    *   (NACK) Negative acknowledgement
    */

    // Is Negative acknowledgement requests on/off?
    virtual NACKMethod NACK() const ;

    // Turn negative acknowledgement requests on/off
    virtual WebRtc_Word32 SetNACKStatus(const NACKMethod method);

    // Send a Negative acknowledgement packet
    virtual WebRtc_Word32 SendNACK(const WebRtc_UWord16* nackList,
                                   const WebRtc_UWord16 size);

    // Store the sent packets, needed to answer to a Negative acknowledgement requests
    virtual WebRtc_Word32 SetStorePacketsStatus(const bool enable, const WebRtc_UWord16 numberToStore = 200);

    /*
    *   (APP) Application specific data
    */
    virtual WebRtc_Word32 SetRTCPApplicationSpecificData(const WebRtc_UWord8 subType,
                                                       const WebRtc_UWord32 name,
                                                       const WebRtc_UWord8* data,
                                                       const WebRtc_UWord16 length);
    /*
    *   (XR) VOIP metric
    */
    virtual WebRtc_Word32 SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric);

    /*
    *   Audio
    */

    // set audio packet size, used to determine when it's time to send a DTMF packet in silence (CNG)
    virtual WebRtc_Word32 SetAudioPacketSize(const WebRtc_UWord16 packetSizeSamples);

    // Outband DTMF detection
    virtual WebRtc_Word32 SetTelephoneEventStatus(const bool enable,
                                                const bool forwardToDecoder,
                                                const bool detectEndOfTone = false);

    // Is outband DTMF turned on/off?
    virtual bool TelephoneEvent() const;

    // Is forwarding of outband telephone events turned on/off?
    virtual bool TelephoneEventForwardToDecoder() const;

    virtual bool SendTelephoneEventActive(WebRtc_Word8& telephoneEvent) const;

    // Send a TelephoneEvent tone using RFC 2833 (4733)
    virtual WebRtc_Word32 SendTelephoneEventOutband(const WebRtc_UWord8 key,
                                                  const WebRtc_UWord16 time_ms,
                                                  const WebRtc_UWord8 level);

    // Set payload type for Redundant Audio Data RFC 2198
    virtual WebRtc_Word32 SetSendREDPayloadType(const WebRtc_Word8 payloadType);

    // Get payload type for Redundant Audio Data RFC 2198
    virtual WebRtc_Word32 SendREDPayloadType(WebRtc_Word8& payloadType) const;

    // Set status and ID for header-extension-for-audio-level-indication.
    virtual WebRtc_Word32 SetRTPAudioLevelIndicationStatus(const bool enable,
                                                         const WebRtc_UWord8 ID);

    // Get status and ID for header-extension-for-audio-level-indication.
    virtual WebRtc_Word32 GetRTPAudioLevelIndicationStatus(bool& enable,
                                                         WebRtc_UWord8& ID) const;

    // Store the audio level in dBov for header-extension-for-audio-level-indication.
    virtual WebRtc_Word32 SetAudioLevel(const WebRtc_UWord8 level_dBov);

    /*
    *   Video
    */
    virtual RtpVideoCodecTypes ReceivedVideoCodec() const;

    virtual RtpVideoCodecTypes SendVideoCodec() const;

    virtual WebRtc_Word32 SendRTCPSliceLossIndication(const WebRtc_UWord8 pictureID);

    // Set method for requestion a new key frame
    virtual WebRtc_Word32 SetKeyFrameRequestMethod(const KeyFrameRequestMethod method);

    // send a request for a keyframe
    virtual WebRtc_Word32 RequestKeyFrame(const FrameType frameType);

    virtual WebRtc_Word32 SetCameraDelay(const WebRtc_Word32 delayMS);

    virtual WebRtc_Word32 SetSendBitrate(const WebRtc_UWord32 startBitrate,
                                       const WebRtc_UWord16 minBitrateKbit,
                                       const WebRtc_UWord16 maxBitrateKbit);

    virtual WebRtc_Word32 SetGenericFECStatus(const bool enable,
                                            const WebRtc_UWord8 payloadTypeRED,
                                            const WebRtc_UWord8 payloadTypeFEC);

    virtual WebRtc_Word32 GenericFECStatus(bool& enable,
                                         WebRtc_UWord8& payloadTypeRED,
                                         WebRtc_UWord8& payloadTypeFEC);


    virtual WebRtc_Word32 SetFECCodeRate(const WebRtc_UWord8 keyFrameCodeRate,
                                         const WebRtc_UWord8 deltaFrameCodeRate);

    virtual WebRtc_Word32 SetFECUepProtection(const bool keyUseUepProtection,
                                              const bool deltaUseUepProtection);

    virtual WebRtc_Word32 SetH263InverseLogic(const bool enable);

    virtual WebRtc_Word32 LastReceivedNTP(WebRtc_UWord32& NTPsecs,
                                          WebRtc_UWord32& NTPfrac,
                                          WebRtc_UWord32& remoteSR);

    virtual WebRtc_Word32 BoundingSet(bool &tmmbrOwner,
                                      TMMBRSet*& boundingSetRec);

    virtual void BitrateSent(WebRtc_UWord32* totalRate,
                             WebRtc_UWord32* videoRate,
                             WebRtc_UWord32* fecRate,
                             WebRtc_UWord32* nackRate) const;

    virtual void SetRemoteSSRC(const WebRtc_UWord32 SSRC);
    
    virtual WebRtc_UWord32 SendTimeOfSendReport(const WebRtc_UWord32 sendReport);

    virtual RateControlRegion OnOverUseStateUpdate(const RateControlInput& rateControlInput);

    // good state of RTP receiver inform sender
    virtual WebRtc_Word32 SendRTCPReferencePictureSelection(const WebRtc_UWord64 pictureID);

    virtual void OnBandwidthEstimateUpdate(WebRtc_UWord16 bandWidthKbit);

    void OnReceivedNTP() ;

    // bw estimation
    void OnPacketLossStatisticsUpdate(
        const WebRtc_UWord8 fractionLost,
        const WebRtc_UWord16 roundTripTime,
        const WebRtc_UWord32 lastReceivedExtendedHighSeqNum,
        bool triggerOnNetworkChanged);

    void OnReceivedTMMBR();

    void OnReceivedEstimatedMaxBitrate(const WebRtc_UWord32 maxBitrate);

    void OnReceivedBandwidthEstimateUpdate(const WebRtc_UWord16 bwEstimateKbit);

    // bad state of RTP receiver request a keyframe
    void OnRequestIntraFrame(const FrameType frameType);

    void OnReceivedIntraFrameRequest(const RtpRtcp* caller);

    // received a request for a new SLI
    void OnReceivedSliceLossIndication(const WebRtc_UWord8 pictureID);

    // received a new refereence frame
    void OnReceivedReferencePictureSelectionIndication(
        const WebRtc_UWord64 pitureID);

    void OnReceivedNACK(const WebRtc_UWord16 nackSequenceNumbersLength,
                        const WebRtc_UWord16* nackSequenceNumbers);

    void OnRequestSendReport();

protected:
    void RegisterChildModule(RtpRtcp* module);

    void DeRegisterChildModule(RtpRtcp* module);

    bool UpdateRTCPReceiveInformationTimers();

    void ProcessDeadOrAliveTimer();

    WebRtc_UWord32 BitrateReceivedNow() const;

    // Get remote SequenceNumber
    WebRtc_UWord16 RemoteSequenceNumber() const;

    WebRtc_Word32 UpdateTMMBR();

    // only for internal testing
    WebRtc_UWord32 LastSendReport(WebRtc_UWord32& lastRTCPTime);

    RTPSender                 _rtpSender;
    RTPReceiver               _rtpReceiver;

    RTCPSender                _rtcpSender;
    RTCPReceiver              _rtcpReceiver;

    RtpRtcpClock&             _clock;
private:
    void SendKeyFrame();
    void ProcessDefaultModuleBandwidth(bool triggerOnNetworkChanged);

    WebRtc_Word32             _id;
    const bool                _audio;
    bool                      _collisionDetected;
    WebRtc_UWord32            _lastProcessTime;
    WebRtc_UWord16            _packetOverHead;

    CriticalSectionWrapper*       _criticalSectionModulePtrs;
    CriticalSectionWrapper*       _criticalSectionModulePtrsFeedback;
    ModuleRtpRtcpImpl*            _defaultModule;
    ModuleRtpRtcpImpl*            _audioModule;
    ModuleRtpRtcpImpl*            _videoModule;
    std::list<ModuleRtpRtcpImpl*> _childModules;

    // Dead or alive
    bool                  _deadOrAliveActive;
    WebRtc_UWord32        _deadOrAliveTimeoutMS;
    WebRtc_UWord32        _deadOrAliveLastTimer;

    // receive side
    BandwidthManagement   _bandwidthManagement;

    WebRtc_UWord32        _receivedNTPsecsAudio;
    WebRtc_UWord32        _receivedNTPfracAudio;
    WebRtc_UWord32        _RTCPArrivalTimeSecsAudio;
    WebRtc_UWord32        _RTCPArrivalTimeFracAudio;

    // send side
    NACKMethod            _nackMethod;
    WebRtc_UWord32        _nackLastTimeSent;
    WebRtc_UWord16        _nackLastSeqNumberSent;

    bool                  _simulcast;
    VideoCodec            _sendVideoCodec;
    KeyFrameRequestMethod _keyFrameReqMethod;

#ifdef MATLAB
    MatlabPlot*           _plot1;
#endif
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
