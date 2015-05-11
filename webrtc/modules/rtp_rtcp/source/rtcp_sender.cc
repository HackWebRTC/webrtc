/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"

#include <assert.h>  // assert
#include <stdlib.h>  // rand
#include <string.h>  // memcpy

#include <algorithm>  // min
#include <limits>     // max

#include "webrtc/base/checks.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

using RTCPUtility::RTCPCnameInformation;

NACKStringBuilder::NACKStringBuilder()
    : stream_(""), count_(0), prevNack_(0), consecutive_(false) {
}

NACKStringBuilder::~NACKStringBuilder() {}

void NACKStringBuilder::PushNACK(uint16_t nack)
{
  if (count_ == 0) {
    stream_ << nack;
  } else if (nack == prevNack_ + 1) {
    consecutive_ = true;
  } else {
    if (consecutive_) {
      stream_ << "-" << prevNack_;
      consecutive_ = false;
    }
    stream_ << "," << nack;
  }
  count_++;
  prevNack_ = nack;
}

std::string NACKStringBuilder::GetResult() {
  if (consecutive_) {
    stream_ << "-" << prevNack_;
    consecutive_ = false;
  }
  return stream_.str();
}

RTCPSender::FeedbackState::FeedbackState()
    : send_payload_type(0),
      frequency_hz(0),
      packets_sent(0),
      media_bytes_sent(0),
      send_bitrate(0),
      last_rr_ntp_secs(0),
      last_rr_ntp_frac(0),
      remote_sr(0),
      has_last_xr_rr(false),
      module(nullptr) {
}

struct RTCPSender::RtcpContext {
  RtcpContext(const FeedbackState& feedback_state,
              int32_t nack_size,
              const uint16_t* nack_list,
              bool repeat,
              uint64_t picture_id,
              uint8_t* buffer,
              uint32_t buffer_size)
      : feedback_state(feedback_state),
        nack_size(nack_size),
        nack_list(nack_list),
        repeat(repeat),
        picture_id(picture_id),
        buffer(buffer),
        buffer_size(buffer_size),
        ntp_sec(0),
        ntp_frac(0),
        jitter_transmission_offset(0),
        position(0) {}

  uint8_t* AllocateData(uint32_t bytes) {
    DCHECK_LE(position + bytes, buffer_size);
    uint8_t* ptr = &buffer[position];
    position += bytes;
    return ptr;
  }

  const FeedbackState& feedback_state;
  int32_t nack_size;
  const uint16_t* nack_list;
  bool repeat;
  uint64_t picture_id;
  uint8_t* buffer;
  uint32_t buffer_size;
  uint32_t ntp_sec;
  uint32_t ntp_frac;
  uint32_t jitter_transmission_offset;
  uint32_t position;
};

RTCPSender::RTCPSender(
    int32_t id,
    bool audio,
    Clock* clock,
    ReceiveStatistics* receive_statistics,
    RtcpPacketTypeCounterObserver* packet_type_counter_observer)
    : id_(id),
      audio_(audio),
      clock_(clock),
      method_(kRtcpOff),
      critical_section_transport_(
          CriticalSectionWrapper::CreateCriticalSection()),
      cbTransport_(nullptr),

      critical_section_rtcp_sender_(
          CriticalSectionWrapper::CreateCriticalSection()),
      using_nack_(false),
      sending_(false),
      remb_enabled_(false),
      extended_jitter_report_enabled_(false),
      next_time_to_send_rtcp_(0),
      start_timestamp_(0),
      last_rtp_timestamp_(0),
      last_frame_capture_time_ms_(-1),
      ssrc_(0),
      remote_ssrc_(0),
      receive_statistics_(receive_statistics),

      sequence_number_fir_(0),

      remb_bitrate_(0),

      tmmbr_help_(),
      tmmbr_send_(0),
      packet_oh_send_(0),

      app_sub_type_(0),
      app_data_(nullptr),
      app_length_(0),

      xr_send_receiver_reference_time_enabled_(false),
      packet_type_counter_observer_(packet_type_counter_observer) {
  memset(cname_, 0, sizeof(cname_));
  memset(last_send_report_, 0, sizeof(last_send_report_));
  memset(last_rtcp_time_, 0, sizeof(last_rtcp_time_));

  builders_[kRtcpSr] = &RTCPSender::BuildSR;
  builders_[kRtcpRr] = &RTCPSender::BuildRR;
  builders_[kRtcpSdes] = &RTCPSender::BuildSDEC;
  builders_[kRtcpTransmissionTimeOffset] =
      &RTCPSender::BuildExtendedJitterReport;
  builders_[kRtcpPli] = &RTCPSender::BuildPLI;
  builders_[kRtcpFir] = &RTCPSender::BuildFIR;
  builders_[kRtcpSli] = &RTCPSender::BuildSLI;
  builders_[kRtcpRpsi] = &RTCPSender::BuildRPSI;
  builders_[kRtcpRemb] = &RTCPSender::BuildREMB;
  builders_[kRtcpBye] = &RTCPSender::BuildBYE;
  builders_[kRtcpApp] = &RTCPSender::BuildAPP;
  builders_[kRtcpTmmbr] = &RTCPSender::BuildTMMBR;
  builders_[kRtcpTmmbn] = &RTCPSender::BuildTMMBN;
  builders_[kRtcpNack] = &RTCPSender::BuildNACK;
  builders_[kRtcpXrVoipMetric] = &RTCPSender::BuildVoIPMetric;
  builders_[kRtcpXrReceiverReferenceTime] =
      &RTCPSender::BuildReceiverReferenceTime;
  builders_[kRtcpXrDlrrReportBlock] = &RTCPSender::BuildDlrr;
}

RTCPSender::~RTCPSender() {
  for (auto it : internal_report_blocks_)
    delete it.second;

  for (auto it : external_report_blocks_)
    delete it.second;

  for (auto it : csrc_cnames_)
    delete it.second;
}

int32_t RTCPSender::RegisterSendTransport(Transport* outgoingTransport) {
  CriticalSectionScoped lock(critical_section_transport_.get());
  cbTransport_ = outgoingTransport;
  return 0;
}

RTCPMethod RTCPSender::Status() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return method_;
}

void RTCPSender::SetRTCPStatus(RTCPMethod method) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  method_ = method;

  if (method == kRtcpOff)
    return;
  next_time_to_send_rtcp_ =
      clock_->TimeInMilliseconds() +
      (audio_ ? RTCP_INTERVAL_AUDIO_MS / 2 : RTCP_INTERVAL_VIDEO_MS / 2);
}

bool RTCPSender::Sending() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return sending_;
}

int32_t RTCPSender::SetSendingStatus(const FeedbackState& feedback_state,
                                     bool sending) {
  bool sendRTCPBye = false;
  {
    CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

    if (method_ != kRtcpOff) {
      if (sending == false && sending_ == true) {
        // Trigger RTCP bye
        sendRTCPBye = true;
      }
    }
    sending_ = sending;
  }
  if (sendRTCPBye)
    return SendRTCP(feedback_state, kRtcpBye);
  return 0;
}

