/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
#include <vector>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

#ifdef MATLAB
class MatlabPlot;
#endif

namespace webrtc {

class ModuleRtpRtcpImpl : public RtpRtcp {
 public:
  explicit ModuleRtpRtcpImpl(const RtpRtcp::Configuration& configuration);

  virtual ~ModuleRtpRtcpImpl();

  // Returns the number of milliseconds until the module want a worker thread to
  // call Process.
  virtual WebRtc_Word32 TimeUntilNextProcess();

  // Process any pending tasks such as timeouts.
  virtual WebRtc_Word32 Process();

  // Receiver part.

  // Configure a timeout value.
  virtual WebRtc_Word32 SetPacketTimeout(const WebRtc_UWord32 rtp_timeout_ms,
                                         const WebRtc_UWord32 rtcp_timeout_ms);

  // Set periodic dead or alive notification.
  virtual WebRtc_Word32 SetPeriodicDeadOrAliveStatus(
      const bool enable,
      const WebRtc_UWord8 sample_time_seconds);

  // Get periodic dead or alive notification status.
  virtual WebRtc_Word32 PeriodicDeadOrAliveStatus(
      bool& enable,
      WebRtc_UWord8& sample_time_seconds);

  virtual WebRtc_Word32 RegisterReceivePayload(const CodecInst& voice_codec);

  virtual WebRtc_Word32 RegisterReceivePayload(const VideoCodec& video_codec);

  virtual WebRtc_Word32 ReceivePayloadType(const CodecInst& voice_codec,
                                           WebRtc_Word8* pl_type);

  virtual WebRtc_Word32 ReceivePayloadType(const VideoCodec& video_codec,
                                           WebRtc_Word8* pl_type);

  virtual WebRtc_Word32 DeRegisterReceivePayload(
      const WebRtc_Word8 payload_type);

  // Register RTP header extension.
  virtual WebRtc_Word32 RegisterReceiveRtpHeaderExtension(
      const RTPExtensionType type,
      const WebRtc_UWord8 id);

  virtual WebRtc_Word32 DeregisterReceiveRtpHeaderExtension(
      const RTPExtensionType type);

  // Get the currently configured SSRC filter.
  virtual WebRtc_Word32 SSRCFilter(WebRtc_UWord32& allowed_ssrc) const;

  // Set a SSRC to be used as a filter for incoming RTP streams.
  virtual WebRtc_Word32 SetSSRCFilter(const bool enable,
                                      const WebRtc_UWord32 allowed_ssrc);

  // Get last received remote timestamp.
  virtual WebRtc_UWord32 RemoteTimestamp() const;

  // Get the local time of the last received remote timestamp.
  virtual int64_t LocalTimeOfRemoteTimeStamp() const;

  // Get the current estimated remote timestamp.
  virtual WebRtc_Word32 EstimatedRemoteTimeStamp(
      WebRtc_UWord32& timestamp) const;

  virtual WebRtc_UWord32 RemoteSSRC() const;

  virtual WebRtc_Word32 RemoteCSRCs(
      WebRtc_UWord32 arr_of_csrc[kRtpCsrcSize]) const;

  virtual WebRtc_Word32 SetRTXReceiveStatus(const bool enable,
                                            const WebRtc_UWord32 ssrc);

  virtual WebRtc_Word32 RTXReceiveStatus(bool* enable,
                                         WebRtc_UWord32* ssrc) const;

  // Called by the network module when we receive a packet.
  virtual WebRtc_Word32 IncomingPacket(const WebRtc_UWord8* incoming_packet,
                                       const WebRtc_UWord16 packet_length);

  // Sender part.

  virtual WebRtc_Word32 RegisterSendPayload(const CodecInst& voice_codec);

  virtual WebRtc_Word32 RegisterSendPayload(const VideoCodec& video_codec);

  virtual WebRtc_Word32 DeRegisterSendPayload(const WebRtc_Word8 payload_type);

  virtual WebRtc_Word8 SendPayloadType() const;

