/*
 * libjingle
 * Copyright 2012, Google Inc.
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

// This file contains structures used for retrieving statistics from an ongoing
// libjingle session.

#ifndef TALK_APP_WEBRTC_STATSTYPES_H_
#define TALK_APP_WEBRTC_STATSTYPES_H_

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/common.h"
#include "webrtc/base/stringencode.h"

namespace webrtc {

class StatsReport {
 public:
  // TODO(tommi): Remove this ctor after removing reliance upon it in Chromium
  // (mock_peer_connection_impl.cc).
  StatsReport() : timestamp(0) {}

  // TODO(tommi): Make protected and disallow copy completely once not needed.
  StatsReport(const StatsReport& src);

  // Constructor is protected to force use of StatsSet.
  // TODO(tommi): Make this ctor protected.
  explicit StatsReport(const std::string& id);

  // TODO(tommi): Make this protected.
  StatsReport& operator=(const StatsReport& src);

  // Operators provided for STL container/algorithm support.
  bool operator<(const StatsReport& other) const;
  bool operator==(const StatsReport& other) const;
  // Special support for being able to use std::find on a container
  // without requiring a new StatsReport instance.
  bool operator==(const std::string& other_id) const;

  // The unique identifier for this object.
  // This is used as a key for this report in ordered containers,
  // so it must never be changed.
  // TODO(tommi): Make this member variable const.
  std::string id;  // See below for contents.
  std::string type;  // See below for contents.

  // StatsValue names.
  enum StatsValueName {
    kStatsValueNameActiveConnection,
    kStatsValueNameAudioInputLevel,
    kStatsValueNameAudioOutputLevel,
    kStatsValueNameBytesReceived,
    kStatsValueNameBytesSent,
    kStatsValueNamePacketsLost,
    kStatsValueNamePacketsReceived,
    kStatsValueNamePacketsSent,
    kStatsValueNameReadable,
    kStatsValueNameSsrc,
    kStatsValueNameTransportId,

    // Internal StatsValue names.
    kStatsValueNameActualEncBitrate,
    kStatsValueNameAdaptationChanges,
    kStatsValueNameAvailableReceiveBandwidth,
    kStatsValueNameAvailableSendBandwidth,
    kStatsValueNameAvgEncodeMs,
    kStatsValueNameBandwidthLimitedResolution,
    kStatsValueNameBucketDelay,
    kStatsValueNameCaptureJitterMs,
    kStatsValueNameCaptureQueueDelayMsPerS,
    kStatsValueNameCaptureStartNtpTimeMs,
    kStatsValueNameCandidateIPAddress,
    kStatsValueNameCandidateNetworkType,
    kStatsValueNameCandidatePortNumber,
    kStatsValueNameCandidatePriority,
    kStatsValueNameCandidateTransportType,
    kStatsValueNameCandidateType,
    kStatsValueNameChannelId,
    kStatsValueNameCodecName,
    kStatsValueNameComponent,
    kStatsValueNameContentName,
    kStatsValueNameCpuLimitedResolution,
    kStatsValueNameCurrentDelayMs,
    kStatsValueNameDecodeMs,
    kStatsValueNameDecodingCNG,
    kStatsValueNameDecodingCTN,
    kStatsValueNameDecodingCTSG,
    kStatsValueNameDecodingNormal,
    kStatsValueNameDecodingPLC,
    kStatsValueNameDecodingPLCCNG,
    kStatsValueNameDer,
    kStatsValueNameEchoCancellationQualityMin,
    kStatsValueNameEchoDelayMedian,
    kStatsValueNameEchoDelayStdDev,
    kStatsValueNameEchoReturnLoss,
    kStatsValueNameEchoReturnLossEnhancement,
    kStatsValueNameEncodeUsagePercent,
    kStatsValueNameExpandRate,
    kStatsValueNameFingerprint,
    kStatsValueNameFingerprintAlgorithm,
    kStatsValueNameFirsReceived,
    kStatsValueNameFirsSent,
    kStatsValueNameFrameHeightInput,
    kStatsValueNameFrameHeightReceived,
    kStatsValueNameFrameHeightSent,
    kStatsValueNameFrameRateDecoded,
    kStatsValueNameFrameRateInput,
    kStatsValueNameFrameRateOutput,
    kStatsValueNameFrameRateReceived,
    kStatsValueNameFrameRateSent,
    kStatsValueNameFrameWidthInput,
    kStatsValueNameFrameWidthReceived,
    kStatsValueNameFrameWidthSent,
    kStatsValueNameInitiator,
    kStatsValueNameIssuerId,
    kStatsValueNameJitterBufferMs,
    kStatsValueNameJitterReceived,
    kStatsValueNameLocalAddress,
    kStatsValueNameLocalCandidateId,
    kStatsValueNameLocalCandidateType,
    kStatsValueNameLocalCertificateId,
    kStatsValueNameMaxDecodeMs,
    kStatsValueNameMinPlayoutDelayMs,
    kStatsValueNameNacksReceived,
    kStatsValueNameNacksSent,
    kStatsValueNamePlisReceived,
    kStatsValueNamePlisSent,
    kStatsValueNamePreferredJitterBufferMs,
    kStatsValueNameRecvPacketGroupArrivalTimeDebug,
    kStatsValueNameRecvPacketGroupPropagationDeltaDebug,
    kStatsValueNameRecvPacketGroupPropagationDeltaSumDebug,
    kStatsValueNameRemoteAddress,
    kStatsValueNameRemoteCandidateId,
    kStatsValueNameRemoteCandidateType,
    kStatsValueNameRemoteCertificateId,
    kStatsValueNameRenderDelayMs,
    kStatsValueNameRetransmitBitrate,
    kStatsValueNameRtt,
    kStatsValueNameSendPacketsDiscarded,
    kStatsValueNameTargetDelayMs,
    kStatsValueNameTargetEncBitrate,
    kStatsValueNameTrackId,
    kStatsValueNameTransmitBitrate,
    kStatsValueNameTransportType,
    kStatsValueNameTypingNoiseState,
    kStatsValueNameViewLimitedResolution,
    kStatsValueNameWritable,
  };

  struct Value {
    // The copy ctor can't be declared as explicit due to problems with STL.
    Value(const Value& other);
    explicit Value(StatsValueName name);
    Value(StatsValueName name, const std::string& value);

    // TODO(tommi): Remove this operator once we don't need it.
    // The operator is provided for compatibility with STL containers.
    // The public |name| member variable is otherwise meant to be read-only.
    Value& operator=(const Value& other);

    // Returns the string representation of |name|.
    const char* display_name() const;

    const StatsValueName name;

    std::string value;
  };

  void AddValue(StatsValueName name, const std::string& value);
  void AddValue(StatsValueName name, int64 value);
  template <typename T>
  void AddValue(StatsValueName name, const std::vector<T>& value);
  void AddBoolean(StatsValueName name, bool value);

  void ReplaceValue(StatsValueName name, const std::string& value);

  double timestamp;  // Time since 1970-01-01T00:00:00Z in milliseconds.
  typedef std::vector<Value> Values;
  Values values;

  // TODO(tommi): These should all be enum values.

  // StatsReport types.
  // A StatsReport of |type| = "googSession" contains overall information
  // about the thing libjingle calls a session (which may contain one
  // or more RTP sessions.
  static const char kStatsReportTypeSession[];

  // A StatsReport of |type| = "googTransport" contains information
  // about a libjingle "transport".
  static const char kStatsReportTypeTransport[];

  // A StatsReport of |type| = "googComponent" contains information
  // about a libjingle "channel" (typically, RTP or RTCP for a transport).
  // This is intended to be the same thing as an ICE "Component".
  static const char kStatsReportTypeComponent[];

  // A StatsReport of |type| = "googCandidatePair" contains information
  // about a libjingle "connection" - a single source/destination port pair.
  // This is intended to be the same thing as an ICE "candidate pair".
  static const char kStatsReportTypeCandidatePair[];

  // StatsReport of |type| = "VideoBWE" is statistics for video Bandwidth
  // Estimation, which is global per-session.  The |id| field is "bweforvideo"
  // (will probably change in the future).
  static const char kStatsReportTypeBwe[];

  // StatsReport of |type| = "ssrc" is statistics for a specific rtp stream.
  // The |id| field is the SSRC in decimal form of the rtp stream.
  static const char kStatsReportTypeSsrc[];

  // StatsReport of |type| = "remoteSsrc" is statistics for a specific
  // rtp stream, generated by the remote end of the connection.
  static const char kStatsReportTypeRemoteSsrc[];

  // StatsReport of |type| = "googTrack" is statistics for a specific media
  // track. The |id| field is the track id.
  static const char kStatsReportTypeTrack[];

  // StatsReport of |type| = "localcandidate" or "remotecandidate" is attributes
  // on a specific ICE Candidate. It links to its connection pair by candidate
  // id. The string value is taken from
  // http://w3c.github.io/webrtc-stats/#rtcstatstype-enum*.
  static const char kStatsReportTypeIceLocalCandidate[];
  static const char kStatsReportTypeIceRemoteCandidate[];

  // A StatsReport of |type| = "googCertificate" contains an SSL certificate
  // transmitted by one of the endpoints of this connection.  The |id| is
  // controlled by the fingerprint, and is used to identify the certificate in
  // the Channel stats (as "googLocalCertificateId" or
  // "googRemoteCertificateId") and in any child certificates (as
  // "googIssuerId").
  static const char kStatsReportTypeCertificate[];

  // The id of StatsReport of type VideoBWE.
  static const char kStatsReportVideoBweId[];
};

// This class is provided for the cases where we need to keep
// snapshots of reports around.  This is an edge case.
// TODO(tommi): Move into the private section of StatsSet.
class StatsReportCopyable : public StatsReport {
 public:
  StatsReportCopyable(const std::string& id) : StatsReport(id) {}
  explicit StatsReportCopyable(const StatsReport& src)
      : StatsReport(src) {}

  using StatsReport::operator=;
};

// Typedef for an array of const StatsReport pointers.
// Ownership of the pointers held by this implementation is assumed to lie
// elsewhere and lifetime guarantees are made by the implementation that uses
// this type.  In the StatsCollector, object ownership lies with the StatsSet
// class.
typedef std::vector<const StatsReport*> StatsReports;

// A map from the report id to the report.
// This class wraps an STL container and provides a limited set of
// functionality in order to keep things simple.
// TODO(tommi): Use a thread checker here (currently not in libjingle).
class StatsSet {
 public:
  StatsSet();
  ~StatsSet();

  typedef std::set<StatsReportCopyable> Container;
  typedef Container::iterator iterator;
  typedef Container::const_iterator const_iterator;

  const_iterator begin() const;
  const_iterator end() const;

  // Creates a new report object with |id| that does not already
  // exist in the list of reports.
  StatsReport* InsertNew(const std::string& id);
  StatsReport* FindOrAddNew(const std::string& id);
  StatsReport* ReplaceOrAddNew(const std::string& id);

  // Looks for a report with the given |id|.  If one is not found, NULL
  // will be returned.
  StatsReport* Find(const std::string& id);

 private:
  Container list_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_STATSTYPES_H_