bool RTCPSender::REMB() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return remb_enabled_;
}

void RTCPSender::SetREMBStatus(bool enable) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  remb_enabled_ = enable;
}

void RTCPSender::SetREMBData(uint32_t bitrate,
                             const std::vector<uint32_t>& ssrcs) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  remb_bitrate_ = bitrate;
  remb_ssrcs_ = ssrcs;

  if (remb_enabled_)
    SetFlag(kRtcpRemb, false);
  // Send a REMB immediately if we have a new REMB. The frequency of REMBs is
  // throttled by the caller.
  next_time_to_send_rtcp_ = clock_->TimeInMilliseconds();
}

bool RTCPSender::TMMBR() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return IsFlagPresent(RTCPPacketType::kRtcpTmmbr);
}

void RTCPSender::SetTMMBRStatus(bool enable) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  if (enable) {
    SetFlag(RTCPPacketType::kRtcpTmmbr, false);
  } else {
    ConsumeFlag(RTCPPacketType::kRtcpTmmbr, true);
  }
}

bool RTCPSender::IJ() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return extended_jitter_report_enabled_;
}

void RTCPSender::SetIJStatus(bool enable) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  extended_jitter_report_enabled_ = enable;
}

void RTCPSender::SetStartTimestamp(uint32_t start_timestamp) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  start_timestamp_ = start_timestamp;
}

void RTCPSender::SetLastRtpTime(uint32_t rtp_timestamp,
                                int64_t capture_time_ms) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  last_rtp_timestamp_ = rtp_timestamp;
  if (capture_time_ms < 0) {
    // We don't currently get a capture time from VoiceEngine.
    last_frame_capture_time_ms_ = clock_->TimeInMilliseconds();
  } else {
    last_frame_capture_time_ms_ = capture_time_ms;
  }
}

void RTCPSender::SetSSRC(uint32_t ssrc) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  if (ssrc_ != 0) {
    // not first SetSSRC, probably due to a collision
    // schedule a new RTCP report
    // make sure that we send a RTP packet
    next_time_to_send_rtcp_ = clock_->TimeInMilliseconds() + 100;
  }
  ssrc_ = ssrc;
}

void RTCPSender::SetRemoteSSRC(uint32_t ssrc) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  remote_ssrc_ = ssrc;
}

int32_t RTCPSender::SetCNAME(const char cName[RTCP_CNAME_SIZE]) {
  if (!cName)
    return -1;

  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  cname_[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(cname_, cName, RTCP_CNAME_SIZE - 1);
  return 0;
}

int32_t RTCPSender::AddMixedCNAME(uint32_t SSRC,
                                  const char cName[RTCP_CNAME_SIZE]) {
  assert(cName);
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  if (csrc_cnames_.size() >= kRtpCsrcSize) {
    return -1;
  }
  RTCPCnameInformation* ptr = new RTCPCnameInformation();
  ptr->name[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(ptr->name, cName, RTCP_CNAME_SIZE - 1);
  csrc_cnames_[SSRC] = ptr;
  return 0;
}

int32_t RTCPSender::RemoveMixedCNAME(uint32_t SSRC) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  std::map<uint32_t, RTCPCnameInformation*>::iterator it =
      csrc_cnames_.find(SSRC);

  if (it == csrc_cnames_.end())
    return -1;

  delete it->second;
  csrc_cnames_.erase(it);
  return 0;
}

bool RTCPSender::TimeToSendRTCPReport(bool sendKeyframeBeforeRTP) const {
/*
    For audio we use a fix 5 sec interval

    For video we use 1 sec interval fo a BW smaller than 360 kbit/s,
        technicaly we break the max 5% RTCP BW for video below 10 kbit/s but
        that should be extremely rare


From RFC 3550

    MAX RTCP BW is 5% if the session BW
        A send report is approximately 65 bytes inc CNAME
        A receiver report is approximately 28 bytes

    The RECOMMENDED value for the reduced minimum in seconds is 360
      divided by the session bandwidth in kilobits/second.  This minimum
      is smaller than 5 seconds for bandwidths greater than 72 kb/s.

    If the participant has not yet sent an RTCP packet (the variable
      initial is true), the constant Tmin is set to 2.5 seconds, else it
      is set to 5 seconds.

    The interval between RTCP packets is varied randomly over the
      range [0.5,1.5] times the calculated interval to avoid unintended
      synchronization of all participants

    if we send
    If the participant is a sender (we_sent true), the constant C is
      set to the average RTCP packet size (avg_rtcp_size) divided by 25%
      of the RTCP bandwidth (rtcp_bw), and the constant n is set to the
      number of senders.

    if we receive only
      If we_sent is not true, the constant C is set
      to the average RTCP packet size divided by 75% of the RTCP
      bandwidth.  The constant n is set to the number of receivers
      (members - senders).  If the number of senders is greater than
      25%, senders and receivers are treated together.

    reconsideration NOT required for peer-to-peer
      "timer reconsideration" is
      employed.  This algorithm implements a simple back-off mechanism
      which causes users to hold back RTCP packet transmission if the
      group sizes are increasing.

      n = number of members
      C = avg_size/(rtcpBW/4)

   3. The deterministic calculated interval Td is set to max(Tmin, n*C).

   4. The calculated interval T is set to a number uniformly distributed
      between 0.5 and 1.5 times the deterministic calculated interval.

   5. The resulting value of T is divided by e-3/2=1.21828 to compensate
      for the fact that the timer reconsideration algorithm converges to
      a value of the RTCP bandwidth below the intended average
*/

  int64_t now = clock_->TimeInMilliseconds();

  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  if (method_ == kRtcpOff)
    return false;

  if (!audio_ && sendKeyframeBeforeRTP) {
    // for video key-frames we want to send the RTCP before the large key-frame
    // if we have a 100 ms margin
    now += RTCP_SEND_BEFORE_KEY_FRAME_MS;
  }

  if (now >= next_time_to_send_rtcp_) {
    return true;
  } else if (now < 0x0000ffff &&
             next_time_to_send_rtcp_ > 0xffff0000) {  // 65 sec margin
    // wrap
    return true;
  }
  return false;
}

int64_t RTCPSender::SendTimeOfSendReport(uint32_t sendReport) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  // This is only saved when we are the sender
  if ((last_send_report_[0] == 0) || (sendReport == 0)) {
    return 0;  // will be ignored
  } else {
    for (int i = 0; i < RTCP_NUMBER_OF_SR; ++i) {
      if (last_send_report_[i] == sendReport)
        return last_rtcp_time_[i];
    }
  }
  return 0;
}

bool RTCPSender::SendTimeOfXrRrReport(uint32_t mid_ntp,
                                      int64_t* time_ms) const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  if (last_xr_rr_.empty()) {
    return false;
  }
  std::map<uint32_t, int64_t>::const_iterator it = last_xr_rr_.find(mid_ntp);
  if (it == last_xr_rr_.end()) {
    return false;
  }
  *time_ms = it->second;
  return true;
}

int32_t RTCPSender::AddExternalReportBlock(
    uint32_t SSRC,
    const RTCPReportBlock* reportBlock) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return AddReportBlock(SSRC, &external_report_blocks_, reportBlock);
}