  // Register RTP header extension.
  virtual WebRtc_Word32 RegisterSendRtpHeaderExtension(
      const RTPExtensionType type,
      const WebRtc_UWord8 id);

  virtual WebRtc_Word32 DeregisterSendRtpHeaderExtension(
      const RTPExtensionType type);

  // Get start timestamp.
  virtual WebRtc_UWord32 StartTimestamp() const;

  // Configure start timestamp, default is a random number.
  virtual WebRtc_Word32 SetStartTimestamp(const WebRtc_UWord32 timestamp);

  virtual WebRtc_UWord16 SequenceNumber() const;

  // Set SequenceNumber, default is a random number.
  virtual WebRtc_Word32 SetSequenceNumber(const WebRtc_UWord16 seq);

  virtual WebRtc_UWord32 SSRC() const;

  // Configure SSRC, default is a random number.
  virtual WebRtc_Word32 SetSSRC(const WebRtc_UWord32 ssrc);

  virtual WebRtc_Word32 CSRCs(WebRtc_UWord32 arr_of_csrc[kRtpCsrcSize]) const;

  virtual WebRtc_Word32 SetCSRCs(const WebRtc_UWord32 arr_of_csrc[kRtpCsrcSize],
                                 const WebRtc_UWord8 arr_length);

  virtual WebRtc_Word32 SetCSRCStatus(const bool include);

  virtual WebRtc_UWord32 PacketCountSent() const;

  virtual int CurrentSendFrequencyHz() const;

  virtual WebRtc_UWord32 ByteCountSent() const;

  virtual WebRtc_Word32 SetRTXSendStatus(const bool enable,
                                         const bool set_ssrc,
                                         const WebRtc_UWord32 ssrc);

  virtual WebRtc_Word32 RTXSendStatus(bool* enable,
                                      WebRtc_UWord32* ssrc) const;

  // Sends kRtcpByeCode when going from true to false.
  virtual WebRtc_Word32 SetSendingStatus(const bool sending);

  virtual bool Sending() const;

  // Drops or relays media packets.
  virtual WebRtc_Word32 SetSendingMediaStatus(const bool sending);

  virtual bool SendingMedia() const;

  // Used by the codec module to deliver a video or audio frame for
  // packetization.
  virtual WebRtc_Word32 SendOutgoingData(
      const FrameType frame_type,
      const WebRtc_Word8 payload_type,
      const WebRtc_UWord32 time_stamp,
      int64_t capture_time_ms,
      const WebRtc_UWord8* payload_data,
      const WebRtc_UWord32 payload_size,
      const RTPFragmentationHeader* fragmentation = NULL,
      const RTPVideoHeader* rtp_video_hdr = NULL);

  virtual void TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                                int64_t capture_time_ms);
  // RTCP part.

  // Get RTCP status.
  virtual RTCPMethod RTCP() const;

  // Configure RTCP status i.e on/off.
  virtual WebRtc_Word32 SetRTCPStatus(const RTCPMethod method);

  // Set RTCP CName.
  virtual WebRtc_Word32 SetCNAME(const char c_name[RTCP_CNAME_SIZE]);

  // Get RTCP CName.
  virtual WebRtc_Word32 CNAME(char c_name[RTCP_CNAME_SIZE]);

  // Get remote CName.
  virtual WebRtc_Word32 RemoteCNAME(const WebRtc_UWord32 remote_ssrc,
                                    char c_name[RTCP_CNAME_SIZE]) const;

  // Get remote NTP.
  virtual WebRtc_Word32 RemoteNTP(WebRtc_UWord32* received_ntp_secs,
                                  WebRtc_UWord32* received_ntp_frac,
                                  WebRtc_UWord32* rtcp_arrival_time_secs,
                                  WebRtc_UWord32* rtcp_arrival_time_frac,
                                  WebRtc_UWord32* rtcp_timestamp) const;

  virtual WebRtc_Word32 AddMixedCNAME(const WebRtc_UWord32 ssrc,
                                      const char c_name[RTCP_CNAME_SIZE]);

  virtual WebRtc_Word32 RemoveMixedCNAME(const WebRtc_UWord32 ssrc);

