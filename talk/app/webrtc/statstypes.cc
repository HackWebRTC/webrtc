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
namespace {

// The id of StatsReport of type kStatsReportTypeBwe.
const char kStatsReportVideoBweId[] = "bweforvideo";

// NOTE: These names need to be consistent with an external
// specification (W3C Stats Identifiers).
const char* InternalTypeToString(StatsReport::StatsType type) {
  switch (type) {
    case StatsReport::kStatsReportTypeSession:
      return "googLibjingleSession";
    case StatsReport::kStatsReportTypeBwe:
      return "VideoBwe";
    case StatsReport::kStatsReportTypeRemoteSsrc:
      return "remoteSsrc";
    case StatsReport::kStatsReportTypeSsrc:
      return "ssrc";
    case StatsReport::kStatsReportTypeTrack:
      return "googTrack";
    case StatsReport::kStatsReportTypeIceLocalCandidate:
      return "localcandidate";
    case StatsReport::kStatsReportTypeIceRemoteCandidate:
      return "remotecandidate";
    case StatsReport::kStatsReportTypeTransport:
      return "googTransport";
    case StatsReport::kStatsReportTypeComponent:
      return "googComponent";
    case StatsReport::kStatsReportTypeCandidatePair:
      return "googCandidatePair";
    case StatsReport::kStatsReportTypeCertificate:
      return "googCertificate";
    case StatsReport::kStatsReportTypeDataChannel:
      return "datachannel";
  }
  ASSERT(false);
  return nullptr;
}

class BandwidthEstimationId : public StatsReport::Id {
 public:
  BandwidthEstimationId() : StatsReport::Id(StatsReport::kStatsReportTypeBwe) {}
  std::string ToString() const override { return kStatsReportVideoBweId; }
};

class TypedId : public StatsReport::Id {
 public:
  TypedId(StatsReport::StatsType type, const std::string& id)
      : StatsReport::Id(type), id_(id) {}

  bool Equals(const Id& other) const override {
    return Id::Equals(other) &&
           static_cast<const TypedId&>(other).id_ == id_;
  }

  std::string ToString() const override {
    return std::string(InternalTypeToString(type_)) + kSeparator + id_;
  }

 protected:
  const std::string id_;
};

class TypedIntId : public StatsReport::Id {
 public:
  TypedIntId(StatsReport::StatsType type, int id)
      : StatsReport::Id(type), id_(id) {}

  bool Equals(const Id& other) const override {
    return Id::Equals(other) &&
           static_cast<const TypedIntId&>(other).id_ == id_;
  }

  std::string ToString() const override {
    return std::string(InternalTypeToString(type_)) +
           kSeparator +
           rtc::ToString<int>(id_);
  }

 protected:
  const int id_;
};

class IdWithDirection : public TypedId {
 public:
  IdWithDirection(StatsReport::StatsType type, const std::string& id,
                  StatsReport::Direction direction)
      : TypedId(type, id), direction_(direction) {}

  bool Equals(const Id& other) const override {
    return TypedId::Equals(other) &&
           static_cast<const IdWithDirection&>(other).direction_ == direction_;
  }

  std::string ToString() const override {
    std::string ret(TypedId::ToString());
    ret += kSeparator;
    ret += direction_ == StatsReport::kSend ? "send" : "recv";
    return ret;
  }

 private:
  const StatsReport::Direction direction_;
};

class CandidateId : public TypedId {
 public:
  CandidateId(bool local, const std::string& id)
      : TypedId(local ?
                    StatsReport::kStatsReportTypeIceLocalCandidate :
                    StatsReport::kStatsReportTypeIceRemoteCandidate,
                id) {
  }

  std::string ToString() const override {
    return "Cand-" + id_;
  }
};

class ComponentId : public StatsReport::Id {
 public:
  ComponentId(const std::string& content_name, int component)
      : ComponentId(StatsReport::kStatsReportTypeComponent, content_name,
            component) {}

  bool Equals(const Id& other) const override {
    return Id::Equals(other) &&
        static_cast<const ComponentId&>(other).component_ == component_ &&
        static_cast<const ComponentId&>(other).content_name_ == content_name_;
  }

  std::string ToString() const override {
    return ToString("Channel-");
  }

 protected:
  ComponentId(StatsReport::StatsType type, const std::string& content_name,
              int component)
      : Id(type),
        content_name_(content_name),
        component_(component) {}

  std::string ToString(const char* prefix) const {
    std::string ret(prefix);
    ret += content_name_;
    ret += '-';
    ret += rtc::ToString<>(component_);
    return ret;
  }

 private:
  const std::string content_name_;
  const int component_;
};

class CandidatePairId : public ComponentId {
 public:
  CandidatePairId(const std::string& content_name, int component, int index)
      : ComponentId(StatsReport::kStatsReportTypeCandidatePair, content_name,
            component),
        index_(index) {}

  bool Equals(const Id& other) const override {
    return ComponentId::Equals(other) &&
        static_cast<const CandidatePairId&>(other).index_ == index_;
  }

  std::string ToString() const override {
    std::string ret(ComponentId::ToString("Conn-"));
    ret += '-';
    ret += rtc::ToString<>(index_);
    return ret;
  }