int32_t RTCPSender::AddReportBlock(
    uint32_t SSRC,
    std::map<uint32_t, RTCPReportBlock*>* report_blocks,
    const RTCPReportBlock* reportBlock) {
  assert(reportBlock);

  if (report_blocks->size() >= RTCP_MAX_REPORT_BLOCKS) {
    LOG(LS_WARNING) << "Too many report blocks.";
    return -1;
  }
  std::map<uint32_t, RTCPReportBlock*>::iterator it =
      report_blocks->find(SSRC);
  if (it != report_blocks->end()) {
    delete it->second;
    report_blocks->erase(it);
  }
  RTCPReportBlock* copyReportBlock = new RTCPReportBlock();
  memcpy(copyReportBlock, reportBlock, sizeof(RTCPReportBlock));
  (*report_blocks)[SSRC] = copyReportBlock;
  return 0;
}

int32_t RTCPSender::RemoveExternalReportBlock(uint32_t SSRC) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  std::map<uint32_t, RTCPReportBlock*>::iterator it =
      external_report_blocks_.find(SSRC);

  if (it == external_report_blocks_.end()) {
    return -1;
  }
  delete it->second;
  external_report_blocks_.erase(it);
  return 0;
}

RTCPSender::BuildResult RTCPSender::BuildSR(RtcpContext* ctx) {
  // sanity
  if (ctx->position + 52 >= IP_PACKET_SIZE) {
    LOG(LS_WARNING) << "Failed to build Sender Report.";
    return BuildResult::kTruncated;
  }
  uint32_t RTPtime;

  uint32_t posNumberOfReportBlocks = ctx->position;
  *ctx->AllocateData(1) = 0x80;

  // Sender report
  *ctx->AllocateData(1) = 200;

  for (int i = (RTCP_NUMBER_OF_SR - 2); i >= 0; i--) {
    // shift old
    last_send_report_[i + 1] = last_send_report_[i];
    last_rtcp_time_[i + 1] = last_rtcp_time_[i];
  }

  last_rtcp_time_[0] = Clock::NtpToMs(ctx->ntp_sec, ctx->ntp_frac);
  last_send_report_[0] = (ctx->ntp_sec << 16) + (ctx->ntp_frac >> 16);

  // The timestamp of this RTCP packet should be estimated as the timestamp of
  // the frame being captured at this moment. We are calculating that
  // timestamp as the last frame's timestamp + the time since the last frame
  // was captured.
  RTPtime = start_timestamp_ + last_rtp_timestamp_ +
            (clock_->TimeInMilliseconds() - last_frame_capture_time_ms_) *
                (ctx->feedback_state.frequency_hz / 1000);

  // Add sender data
  // Save  for our length field
  ctx->AllocateData(2);

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);
  // NTP
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ctx->ntp_sec);
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ctx->ntp_frac);
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), RTPtime);

  // sender's packet count
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4),
                                       ctx->feedback_state.packets_sent);

  // sender's octet count
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4),
                                       ctx->feedback_state.media_bytes_sent);

  uint8_t numberOfReportBlocks = 0;
  BuildResult result = WriteAllReportBlocksToBuffer(ctx, &numberOfReportBlocks);
  switch (result) {
    case BuildResult::kError:
    case BuildResult::kTruncated:
    case BuildResult::kAborted:
      return result;
    case BuildResult::kSuccess:
      break;
    default:
      abort();
  }

  ctx->buffer[posNumberOfReportBlocks] += numberOfReportBlocks;

  uint16_t len = static_cast<uint16_t>((ctx->position / 4) - 1);
  ByteWriter<uint16_t>::WriteBigEndian(&ctx->buffer[2], len);

  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildSDEC(RtcpContext* ctx) {
  size_t lengthCname = strlen(cname_);
  assert(lengthCname < RTCP_CNAME_SIZE);

  // sanity
  if (ctx->position + 12 + lengthCname >= IP_PACKET_SIZE) {
    LOG(LS_WARNING) << "Failed to build SDEC.";
    return BuildResult::kTruncated;
  }
  // SDEC Source Description

  // We always need to add SDES CNAME
  size_t size = 0x80 + 1 + csrc_cnames_.size();
  DCHECK_LE(size, std::numeric_limits<uint8_t>::max());
  *ctx->AllocateData(1) = static_cast<uint8_t>(size);
  *ctx->AllocateData(1) = 202;

  // handle SDES length later on
  uint32_t SDESLengthPos = ctx->position;
  ctx->AllocateData(2);

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // CNAME = 1
  *ctx->AllocateData(1) = 1;
  DCHECK_LE(lengthCname, std::numeric_limits<uint8_t>::max());
  *ctx->AllocateData(1) = static_cast<uint8_t>(lengthCname);

  uint16_t SDESLength = 10;

  memcpy(ctx->AllocateData(lengthCname), cname_, lengthCname);
  SDESLength += static_cast<uint16_t>(lengthCname);

  uint16_t padding = 0;
  // We must have a zero field even if we have an even multiple of 4 bytes
  do {
    ++padding;
    *ctx->AllocateData(1) = 0;
  } while ((ctx->position % 4) != 0);
  SDESLength += padding;

  for (auto it = csrc_cnames_.begin(); it != csrc_cnames_.end(); ++it) {
    RTCPCnameInformation* cname = it->second;
    uint32_t SSRC = it->first;

    // Add SSRC
    ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), SSRC);

    // CNAME = 1
    *ctx->AllocateData(1) = 1;

    size_t length = strlen(cname->name);
    assert(length < RTCP_CNAME_SIZE);

    *ctx->AllocateData(1) = static_cast<uint8_t>(length);
    SDESLength += 6;

    memcpy(ctx->AllocateData(length), cname->name, length);

    SDESLength += length;
    uint16_t padding = 0;

    // We must have a zero field even if we have an even multiple of 4 bytes
    do {
      ++padding;
      *ctx->AllocateData(1) = 0;
    } while ((ctx->position % 4) != 0);
    SDESLength += padding;
  }
  // in 32-bit words minus one and we don't count the header
  uint16_t buffer_length = (SDESLength / 4) - 1;
  ByteWriter<uint16_t>::WriteBigEndian(&ctx->buffer[SDESLengthPos],
                                       buffer_length);
  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildRR(RtcpContext* ctx) {
  // sanity one block
  if (ctx->position + 32 >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  uint32_t posNumberOfReportBlocks = ctx->position;

  *ctx->AllocateData(1) = 0x80;
  *ctx->AllocateData(1) = 201;

  // Save  for our length field
  uint32_t len_pos = ctx->position;
  ctx->AllocateData(2);

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  uint8_t numberOfReportBlocks = 0;
  BuildResult result = WriteAllReportBlocksToBuffer(ctx, &numberOfReportBlocks);
  switch (result) {
    case BuildResult::kError:
    case BuildResult::kTruncated:
    case BuildResult::kAborted:
      return result;
    case BuildResult::kSuccess:
      break;
    default:
      abort();
  }

  ctx->buffer[posNumberOfReportBlocks] += numberOfReportBlocks;

  uint16_t len = uint16_t((ctx->position) / 4 - 1);
  ByteWriter<uint16_t>::WriteBigEndian(&ctx->buffer[len_pos], len);

  return BuildResult::kSuccess;
}

// From RFC 5450: Transmission Time Offsets in RTP Streams.
//        0                   1                   2                   3
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   hdr |V=2|P|    RC   |   PT=IJ=195   |             length            |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |                      inter-arrival jitter                     |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       .                                                               .
//       .                                                               .
//       .                                                               .
//       |                      inter-arrival jitter                     |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  If present, this RTCP packet must be placed after a receiver report
//  (inside a compound RTCP packet), and MUST have the same value for RC
//  (reception report count) as the receiver report.

RTCPSender::BuildResult RTCPSender::BuildExtendedJitterReport(
    RtcpContext* ctx) {
  if (external_report_blocks_.size() > 0) {
    // TODO(andresp): Remove external report blocks since they are not
    // supported.
    LOG(LS_ERROR) << "Handling of external report blocks not implemented.";
    return BuildResult::kError;
  }

  // sanity
  if (ctx->position + 8 >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  // add picture loss indicator
  uint8_t RC = 1;
  *ctx->AllocateData(1) = 0x80 + RC;
  *ctx->AllocateData(1) = 195;

  // Used fixed length of 2
  *ctx->AllocateData(1) = 0;
  *ctx->AllocateData(1) = 1;

  // Add inter-arrival jitter
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4),
                                       ctx->jitter_transmission_offset);
  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildPLI(RtcpContext* ctx) {
  // sanity
  if (ctx->position + 12 >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  // add picture loss indicator
  uint8_t FMT = 1;
  *ctx->AllocateData(1) = 0x80 + FMT;
  *ctx->AllocateData(1) = 206;

  // Used fixed length of 2
  *ctx->AllocateData(1) = 0;
  *ctx->AllocateData(1) = 2;

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // Add the remote SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), remote_ssrc_);

  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "RTCPSender::PLI");
  ++packet_type_counter_.pli_packets;
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RTCP_PLICount",
                    ssrc_, packet_type_counter_.pli_packets);

  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildFIR(RtcpContext* ctx) {
  // sanity
  if (ctx->position + 20 >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  if (!ctx->repeat)
    sequence_number_fir_++;  // do not increase if repetition

  // add full intra request indicator
  uint8_t FMT = 4;
  *ctx->AllocateData(1) = 0x80 + FMT;
  *ctx->AllocateData(1) = 206;

  //Length of 4
  *ctx->AllocateData(1) = 0;
  *ctx->AllocateData(1) = 4;

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // RFC 5104     4.3.1.2.  Semantics
  // SSRC of media source
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), 0);

  // Additional Feedback Control Information (FCI)
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), remote_ssrc_);

  *ctx->AllocateData(1) = sequence_number_fir_;
  *ctx->AllocateData(1) = 0;
  *ctx->AllocateData(1) = 0;
  *ctx->AllocateData(1) = 0;

  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "RTCPSender::FIR");
  ++packet_type_counter_.fir_packets;
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RTCP_FIRCount",
                    ssrc_, packet_type_counter_.fir_packets);

  return BuildResult::kSuccess;
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            First        |        Number           | PictureID |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
RTCPSender::BuildResult RTCPSender::BuildSLI(RtcpContext* ctx) {
  // sanity
  if (ctx->position + 16 >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  // add slice loss indicator
  uint8_t FMT = 2;
  *ctx->AllocateData(1) = 0x80 + FMT;
  *ctx->AllocateData(1) = 206;

  // Used fixed length of 3
  *ctx->AllocateData(1) = 0;
  *ctx->AllocateData(1) = 3;

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // Add the remote SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), remote_ssrc_);

  // Add first, number & picture ID 6 bits
  // first  = 0, 13 - bits
  // number = 0x1fff, 13 - bits only ones for now
  uint32_t sliField = (0x1fff << 6) + (0x3f & ctx->picture_id);
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), sliField);

  return BuildResult::kSuccess;
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      PB       |0| Payload Type|    Native RPSI bit string     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   defined per codec          ...                | Padding (0) |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
/*
*    Note: not generic made for VP8
*/
RTCPSender::BuildResult RTCPSender::BuildRPSI(RtcpContext* ctx) {
  if (ctx->feedback_state.send_payload_type == 0xFF)
    return BuildResult::kError;

  // sanity
  if (ctx->position + 24 >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  // add Reference Picture Selection Indication
  uint8_t FMT = 3;
  *ctx->AllocateData(1) = 0x80 + FMT;
  *ctx->AllocateData(1) = 206;

  // calc length
  uint32_t bitsRequired = 7;
  uint8_t bytesRequired = 1;
  while ((ctx->picture_id >> bitsRequired) > 0) {
    bitsRequired += 7;
    bytesRequired++;
  }

  uint8_t size = 3;
  if (bytesRequired > 6) {
    size = 5;
  } else if (bytesRequired > 2) {
    size = 4;
  }
  *ctx->AllocateData(1) = 0;
  *ctx->AllocateData(1) = size;

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // Add the remote SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), remote_ssrc_);

  // calc padding length
  uint8_t paddingBytes = 4 - ((2 + bytesRequired) % 4);
  if (paddingBytes == 4)
    paddingBytes = 0;
  // add padding length in bits
  *ctx->AllocateData(1) = paddingBytes * 8;  // padding can be 0, 8, 16 or 24

  // add payload type
  *ctx->AllocateData(1) = ctx->feedback_state.send_payload_type;

  // add picture ID
  for (int i = bytesRequired - 1; i > 0; --i) {
    *ctx->AllocateData(1) =
        0x80 | static_cast<uint8_t>(ctx->picture_id >> (i * 7));
  }
  // add last byte of picture ID
  *ctx->AllocateData(1) = static_cast<uint8_t>(ctx->picture_id & 0x7f);

  // add padding
  for (int j = 0; j < paddingBytes; j++) {
    *ctx->AllocateData(1) = 0;
  }

  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildREMB(RtcpContext* ctx) {
  // sanity
  if (ctx->position + 20 + 4 * remb_ssrcs_.size() >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  // add application layer feedback
  uint8_t FMT = 15;
  *ctx->AllocateData(1) = 0x80 + FMT;
  *ctx->AllocateData(1) = 206;

  *ctx->AllocateData(1) = 0;
  *ctx->AllocateData(1) = static_cast<uint8_t>(remb_ssrcs_.size() + 4);

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // Remote SSRC must be 0
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), 0);

  *ctx->AllocateData(1) = 'R';
  *ctx->AllocateData(1) = 'E';
  *ctx->AllocateData(1) = 'M';
  *ctx->AllocateData(1) = 'B';

  *ctx->AllocateData(1) = remb_ssrcs_.size();
  // 6 bit Exp
  // 18 bit mantissa
  uint8_t brExp = 0;
  for (uint32_t i = 0; i < 64; i++) {
    if (remb_bitrate_ <= (0x3FFFFu << i)) {
      brExp = i;
      break;
    }
  }
  const uint32_t brMantissa = (remb_bitrate_ >> brExp);
  *ctx->AllocateData(1) =
      static_cast<uint8_t>((brExp << 2) + ((brMantissa >> 16) & 0x03));
  *ctx->AllocateData(1) = static_cast<uint8_t>(brMantissa >> 8);
  *ctx->AllocateData(1) = static_cast<uint8_t>(brMantissa);

  for (size_t i = 0; i < remb_ssrcs_.size(); i++)
    ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), remb_ssrcs_[i]);

  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "RTCPSender::REMB");

  return BuildResult::kSuccess;
}

