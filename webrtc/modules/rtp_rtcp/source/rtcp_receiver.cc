/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"

#include <assert.h>
#include <string.h>

#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/time_util.h"
#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"
#include "webrtc/system_wrappers/include/ntp_time.h"

namespace webrtc {
using RTCPHelp::RTCPPacketInformation;
using RTCPHelp::RTCPReceiveInformation;
using RTCPHelp::RTCPReportBlockInformation;
using RTCPUtility::kBtVoipMetric;
using RTCPUtility::RTCPCnameInformation;
using RTCPUtility::RTCPPacketReportBlockItem;
using RTCPUtility::RTCPPacketTypes;

// The number of RTCP time intervals needed to trigger a timeout.
const int kRrTimeoutIntervals = 3;

const int64_t kMaxWarningLogIntervalMs = 10000;

RTCPReceiver::RTCPReceiver(
    Clock* clock,
    bool receiver_only,
    RtcpPacketTypeCounterObserver* packet_type_counter_observer,
    RtcpBandwidthObserver* rtcp_bandwidth_observer,
    RtcpIntraFrameObserver* rtcp_intra_frame_observer,
    TransportFeedbackObserver* transport_feedback_observer,
    ModuleRtpRtcp* owner)
    : _clock(clock),
      receiver_only_(receiver_only),
      _lastReceived(0),
      _rtpRtcp(*owner),
      _cbRtcpBandwidthObserver(rtcp_bandwidth_observer),
      _cbRtcpIntraFrameObserver(rtcp_intra_frame_observer),
      _cbTransportFeedbackObserver(transport_feedback_observer),
      main_ssrc_(0),
      _remoteSSRC(0),
      _remoteSenderInfo(),
      _lastReceivedSRNTPsecs(0),
      _lastReceivedSRNTPfrac(0),
      _lastReceivedXRNTPsecs(0),
      _lastReceivedXRNTPfrac(0),
      xr_rrtr_status_(false),
      xr_rr_rtt_ms_(0),
      _receivedInfoMap(),
      _lastReceivedRrMs(0),
      _lastIncreasedSequenceNumberMs(0),
      stats_callback_(NULL),
      packet_type_counter_observer_(packet_type_counter_observer),
      num_skipped_packets_(0),
      last_skipped_packets_warning_(clock->TimeInMilliseconds()) {
  memset(&_remoteSenderInfo, 0, sizeof(_remoteSenderInfo));
}

RTCPReceiver::~RTCPReceiver() {
  ReportBlockMap::iterator it = _receivedReportBlockMap.begin();
  for (; it != _receivedReportBlockMap.end(); ++it) {
    ReportBlockInfoMap* info_map = &(it->second);
    while (!info_map->empty()) {
      ReportBlockInfoMap::iterator it_info = info_map->begin();
      delete it_info->second;
      info_map->erase(it_info);
    }
  }
  while (!_receivedInfoMap.empty()) {
    std::map<uint32_t, RTCPReceiveInformation*>::iterator first =
        _receivedInfoMap.begin();
    delete first->second;
    _receivedInfoMap.erase(first);
  }
  while (!_receivedCnameMap.empty()) {
    std::map<uint32_t, RTCPCnameInformation*>::iterator first =
        _receivedCnameMap.begin();
    delete first->second;
    _receivedCnameMap.erase(first);
  }
}

bool RTCPReceiver::IncomingPacket(const uint8_t* packet, size_t packet_size) {
  // Allow receive of non-compound RTCP packets.
  RTCPUtility::RTCPParserV2 rtcp_parser(packet, packet_size, true);

  if (!rtcp_parser.IsValid()) {
    LOG(LS_WARNING) << "Incoming invalid RTCP packet";
    return false;
  }
  RTCPHelp::RTCPPacketInformation rtcp_packet_information;
  IncomingRTCPPacket(rtcp_packet_information, &rtcp_parser);
  TriggerCallbacksFromRTCPPacket(rtcp_packet_information);
  return true;
}

int64_t RTCPReceiver::LastReceived() {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  return _lastReceived;
}

int64_t RTCPReceiver::LastReceivedReceiverReport() const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  int64_t last_received_rr = -1;
  for (ReceivedInfoMap::const_iterator it = _receivedInfoMap.begin();
       it != _receivedInfoMap.end(); ++it) {
    if (it->second->last_time_received_ms > last_received_rr) {
      last_received_rr = it->second->last_time_received_ms;
    }
  }
  return last_received_rr;
}

void RTCPReceiver::SetRemoteSSRC(uint32_t ssrc) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  // new SSRC reset old reports
  memset(&_remoteSenderInfo, 0, sizeof(_remoteSenderInfo));
  _lastReceivedSRNTPsecs = 0;
  _lastReceivedSRNTPfrac = 0;

  _remoteSSRC = ssrc;
}

uint32_t RTCPReceiver::RemoteSSRC() const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  return _remoteSSRC;
}

void RTCPReceiver::SetSsrcs(uint32_t main_ssrc,
                            const std::set<uint32_t>& registered_ssrcs) {
  uint32_t old_ssrc = 0;
  {
    rtc::CritScope lock(&_criticalSectionRTCPReceiver);
    old_ssrc = main_ssrc_;
    main_ssrc_ = main_ssrc;
    registered_ssrcs_ = registered_ssrcs;
  }
  {
    if (_cbRtcpIntraFrameObserver && old_ssrc != main_ssrc) {
      _cbRtcpIntraFrameObserver->OnLocalSsrcChanged(old_ssrc, main_ssrc);
    }
  }
}

int32_t RTCPReceiver::RTT(uint32_t remoteSSRC,
                          int64_t* RTT,
                          int64_t* avgRTT,
                          int64_t* minRTT,
                          int64_t* maxRTT) const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  RTCPReportBlockInformation* reportBlock =
      GetReportBlockInformation(remoteSSRC, main_ssrc_);

  if (reportBlock == NULL) {
    return -1;
  }
  if (RTT) {
    *RTT = reportBlock->RTT;
  }
  if (avgRTT) {
    *avgRTT = reportBlock->avgRTT;
  }
  if (minRTT) {
    *minRTT = reportBlock->minRTT;
  }
  if (maxRTT) {
    *maxRTT = reportBlock->maxRTT;
  }
  return 0;
}