  // Get RoundTripTime.
  virtual WebRtc_Word32 RTT(const WebRtc_UWord32 remote_ssrc,
                            WebRtc_UWord16* rtt,
                            WebRtc_UWord16* avg_rtt,
                            WebRtc_UWord16* min_rtt,
                            WebRtc_UWord16* max_rtt) const;

  // Reset RoundTripTime statistics.
  virtual WebRtc_Word32 ResetRTT(const WebRtc_UWord32 remote_ssrc);

  virtual void SetRtt(uint32_t rtt);

  // Force a send of an RTCP packet.
  // Normal SR and RR are triggered via the process function.
  virtual WebRtc_Word32 SendRTCP(WebRtc_UWord32 rtcp_packet_type = kRtcpReport);

  // Statistics of our locally created statistics of the received RTP stream.
  virtual WebRtc_Word32 StatisticsRTP(WebRtc_UWord8*  fraction_lost,
                                      WebRtc_UWord32* cum_lost,
                                      WebRtc_UWord32* ext_max,
                                      WebRtc_UWord32* jitter,
                                      WebRtc_UWord32* max_jitter = NULL) const;

  // Reset RTP statistics.
  virtual WebRtc_Word32 ResetStatisticsRTP();

  virtual WebRtc_Word32 ResetReceiveDataCountersRTP();

  virtual WebRtc_Word32 ResetSendDataCountersRTP();

  // Statistics of the amount of data sent and received.
  virtual WebRtc_Word32 DataCountersRTP(WebRtc_UWord32* bytes_sent,
                                        WebRtc_UWord32* packets_sent,
                                        WebRtc_UWord32* bytes_received,
                                        WebRtc_UWord32* packets_received) const;

  virtual WebRtc_Word32 ReportBlockStatistics(
      WebRtc_UWord8* fraction_lost,
      WebRtc_UWord32* cum_lost,
      WebRtc_UWord32* ext_max,
      WebRtc_UWord32* jitter,
      WebRtc_UWord32* jitter_transmission_time_offset);

  // Get received RTCP report, sender info.
  virtual WebRtc_Word32 RemoteRTCPStat(RTCPSenderInfo* sender_info);

  // Get received RTCP report, report block.
  virtual WebRtc_Word32 RemoteRTCPStat(
      std::vector<RTCPReportBlock>* receive_blocks) const;

  // Set received RTCP report block.
  virtual WebRtc_Word32 AddRTCPReportBlock(
    const WebRtc_UWord32 ssrc, const RTCPReportBlock* receive_block);

  virtual WebRtc_Word32 RemoveRTCPReportBlock(const WebRtc_UWord32 ssrc);

  // (REMB) Receiver Estimated Max Bitrate.
  virtual bool REMB() const;

  virtual WebRtc_Word32 SetREMBStatus(const bool enable);

  virtual WebRtc_Word32 SetREMBData(const WebRtc_UWord32 bitrate,
                                    const WebRtc_UWord8 number_of_ssrc,
                                    const WebRtc_UWord32* ssrc);

  // (IJ) Extended jitter report.
  virtual bool IJ() const;

  virtual WebRtc_Word32 SetIJStatus(const bool enable);

  // (TMMBR) Temporary Max Media Bit Rate.
  virtual bool TMMBR() const;

  virtual WebRtc_Word32 SetTMMBRStatus(const bool enable);

  WebRtc_Word32 SetTMMBN(const TMMBRSet* bounding_set);

  virtual WebRtc_UWord16 MaxPayloadLength() const;

  virtual WebRtc_UWord16 MaxDataPayloadLength() const;

  virtual WebRtc_Word32 SetMaxTransferUnit(const WebRtc_UWord16 size);

  virtual WebRtc_Word32 SetTransportOverhead(
      const bool tcp,
      const bool ipv6,
      const WebRtc_UWord8 authentication_overhead = 0);

  // (NACK) Negative acknowledgment part.

  // Is Negative acknowledgment requests on/off?
  virtual NACKMethod NACK() const;