void RTCPSender::SetTargetBitrate(unsigned int target_bitrate) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  tmmbr_send_ = target_bitrate / 1000;
}

RTCPSender::BuildResult RTCPSender::BuildTMMBR(RtcpContext* ctx) {
  if (ctx->feedback_state.module == NULL)
    return BuildResult::kError;
  // Before sending the TMMBR check the received TMMBN, only an owner is
  // allowed to raise the bitrate:
  // * If the sender is an owner of the TMMBN -> send TMMBR
  // * If not an owner but the TMMBR would enter the TMMBN -> send TMMBR

  // get current bounding set from RTCP receiver
  bool tmmbrOwner = false;
  // store in candidateSet, allocates one extra slot
  TMMBRSet* candidateSet = tmmbr_help_.CandidateSet();

  // holding critical_section_rtcp_sender_ while calling RTCPreceiver which
  // will accuire criticalSectionRTCPReceiver_ is a potental deadlock but
  // since RTCPreceiver is not doing the reverse we should be fine
  int32_t lengthOfBoundingSet =
      ctx->feedback_state.module->BoundingSet(tmmbrOwner, candidateSet);

  if (lengthOfBoundingSet > 0) {
    for (int32_t i = 0; i < lengthOfBoundingSet; i++) {
      if (candidateSet->Tmmbr(i) == tmmbr_send_ &&
          candidateSet->PacketOH(i) == packet_oh_send_) {
        // do not send the same tuple
        return BuildResult::kAborted;
      }
    }
    if (!tmmbrOwner) {
      // use received bounding set as candidate set
      // add current tuple
      candidateSet->SetEntry(lengthOfBoundingSet, tmmbr_send_, packet_oh_send_,
                             ssrc_);
      int numCandidates = lengthOfBoundingSet + 1;

      // find bounding set
      TMMBRSet* boundingSet = NULL;
      int numBoundingSet = tmmbr_help_.FindTMMBRBoundingSet(boundingSet);
      if (numBoundingSet > 0 || numBoundingSet <= numCandidates)
        tmmbrOwner = tmmbr_help_.IsOwner(ssrc_, numBoundingSet);
      if (!tmmbrOwner) {
        // did not enter bounding set, no meaning to send this request
        return BuildResult::kAborted;
      }
    }
  }

  if (tmmbr_send_) {
    // sanity
    if (ctx->position + 20 >= IP_PACKET_SIZE)
      return BuildResult::kTruncated;

    // add TMMBR indicator
    uint8_t FMT = 3;
    *ctx->AllocateData(1) = 0x80 + FMT;
    *ctx->AllocateData(1) = 205;

    // Length of 4
    *ctx->AllocateData(1) = 0;
    *ctx->AllocateData(1) = 4;

    // Add our own SSRC
    ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

    // RFC 5104     4.2.1.2.  Semantics

    // SSRC of media source
    ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), 0);

    // Additional Feedback Control Information (FCI)
    ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), remote_ssrc_);

    uint32_t bitRate = tmmbr_send_ * 1000;
    uint32_t mmbrExp = 0;
    for (uint32_t i = 0; i < 64; i++) {
      if (bitRate <= (0x1FFFFu << i)) {
        mmbrExp = i;
        break;
      }
    }
    uint32_t mmbrMantissa = (bitRate >> mmbrExp);

    *ctx->AllocateData(1) =
        static_cast<uint8_t>((mmbrExp << 2) + ((mmbrMantissa >> 15) & 0x03));
    *ctx->AllocateData(1) = static_cast<uint8_t>(mmbrMantissa >> 7);
    *ctx->AllocateData(1) = static_cast<uint8_t>(
        (mmbrMantissa << 1) + ((packet_oh_send_ >> 8) & 0x01));
    *ctx->AllocateData(1) = static_cast<uint8_t>(packet_oh_send_);
  }
  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildTMMBN(RtcpContext* ctx) {
  TMMBRSet* boundingSet = tmmbr_help_.BoundingSetToSend();
  if (boundingSet == NULL)
    return BuildResult::kError;

  // sanity
  if (ctx->position + 12 + boundingSet->lengthOfSet() * 8 >= IP_PACKET_SIZE) {
    LOG(LS_WARNING) << "Failed to build TMMBN.";
    return BuildResult::kTruncated;
  }

  uint8_t FMT = 4;
  // add TMMBN indicator
  *ctx->AllocateData(1) = 0x80 + FMT;
  *ctx->AllocateData(1) = 205;

  // Add length later
  int posLength = ctx->position;
  ctx->AllocateData(2);

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // RFC 5104     4.2.2.2.  Semantics

  // SSRC of media source
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), 0);

  // Additional Feedback Control Information (FCI)
  int numBoundingSet = 0;
  for (uint32_t n = 0; n < boundingSet->lengthOfSet(); n++) {
    if (boundingSet->Tmmbr(n) > 0) {
      uint32_t tmmbrSSRC = boundingSet->Ssrc(n);
      ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), tmmbrSSRC);

      uint32_t bitRate = boundingSet->Tmmbr(n) * 1000;
      uint32_t mmbrExp = 0;
      for (int i = 0; i < 64; i++) {
        if (bitRate <= (0x1FFFFu << i)) {
          mmbrExp = i;
          break;
        }
      }
      uint32_t mmbrMantissa = (bitRate >> mmbrExp);
      uint32_t measuredOH = boundingSet->PacketOH(n);

      *ctx->AllocateData(1) =
          static_cast<uint8_t>((mmbrExp << 2) + ((mmbrMantissa >> 15) & 0x03));
      *ctx->AllocateData(1) = static_cast<uint8_t>(mmbrMantissa >> 7);
      *ctx->AllocateData(1) = static_cast<uint8_t>((mmbrMantissa << 1) +
                                                   ((measuredOH >> 8) & 0x01));
      *ctx->AllocateData(1) = static_cast<uint8_t>(measuredOH);
      numBoundingSet++;
    }
  }
  uint16_t length = static_cast<uint16_t>(2 + 2 * numBoundingSet);
  ctx->buffer[posLength++] = static_cast<uint8_t>(length >> 8);
  ctx->buffer[posLength] = static_cast<uint8_t>(length);

  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildAPP(RtcpContext* ctx) {
  // sanity
  if (app_data_ == NULL) {
    LOG(LS_WARNING) << "Failed to build app specific.";
    return BuildResult::kError;
  }
  if (ctx->position + 12 + app_length_ >= IP_PACKET_SIZE) {
    LOG(LS_WARNING) << "Failed to build app specific.";
    return BuildResult::kTruncated;
  }
  *ctx->AllocateData(1) = 0x80 + app_sub_type_;

  // Add APP ID
  *ctx->AllocateData(1) = 204;

  uint16_t length = (app_length_ >> 2) + 2;  // include SSRC and name
  *ctx->AllocateData(1) = static_cast<uint8_t>(length >> 8);
  *ctx->AllocateData(1) = static_cast<uint8_t>(length);

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // Add our application name
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), app_name_);

  // Add the data
  memcpy(ctx->AllocateData(app_length_), app_data_.get(), app_length_);

  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildNACK(RtcpContext* ctx) {
  // sanity
  if (ctx->position + 16 >= IP_PACKET_SIZE) {
    LOG(LS_WARNING) << "Failed to build NACK.";
    return BuildResult::kTruncated;
  }

  // int size, uint16_t* nack_list
  // add nack list
  uint8_t FMT = 1;
  *ctx->AllocateData(1) = 0x80 + FMT;
  *ctx->AllocateData(1) = 205;

  *ctx->AllocateData(1) = 0;
  int nack_size_pos_ = ctx->position;
  *ctx->AllocateData(1) = 3;  // setting it to one kNACK signal as default

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // Add the remote SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), remote_ssrc_);

  // Build NACK bitmasks and write them to the RTCP message.
  // The nack list should be sorted and not contain duplicates if one
  // wants to build the smallest rtcp nack packet.
  int numOfNackFields = 0;
  int maxNackFields =
      std::min<int>(kRtcpMaxNackFields, (IP_PACKET_SIZE - ctx->position) / 4);
  int i = 0;
  while (i < ctx->nack_size && numOfNackFields < maxNackFields) {
    uint16_t nack = ctx->nack_list[i++];
    uint16_t bitmask = 0;
    while (i < ctx->nack_size) {
      int shift = static_cast<uint16_t>(ctx->nack_list[i] - nack) - 1;
      if (shift >= 0 && shift <= 15) {
        bitmask |= (1 << shift);
        ++i;
      } else {
        break;
      }
    }
    // Write the sequence number and the bitmask to the packet.
    assert(ctx->position + 4 < IP_PACKET_SIZE);
    ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2), nack);
    ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2), bitmask);
    numOfNackFields++;
  }
  ctx->buffer[nack_size_pos_] = static_cast<uint8_t>(2 + numOfNackFields);

  if (i != ctx->nack_size)
    LOG(LS_WARNING) << "Nack list too large for one packet.";

  // Report stats.
  NACKStringBuilder stringBuilder;
  for (int idx = 0; idx < i; ++idx) {
    stringBuilder.PushNACK(ctx->nack_list[idx]);
    nack_stats_.ReportRequest(ctx->nack_list[idx]);
  }
  packet_type_counter_.nack_requests = nack_stats_.requests();
  packet_type_counter_.unique_nack_requests = nack_stats_.unique_requests();

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "RTCPSender::NACK", "nacks",
                       TRACE_STR_COPY(stringBuilder.GetResult().c_str()));
  ++packet_type_counter_.nack_packets;
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RTCP_NACKCount",
                    ssrc_, packet_type_counter_.nack_packets);

  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildBYE(RtcpContext* ctx) {
  // sanity
  if (ctx->position + 8 >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  // Add a bye packet
  // Number of SSRC + CSRCs.
  *ctx->AllocateData(1) = static_cast<uint8_t>(0x80 + 1 + csrcs_.size());
  *ctx->AllocateData(1) = 203;

  // length
  *ctx->AllocateData(1) = 0;
  *ctx->AllocateData(1) = static_cast<uint8_t>(1 + csrcs_.size());

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // add CSRCs
  for (size_t i = 0; i < csrcs_.size(); i++)
    ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), csrcs_[i]);

  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildReceiverReferenceTime(
    RtcpContext* ctx) {
  const int kRrTimeBlockLength = 20;
  if (ctx->position + kRrTimeBlockLength >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  if (last_xr_rr_.size() >= RTCP_NUMBER_OF_SR)
    last_xr_rr_.erase(last_xr_rr_.begin());
  last_xr_rr_.insert(std::pair<uint32_t, int64_t>(
      RTCPUtility::MidNtp(ctx->ntp_sec, ctx->ntp_frac),
      Clock::NtpToMs(ctx->ntp_sec, ctx->ntp_frac)));

  // Add XR header.
  *ctx->AllocateData(1) = 0x80;
  *ctx->AllocateData(1) = 207;
  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       4);  // XR packet length.

  // Add our own SSRC.
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |     BT=4      |   reserved    |       block length = 2        |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |              NTP timestamp, most significant word             |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |             NTP timestamp, least significant word             |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Add Receiver Reference Time Report block.
  *ctx->AllocateData(1) = 4;  // BT.
  *ctx->AllocateData(1) = 0;  // Reserved.
  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       2);  // Block length.

  // NTP timestamp.
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ctx->ntp_sec);
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ctx->ntp_frac);

  return BuildResult::kSuccess;
}