void RTCPReceiver::SetRtcpXrRrtrStatus(bool enable) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  xr_rrtr_status_ = enable;
}

bool RTCPReceiver::GetAndResetXrRrRtt(int64_t* rtt_ms) {
  assert(rtt_ms);
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (xr_rr_rtt_ms_ == 0) {
    return false;
  }
  *rtt_ms = xr_rr_rtt_ms_;
  xr_rr_rtt_ms_ = 0;
  return true;
}

// TODO(pbos): Make this fail when we haven't received NTP.
bool RTCPReceiver::NTP(uint32_t* ReceivedNTPsecs,
                       uint32_t* ReceivedNTPfrac,
                       uint32_t* RTCPArrivalTimeSecs,
                       uint32_t* RTCPArrivalTimeFrac,
                       uint32_t* rtcp_timestamp) const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (ReceivedNTPsecs) {
    *ReceivedNTPsecs =
        _remoteSenderInfo.NTPseconds;  // NTP from incoming SendReport
  }
  if (ReceivedNTPfrac) {
    *ReceivedNTPfrac = _remoteSenderInfo.NTPfraction;
  }
  if (RTCPArrivalTimeFrac) {
    *RTCPArrivalTimeFrac = _lastReceivedSRNTPfrac;  // local NTP time when we
                                                    // received a RTCP packet
                                                    // with a send block
  }
  if (RTCPArrivalTimeSecs) {
    *RTCPArrivalTimeSecs = _lastReceivedSRNTPsecs;
  }
  if (rtcp_timestamp) {
    *rtcp_timestamp = _remoteSenderInfo.RTPtimeStamp;
  }
  return true;
}

bool RTCPReceiver::LastReceivedXrReferenceTimeInfo(
    RtcpReceiveTimeInfo* info) const {
  assert(info);
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (_lastReceivedXRNTPsecs == 0 && _lastReceivedXRNTPfrac == 0) {
    return false;
  }

  info->sourceSSRC = _remoteXRReceiveTimeInfo.sourceSSRC;
  info->lastRR = _remoteXRReceiveTimeInfo.lastRR;

  // Get the delay since last received report (RFC 3611).
  uint32_t receive_time =
      RTCPUtility::MidNtp(_lastReceivedXRNTPsecs, _lastReceivedXRNTPfrac);

  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 0;
  _clock->CurrentNtp(ntp_sec, ntp_frac);
  uint32_t now = RTCPUtility::MidNtp(ntp_sec, ntp_frac);

  info->delaySinceLastRR = now - receive_time;
  return true;
}

int32_t RTCPReceiver::SenderInfoReceived(RTCPSenderInfo* senderInfo) const {
  assert(senderInfo);
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (_lastReceivedSRNTPsecs == 0) {
    return -1;
  }
  memcpy(senderInfo, &(_remoteSenderInfo), sizeof(RTCPSenderInfo));
  return 0;
}

// statistics
// we can get multiple receive reports when we receive the report from a CE
int32_t RTCPReceiver::StatisticsReceived(
    std::vector<RTCPReportBlock>* receiveBlocks) const {
  assert(receiveBlocks);
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  ReportBlockMap::const_iterator it = _receivedReportBlockMap.begin();
  for (; it != _receivedReportBlockMap.end(); ++it) {
    const ReportBlockInfoMap* info_map = &(it->second);
    ReportBlockInfoMap::const_iterator it_info = info_map->begin();
    for (; it_info != info_map->end(); ++it_info) {
      receiveBlocks->push_back(it_info->second->remoteReceiveBlock);
    }
  }
  return 0;
}