 private:
  const int index_;
};

}  // namespace

StatsReport::Id::Id(StatsType type) : type_(type) {}
StatsReport::Id::~Id() {}

StatsReport::StatsType StatsReport::Id::type() const { return type_; }

bool StatsReport::Id::Equals(const Id& other) const {
  return other.type_ == type_;
}

StatsReport::Value::Value(StatsValueName name, const std::string& value)
    : name(name), value(value) {
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
    case kStatsValueNameSecondaryDecodedRate:
      return "googSecondaryDecodedRate";
    case kStatsValueNameSendPacketsDiscarded:
      return "packetsDiscardedOnSend";
    case kStatsValueNameSpeechExpandRate:
      return "googSpeechExpandRate";
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

StatsReport::StatsReport(scoped_ptr<Id> id) : id_(id.Pass()), timestamp_(0.0) {
  ASSERT(id_.get());
}

// static
scoped_ptr<StatsReport::Id> StatsReport::NewBandwidthEstimationId() {
  return scoped_ptr<Id>(new BandwidthEstimationId()).Pass();
}

// static
scoped_ptr<StatsReport::Id> StatsReport::NewTypedId(
    StatsType type, const std::string& id) {
  return scoped_ptr<Id>(new TypedId(type, id)).Pass();
}

// static
scoped_ptr<StatsReport::Id> StatsReport::NewTypedIntId(StatsType type, int id) {
  return scoped_ptr<Id>(new TypedIntId(type, id)).Pass();
}

// static
scoped_ptr<StatsReport::Id> StatsReport::NewIdWithDirection(
    StatsType type, const std::string& id, StatsReport::Direction direction) {
  return scoped_ptr<Id>(new IdWithDirection(type, id, direction)).Pass();
}

// static
scoped_ptr<StatsReport::Id> StatsReport::NewCandidateId(
    bool local, const std::string& id) {
  return scoped_ptr<Id>(new CandidateId(local, id)).Pass();
}

// static
scoped_ptr<StatsReport::Id> StatsReport::NewComponentId(
    const std::string& content_name, int component) {
  return scoped_ptr<Id>(new ComponentId(content_name, component)).Pass();
}

// static
scoped_ptr<StatsReport::Id> StatsReport::NewCandidatePairId(
    const std::string& content_name, int component, int index) {
  return scoped_ptr<Id>(new CandidatePairId(content_name, component, index))
      .Pass();
}

const char* StatsReport::TypeToString() const {
  return InternalTypeToString(id_->type());
}

void StatsReport::AddValue(StatsReport::StatsValueName name,
                           const std::string& value) {
  values_.push_back(ValuePtr(new Value(name, value)));
}

void StatsReport::AddValue(StatsReport::StatsValueName name, int64 value) {
  AddValue(name, rtc::ToString<int64>(value));
}

// TODO(tommi): Change the way we store vector values.
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
  // TODO(tommi): Store bools as bool.
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

const StatsReport::Value* StatsReport::FindValue(StatsValueName name) const {
  Values::const_iterator it = std::find_if(values_.begin(), values_.end(),
      [&name](const ValuePtr& v)->bool { return v->name == name; });
  return it == values_.end() ? nullptr : (*it).get();
}

StatsCollection::StatsCollection() {
}

StatsCollection::~StatsCollection() {
  for (auto* r : list_)
    delete r;
}

StatsCollection::const_iterator StatsCollection::begin() const {
  return list_.begin();
}

StatsCollection::const_iterator StatsCollection::end() const {
  return list_.end();
}

size_t StatsCollection::size() const {
  return list_.size();
}

StatsReport* StatsCollection::InsertNew(scoped_ptr<StatsReport::Id> id) {
  ASSERT(Find(*id.get()) == NULL);
  StatsReport* report = new StatsReport(id.Pass());
  list_.push_back(report);
  return report;
}

StatsReport* StatsCollection::FindOrAddNew(scoped_ptr<StatsReport::Id> id) {
  StatsReport* ret = Find(*id.get());
  return ret ? ret : InsertNew(id.Pass());
}

StatsReport* StatsCollection::ReplaceOrAddNew(scoped_ptr<StatsReport::Id> id) {
  ASSERT(id.get());
  Container::iterator it = std::find_if(list_.begin(), list_.end(),
      [&id](const StatsReport* r)->bool { return r->id().Equals(*id.get()); });
  if (it != end()) {
    delete *it;
    StatsReport* report = new StatsReport(id.Pass());
    *it = report;
    return report;
  }
  return InsertNew(id.Pass());
}

// Looks for a report with the given |id|.  If one is not found, NULL
// will be returned.
StatsReport* StatsCollection::Find(const StatsReport::Id& id) {
  Container::iterator it = std::find_if(list_.begin(), list_.end(),
      [&id](const StatsReport* r)->bool { return r->id().Equals(id); });
  return it == list_.end() ? nullptr : *it;
}

StatsReport* StatsCollection::Find(const scoped_ptr<StatsReport::Id>& id) {
  return Find(*id.get());
}

}  // namespace webrtc