RTCPSender::BuildResult RTCPSender::BuildDlrr(RtcpContext* ctx) {
  const int kDlrrBlockLength = 24;
  if (ctx->position + kDlrrBlockLength >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  // Add XR header.
  *ctx->AllocateData(1) = 0x80;
  *ctx->AllocateData(1) = 207;
  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       5);  // XR packet length.

  // Add our own SSRC.
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  //   0                   1                   2                   3
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |     BT=5      |   reserved    |         block length          |
  //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  //  |                 SSRC_1 (SSRC of first receiver)               | sub-
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  //  |                         last RR (LRR)                         |   1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |                   delay since last RR (DLRR)                  |
  //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  //  |                 SSRC_2 (SSRC of second receiver)              | sub-
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  //  :                               ...                             :   2

  // Add DLRR sub block.
  *ctx->AllocateData(1) = 5;  // BT.
  *ctx->AllocateData(1) = 0;  // Reserved.
  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       3);  // Block length.

  // NTP timestamp.

  const RtcpReceiveTimeInfo& info = ctx->feedback_state.last_xr_rr;
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), info.sourceSSRC);
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), info.lastRR);
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4),
                                       info.delaySinceLastRR);

  return BuildResult::kSuccess;
}

// TODO(sprang): Add a unit test for this, or remove if the code isn't used.
RTCPSender::BuildResult RTCPSender::BuildVoIPMetric(RtcpContext* ctx) {
  // sanity
  if (ctx->position + 44 >= IP_PACKET_SIZE)
    return BuildResult::kTruncated;

  // Add XR header
  *ctx->AllocateData(1) = 0x80;
  *ctx->AllocateData(1) = 207;

  uint32_t XRLengthPos = ctx->position;

  // handle length later on
  ctx->AllocateData(2);

  // Add our own SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), ssrc_);

  // Add a VoIP metrics block
  *ctx->AllocateData(1) = 7;
  *ctx->AllocateData(1) = 0;
  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2), 8);

  // Add the remote SSRC
  ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), remote_ssrc_);

  *ctx->AllocateData(1) = xr_voip_metric_.lossRate;
  *ctx->AllocateData(1) = xr_voip_metric_.discardRate;
  *ctx->AllocateData(1) = xr_voip_metric_.burstDensity;
  *ctx->AllocateData(1) = xr_voip_metric_.gapDensity;

  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       xr_voip_metric_.burstDuration);
  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       xr_voip_metric_.gapDuration);

  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       xr_voip_metric_.roundTripDelay);
  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       xr_voip_metric_.endSystemDelay);

  *ctx->AllocateData(1) = xr_voip_metric_.signalLevel;
  *ctx->AllocateData(1) = xr_voip_metric_.noiseLevel;
  *ctx->AllocateData(1) = xr_voip_metric_.RERL;
  *ctx->AllocateData(1) = xr_voip_metric_.Gmin;

  *ctx->AllocateData(1) = xr_voip_metric_.Rfactor;
  *ctx->AllocateData(1) = xr_voip_metric_.extRfactor;
  *ctx->AllocateData(1) = xr_voip_metric_.MOSLQ;
  *ctx->AllocateData(1) = xr_voip_metric_.MOSCQ;

  *ctx->AllocateData(1) = xr_voip_metric_.RXconfig;
  *ctx->AllocateData(1) = 0;  // reserved

  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       xr_voip_metric_.JBnominal);
  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       xr_voip_metric_.JBmax);
  ByteWriter<uint16_t>::WriteBigEndian(ctx->AllocateData(2),
                                       xr_voip_metric_.JBabsMax);

  ByteWriter<uint16_t>::WriteBigEndian(&ctx->buffer[XRLengthPos], 10);

  return BuildResult::kSuccess;
}