int32_t RTCPReceiver::IncomingRTCPPacket(
    RTCPPacketInformation& rtcpPacketInformation,
    RTCPUtility::RTCPParserV2* rtcpParser) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  _lastReceived = _clock->TimeInMilliseconds();

  if (packet_type_counter_.first_packet_time_ms == -1) {
    packet_type_counter_.first_packet_time_ms = _lastReceived;
  }

  RTCPUtility::RTCPPacketTypes pktType = rtcpParser->Begin();
  while (pktType != RTCPPacketTypes::kInvalid) {
    // Each "case" is responsible for iterate the parser to the
    // next top level packet.
    switch (pktType) {
      case RTCPPacketTypes::kSr:
        HandleSenderReport(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kRr:
        HandleReceiverReport(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kSdes:
        HandleSDES(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kXrHeader:
        HandleXrHeader(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kXrReceiverReferenceTime:
        HandleXrReceiveReferenceTime(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kXrDlrrReportBlock:
        HandleXrDlrrReportBlock(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kBye:
        HandleBYE(*rtcpParser);
        break;
      case RTCPPacketTypes::kRtpfbNack:
        HandleNACK(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kRtpfbTmmbr:
        HandleTMMBR(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kRtpfbTmmbn:
        HandleTMMBN(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kRtpfbSrReq:
        HandleSR_REQ(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kPsfbPli:
        HandlePLI(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kPsfbSli:
        HandleSLI(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kPsfbRpsi:
        HandleRPSI(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kPsfbFir:
        HandleFIR(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kPsfbApp:
        HandlePsfbApp(*rtcpParser, rtcpPacketInformation);
        break;
      case RTCPPacketTypes::kTransportFeedback:
        HandleTransportFeedback(rtcpParser, &rtcpPacketInformation);
        break;
      default:
        rtcpParser->Iterate();
        break;
    }
    pktType = rtcpParser->PacketType();
  }

  if (packet_type_counter_observer_ != NULL) {
    packet_type_counter_observer_->RtcpPacketTypesCounterUpdated(
        main_ssrc_, packet_type_counter_);
  }

  num_skipped_packets_ += rtcpParser->NumSkippedBlocks();

  int64_t now = _clock->TimeInMilliseconds();
  if (now - last_skipped_packets_warning_ >= kMaxWarningLogIntervalMs &&
      num_skipped_packets_ > 0) {
    last_skipped_packets_warning_ = now;
    LOG(LS_WARNING) << num_skipped_packets_
                    << " RTCP blocks were skipped due to being malformed or of "
                       "unrecognized/unsupported type, during the past "
                    << (kMaxWarningLogIntervalMs / 1000) << " second period.";
  }

  return 0;
}

void RTCPReceiver::HandleSenderReport(
    RTCPUtility::RTCPParserV2& rtcpParser,
    RTCPPacketInformation& rtcpPacketInformation) {
  RTCPUtility::RTCPPacketTypes rtcpPacketType = rtcpParser.PacketType();
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

  RTC_DCHECK(rtcpPacketType == RTCPPacketTypes::kSr);

  // SR.SenderSSRC
  // The synchronization source identifier for the originator of this SR packet

  const uint32_t remoteSSRC = rtcpPacket.SR.SenderSSRC;

  rtcpPacketInformation.remoteSSRC = remoteSSRC;

  RTCPReceiveInformation* ptrReceiveInfo = CreateReceiveInformation(remoteSSRC);
  if (!ptrReceiveInfo) {
    rtcpParser.Iterate();
    return;
  }

  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "SR",
                       "remote_ssrc", remoteSSRC, "ssrc", main_ssrc_);

  // Have I received RTP packets from this party?
  if (_remoteSSRC == remoteSSRC) {
    // Only signal that we have received a SR when we accept one.
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpSr;

    rtcpPacketInformation.ntp_secs = rtcpPacket.SR.NTPMostSignificant;
    rtcpPacketInformation.ntp_frac = rtcpPacket.SR.NTPLeastSignificant;
    rtcpPacketInformation.rtp_timestamp = rtcpPacket.SR.RTPTimestamp;

    // Save the NTP time of this report.
    _remoteSenderInfo.NTPseconds = rtcpPacket.SR.NTPMostSignificant;
    _remoteSenderInfo.NTPfraction = rtcpPacket.SR.NTPLeastSignificant;
    _remoteSenderInfo.RTPtimeStamp = rtcpPacket.SR.RTPTimestamp;
    _remoteSenderInfo.sendPacketCount = rtcpPacket.SR.SenderPacketCount;
    _remoteSenderInfo.sendOctetCount = rtcpPacket.SR.SenderOctetCount;

    _clock->CurrentNtp(_lastReceivedSRNTPsecs, _lastReceivedSRNTPfrac);
  } else {
    // We will only store the send report from one source, but
    // we will store all the receive blocks.
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRr;
  }
  // Update that this remote is alive.
  ptrReceiveInfo->last_time_received_ms = _clock->TimeInMilliseconds();

  rtcpPacketType = rtcpParser.Iterate();

  while (rtcpPacketType == RTCPPacketTypes::kReportBlockItem) {
    HandleReportBlock(rtcpPacket, rtcpPacketInformation, remoteSSRC);
    rtcpPacketType = rtcpParser.Iterate();
  }
}

void RTCPReceiver::HandleReceiverReport(
    RTCPUtility::RTCPParserV2& rtcpParser,
    RTCPPacketInformation& rtcpPacketInformation) {
  RTCPUtility::RTCPPacketTypes rtcpPacketType = rtcpParser.PacketType();
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

  RTC_DCHECK(rtcpPacketType == RTCPPacketTypes::kRr);

  // rtcpPacket.RR.SenderSSRC
  // The source of the packet sender, same as of SR? or is this a CE?
  const uint32_t remoteSSRC = rtcpPacket.RR.SenderSSRC;

  rtcpPacketInformation.remoteSSRC = remoteSSRC;

  RTCPReceiveInformation* ptrReceiveInfo = CreateReceiveInformation(remoteSSRC);
  if (!ptrReceiveInfo) {
    rtcpParser.Iterate();
    return;
  }

  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RR",
                       "remote_ssrc", remoteSSRC, "ssrc", main_ssrc_);

  rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRr;

  // Update that this remote is alive.
  ptrReceiveInfo->last_time_received_ms = _clock->TimeInMilliseconds();

  rtcpPacketType = rtcpParser.Iterate();

  while (rtcpPacketType == RTCPPacketTypes::kReportBlockItem) {
    HandleReportBlock(rtcpPacket, rtcpPacketInformation, remoteSSRC);
    rtcpPacketType = rtcpParser.Iterate();
  }
}

void RTCPReceiver::HandleReportBlock(
    const RTCPUtility::RTCPPacket& rtcpPacket,
    RTCPPacketInformation& rtcpPacketInformation,
    uint32_t remoteSSRC)
    EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPReceiver) {
  // This will be called once per report block in the RTCP packet.
  // We filter out all report blocks that are not for us.
  // Each packet has max 31 RR blocks.
  //
  // We can calc RTT if we send a send report and get a report block back.

  // |rtcpPacket.ReportBlockItem.SSRC| is the SSRC identifier of the source to
  // which the information in this reception report block pertains.

  // Filter out all report blocks that are not for us.
  if (registered_ssrcs_.find(rtcpPacket.ReportBlockItem.SSRC) ==
      registered_ssrcs_.end()) {
    // This block is not for us ignore it.
    return;
  }

  RTCPReportBlockInformation* reportBlock = CreateOrGetReportBlockInformation(
      remoteSSRC, rtcpPacket.ReportBlockItem.SSRC);
  if (reportBlock == NULL) {
    LOG(LS_WARNING) << "Failed to CreateReportBlockInformation(" << remoteSSRC
                    << ")";
    return;
  }

  _lastReceivedRrMs = _clock->TimeInMilliseconds();
  const RTCPPacketReportBlockItem& rb = rtcpPacket.ReportBlockItem;
  reportBlock->remoteReceiveBlock.remoteSSRC = remoteSSRC;
  reportBlock->remoteReceiveBlock.sourceSSRC = rb.SSRC;
  reportBlock->remoteReceiveBlock.fractionLost = rb.FractionLost;
  reportBlock->remoteReceiveBlock.cumulativeLost =
      rb.CumulativeNumOfPacketsLost;
  if (rb.ExtendedHighestSequenceNumber >
      reportBlock->remoteReceiveBlock.extendedHighSeqNum) {
    // We have successfully delivered new RTP packets to the remote side after
    // the last RR was sent from the remote side.
    _lastIncreasedSequenceNumberMs = _lastReceivedRrMs;
  }
  reportBlock->remoteReceiveBlock.extendedHighSeqNum =
      rb.ExtendedHighestSequenceNumber;
  reportBlock->remoteReceiveBlock.jitter = rb.Jitter;
  reportBlock->remoteReceiveBlock.delaySinceLastSR = rb.DelayLastSR;
  reportBlock->remoteReceiveBlock.lastSR = rb.LastSR;

  if (rtcpPacket.ReportBlockItem.Jitter > reportBlock->remoteMaxJitter) {
    reportBlock->remoteMaxJitter = rtcpPacket.ReportBlockItem.Jitter;
  }

  int64_t rtt = 0;
  uint32_t send_time = rtcpPacket.ReportBlockItem.LastSR;
  // RFC3550, section 6.4.1, LSR field discription states:
  // If no SR has been received yet, the field is set to zero.
  // Receiver rtp_rtcp module is not expected to calculate rtt using
  // Sender Reports even if it accidentally can.
  if (!receiver_only_ && send_time != 0) {
    uint32_t delay = rtcpPacket.ReportBlockItem.DelayLastSR;
    // Local NTP time.
    uint32_t receive_time = CompactNtp(NtpTime(*_clock));

    // RTT in 1/(2^16) seconds.
    uint32_t rtt_ntp = receive_time - delay - send_time;
    // Convert to 1/1000 seconds (milliseconds).
    rtt = CompactNtpRttToMs(rtt_ntp);
    if (rtt > reportBlock->maxRTT) {
      // Store max RTT.
      reportBlock->maxRTT = rtt;
    }
    if (reportBlock->minRTT == 0) {
      // First RTT.
      reportBlock->minRTT = rtt;
    } else if (rtt < reportBlock->minRTT) {
      // Store min RTT.
      reportBlock->minRTT = rtt;
    }
    // Store last RTT.
    reportBlock->RTT = rtt;

    // store average RTT
    if (reportBlock->numAverageCalcs != 0) {
      float ac = static_cast<float>(reportBlock->numAverageCalcs);
      float newAverage =
          ((ac / (ac + 1)) * reportBlock->avgRTT) + ((1 / (ac + 1)) * rtt);
      reportBlock->avgRTT = static_cast<int64_t>(newAverage + 0.5f);
    } else {
      // First RTT.
      reportBlock->avgRTT = rtt;
    }
    reportBlock->numAverageCalcs++;
  }

  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RR_RTT", rb.SSRC,
                    rtt);

  rtcpPacketInformation.AddReportInfo(*reportBlock);
}

RTCPReportBlockInformation* RTCPReceiver::CreateOrGetReportBlockInformation(
    uint32_t remote_ssrc,
    uint32_t source_ssrc) {
  RTCPReportBlockInformation* info =
      GetReportBlockInformation(remote_ssrc, source_ssrc);
  if (info == NULL) {
    info = new RTCPReportBlockInformation;
    _receivedReportBlockMap[source_ssrc][remote_ssrc] = info;
  }
  return info;
}

RTCPReportBlockInformation* RTCPReceiver::GetReportBlockInformation(
    uint32_t remote_ssrc,
    uint32_t source_ssrc) const {
  ReportBlockMap::const_iterator it = _receivedReportBlockMap.find(source_ssrc);
  if (it == _receivedReportBlockMap.end()) {
    return NULL;
  }
  const ReportBlockInfoMap* info_map = &(it->second);
  ReportBlockInfoMap::const_iterator it_info = info_map->find(remote_ssrc);
  if (it_info == info_map->end()) {
    return NULL;
  }
  return it_info->second;
}

RTCPCnameInformation* RTCPReceiver::CreateCnameInformation(
    uint32_t remoteSSRC) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPCnameInformation*>::iterator it =
      _receivedCnameMap.find(remoteSSRC);

  if (it != _receivedCnameMap.end()) {
    return it->second;
  }
  RTCPCnameInformation* cnameInfo = new RTCPCnameInformation;
  memset(cnameInfo->name, 0, RTCP_CNAME_SIZE);
  _receivedCnameMap[remoteSSRC] = cnameInfo;
  return cnameInfo;
}

RTCPCnameInformation* RTCPReceiver::GetCnameInformation(
    uint32_t remoteSSRC) const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPCnameInformation*>::const_iterator it =
      _receivedCnameMap.find(remoteSSRC);

  if (it == _receivedCnameMap.end()) {
    return NULL;
  }
  return it->second;
}

RTCPReceiveInformation* RTCPReceiver::CreateReceiveInformation(
    uint32_t remoteSSRC) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReceiveInformation*>::iterator it =
      _receivedInfoMap.find(remoteSSRC);

  if (it != _receivedInfoMap.end()) {
    return it->second;
  }
  RTCPReceiveInformation* receiveInfo = new RTCPReceiveInformation;
  _receivedInfoMap[remoteSSRC] = receiveInfo;
  return receiveInfo;
}

RTCPReceiveInformation* RTCPReceiver::GetReceiveInformation(
    uint32_t remoteSSRC) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReceiveInformation*>::iterator it =
      _receivedInfoMap.find(remoteSSRC);
  if (it == _receivedInfoMap.end()) {
    return NULL;
  }
  return it->second;
}

bool RTCPReceiver::RtcpRrTimeout(int64_t rtcp_interval_ms) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (_lastReceivedRrMs == 0)
    return false;

  int64_t time_out_ms = kRrTimeoutIntervals * rtcp_interval_ms;
  if (_clock->TimeInMilliseconds() > _lastReceivedRrMs + time_out_ms) {
    // Reset the timer to only trigger one log.
    _lastReceivedRrMs = 0;
    return true;
  }
  return false;
}

bool RTCPReceiver::RtcpRrSequenceNumberTimeout(int64_t rtcp_interval_ms) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  if (_lastIncreasedSequenceNumberMs == 0)
    return false;

  int64_t time_out_ms = kRrTimeoutIntervals * rtcp_interval_ms;
  if (_clock->TimeInMilliseconds() >
      _lastIncreasedSequenceNumberMs + time_out_ms) {
    // Reset the timer to only trigger one log.
    _lastIncreasedSequenceNumberMs = 0;
    return true;
  }
  return false;
}

bool RTCPReceiver::UpdateRTCPReceiveInformationTimers() {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  bool updateBoundingSet = false;
  int64_t timeNow = _clock->TimeInMilliseconds();

  std::map<uint32_t, RTCPReceiveInformation*>::iterator receiveInfoIt =
      _receivedInfoMap.begin();

  while (receiveInfoIt != _receivedInfoMap.end()) {
    RTCPReceiveInformation* receiveInfo = receiveInfoIt->second;
    if (receiveInfo == NULL) {
      return updateBoundingSet;
    }
    // time since last received rtcp packet
    // when we dont have a lastTimeReceived and the object is marked
    // readyForDelete it's removed from the map
    if (receiveInfo->last_time_received_ms > 0) {
      /// use audio define since we don't know what interval the remote peer is
      // using
      if ((timeNow - receiveInfo->last_time_received_ms) >
          5 * RTCP_INTERVAL_AUDIO_MS) {
        // no rtcp packet for the last five regular intervals, reset limitations
        receiveInfo->ClearTmmbr();
        // prevent that we call this over and over again
        receiveInfo->last_time_received_ms = 0;
        // send new TMMBN to all channels using the default codec
        updateBoundingSet = true;
      }
      receiveInfoIt++;
    } else if (receiveInfo->ready_for_delete) {
      // store our current receiveInfoItem
      std::map<uint32_t, RTCPReceiveInformation*>::iterator
          receiveInfoItemToBeErased = receiveInfoIt;
      receiveInfoIt++;
      delete receiveInfoItemToBeErased->second;
      _receivedInfoMap.erase(receiveInfoItemToBeErased);
    } else {
      receiveInfoIt++;
    }
  }
  return updateBoundingSet;
}

std::vector<rtcp::TmmbItem> RTCPReceiver::BoundingSet(bool* tmmbr_owner) {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReceiveInformation*>::iterator receiveInfoIt =
      _receivedInfoMap.find(_remoteSSRC);

  if (receiveInfoIt == _receivedInfoMap.end()) {
    return std::vector<rtcp::TmmbItem>();
  }
  RTCPReceiveInformation* receiveInfo = receiveInfoIt->second;
  RTC_DCHECK(receiveInfo);

  *tmmbr_owner = TMMBRHelp::IsOwner(receiveInfo->tmmbn, main_ssrc_);
  return receiveInfo->tmmbn;
}

void RTCPReceiver::HandleSDES(RTCPUtility::RTCPParserV2& rtcpParser,
                              RTCPPacketInformation& rtcpPacketInformation) {
  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  while (pktType == RTCPPacketTypes::kSdesChunk) {
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
    RTCPCnameInformation* cnameInfo =
        CreateCnameInformation(rtcpPacket.CName.SenderSSRC);
    RTC_DCHECK(cnameInfo);

    cnameInfo->name[RTCP_CNAME_SIZE - 1] = 0;
    strncpy(cnameInfo->name, rtcpPacket.CName.CName, RTCP_CNAME_SIZE - 1);
    {
      rtc::CritScope lock(&_criticalSectionFeedbacks);
      if (stats_callback_) {
        stats_callback_->CNameChanged(rtcpPacket.CName.CName,
                                      rtcpPacket.CName.SenderSSRC);
      }
    }

    pktType = rtcpParser.Iterate();
  }
  rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpSdes;
}

void RTCPReceiver::HandleNACK(RTCPUtility::RTCPParserV2& rtcpParser,
                              RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  if (receiver_only_ || main_ssrc_ != rtcpPacket.NACK.MediaSSRC) {
    // Not to us.
    rtcpParser.Iterate();
    return;
  }
  rtcpPacketInformation.ResetNACKPacketIdArray();

  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  while (pktType == RTCPPacketTypes::kRtpfbNackItem) {
    rtcpPacketInformation.AddNACKPacket(rtcpPacket.NACKItem.PacketID);
    nack_stats_.ReportRequest(rtcpPacket.NACKItem.PacketID);

    uint16_t bitMask = rtcpPacket.NACKItem.BitMask;
    if (bitMask) {
      for (int i = 1; i <= 16; ++i) {
        if (bitMask & 0x01) {
          rtcpPacketInformation.AddNACKPacket(rtcpPacket.NACKItem.PacketID + i);
          nack_stats_.ReportRequest(rtcpPacket.NACKItem.PacketID + i);
        }
        bitMask = bitMask >> 1;
      }
    }
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpNack;

    pktType = rtcpParser.Iterate();
  }

  if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpNack) {
    ++packet_type_counter_.nack_packets;
    packet_type_counter_.nack_requests = nack_stats_.requests();
    packet_type_counter_.unique_nack_requests = nack_stats_.unique_requests();
  }
}

void RTCPReceiver::HandleBYE(RTCPUtility::RTCPParserV2& rtcpParser) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

  // clear our lists
  ReportBlockMap::iterator it = _receivedReportBlockMap.begin();
  for (; it != _receivedReportBlockMap.end(); ++it) {
    ReportBlockInfoMap* info_map = &(it->second);
    ReportBlockInfoMap::iterator it_info =
        info_map->find(rtcpPacket.BYE.SenderSSRC);
    if (it_info != info_map->end()) {
      delete it_info->second;
      info_map->erase(it_info);
    }
  }

  //  we can't delete it due to TMMBR
  std::map<uint32_t, RTCPReceiveInformation*>::iterator receiveInfoIt =
      _receivedInfoMap.find(rtcpPacket.BYE.SenderSSRC);

  if (receiveInfoIt != _receivedInfoMap.end()) {
    receiveInfoIt->second->ready_for_delete = true;
  }

  std::map<uint32_t, RTCPCnameInformation*>::iterator cnameInfoIt =
      _receivedCnameMap.find(rtcpPacket.BYE.SenderSSRC);

  if (cnameInfoIt != _receivedCnameMap.end()) {
    delete cnameInfoIt->second;
    _receivedCnameMap.erase(cnameInfoIt);
  }
  xr_rr_rtt_ms_ = 0;
  rtcpParser.Iterate();
}

void RTCPReceiver::HandleXrHeader(
    RTCPUtility::RTCPParserV2& parser,
    RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& packet = parser.Packet();

  rtcpPacketInformation.xr_originator_ssrc = packet.XR.OriginatorSSRC;

  parser.Iterate();
}

void RTCPReceiver::HandleXrReceiveReferenceTime(
    RTCPUtility::RTCPParserV2& parser,
    RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& packet = parser.Packet();

  _remoteXRReceiveTimeInfo.sourceSSRC =
      rtcpPacketInformation.xr_originator_ssrc;

  _remoteXRReceiveTimeInfo.lastRR = RTCPUtility::MidNtp(
      packet.XRReceiverReferenceTimeItem.NTPMostSignificant,
      packet.XRReceiverReferenceTimeItem.NTPLeastSignificant);

  _clock->CurrentNtp(_lastReceivedXRNTPsecs, _lastReceivedXRNTPfrac);

  rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpXrReceiverReferenceTime;

  parser.Iterate();
}

void RTCPReceiver::HandleXrDlrrReportBlock(
    RTCPUtility::RTCPParserV2& parser,
    RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& packet = parser.Packet();
  // Iterate through sub-block(s), if any.
  RTCPUtility::RTCPPacketTypes packet_type = parser.Iterate();

  while (packet_type == RTCPPacketTypes::kXrDlrrReportBlockItem) {
    if (registered_ssrcs_.find(packet.XRDLRRReportBlockItem.SSRC) ==
        registered_ssrcs_.end()) {
      // Not to us.
      return;
    }

    rtcpPacketInformation.xr_dlrr_item = true;

    // Caller should explicitly enable rtt calculation using extended reports.
    if (!xr_rrtr_status_)
      return;

    // The send_time and delay_rr fields are in units of 1/2^16 sec.
    uint32_t send_time = packet.XRDLRRReportBlockItem.LastRR;
    // RFC3611, section 4.5, LRR field discription states:
    // If no such block has been received, the field is set to zero.
    if (send_time == 0)
      return;

    uint32_t delay_rr = packet.XRDLRRReportBlockItem.DelayLastRR;
    uint32_t now = CompactNtp(NtpTime(*_clock));

    uint32_t rtt_ntp = now - delay_rr - send_time;
    xr_rr_rtt_ms_ = CompactNtpRttToMs(rtt_ntp);

    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpXrDlrrReportBlock;

    packet_type = parser.Iterate();
  }
}

void RTCPReceiver::HandlePLI(RTCPUtility::RTCPParserV2& rtcpParser,
                             RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  if (main_ssrc_ == rtcpPacket.PLI.MediaSSRC) {
    TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "PLI");

    ++packet_type_counter_.pli_packets;
    // Received a signal that we need to send a new key frame.
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpPli;
  }
  rtcpParser.Iterate();
}