  // Turn negative acknowledgment requests on/off.
  virtual WebRtc_Word32 SetNACKStatus(const NACKMethod method);

  virtual int SelectiveRetransmissions() const;

  virtual int SetSelectiveRetransmissions(uint8_t settings);

  // Send a Negative acknowledgment packet.
  virtual WebRtc_Word32 SendNACK(const WebRtc_UWord16* nack_list,
                                 const WebRtc_UWord16 size);

  // Store the sent packets, needed to answer to a negative acknowledgment
  // requests.
  virtual WebRtc_Word32 SetStorePacketsStatus(
      const bool enable, const WebRtc_UWord16 number_to_store = 200);

  // (APP) Application specific data.
  virtual WebRtc_Word32 SetRTCPApplicationSpecificData(
      const WebRtc_UWord8 sub_type,
      const WebRtc_UWord32 name,
      const WebRtc_UWord8* data,
      const WebRtc_UWord16 length);

  // (XR) VOIP metric.
  virtual WebRtc_Word32 SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric);

  // Audio part.

  // Set audio packet size, used to determine when it's time to send a DTMF
  // packet in silence (CNG).
  virtual WebRtc_Word32 SetAudioPacketSize(
      const WebRtc_UWord16 packet_size_samples);

  // Outband DTMF detection.
  virtual WebRtc_Word32 SetTelephoneEventStatus(
      const bool enable,
      const bool forward_to_decoder,
      const bool detect_end_of_tone = false);

  // Is outband DTMF turned on/off?
  virtual bool TelephoneEvent() const;

  // Is forwarding of outband telephone events turned on/off?
  virtual bool TelephoneEventForwardToDecoder() const;

  virtual bool SendTelephoneEventActive(WebRtc_Word8& telephone_event) const;

  // Send a TelephoneEvent tone using RFC 2833 (4733).
  virtual WebRtc_Word32 SendTelephoneEventOutband(const WebRtc_UWord8 key,
                                                  const WebRtc_UWord16 time_ms,
                                                  const WebRtc_UWord8 level);

  // Set payload type for Redundant Audio Data RFC 2198.
  virtual WebRtc_Word32 SetSendREDPayloadType(const WebRtc_Word8 payload_type);

  // Get payload type for Redundant Audio Data RFC 2198.
  virtual WebRtc_Word32 SendREDPayloadType(WebRtc_Word8& payload_type) const;

  // Set status and id for header-extension-for-audio-level-indication.
  virtual WebRtc_Word32 SetRTPAudioLevelIndicationStatus(
      const bool enable, const WebRtc_UWord8 id);

  // Get status and id for header-extension-for-audio-level-indication.
  virtual WebRtc_Word32 GetRTPAudioLevelIndicationStatus(
      bool& enable, WebRtc_UWord8& id) const;

  // Store the audio level in d_bov for header-extension-for-audio-level-
  // indication.
  virtual WebRtc_Word32 SetAudioLevel(const WebRtc_UWord8 level_d_bov);

  // Video part.

  virtual RtpVideoCodecTypes ReceivedVideoCodec() const;

  virtual RtpVideoCodecTypes SendVideoCodec() const;

  virtual WebRtc_Word32 SendRTCPSliceLossIndication(
      const WebRtc_UWord8 picture_id);

  // Set method for requestion a new key frame.
  virtual WebRtc_Word32 SetKeyFrameRequestMethod(
      const KeyFrameRequestMethod method);

  // Send a request for a keyframe.
  virtual WebRtc_Word32 RequestKeyFrame();

  virtual WebRtc_Word32 SetCameraDelay(const WebRtc_Word32 delay_ms);

  virtual void SetTargetSendBitrate(const WebRtc_UWord32 bitrate);

  virtual WebRtc_Word32 SetGenericFECStatus(
      const bool enable,
      const WebRtc_UWord8 payload_type_red,
      const WebRtc_UWord8 payload_type_fec);

  virtual WebRtc_Word32 GenericFECStatus(
      bool& enable,
      WebRtc_UWord8& payload_type_red,
      WebRtc_UWord8& payload_type_fec);

  virtual WebRtc_Word32 SetFecParameters(
      const FecProtectionParams* delta_params,
      const FecProtectionParams* key_params);

  virtual WebRtc_Word32 LastReceivedNTP(WebRtc_UWord32& NTPsecs,
                                        WebRtc_UWord32& NTPfrac,
                                        WebRtc_UWord32& remote_sr);

  virtual WebRtc_Word32 BoundingSet(bool& tmmbr_owner,
                                    TMMBRSet*& bounding_set_rec);

  virtual void BitrateSent(WebRtc_UWord32* total_rate,
                           WebRtc_UWord32* video_rate,
                           WebRtc_UWord32* fec_rate,
                           WebRtc_UWord32* nackRate) const;

  virtual int EstimatedReceiveBandwidth(
      WebRtc_UWord32* available_bandwidth) const;

  virtual void SetRemoteSSRC(const WebRtc_UWord32 ssrc);

  virtual WebRtc_UWord32 SendTimeOfSendReport(const WebRtc_UWord32 send_report);

  // Good state of RTP receiver inform sender.
  virtual WebRtc_Word32 SendRTCPReferencePictureSelection(
      const WebRtc_UWord64 picture_id);

  void OnReceivedTMMBR();

  // Bad state of RTP receiver request a keyframe.
  void OnRequestIntraFrame();

  // Received a request for a new SLI.
  void OnReceivedSliceLossIndication(const WebRtc_UWord8 picture_id);

  // Received a new reference frame.
  void OnReceivedReferencePictureSelectionIndication(
      const WebRtc_UWord64 picture_id);

  void OnReceivedNACK(const WebRtc_UWord16 nack_sequence_numbers_length,
                      const WebRtc_UWord16* nack_sequence_numbers);

  void OnRequestSendReport();

 protected:
  void RegisterChildModule(RtpRtcp* module);

  void DeRegisterChildModule(RtpRtcp* module);

  bool UpdateRTCPReceiveInformationTimers();

  void ProcessDeadOrAliveTimer();

  WebRtc_UWord32 BitrateReceivedNow() const;

  // Get remote SequenceNumber.
  WebRtc_UWord16 RemoteSequenceNumber() const;

  // Only for internal testing.
  WebRtc_UWord32 LastSendReport(WebRtc_UWord32& last_rtcptime);

  RTPPayloadRegistry        rtp_payload_registry_;

  RTPSender                 rtp_sender_;
  scoped_ptr<RTPReceiver>   rtp_receiver_;

  RTCPSender                rtcp_sender_;
  RTCPReceiver              rtcp_receiver_;

  Clock*                    clock_;

 private:
  int64_t RtcpReportInterval();

  RTPReceiverAudio*         rtp_telephone_event_handler_;

  WebRtc_Word32             id_;
  const bool                audio_;
  bool                      collision_detected_;
  WebRtc_Word64             last_process_time_;
  WebRtc_Word64             last_bitrate_process_time_;
  WebRtc_Word64             last_packet_timeout_process_time_;
  WebRtc_UWord16            packet_overhead_;

  scoped_ptr<CriticalSectionWrapper> critical_section_module_ptrs_;
  scoped_ptr<CriticalSectionWrapper> critical_section_module_ptrs_feedback_;
  ModuleRtpRtcpImpl*            default_module_;
  std::list<ModuleRtpRtcpImpl*> child_modules_;

  // Dead or alive.
  bool                  dead_or_alive_active_;
  WebRtc_UWord32        dead_or_alive_timeout_ms_;
  WebRtc_Word64         dead_or_alive_last_timer_;
  // Send side
  NACKMethod            nack_method_;
  WebRtc_UWord32        nack_last_time_sent_;
  WebRtc_UWord16        nack_last_seq_number_sent_;

  bool                  simulcast_;
  VideoCodec            send_video_codec_;
  KeyFrameRequestMethod key_frame_req_method_;

  RemoteBitrateEstimator* remote_bitrate_;

  RtcpRttObserver* rtt_observer_;

#ifdef MATLAB
  MatlabPlot*           plot1_;
#endif
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
