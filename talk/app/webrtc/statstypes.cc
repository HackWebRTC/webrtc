/*
 * libjingle
 * Copyright 2014, Google Inc.
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

namespace webrtc {

// The items below are in alphabetical order.
const char StatsReport::kStatsValueNameActiveConnection[] =
    "googActiveConnection";
const char StatsReport::kStatsValueNameActualEncBitrate[] =
    "googActualEncBitrate";
const char StatsReport::kStatsValueNameAudioOutputLevel[] = "audioOutputLevel";
const char StatsReport::kStatsValueNameAudioInputLevel[] = "audioInputLevel";
const char StatsReport::kStatsValueNameAvailableReceiveBandwidth[] =
    "googAvailableReceiveBandwidth";
const char StatsReport::kStatsValueNameAvailableSendBandwidth[] =
    "googAvailableSendBandwidth";
const char StatsReport::kStatsValueNameAvgEncodeMs[] = "googAvgEncodeMs";
const char StatsReport::kStatsValueNameBucketDelay[] = "googBucketDelay";
const char StatsReport::kStatsValueNameBytesReceived[] = "bytesReceived";
const char StatsReport::kStatsValueNameBytesSent[] = "bytesSent";
const char StatsReport::kStatsValueNameBandwidthLimitedResolution[] =
    "googBandwidthLimitedResolution";
const char StatsReport::kStatsValueNameCaptureJitterMs[] =
    "googCaptureJitterMs";
const char StatsReport::kStatsValueNameCaptureQueueDelayMsPerS[] =
    "googCaptureQueueDelayMsPerS";
const char StatsReport::kStatsValueNameChannelId[] = "googChannelId";
const char StatsReport::kStatsValueNameCodecName[] = "googCodecName";
const char StatsReport::kStatsValueNameComponent[] = "googComponent";
const char StatsReport::kStatsValueNameContentName[] = "googContentName";
const char StatsReport::kStatsValueNameCpuLimitedResolution[] =
    "googCpuLimitedResolution";
const char StatsReport::kStatsValueNameDecodingCTSG[] =
    "googDecodingCTSG";
const char StatsReport::kStatsValueNameDecodingCTN[] =
    "googDecodingCTN";
const char StatsReport::kStatsValueNameDecodingNormal[] =
    "googDecodingNormal";
const char StatsReport::kStatsValueNameDecodingPLC[] =
    "googDecodingPLC";
const char StatsReport::kStatsValueNameDecodingCNG[] =
    "googDecodingCNG";
const char StatsReport::kStatsValueNameDecodingPLCCNG[] =
    "googDecodingPLCCNG";
const char StatsReport::kStatsValueNameDer[] = "googDerBase64";
// Echo metrics from the audio processing module.
const char StatsReport::kStatsValueNameEchoCancellationQualityMin[] =
    "googEchoCancellationQualityMin";
const char StatsReport::kStatsValueNameEchoDelayMedian[] =
    "googEchoCancellationEchoDelayMedian";
const char StatsReport::kStatsValueNameEchoDelayStdDev[] =
    "googEchoCancellationEchoDelayStdDev";
const char StatsReport::kStatsValueNameEchoReturnLoss[] =
    "googEchoCancellationReturnLoss";
const char StatsReport::kStatsValueNameEchoReturnLossEnhancement[] =
    "googEchoCancellationReturnLossEnhancement";

const char StatsReport::kStatsValueNameEncodeUsagePercent[] =
    "googEncodeUsagePercent";
const char StatsReport::kStatsValueNameExpandRate[] = "googExpandRate";
const char StatsReport::kStatsValueNameFingerprint[] = "googFingerprint";
const char StatsReport::kStatsValueNameFingerprintAlgorithm[] =
    "googFingerprintAlgorithm";
const char StatsReport::kStatsValueNameFirsReceived[] = "googFirsReceived";
const char StatsReport::kStatsValueNameFirsSent[] = "googFirsSent";
const char StatsReport::kStatsValueNameFrameHeightInput[] =
    "googFrameHeightInput";
const char StatsReport::kStatsValueNameFrameHeightReceived[] =
    "googFrameHeightReceived";
const char StatsReport::kStatsValueNameFrameHeightSent[] =
    "googFrameHeightSent";
const char StatsReport::kStatsValueNameFrameRateReceived[] =
    "googFrameRateReceived";
const char StatsReport::kStatsValueNameFrameRateDecoded[] =
    "googFrameRateDecoded";
const char StatsReport::kStatsValueNameFrameRateOutput[] =
    "googFrameRateOutput";
const char StatsReport::kStatsValueNameDecodeMs[] = "googDecodeMs";
const char StatsReport::kStatsValueNameMaxDecodeMs[] = "googMaxDecodeMs";
const char StatsReport::kStatsValueNameCurrentDelayMs[] = "googCurrentDelayMs";
const char StatsReport::kStatsValueNameTargetDelayMs[] = "googTargetDelayMs";
const char StatsReport::kStatsValueNameJitterBufferMs[] = "googJitterBufferMs";
const char StatsReport::kStatsValueNameMinPlayoutDelayMs[] =
    "googMinPlayoutDelayMs";
const char StatsReport::kStatsValueNameRenderDelayMs[] = "googRenderDelayMs";

const char StatsReport::kStatsValueNameCaptureStartNtpTimeMs[] =
    "googCaptureStartNtpTimeMs";

const char StatsReport::kStatsValueNameFrameRateInput[] = "googFrameRateInput";
const char StatsReport::kStatsValueNameFrameRateSent[] = "googFrameRateSent";
const char StatsReport::kStatsValueNameFrameWidthInput[] =
    "googFrameWidthInput";
const char StatsReport::kStatsValueNameFrameWidthReceived[] =
    "googFrameWidthReceived";
const char StatsReport::kStatsValueNameFrameWidthSent[] = "googFrameWidthSent";
const char StatsReport::kStatsValueNameInitiator[] = "googInitiator";
const char StatsReport::kStatsValueNameIssuerId[] = "googIssuerId";
const char StatsReport::kStatsValueNameJitterReceived[] = "googJitterReceived";
const char StatsReport::kStatsValueNameLocalAddress[] = "googLocalAddress";
const char StatsReport::kStatsValueNameLocalCandidateType[] =
    "googLocalCandidateType";
const char StatsReport::kStatsValueNameLocalCertificateId[] =
    "googLocalCertificateId";
const char StatsReport::kStatsValueNameAdaptationChanges[] =
    "googAdaptationChanges";
const char StatsReport::kStatsValueNameNacksReceived[] = "googNacksReceived";
const char StatsReport::kStatsValueNameNacksSent[] = "googNacksSent";
const char StatsReport::kStatsValueNamePlisReceived[] = "googPlisReceived";
const char StatsReport::kStatsValueNamePlisSent[] = "googPlisSent";
const char StatsReport::kStatsValueNamePacketsReceived[] = "packetsReceived";
const char StatsReport::kStatsValueNamePacketsSent[] = "packetsSent";
const char StatsReport::kStatsValueNamePacketsLost[] = "packetsLost";
const char StatsReport::kStatsValueNamePreferredJitterBufferMs[] =
    "googPreferredJitterBufferMs";
const char StatsReport::kStatsValueNameReadable[] = "googReadable";
const char StatsReport::kStatsValueNameRecvPacketGroupArrivalTimeDebug[] =
    "googReceivedPacketGroupArrivalTimeDebug";
const char StatsReport::kStatsValueNameRecvPacketGroupPropagationDeltaDebug[] =
    "googReceivedPacketGroupPropagationDeltaDebug";
const char
StatsReport::kStatsValueNameRecvPacketGroupPropagationDeltaSumDebug[] =
    "googReceivedPacketGroupPropagationDeltaSumDebug";
const char StatsReport::kStatsValueNameRemoteAddress[] = "googRemoteAddress";
const char StatsReport::kStatsValueNameRemoteCandidateType[] =
    "googRemoteCandidateType";
const char StatsReport::kStatsValueNameRemoteCertificateId[] =
    "googRemoteCertificateId";
const char StatsReport::kStatsValueNameRetransmitBitrate[] =
    "googRetransmitBitrate";
const char StatsReport::kStatsValueNameRtt[] = "googRtt";
const char StatsReport::kStatsValueNameSsrc[] = "ssrc";
const char StatsReport::kStatsValueNameSendPacketsDiscarded[] =
    "packetsDiscardedOnSend";
const char StatsReport::kStatsValueNameTargetEncBitrate[] =
    "googTargetEncBitrate";
const char StatsReport::kStatsValueNameTransmitBitrate[] =
    "googTransmitBitrate";
const char StatsReport::kStatsValueNameTransportId[] = "transportId";
const char StatsReport::kStatsValueNameTransportType[] = "googTransportType";
const char StatsReport::kStatsValueNameTrackId[] = "googTrackId";
const char StatsReport::kStatsValueNameTypingNoiseState[] =
    "googTypingNoiseState";
const char StatsReport::kStatsValueNameViewLimitedResolution[] =
    "googViewLimitedResolution";
const char StatsReport::kStatsValueNameWritable[] = "googWritable";

const char StatsReport::kStatsReportTypeSession[] = "googLibjingleSession";
const char StatsReport::kStatsReportTypeBwe[] = "VideoBwe";
const char StatsReport::kStatsReportTypeRemoteSsrc[] = "remoteSsrc";
const char StatsReport::kStatsReportTypeSsrc[] = "ssrc";
const char StatsReport::kStatsReportTypeTrack[] = "googTrack";
const char StatsReport::kStatsReportTypeIceCandidate[] = "iceCandidate";
const char StatsReport::kStatsReportTypeTransport[] = "googTransport";
const char StatsReport::kStatsReportTypeComponent[] = "googComponent";
const char StatsReport::kStatsReportTypeCandidatePair[] = "googCandidatePair";
const char StatsReport::kStatsReportTypeCertificate[] = "googCertificate";

const char StatsReport::kStatsReportVideoBweId[] = "bweforvideo";

StatsReport::StatsReport(const StatsReport& src)
  : id(src.id),
    type(src.type),
    timestamp(src.timestamp),
    values(src.values) {
}

StatsReport::StatsReport(const std::string& id)
    : id(id), timestamp(0) {
}

StatsReport& StatsReport::operator=(const StatsReport& src) {
  ASSERT(id == src.id);
  type = src.type;
  timestamp = src.timestamp;
  values = src.values;
  return *this;
}

// Operators provided for STL container/algorithm support.
bool StatsReport::operator<(const StatsReport& other) const {
  return id < other.id;
}

bool StatsReport::operator==(const StatsReport& other) const {
  return id == other.id;
}

// Special support for being able to use std::find on a container
// without requiring a new StatsReport instance.
bool StatsReport::operator==(const std::string& other_id) const {
  return id == other_id;
}

StatsReport::Value::Value()
    : name(NULL) {
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

// TODO(tommi): Change implementation to do a simple enum value-to-static-
// string conversion when client code has been updated to use this method
// instead of the |name| member variable.
const char* StatsReport::Value::display_name() const {
  return name;
}

void StatsReport::AddValue(StatsReport::StatsValueName name,
                           const std::string& value) {
  values.push_back(Value(name, value));
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
  for (Values::iterator it = values.begin(); it != values.end(); ++it) {
    if ((*it).name == name) {
      it->value = value;
      return;
    }
  }
  // It is not reachable here, add an ASSERT to make sure the overwriting is
  // always a success.
  ASSERT(false);
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