void RTCPReceiver::HandleTMMBR(RTCPUtility::RTCPParserV2& rtcpParser,
                               RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

  uint32_t senderSSRC = rtcpPacket.TMMBR.SenderSSRC;
  RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(senderSSRC);
  if (ptrReceiveInfo == NULL) {
    // This remote SSRC must be saved before.
    rtcpParser.Iterate();
    return;
  }
  if (rtcpPacket.TMMBR.MediaSSRC) {
    // rtcpPacket.TMMBR.MediaSSRC SHOULD be 0 if same as SenderSSRC
    // in relay mode this is a valid number
    senderSSRC = rtcpPacket.TMMBR.MediaSSRC;
  }

  // Use packet length to calc max number of TMMBR blocks
  // each TMMBR block is 8 bytes
  ptrdiff_t maxNumOfTMMBRBlocks = rtcpParser.LengthLeft() / 8;

  // sanity, we can't have more than what's in one packet
  if (maxNumOfTMMBRBlocks > 200) {
    assert(false);
    rtcpParser.Iterate();
    return;
  }

  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  while (pktType == RTCPPacketTypes::kRtpfbTmmbrItem) {
    if (main_ssrc_ == rtcpPacket.TMMBRItem.SSRC &&
        rtcpPacket.TMMBRItem.MaxTotalMediaBitRate > 0) {
      ptrReceiveInfo->InsertTmmbrItem(
          senderSSRC,
          rtcp::TmmbItem(rtcpPacket.TMMBRItem.SSRC,
                         rtcpPacket.TMMBRItem.MaxTotalMediaBitRate * 1000,
                         rtcpPacket.TMMBRItem.MeasuredOverhead),
          _clock->TimeInMilliseconds());
      rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpTmmbr;
    }

    pktType = rtcpParser.Iterate();
  }
}

