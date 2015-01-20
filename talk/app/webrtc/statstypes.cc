/*
 * libjingle
 * Copyright 2014 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/app/webrtc/statstypes.h"

using rtc::scoped_ptr;

namespace webrtc {

const char StatsReport::kStatsReportTypeSession[] = "googLibjingleSession";
const char StatsReport::kStatsReportTypeBwe[] = "VideoBwe";
const char StatsReport::kStatsReportTypeRemoteSsrc[] = "remoteSsrc";
const char StatsReport::kStatsReportTypeSsrc[] = "ssrc";
const char StatsReport::kStatsReportTypeTrack[] = "googTrack";
const char StatsReport::kStatsReportTypeIceLocalCandidate[] = "localcandidate";
const char StatsReport::kStatsReportTypeIceRemoteCandidate[] =
    "remotecandidate";
const char StatsReport::kStatsReportTypeTransport[] = "googTransport";
const char StatsReport::kStatsReportTypeComponent[] = "googComponent";
const char StatsReport::kStatsReportTypeCandidatePair[] = "googCandidatePair";
const char StatsReport::kStatsReportTypeCertificate[] = "googCertificate";
const char StatsReport::kStatsReportTypeDataChannel[] = "datachannel";

const char StatsReport::kStatsReportVideoBweId[] = "bweforvideo";

StatsReport::StatsReport(const StatsReport& src)
  : id_(src.id_),
    type(src.type),
    timestamp_(src.timestamp_),
    values_(src.values_) {
}

StatsReport::StatsReport(const std::string& id)
    : id_(id), timestamp_(0) {
}

StatsReport::StatsReport(scoped_ptr<StatsReport::Id> id)
    : id_(id->ToString()), timestamp_(0) {
}

// static
scoped_ptr<StatsReport::Id> StatsReport::NewTypedId(
    StatsReport::StatsType type, const std::string& id) {
  std::string internal_id(type);
  internal_id += '_';
  internal_id += id;
  return scoped_ptr<Id>(new Id(internal_id)).Pass();
}

StatsReport& StatsReport::operator=(const StatsReport& src) {
  ASSERT(id_ == src.id_);
  type = src.type;
  timestamp_ = src.timestamp_;
  values_ = src.values_;
  return *this;
}

// Operators provided for STL container/algorithm support.
bool StatsReport::operator<(const StatsReport& other) const {
  return id_ < other.id_;
}

bool StatsReport::operator==(const StatsReport& other) const {
  return id_ == other.id_;
}

// Special support for being able to use std::find on a container
// without requiring a new StatsReport instance.
bool StatsReport::operator==(const std::string& other_id) const {
  return id_ == other_id;
}

// The copy ctor can't be declared as explicit due to problems with STL.
StatsReport::Value::Value(const Value& other)
    : name(other.name), value(other.value) {
}

StatsReport::Value::Value(StatsValueName name)
    : name(name) {
}

StatsReport::Value::Value(StatsValueName name, const std::string& value)
    : name(name), value(value) {
}

StatsReport::Value& StatsReport::Value::operator=(const Value& other) {
  const_cast<StatsValueName&>(name) = other.name;
  value = other.value;
  return *this;
}

const char* StatsReport::Value::display_name() const {
  switch (name) {
    case kStatsValueNameAudioOutputLevel:
      return "audioOutputLevel";
    case kStatsValueNameAudioInputLevel:
      return "audioInputLevel";
    case kStatsValueNameBytesSent:
      return "bytesSent";
    case kStatsValueNamePacketsSent:
      return "packetsSent";
    case kStatsValueNameBytesReceived:
      return "bytesReceived";
    case kStatsValueNameLabel:
      return "label";
    case kStatsValueNamePacketsReceived:
      return "packetsReceived";
    case kStatsValueNamePacketsLost:
      return "packetsLost";
    case kStatsValueNameProtocol:
      return "protocol";
    case kStatsValueNameTransportId:
      return "transportId";
    case kStatsValueNameSsrc:
      return "ssrc";
    case kStatsValueNameState:
      return "state";
    case kStatsValueNameDataChannelId:
      return "datachannelid";

    // 'goog' prefixed constants.
    case kStatsValueNameActiveConnection:
      return "googActiveConnection";
    case kStatsValueNameActualEncBitrate:
      return "googActualEncBitrate";
    case kStatsValueNameAvailableReceiveBandwidth:
      return "googAvailableReceiveBandwidth";
    case kStatsValueNameAvailableSendBandwidth:
      return "googAvailableSendBandwidth";
    case kStatsValueNameAvgEncodeMs:
      return "googAvgEncodeMs";
    case kStatsValueNameBucketDelay:
      return "googBucketDelay";
    case kStatsValueNameBandwidthLimitedResolution:
      return "googBandwidthLimitedResolution";
    case kStatsValueNameCaptureJitterMs:
      return "googCaptureJitterMs";
    case kStatsValueNameCaptureQueueDelayMsPerS:
      return "googCaptureQueueDelayMsPerS";

    // Candidate related attributes. Values are taken from
    // http://w3c.github.io/webrtc-stats/#rtcstatstype-enum*.
    case kStatsValueNameCandidateIPAddress:
      return "ipAddress";
    case kStatsValueNameCandidateNetworkType:
      return "networkType";
    case kStatsValueNameCandidatePortNumber:
      return "portNumber";
    case kStatsValueNameCandidatePriority:
      return "priority";
    case kStatsValueNameCandidateTransportType:
      return "transport";
    case kStatsValueNameCandidateType:
      return "candidateType";

    case kStatsValueNameChannelId:
      return "googChannelId";
    case kStatsValueNameCodecName:
      return "googCodecName";
    case kStatsValueNameComponent:
      return "googComponent";
    case kStatsValueNameContentName:
      return "googContentName";
    case kStatsValueNameCpuLimitedResolution:
      return "googCpuLimitedResolution";
    case kStatsValueNameDecodingCTSG:
      return "googDecodingCTSG";
    case kStatsValueNameDecodingCTN:
      return "googDecodingCTN";
    case kStatsValueNameDecodingNormal:
      return "googDecodingNormal";
    case kStatsValueNameDecodingPLC:
      return "googDecodingPLC";
    case kStatsValueNameDecodingCNG:
      return "googDecodingCNG";
    case kStatsValueNameDecodingPLCCNG:
      return "googDecodingPLCCNG";
    case kStatsValueNameDer:
      return "googDerBase64";
    case kStatsValueNameEchoCancellationQualityMin:
      return "googEchoCancellationQualityMin";
    case kStatsValueNameEchoDelayMedian:
      return "googEchoCancellationEchoDelayMedian";
    case kStatsValueNameEchoDelayStdDev:
      return "googEchoCancellationEchoDelayStdDev";
    case kStatsValueNameEchoReturnLoss:
      return "googEchoCancellationReturnLoss";
    case kStatsValueNameEchoReturnLossEnhancement:
      return "googEchoCancellationReturnLossEnhancement";
    case kStatsValueNameEncodeUsagePercent:
      return "googEncodeUsagePercent";
    case kStatsValueNameExpandRate:
      return "googExpandRate";
    case kStatsValueNameFingerprint:
      return "googFingerprint";
    case kStatsValueNameFingerprintAlgorithm:
      return "googFingerprintAlgorithm";
    case kStatsValueNameFirsReceived:
      return "googFirsReceived";
    case kStatsValueNameFirsSent:
      return "googFirsSent";
    case kStatsValueNameFrameHeightInput:
      return "googFrameHeightInput";
    case kStatsValueNameFrameHeightReceived:
      return "googFrameHeightReceived";
    case kStatsValueNameFrameHeightSent:
      return "googFrameHeightSent";
    case kStatsValueNameFrameRateReceived:
      return "googFrameRateReceived";
    case kStatsValueNameFrameRateDecoded:
      return "googFrameRateDecoded";
    case kStatsValueNameFrameRateOutput:
      return "googFrameRateOutput";
    case kStatsValueNameDecodeMs:
      return "googDecodeMs";
    case kStatsValueNameMaxDecodeMs:
      return "googMaxDecodeMs";
    case kStatsValueNameCurrentDelayMs:
      return "googCurrentDelayMs";
    case kStatsValueNameTargetDelayMs:
      return "googTargetDelayMs";
    case kStatsValueNameJitterBufferMs:
      return "googJitterBufferMs";
    case kStatsValueNameMinPlayoutDelayMs:
      return "googMinPlayoutDelayMs";
    case kStatsValueNameRenderDelayMs:
      return "googRenderDelayMs";
    case kStatsValueNameCaptureStartNtpTimeMs:
      return "googCaptureStartNtpTimeMs";
    case kStatsValueNameFrameRateInput:
      return "googFrameRateInput";
    case kStatsValueNameFrameRateSent:
      return "googFrameRateSent";
    case kStatsValueNameFrameWidthInput:
      return "googFrameWidthInput";
    case kStatsValueNameFrameWidthReceived:
      return "googFrameWidthReceived";
    case kStatsValueNameFrameWidthSent:
      return "googFrameWidthSent";
    case kStatsValueNameInitiator:
      return "googInitiator";
    case kStatsValueNameIssuerId:
      return "googIssuerId";
    case kStatsValueNameJitterReceived:
      return "googJitterReceived";
    case kStatsValueNameLocalAddress:
      return "googLocalAddress";
    case kStatsValueNameLocalCandidateId:
      return "localCandidateId";
    case kStatsValueNameLocalCandidateType:
      return "googLocalCandidateType";
    case kStatsValueNameLocalCertificateId:
      return "googLocalCertificateId";
    case kStatsValueNameAdaptationChanges:
      return "googAdaptationChanges";
    case kStatsValueNameNacksReceived:
      return "googNacksReceived";
    case kStatsValueNameNacksSent:
      return "googNacksSent";
    case kStatsValueNamePlisReceived:
      return "googPlisReceived";
    case kStatsValueNamePlisSent:
      return "googPlisSent";
    case kStatsValueNamePreferredJitterBufferMs:
      return "googPreferredJitterBufferMs";
    case kStatsValueNameReadable:
      return "googReadable";
    case kStatsValueNameRecvPacketGroupArrivalTimeDebug:
      return "googReceivedPacketGroupArrivalTimeDebug";
    case kStatsValueNameRecvPacketGroupPropagationDeltaDebug:
      return "googReceivedPacketGroupPropagationDeltaDebug";
    case kStatsValueNameRecvPacketGroupPropagationDeltaSumDebug:
      return "googReceivedPacketGroupPropagationDeltaSumDebug";
    case kStatsValueNameRemoteAddress:
      return "googRemoteAddress";
    case kStatsValueNameRemoteCandidateId:
      return "remoteCandidateId";
    case kStatsValueNameRemoteCandidateType:
      return "googRemoteCandidateType";
    case kStatsValueNameRemoteCertificateId:
      return "googRemoteCertificateId";
    case kStatsValueNameRetransmitBitrate:
      return "googRetransmitBitrate";
    case kStatsValueNameRtt:
      return "googRtt";
    case kStatsValueNameSendPacketsDiscarded:
      return "packetsDiscardedOnSend";
    case kStatsValueNameTargetEncBitrate:
      return "googTargetEncBitrate";
    case kStatsValueNameTransmitBitrate:
      return "googTransmitBitrate";
    case kStatsValueNameTransportType:
      return "googTransportType";
    case kStatsValueNameTrackId:
      return "googTrackId";
    case kStatsValueNameTypingNoiseState:
      return "googTypingNoiseState";
    case kStatsValueNameViewLimitedResolution:
      return "googViewLimitedResolution";
    case kStatsValueNameWritable:
      return "googWritable";
    default:
      ASSERT(false);
      break;
  }

  return nullptr;
}

void StatsReport::AddValue(StatsReport::StatsValueName name,
                           const std::string& value) {
  values_.push_back(ValuePtr(new Value(name, value)));
}

void StatsReport::AddValue(StatsReport::StatsValueName name, int64 value) {
  AddValue(name, rtc::ToString<int64>(value));
}

template <typename T>
void StatsReport::AddValue(StatsReport::StatsValueName name,
                           const std::vector<T>& value) {
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < value.size(); ++i) {
    oss << rtc::ToString<T>(value[i]);
    if (i != value.size() - 1)
      oss << ", ";
  }
  oss << "]";
  AddValue(name, oss.str());
}

// Implementation specializations for the variants of AddValue that we use.
// TODO(tommi): Converting these ints to strings and copying strings, is not
// very efficient.  Figure out a way to reduce the string churn.
template
void StatsReport::AddValue<std::string>(
    StatsReport::StatsValueName, const std::vector<std::string>&);

template
void StatsReport::AddValue<int>(
    StatsReport::StatsValueName, const std::vector<int>&);

template
void StatsReport::AddValue<int64_t>(
    StatsReport::StatsValueName, const std::vector<int64_t>&);

void StatsReport::AddBoolean(StatsReport::StatsValueName name, bool value) {
  AddValue(name, value ? "true" : "false");
}

void StatsReport::ReplaceValue(StatsReport::StatsValueName name,
                               const std::string& value) {
  Values::iterator it = std::find_if(values_.begin(), values_.end(),
      [&name](const ValuePtr& v)->bool { return v->name == name; });
  // Values are const once added since they may be used outside of the stats
  // collection. So we remove it from values_ when replacing and add a new one.
  if (it != values_.end()) {
    if ((*it)->value == value)
      return;
    values_.erase(it);
  }

  AddValue(name, value);
}

void StatsReport::ResetValues() {
  values_.clear();
}

StatsSet::StatsSet() {
}

StatsSet::~StatsSet() {
}

StatsSet::const_iterator StatsSet::begin() const {
  return list_.begin();
}

StatsSet::const_iterator StatsSet::end() const {
  return list_.end();
}

StatsReport* StatsSet::InsertNew(const std::string& id) {
  ASSERT(Find(id) == NULL);
  const StatsReport* ret = &(*list_.insert(StatsReportCopyable(id)).first);
  return const_cast<StatsReport*>(ret);
}

StatsReport* StatsSet::FindOrAddNew(const std::string& id) {
  StatsReport* ret = Find(id);
  return ret ? ret : InsertNew(id);
}

StatsReport* StatsSet::ReplaceOrAddNew(const std::string& id) {
  list_.erase(id);
  return InsertNew(id);
}

// Looks for a report with the given |id|.  If one is not found, NULL
// will be returned.
StatsReport* StatsSet::Find(const std::string& id) {
  const_iterator it = std::find(begin(), end(), id);
  return it == end() ? NULL :
      const_cast<StatsReport*>(static_cast<const StatsReport*>(&(*it)));
}

}  // namespace webrtc
