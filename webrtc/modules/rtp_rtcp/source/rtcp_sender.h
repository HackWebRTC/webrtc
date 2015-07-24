/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_

#include <map>
#include <set>
#include <sstream>
#include <string>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ModuleRtpRtcpImpl;
class RTCPReceiver;

class NACKStringBuilder {
 public:
  NACKStringBuilder();
  ~NACKStringBuilder();

  void PushNACK(uint16_t nack);
  std::string GetResult();

 private:
  std::ostringstream stream_;
  int count_;
  uint16_t prevNack_;
  bool consecutive_;
};

class RTCPSender {
public:
 struct FeedbackState {
   FeedbackState();

   uint8_t send_payload_type;
   uint32_t frequency_hz;
   uint32_t packets_sent;
   size_t media_bytes_sent;
   uint32_t send_bitrate;

   uint32_t last_rr_ntp_secs;
   uint32_t last_rr_ntp_frac;
   uint32_t remote_sr;

   bool has_last_xr_rr;
   RtcpReceiveTimeInfo last_xr_rr;

   // Used when generating TMMBR.
   ModuleRtpRtcpImpl* module;
 };

 RTCPSender(int32_t id,
            bool audio,
            Clock* clock,
            ReceiveStatistics* receive_statistics,
            RtcpPacketTypeCounterObserver* packet_type_counter_observer);
 virtual ~RTCPSender();

 int32_t RegisterSendTransport(Transport* outgoingTransport);

 RTCPMethod Status() const;
 void SetRTCPStatus(RTCPMethod method);

 bool Sending() const;
 int32_t SetSendingStatus(const FeedbackState& feedback_state,
                          bool enabled);  // combine the functions

 int32_t SetNackStatus(bool enable);

 void SetStartTimestamp(uint32_t start_timestamp);

 void SetLastRtpTime(uint32_t rtp_timestamp, int64_t capture_time_ms);

 void SetSSRC(uint32_t ssrc);

 void SetRemoteSSRC(uint32_t ssrc);

 int32_t SetCNAME(const char* cName);

 int32_t AddMixedCNAME(uint32_t SSRC, const char* c_name);

 int32_t RemoveMixedCNAME(uint32_t SSRC);

 int64_t SendTimeOfSendReport(uint32_t sendReport);

 bool SendTimeOfXrRrReport(uint32_t mid_ntp, int64_t* time_ms) const;

 bool TimeToSendRTCPReport(bool sendKeyframeBeforeRTP = false) const;

 int32_t SendRTCP(const FeedbackState& feedback_state,
                  RTCPPacketType packetType,
                  int32_t nackSize = 0,
                  const uint16_t* nackList = 0,
                  bool repeat = false,
                  uint64_t pictureID = 0);

 int32_t SendCompoundRTCP(const FeedbackState& feedback_state,
                          const std::set<RTCPPacketType>& packetTypes,
                          int32_t nackSize = 0,
                          const uint16_t* nackList = 0,
                          bool repeat = false,
                          uint64_t pictureID = 0);

 bool REMB() const;

 void SetREMBStatus(bool enable);

 void SetREMBData(uint32_t bitrate, const std::vector<uint32_t>& ssrcs);

 bool TMMBR() const;

 void SetTMMBRStatus(bool enable);

 int32_t SetTMMBN(const TMMBRSet* boundingSet, uint32_t maxBitrateKbit);

 int32_t SetApplicationSpecificData(uint8_t subType,
                                    uint32_t name,
                                    const uint8_t* data,
                                    uint16_t length);
 int32_t SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric);

 void SendRtcpXrReceiverReferenceTime(bool enable);

 bool RtcpXrReceiverReferenceTime() const;

 void SetCsrcs(const std::vector<uint32_t>& csrcs);

 void SetTargetBitrate(unsigned int target_bitrate);