void RTCPReceiver::HandleTMMBN(RTCPUtility::RTCPParserV2& rtcpParser,
                               RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  RTCPReceiveInformation* ptrReceiveInfo =
      GetReceiveInformation(rtcpPacket.TMMBN.SenderSSRC);
  if (ptrReceiveInfo == NULL) {
    // This remote SSRC must be saved before.
    rtcpParser.Iterate();
    return;
  }
  rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpTmmbn;
  // Use packet length to calc max number of TMMBN blocks
  // each TMMBN block is 8 bytes
  ptrdiff_t maxNumOfTMMBNBlocks = rtcpParser.LengthLeft() / 8;

  // sanity, we cant have more than what's in one packet
  if (maxNumOfTMMBNBlocks > 200) {
    assert(false);
    rtcpParser.Iterate();
    return;
  }

  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  while (pktType == RTCPPacketTypes::kRtpfbTmmbnItem) {
    ptrReceiveInfo->tmmbn.emplace_back(
        rtcpPacket.TMMBNItem.SSRC,
        rtcpPacket.TMMBNItem.MaxTotalMediaBitRate * 1000,
        rtcpPacket.TMMBNItem.MeasuredOverhead);
    pktType = rtcpParser.Iterate();
  }
}

void RTCPReceiver::HandleSR_REQ(RTCPUtility::RTCPParserV2& rtcpParser,
                                RTCPPacketInformation& rtcpPacketInformation) {
  rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpSrReq;
  rtcpParser.Iterate();
}