int32_t RTCPSender::SendRTCP(const FeedbackState& feedback_state,
                             RTCPPacketType packetType,
                             int32_t nack_size,
                             const uint16_t* nack_list,
                             bool repeat,
                             uint64_t pictureID) {
  return SendCompoundRTCP(
      feedback_state, std::set<RTCPPacketType>(&packetType, &packetType + 1),
      nack_size, nack_list, repeat, pictureID);
}

int32_t RTCPSender::SendCompoundRTCP(
    const FeedbackState& feedback_state,
    const std::set<RTCPPacketType>& packetTypes,
    int32_t nack_size,
    const uint16_t* nack_list,
    bool repeat,
    uint64_t pictureID) {
  {
    CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
    if (method_ == kRtcpOff) {
      LOG(LS_WARNING) << "Can't send rtcp if it is disabled.";
      return -1;
    }
  }
  uint8_t rtcp_buffer[IP_PACKET_SIZE];
  int rtcp_length =
      PrepareRTCP(feedback_state, packetTypes, nack_size, nack_list, repeat,
                  pictureID, rtcp_buffer, IP_PACKET_SIZE);

  // Sanity don't send empty packets.
  if (rtcp_length <= 0)
    return -1;

  return SendToNetwork(rtcp_buffer, static_cast<size_t>(rtcp_length));
}