private:
 struct RtcpContext;

 // The BuildResult indicates the outcome of a call to a builder method,
 // constructing a part of an RTCP packet:
 //
 // kError
 //   Building RTCP packet failed, propagate error out to caller.
 // kAbort
 //   The (partial) block being build should not be included. Reset current
 //   buffer position to the state before the method call and proceed to the
 //   next packet type.
 // kTruncated
 //   There is not enough room in the buffer to fit the data being constructed.
 //   (IP packet is full). Proceed to the next packet type, and call this
 //   method again when a new buffer has been allocated.
 // TODO(sprang): Actually allocate multiple packets if needed.
 // kSuccess
 //   Data has been successfully placed in the buffer.

 enum class BuildResult { kError, kAborted, kTruncated, kSuccess };

 int32_t SendToNetwork(const uint8_t* dataBuffer, size_t length);

 int32_t AddReportBlock(const RTCPReportBlock& report_block)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);

 bool PrepareReport(const FeedbackState& feedback_state,
                    uint32_t ssrc,
                    StreamStatistician* statistician,
                    RTCPReportBlock* report_block);

 int PrepareRTCP(const FeedbackState& feedback_state,
                 const std::set<RTCPPacketType>& packetTypes,
                 int32_t nackSize,
                 const uint16_t* nackList,
                 bool repeat,
                 uint64_t pictureID,
                 uint8_t* rtcp_buffer,
                 int buffer_size);

 BuildResult BuildSR(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildRR(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildSDES(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildPLI(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildREMB(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildTMMBR(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildTMMBN(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildAPP(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildVoIPMetric(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildBYE(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildFIR(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildSLI(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildRPSI(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildNACK(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildReceiverReferenceTime(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 BuildResult BuildDlrr(RtcpContext* context)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);

private:
 const int32_t id_;
 const bool audio_;
 Clock* const clock_;
 RTCPMethod method_ GUARDED_BY(critical_section_rtcp_sender_);

 rtc::scoped_ptr<CriticalSectionWrapper> critical_section_transport_;
 Transport* cbTransport_ GUARDED_BY(critical_section_transport_);

 rtc::scoped_ptr<CriticalSectionWrapper> critical_section_rtcp_sender_;
 bool using_nack_ GUARDED_BY(critical_section_rtcp_sender_);
 bool sending_ GUARDED_BY(critical_section_rtcp_sender_);
 bool remb_enabled_ GUARDED_BY(critical_section_rtcp_sender_);

 int64_t next_time_to_send_rtcp_ GUARDED_BY(critical_section_rtcp_sender_);

 uint32_t start_timestamp_ GUARDED_BY(critical_section_rtcp_sender_);
 uint32_t last_rtp_timestamp_ GUARDED_BY(critical_section_rtcp_sender_);
 int64_t last_frame_capture_time_ms_ GUARDED_BY(critical_section_rtcp_sender_);
 uint32_t ssrc_ GUARDED_BY(critical_section_rtcp_sender_);
 // SSRC that we receive on our RTP channel
 uint32_t remote_ssrc_ GUARDED_BY(critical_section_rtcp_sender_);
 std::string cname_ GUARDED_BY(critical_section_rtcp_sender_);

 ReceiveStatistics* receive_statistics_
     GUARDED_BY(critical_section_rtcp_sender_);
 std::map<uint32_t, rtcp::ReportBlock> report_blocks_
     GUARDED_BY(critical_section_rtcp_sender_);
 std::map<uint32_t, std::string> csrc_cnames_
     GUARDED_BY(critical_section_rtcp_sender_);

 // Sent
 uint32_t last_send_report_[RTCP_NUMBER_OF_SR] GUARDED_BY(
     critical_section_rtcp_sender_);  // allow packet loss and RTT above 1 sec
 int64_t last_rtcp_time_[RTCP_NUMBER_OF_SR] GUARDED_BY(
     critical_section_rtcp_sender_);

 // Sent XR receiver reference time report.
 // <mid ntp (mid 32 bits of the 64 bits NTP timestamp), send time in ms>.
 std::map<uint32_t, int64_t> last_xr_rr_
     GUARDED_BY(critical_section_rtcp_sender_);

 // send CSRCs
 std::vector<uint32_t> csrcs_ GUARDED_BY(critical_section_rtcp_sender_);

 // Full intra request
 uint8_t sequence_number_fir_ GUARDED_BY(critical_section_rtcp_sender_);

 // REMB
 uint32_t remb_bitrate_ GUARDED_BY(critical_section_rtcp_sender_);
 std::vector<uint32_t> remb_ssrcs_ GUARDED_BY(critical_section_rtcp_sender_);

 TMMBRHelp tmmbr_help_ GUARDED_BY(critical_section_rtcp_sender_);
 uint32_t tmmbr_send_ GUARDED_BY(critical_section_rtcp_sender_);
 uint32_t packet_oh_send_ GUARDED_BY(critical_section_rtcp_sender_);

 // APP
 uint8_t app_sub_type_ GUARDED_BY(critical_section_rtcp_sender_);
 uint32_t app_name_ GUARDED_BY(critical_section_rtcp_sender_);
 rtc::scoped_ptr<uint8_t[]> app_data_ GUARDED_BY(critical_section_rtcp_sender_);
 uint16_t app_length_ GUARDED_BY(critical_section_rtcp_sender_);

 // True if sending of XR Receiver reference time report is enabled.
 bool xr_send_receiver_reference_time_enabled_
     GUARDED_BY(critical_section_rtcp_sender_);

 // XR VoIP metric
 RTCPVoIPMetric xr_voip_metric_ GUARDED_BY(critical_section_rtcp_sender_);

 RtcpPacketTypeCounterObserver* const packet_type_counter_observer_;
 RtcpPacketTypeCounter packet_type_counter_
     GUARDED_BY(critical_section_rtcp_sender_);

 RTCPUtility::NackStats nack_stats_ GUARDED_BY(critical_section_rtcp_sender_);

 void SetFlag(RTCPPacketType type, bool is_volatile)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 void SetFlags(const std::set<RTCPPacketType>& types, bool is_volatile)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 bool IsFlagPresent(RTCPPacketType type) const
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 bool ConsumeFlag(RTCPPacketType type, bool forced = false)
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 bool AllVolatileFlagsConsumed() const
     EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
 struct ReportFlag {
   ReportFlag(RTCPPacketType type, bool is_volatile)
       : type(type), is_volatile(is_volatile) {}
   bool operator<(const ReportFlag& flag) const { return type < flag.type; }
   bool operator==(const ReportFlag& flag) const { return type == flag.type; }
   const RTCPPacketType type;
   const bool is_volatile;
 };

 std::set<ReportFlag> report_flags_ GUARDED_BY(critical_section_rtcp_sender_);

 typedef BuildResult (RTCPSender::*Builder)(RtcpContext*);
 std::map<RTCPPacketType, Builder> builders_;

 class PacketBuiltCallback;
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