void RTCPReceiver::HandleSLI(RTCPUtility::RTCPParserV2& rtcpParser,
                             RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  while (pktType == RTCPPacketTypes::kPsfbSliItem) {
    // in theory there could be multiple slices lost
    rtcpPacketInformation.rtcpPacketTypeFlags |=
        kRtcpSli;  // received signal that we need to refresh a slice
    rtcpPacketInformation.sliPictureId = rtcpPacket.SLIItem.PictureId;

    pktType = rtcpParser.Iterate();
  }
}

void RTCPReceiver::HandleRPSI(
    RTCPUtility::RTCPParserV2& rtcpParser,
    RTCPHelp::RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  if (pktType == RTCPPacketTypes::kPsfbRpsiItem) {
    if (rtcpPacket.RPSI.NumberOfValidBits % 8 != 0) {
      // to us unknown
      // continue
      rtcpParser.Iterate();
      return;
    }
    // Received signal that we have a confirmed reference picture.
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRpsi;
    rtcpPacketInformation.rpsiPictureId = 0;

    // convert NativeBitString to rpsiPictureId
    uint8_t numberOfBytes = rtcpPacket.RPSI.NumberOfValidBits / 8;
    for (uint8_t n = 0; n < (numberOfBytes - 1); n++) {
      rtcpPacketInformation.rpsiPictureId +=
          (rtcpPacket.RPSI.NativeBitString[n] & 0x7f);
      rtcpPacketInformation.rpsiPictureId <<= 7;  // prepare next
    }
    rtcpPacketInformation.rpsiPictureId +=
        (rtcpPacket.RPSI.NativeBitString[numberOfBytes - 1] & 0x7f);
  }
}