int RTCPSender::PrepareRTCP(const FeedbackState& feedback_state,
                            const std::set<RTCPPacketType>& packetTypes,
                            int32_t nack_size,
                            const uint16_t* nack_list,
                            bool repeat,
                            uint64_t pictureID,
                            uint8_t* rtcp_buffer,
                            int buffer_size) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  RtcpContext context(feedback_state, nack_size, nack_list, repeat, pictureID,
                      rtcp_buffer, buffer_size);

  // Add all flags as volatile. Non volatile entries will not be overwritten
  // and all new volatile flags added will be consumed by the end of this call.
  SetFlags(packetTypes, true);

  if (packet_type_counter_.first_packet_time_ms == -1)
    packet_type_counter_.first_packet_time_ms = clock_->TimeInMilliseconds();

  bool generate_report;
  if (IsFlagPresent(kRtcpSr) || IsFlagPresent(kRtcpRr)) {
    // Report type already explicitly set, don't automatically populate.
    generate_report = true;
    DCHECK(ConsumeFlag(kRtcpReport) == false);
  } else {
    generate_report =
        (ConsumeFlag(kRtcpReport) && method_ == kRtcpNonCompound) ||
        method_ == kRtcpCompound;
    if (generate_report)
      SetFlag(sending_ ? kRtcpSr : kRtcpRr, true);
  }

  if (IsFlagPresent(kRtcpSr) || (IsFlagPresent(kRtcpRr) && cname_[0] != 0))
    SetFlag(kRtcpSdes, true);

  // We need to send our NTP even if we haven't received any reports.
  clock_->CurrentNtp(context.ntp_sec, context.ntp_frac);

  if (generate_report) {
    if (!sending_ && xr_send_receiver_reference_time_enabled_)
      SetFlag(kRtcpXrReceiverReferenceTime, true);
    if (feedback_state.has_last_xr_rr)
      SetFlag(kRtcpXrDlrrReportBlock, true);

    // generate next time to send an RTCP report
    // seeded from RTP constructor
    int32_t random = rand() % 1000;
    int32_t timeToNext = RTCP_INTERVAL_AUDIO_MS;

    if (audio_) {
      timeToNext = (RTCP_INTERVAL_AUDIO_MS / 2) +
                   (RTCP_INTERVAL_AUDIO_MS * random / 1000);
    } else {
      uint32_t minIntervalMs = RTCP_INTERVAL_AUDIO_MS;
      if (sending_) {
        // Calculate bandwidth for video; 360 / send bandwidth in kbit/s.
        uint32_t send_bitrate_kbit = feedback_state.send_bitrate / 1000;
        if (send_bitrate_kbit != 0)
          minIntervalMs = 360000 / send_bitrate_kbit;
      }
      if (minIntervalMs > RTCP_INTERVAL_VIDEO_MS)
        minIntervalMs = RTCP_INTERVAL_VIDEO_MS;
      timeToNext = (minIntervalMs / 2) + (minIntervalMs * random / 1000);
    }
    next_time_to_send_rtcp_ = clock_->TimeInMilliseconds() + timeToNext;

    StatisticianMap statisticians =
        receive_statistics_->GetActiveStatisticians();
    if (!statisticians.empty()) {
      for (auto it = statisticians.begin(); it != statisticians.end(); ++it) {
        RTCPReportBlock report_block;
        if (PrepareReport(feedback_state, it->second, &report_block,
                          &context.ntp_sec, &context.ntp_frac)) {
          AddReportBlock(it->first, &internal_report_blocks_, &report_block);
        }
      }
      if (extended_jitter_report_enabled_)
        SetFlag(kRtcpTransmissionTimeOffset, true);
    }
  }

  auto it = report_flags_.begin();
  while (it != report_flags_.end()) {
    auto builder = builders_.find(it->type);
    DCHECK(builder != builders_.end());
    if (it->is_volatile) {
      report_flags_.erase(it++);
    } else {
      ++it;
    }

    uint32_t start_position = context.position;
    BuildResult result = (*this.*(builder->second))(&context);
    switch (result) {
      case BuildResult::kError:
        return -1;
      case BuildResult::kTruncated:
        return context.position;
      case BuildResult::kAborted:
        context.position = start_position;
        FALLTHROUGH();
      case BuildResult::kSuccess:
        continue;
      default:
        abort();
    }
  }

  if (packet_type_counter_observer_ != NULL) {
    packet_type_counter_observer_->RtcpPacketTypesCounterUpdated(
        remote_ssrc_, packet_type_counter_);
  }

  DCHECK(AllVolatileFlagsConsumed());

  return context.position;
}