void RTCPReceiver::HandlePsfbApp(RTCPUtility::RTCPParserV2& rtcpParser,
                                 RTCPPacketInformation& rtcpPacketInformation) {
  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  if (pktType == RTCPPacketTypes::kPsfbRemb) {
    pktType = rtcpParser.Iterate();
    if (pktType == RTCPPacketTypes::kPsfbRembItem) {
      const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
      rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRemb;
      rtcpPacketInformation.receiverEstimatedMaxBitrate =
          rtcpPacket.REMBItem.BitRate;

      rtcpParser.Iterate();
    }
  }
}

void RTCPReceiver::HandleFIR(RTCPUtility::RTCPParserV2& rtcpParser,
                             RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  RTCPReceiveInformation* ptrReceiveInfo =
      GetReceiveInformation(rtcpPacket.FIR.SenderSSRC);

  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  while (pktType == RTCPPacketTypes::kPsfbFirItem) {
    // Is it our sender that is requested to generate a new keyframe
    if (main_ssrc_ != rtcpPacket.FIRItem.SSRC) {
      return;
    }

    ++packet_type_counter_.fir_packets;

    // rtcpPacket.FIR.MediaSSRC SHOULD be 0 but we ignore to check it
    // we don't know who this originate from
    if (ptrReceiveInfo) {
      // check if we have reported this FIRSequenceNumber before
      if (rtcpPacket.FIRItem.CommandSequenceNumber !=
          ptrReceiveInfo->last_fir_sequence_number) {
        int64_t now = _clock->TimeInMilliseconds();
        // sanity; don't go crazy with the callbacks
        if ((now - ptrReceiveInfo->last_fir_request_ms) >
            RTCP_MIN_FRAME_LENGTH_MS) {
          ptrReceiveInfo->last_fir_request_ms = now;
          ptrReceiveInfo->last_fir_sequence_number =
              rtcpPacket.FIRItem.CommandSequenceNumber;
          // received signal that we need to send a new key frame
          rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpFir;
        }
      }
    } else {
      // received signal that we need to send a new key frame
      rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpFir;
    }

    pktType = rtcpParser.Iterate();
  }
}

void RTCPReceiver::HandleTransportFeedback(
    RTCPUtility::RTCPParserV2* rtcp_parser,
    RTCPHelp::RTCPPacketInformation* rtcp_packet_information) {
  rtcp::RtcpPacket* packet = rtcp_parser->ReleaseRtcpPacket();
  RTC_DCHECK(packet != nullptr);
  rtcp_packet_information->rtcpPacketTypeFlags |= kRtcpTransportFeedback;
  rtcp_packet_information->transport_feedback_.reset(
      static_cast<rtcp::TransportFeedback*>(packet));

  rtcp_parser->Iterate();
}

void RTCPReceiver::UpdateTmmbr() {
  // Find bounding set.
  std::vector<rtcp::TmmbItem> bounding =
      TMMBRHelp::FindBoundingSet(TmmbrReceived());

  if (!bounding.empty() && _cbRtcpBandwidthObserver) {
    // We have a new bandwidth estimate on this channel.
    uint64_t bitrate_bps = TMMBRHelp::CalcMinBitrateBps(bounding);
    if (bitrate_bps <= std::numeric_limits<uint32_t>::max())
      _cbRtcpBandwidthObserver->OnReceivedEstimatedBitrate(bitrate_bps);
  }

  // Set bounding set: inform remote clients about the new bandwidth.
  _rtpRtcp.SetTmmbn(std::move(bounding));
}

void RTCPReceiver::RegisterRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  rtc::CritScope cs(&_criticalSectionFeedbacks);
  stats_callback_ = callback;
}

RtcpStatisticsCallback* RTCPReceiver::GetRtcpStatisticsCallback() {
  rtc::CritScope cs(&_criticalSectionFeedbacks);
  return stats_callback_;
}

// Holding no Critical section
void RTCPReceiver::TriggerCallbacksFromRTCPPacket(
    RTCPPacketInformation& rtcpPacketInformation) {
  // Process TMMBR and REMB first to avoid multiple callbacks
  // to OnNetworkChanged.
  if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpTmmbr) {
    // Might trigger a OnReceivedBandwidthEstimateUpdate.
    UpdateTmmbr();
  }
  uint32_t local_ssrc;
  std::set<uint32_t> registered_ssrcs;
  {
    // We don't want to hold this critsect when triggering the callbacks below.
    rtc::CritScope lock(&_criticalSectionRTCPReceiver);
    local_ssrc = main_ssrc_;
    registered_ssrcs = registered_ssrcs_;
  }
  if (!receiver_only_ &&
      (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSrReq)) {
    _rtpRtcp.OnRequestSendReport();
  }
  if (!receiver_only_ &&
      (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpNack)) {
    if (rtcpPacketInformation.nackSequenceNumbers.size() > 0) {
      LOG(LS_VERBOSE) << "Incoming NACK length: "
                      << rtcpPacketInformation.nackSequenceNumbers.size();
      _rtpRtcp.OnReceivedNack(rtcpPacketInformation.nackSequenceNumbers);
    }
  }
  {
    // We need feedback that we have received a report block(s) so that we
    // can generate a new packet in a conference relay scenario, one received
    // report can generate several RTCP packets, based on number relayed/mixed
    // a send report block should go out to all receivers.
    if (_cbRtcpIntraFrameObserver) {
      RTC_DCHECK(!receiver_only_);
      if ((rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpPli) ||
          (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpFir)) {
        if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpPli) {
          LOG(LS_VERBOSE) << "Incoming PLI from SSRC "
                          << rtcpPacketInformation.remoteSSRC;
        } else {
          LOG(LS_VERBOSE) << "Incoming FIR from SSRC "
                          << rtcpPacketInformation.remoteSSRC;
        }
        _cbRtcpIntraFrameObserver->OnReceivedIntraFrameRequest(local_ssrc);
      }
      if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSli) {
        _cbRtcpIntraFrameObserver->OnReceivedSLI(
            local_ssrc, rtcpPacketInformation.sliPictureId);
      }
      if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRpsi) {
        _cbRtcpIntraFrameObserver->OnReceivedRPSI(
            local_ssrc, rtcpPacketInformation.rpsiPictureId);
      }
    }
    if (_cbRtcpBandwidthObserver) {
      RTC_DCHECK(!receiver_only_);
      if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRemb) {
        LOG(LS_VERBOSE) << "Incoming REMB: "
                        << rtcpPacketInformation.receiverEstimatedMaxBitrate;
        _cbRtcpBandwidthObserver->OnReceivedEstimatedBitrate(
            rtcpPacketInformation.receiverEstimatedMaxBitrate);
      }
      if ((rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSr) ||
          (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRr)) {
        int64_t now = _clock->TimeInMilliseconds();
        _cbRtcpBandwidthObserver->OnReceivedRtcpReceiverReport(
            rtcpPacketInformation.report_blocks, rtcpPacketInformation.rtt,
            now);
      }
    }
    if ((rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSr) ||
        (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRr)) {
      _rtpRtcp.OnReceivedRtcpReportBlocks(rtcpPacketInformation.report_blocks);
    }

    if (_cbTransportFeedbackObserver &&
        (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpTransportFeedback)) {
      uint32_t media_source_ssrc =
          rtcpPacketInformation.transport_feedback_->GetMediaSourceSsrc();
      if (media_source_ssrc == local_ssrc ||
          registered_ssrcs.find(media_source_ssrc) != registered_ssrcs.end()) {
        _cbTransportFeedbackObserver->OnTransportFeedback(
            *rtcpPacketInformation.transport_feedback_.get());
      }
    }
  }

  if (!receiver_only_) {
    rtc::CritScope cs(&_criticalSectionFeedbacks);
    if (stats_callback_) {
      for (ReportBlockList::const_iterator it =
               rtcpPacketInformation.report_blocks.begin();
           it != rtcpPacketInformation.report_blocks.end(); ++it) {
        RtcpStatistics stats;
        stats.cumulative_lost = it->cumulativeLost;
        stats.extended_max_sequence_number = it->extendedHighSeqNum;
        stats.fraction_lost = it->fractionLost;
        stats.jitter = it->jitter;

        stats_callback_->StatisticsUpdated(stats, it->sourceSSRC);
      }
    }
  }
}

int32_t RTCPReceiver::CNAME(uint32_t remoteSSRC,
                            char cName[RTCP_CNAME_SIZE]) const {
  assert(cName);

  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  RTCPCnameInformation* cnameInfo = GetCnameInformation(remoteSSRC);
  if (cnameInfo == NULL) {
    return -1;
  }
  cName[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(cName, cnameInfo->name, RTCP_CNAME_SIZE - 1);
  return 0;
}

std::vector<rtcp::TmmbItem> RTCPReceiver::TmmbrReceived() const {
  rtc::CritScope lock(&_criticalSectionRTCPReceiver);
  std::vector<rtcp::TmmbItem> candidates;

  int64_t now_ms = _clock->TimeInMilliseconds();

  for (const auto& kv : _receivedInfoMap) {
    RTCPReceiveInformation* receive_info = kv.second;
    RTC_DCHECK(receive_info);
    receive_info->GetTmmbrSet(now_ms, &candidates);
  }
  return candidates;
}

}  // namespace webrtc