bool RTCPSender::PrepareReport(const FeedbackState& feedback_state,
                               StreamStatistician* statistician,
                               RTCPReportBlock* report_block,
                               uint32_t* ntp_secs, uint32_t* ntp_frac) {
  // Do we have receive statistics to send?
  RtcpStatistics stats;
  if (!statistician->GetStatistics(&stats, true))
    return false;
  report_block->fractionLost = stats.fraction_lost;
  report_block->cumulativeLost = stats.cumulative_lost;
  report_block->extendedHighSeqNum =
      stats.extended_max_sequence_number;
  report_block->jitter = stats.jitter;

  // get our NTP as late as possible to avoid a race
  clock_->CurrentNtp(*ntp_secs, *ntp_frac);

  // Delay since last received report
  uint32_t delaySinceLastReceivedSR = 0;
  if ((feedback_state.last_rr_ntp_secs != 0) ||
      (feedback_state.last_rr_ntp_frac != 0)) {
    // get the 16 lowest bits of seconds and the 16 higest bits of fractions
    uint32_t now = *ntp_secs & 0x0000FFFF;
    now <<= 16;
    now += (*ntp_frac & 0xffff0000) >> 16;

    uint32_t receiveTime = feedback_state.last_rr_ntp_secs & 0x0000FFFF;
    receiveTime <<= 16;
    receiveTime += (feedback_state.last_rr_ntp_frac & 0xffff0000) >> 16;

    delaySinceLastReceivedSR = now-receiveTime;
  }
  report_block->delaySinceLastSR = delaySinceLastReceivedSR;
  report_block->lastSR = feedback_state.remote_sr;
  return true;
}

int32_t RTCPSender::SendToNetwork(const uint8_t* dataBuffer, size_t length) {
  CriticalSectionScoped lock(critical_section_transport_.get());
  if (cbTransport_) {
    if (cbTransport_->SendRTCPPacket(id_, dataBuffer, length) > 0)
      return 0;
  }
  return -1;
}

void RTCPSender::SetCsrcs(const std::vector<uint32_t>& csrcs) {
  assert(csrcs.size() <= kRtpCsrcSize);
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  csrcs_ = csrcs;
}

int32_t RTCPSender::SetApplicationSpecificData(uint8_t subType,
                                               uint32_t name,
                                               const uint8_t* data,
                                               uint16_t length) {
  if (length % 4 != 0) {
    LOG(LS_ERROR) << "Failed to SetApplicationSpecificData.";
    return -1;
  }
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  SetFlag(kRtcpApp, true);
  app_sub_type_ = subType;
  app_name_ = name;
  app_data_.reset(new uint8_t[length]);
  app_length_ = length;
  memcpy(app_data_.get(), data, length);
  return 0;
}

int32_t RTCPSender::SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  memcpy(&xr_voip_metric_, VoIPMetric, sizeof(RTCPVoIPMetric));

  SetFlag(kRtcpXrVoipMetric, true);
  return 0;
}

void RTCPSender::SendRtcpXrReceiverReferenceTime(bool enable) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  xr_send_receiver_reference_time_enabled_ = enable;
}

bool RTCPSender::RtcpXrReceiverReferenceTime() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return xr_send_receiver_reference_time_enabled_;
}

// called under critsect critical_section_rtcp_sender_
RTCPSender::BuildResult RTCPSender::WriteAllReportBlocksToBuffer(
    RtcpContext* ctx,
    uint8_t* numberOfReportBlocks) {
  *numberOfReportBlocks = external_report_blocks_.size();
  *numberOfReportBlocks += internal_report_blocks_.size();
  if ((ctx->position + *numberOfReportBlocks * 24) >= IP_PACKET_SIZE) {
    LOG(LS_WARNING) << "Can't fit all report blocks.";
    return BuildResult::kError;
  }
  WriteReportBlocksToBuffer(ctx, internal_report_blocks_);
  while (!internal_report_blocks_.empty()) {
    delete internal_report_blocks_.begin()->second;
    internal_report_blocks_.erase(internal_report_blocks_.begin());
  }
  WriteReportBlocksToBuffer(ctx, external_report_blocks_);
  return BuildResult::kSuccess;
}

void RTCPSender::WriteReportBlocksToBuffer(
    RtcpContext* ctx,
    const std::map<uint32_t, RTCPReportBlock*>& report_blocks) {
  std::map<uint32_t, RTCPReportBlock*>::const_iterator it =
      report_blocks.begin();
  for (; it != report_blocks.end(); it++) {
    uint32_t remoteSSRC = it->first;
    RTCPReportBlock* reportBlock = it->second;
    if (reportBlock) {
      // Remote SSRC
      ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4), remoteSSRC);

      // fraction lost
      *ctx->AllocateData(1) = reportBlock->fractionLost;

      // cumulative loss
      ByteWriter<uint32_t, 3>::WriteBigEndian(ctx->AllocateData(3),
                                              reportBlock->cumulativeLost);

      // extended highest seq_no, contain the highest sequence number received
      ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4),
                                           reportBlock->extendedHighSeqNum);

      // Jitter
      ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4),
                                           reportBlock->jitter);

      ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4),
                                           reportBlock->lastSR);

      ByteWriter<uint32_t>::WriteBigEndian(ctx->AllocateData(4),
                                           reportBlock->delaySinceLastSR);
    }
  }
}

// no callbacks allowed inside this function
int32_t RTCPSender::SetTMMBN(const TMMBRSet* boundingSet,
                             uint32_t maxBitrateKbit) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  if (0 == tmmbr_help_.SetTMMBRBoundingSetToSend(boundingSet, maxBitrateKbit)) {
    SetFlag(kRtcpTmmbn, true);
    return 0;
  }
  return -1;
}

void RTCPSender::SetFlag(RTCPPacketType type, bool is_volatile) {
  report_flags_.insert(ReportFlag(type, is_volatile));
}

void RTCPSender::SetFlags(const std::set<RTCPPacketType>& types,
                          bool is_volatile) {
  for (RTCPPacketType type : types)
    SetFlag(type, is_volatile);
}

bool RTCPSender::IsFlagPresent(RTCPPacketType type) const {
  return report_flags_.find(ReportFlag(type, false)) != report_flags_.end();
}

bool RTCPSender::ConsumeFlag(RTCPPacketType type, bool forced) {
  auto it = report_flags_.find(ReportFlag(type, false));
  if (it == report_flags_.end())
    return false;
  if (it->is_volatile || forced)
    report_flags_.erase((it));
  return true;
}

bool RTCPSender::AllVolatileFlagsConsumed() const {
  for (const ReportFlag& flag : report_flags_) {
    if (flag.is_volatile)
      return false;
  }
  return true;
}

}  // namespace webrtc
