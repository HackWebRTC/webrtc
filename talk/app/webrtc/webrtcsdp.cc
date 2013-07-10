/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#include "talk/app/webrtc/webrtcsdp.h"

#include <limits.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>

#include "talk/app/webrtc/jsepicecandidate.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/messagedigest.h"
#include "talk/base/stringutils.h"
#include "talk/media/base/codec.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/cryptoparams.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/port.h"
#include "talk/session/media/mediasession.h"
#include "talk/session/media/mediasessionclient.h"

using cricket::AudioContentDescription;
using cricket::Candidate;
using cricket::Candidates;
using cricket::ContentDescription;
using cricket::ContentInfo;
using cricket::CryptoParams;
using cricket::DataContentDescription;
using cricket::ICE_CANDIDATE_COMPONENT_RTP;
using cricket::ICE_CANDIDATE_COMPONENT_RTCP;
using cricket::kCodecParamMaxBitrate;
using cricket::kCodecParamMaxPTime;
using cricket::kCodecParamMaxQuantization;
using cricket::kCodecParamMinBitrate;
using cricket::kCodecParamMinPTime;
using cricket::kCodecParamPTime;
using cricket::kCodecParamSPropStereo;
using cricket::kCodecParamStereo;
using cricket::kCodecParamUseInbandFec;
using cricket::kCodecParamSctpProtocol;
using cricket::kCodecParamSctpStreams;
using cricket::kWildcardPayloadType;
using cricket::MediaContentDescription;
using cricket::MediaType;
using cricket::NS_JINGLE_ICE_UDP;
using cricket::RtpHeaderExtension;
using cricket::SsrcGroup;
using cricket::StreamParams;
using cricket::StreamParamsVec;
using cricket::TransportDescription;
using cricket::TransportInfo;
using cricket::VideoContentDescription;
using talk_base::SocketAddress;

typedef std::vector<RtpHeaderExtension> RtpHeaderExtensions;

namespace cricket {
class SessionDescription;
}

namespace webrtc {

// Line type
// RFC 4566
// An SDP session description consists of a number of lines of text of
// the form:
// <type>=<value>
// where <type> MUST be exactly one case-significant character.
static const int kLinePrefixLength = 2;  // Lenght of <type>=
static const char kLineTypeVersion = 'v';
static const char kLineTypeOrigin = 'o';
static const char kLineTypeSessionName = 's';
static const char kLineTypeSessionInfo = 'i';
static const char kLineTypeSessionUri = 'u';
static const char kLineTypeSessionEmail = 'e';
static const char kLineTypeSessionPhone = 'p';
static const char kLineTypeSessionBandwidth = 'b';
static const char kLineTypeTiming = 't';
static const char kLineTypeRepeatTimes = 'r';
static const char kLineTypeTimeZone = 'z';
static const char kLineTypeEncryptionKey = 'k';
static const char kLineTypeMedia = 'm';
static const char kLineTypeConnection = 'c';
static const char kLineTypeAttributes = 'a';

// Attributes
static const char kAttributeGroup[] = "group";
static const char kAttributeMid[] = "mid";
static const char kAttributeRtcpMux[] = "rtcp-mux";
static const char kAttributeSsrc[] = "ssrc";
static const char kSsrcAttributeCname[] = "cname";
static const char kAttributeExtmap[] = "extmap";
// draft-alvestrand-mmusic-msid-01
// a=msid-semantic: WMS
static const char kAttributeMsidSemantics[] = "msid-semantic";
static const char kMediaStreamSemantic[] = "WMS";
static const char kSsrcAttributeMsid[] = "msid";
static const char kDefaultMsid[] = "default";
static const char kMsidAppdataAudio[] = "a";
static const char kMsidAppdataVideo[] = "v";
static const char kMsidAppdataData[] = "d";
static const char kSsrcAttributeMslabel[] = "mslabel";
static const char kSSrcAttributeLabel[] = "label";
static const char kAttributeSsrcGroup[] = "ssrc-group";
static const char kAttributeCrypto[] = "crypto";
static const char kAttributeCandidate[] = "candidate";
static const char kAttributeCandidateTyp[] = "typ";
static const char kAttributeCandidateRaddr[] = "raddr";
static const char kAttributeCandidateRport[] = "rport";
static const char kAttributeCandidateUsername[] = "username";
static const char kAttributeCandidatePassword[] = "password";
static const char kAttributeCandidateGeneration[] = "generation";
static const char kAttributeFingerprint[] = "fingerprint";
static const char kAttributeFmtp[] = "fmtp";
static const char kAttributeRtpmap[] = "rtpmap";
static const char kAttributeRtcp[] = "rtcp";
static const char kAttributeIceUfrag[] = "ice-ufrag";
static const char kAttributeIcePwd[] = "ice-pwd";
static const char kAttributeIceLite[] = "ice-lite";
static const char kAttributeIceOption[] = "ice-options";
static const char kAttributeSendOnly[] = "sendonly";
static const char kAttributeRecvOnly[] = "recvonly";
static const char kAttributeRtcpFb[] = "rtcp-fb";
static const char kAttributeSendRecv[] = "sendrecv";
static const char kAttributeInactive[] = "inactive";

// Experimental flags
static const char kAttributeXGoogleFlag[] = "x-google-flag";
static const char kValueConference[] = "conference";
static const char kAttributeXGoogleBufferLatency[] =
    "x-google-buffer-latency";

// Candidate
static const char kCandidateHost[] = "host";
static const char kCandidateSrflx[] = "srflx";
// TODO: How to map the prflx with circket candidate type
// static const char kCandidatePrflx[] = "prflx";
static const char kCandidateRelay[] = "relay";

static const char kSdpDelimiterEqual = '=';
static const char kSdpDelimiterSpace = ' ';
static const char kSdpDelimiterColon = ':';
static const char kSdpDelimiterSemicolon = ';';
static const char kSdpDelimiterSlash = '/';
static const char kNewLine = '\n';
static const char kReturn = '\r';
static const char kLineBreak[] = "\r\n";

// TODO: Generate the Session and Time description
// instead of hardcoding.
static const char kSessionVersion[] = "v=0";
// RFC 4566
static const char kSessionOriginUsername[] = "-";
static const char kSessionOriginSessionId[] = "0";
static const char kSessionOriginSessionVersion[] = "0";
static const char kSessionOriginNettype[] = "IN";
static const char kSessionOriginAddrtype[] = "IP4";
static const char kSessionOriginAddress[] = "127.0.0.1";
static const char kSessionName[] = "s=-";
static const char kTimeDescription[] = "t=0 0";
static const char kAttrGroup[] = "a=group:BUNDLE";
static const char kConnectionNettype[] = "IN";
static const char kConnectionAddrtype[] = "IP4";
static const char kMediaTypeVideo[] = "video";
static const char kMediaTypeAudio[] = "audio";
static const char kMediaTypeData[] = "application";
static const char kMediaPortRejected[] = "0";
static const char kDefaultAddress[] = "0.0.0.0";
static const char kDefaultPort[] = "1";
// RFC 3556
static const char kApplicationSpecificMaximum[] = "AS";

static const int kDefaultVideoClockrate = 90000;

// ISAC special-case.
static const char kIsacCodecName[] = "ISAC";  // From webrtcvoiceengine.cc
static const int kIsacWbDefaultRate = 32000;  // From acm_common_defs.h
static const int kIsacSwbDefaultRate = 56000;  // From acm_common_defs.h

static const int kDefaultSctpFmt = 5000;
static const char kDefaultSctpFmtProtocol[] = "webrtc-datachannel";

struct SsrcInfo {
  SsrcInfo()
      : msid_identifier(kDefaultMsid),
        // TODO(ronghuawu): What should we do if the appdata doesn't appear?
        // Create random string (which will be used as track label later)?
        msid_appdata(talk_base::CreateRandomString(8)) {
  }
  uint32 ssrc_id;
  std::string cname;
  std::string msid_identifier;
  std::string msid_appdata;

  // For backward compatibility.
  // TODO(ronghuawu): Remove below 2 fields once all the clients support msid.
  std::string label;
  std::string mslabel;
};
typedef std::vector<SsrcInfo> SsrcInfoVec;
typedef std::vector<SsrcGroup> SsrcGroupVec;

// Serializes the passed in SessionDescription to a SDP string.
// desc - The SessionDescription object to be serialized.
static std::string SdpSerializeSessionDescription(
    const JsepSessionDescription& jdesc);
template <class T>
static void AddFmtpLine(const T& codec, std::string* message);
static void BuildMediaDescription(const ContentInfo* content_info,
                                  const TransportInfo* transport_info,
                                  const MediaType media_type,
                                  std::string* message);
static void BuildSctpContentAttributes(std::string* message);
static void BuildRtpContentAttributes(
    const MediaContentDescription* media_desc,
    const MediaType media_type,
    std::string* message);
static void BuildRtpMap(const MediaContentDescription* media_desc,
                        const MediaType media_type,
                        std::string* message);
static void BuildCandidate(const std::vector<Candidate>& candidates,
                           std::string* message);
static void BuildIceOptions(const std::vector<std::string>& transport_options,
                            std::string* message);

static bool ParseSessionDescription(const std::string& message, size_t* pos,
                                    std::string* session_id,
                                    std::string* session_version,
                                    bool* supports_msid,
                                    TransportDescription* session_td,
                                    RtpHeaderExtensions* session_extmaps,
                                    cricket::SessionDescription* desc,
                                    SdpParseError* error);
static bool ParseGroupAttribute(const std::string& line,
                                cricket::SessionDescription* desc,
                                SdpParseError* error);
static bool ParseMediaDescription(
    const std::string& message,
    const TransportDescription& session_td,
    const RtpHeaderExtensions& session_extmaps,
    bool supports_msid,
    size_t* pos, cricket::SessionDescription* desc,
    std::vector<JsepIceCandidate*>* candidates,
    SdpParseError* error);
static bool ParseContent(const std::string& message,
                         const MediaType media_type,
                         int mline_index,
                         const std::string& protocol,
                         const std::vector<int>& codec_preference,
                         size_t* pos,
                         std::string* content_name,
                         MediaContentDescription* media_desc,
                         TransportDescription* transport,
                         std::vector<JsepIceCandidate*>* candidates,
                         SdpParseError* error);
static bool ParseSsrcAttribute(const std::string& line,
                               SsrcInfoVec* ssrc_infos,
                               SdpParseError* error);
static bool ParseSsrcGroupAttribute(const std::string& line,
                                    SsrcGroupVec* ssrc_groups,
                                    SdpParseError* error);
static bool ParseCryptoAttribute(const std::string& line,
                                 MediaContentDescription* media_desc,
                                 SdpParseError* error);
static bool ParseRtpmapAttribute(const std::string& line,
                                 const MediaType media_type,
                                 const std::vector<int>& codec_preference,
                                 MediaContentDescription* media_desc,
                                 SdpParseError* error);
static bool ParseFmtpAttributes(const std::string& line,
                                const MediaType media_type,
                                MediaContentDescription* media_desc,
                                SdpParseError* error);
static bool ParseFmtpParam(const std::string& line, std::string* parameter,
                           std::string* value, SdpParseError* error);
static bool ParseCandidate(const std::string& message, Candidate* candidate,
                           SdpParseError* error, bool is_raw);
static bool ParseRtcpFbAttribute(const std::string& line,
                                 const MediaType media_type,
                                 MediaContentDescription* media_desc,
                                 SdpParseError* error);
static bool ParseIceOptions(const std::string& line,
                            std::vector<std::string>* transport_options,
                            SdpParseError* error);
static bool ParseExtmap(const std::string& line,
                        RtpHeaderExtension* extmap,
                        SdpParseError* error);
static bool ParseFingerprintAttribute(const std::string& line,
                                      talk_base::SSLFingerprint** fingerprint,
                                      SdpParseError* error);

// Helper functions

// Below ParseFailed*** functions output the line that caused the parsing
// failure and the detailed reason (|description|) of the failure to |error|.
// The functions always return false so that they can be used directly in the
// following way when error happens:
// "return ParseFailed***(...);"

// The line starting at |line_start| of |message| is the failing line.
// The reason for the failure should be provided in the |description|.
// An example of a description could be "unknown character".
static bool ParseFailed(const std::string& message,
                        size_t line_start,
                        const std::string& description,
                        SdpParseError* error) {
  // Get the first line of |message| from |line_start|.
  std::string first_line = message;
  size_t line_end = message.find(kNewLine, line_start);
  if (line_end != std::string::npos) {
    if (line_end > 0 && (message.at(line_end - 1) == kReturn)) {
      --line_end;
    }
    first_line = message.substr(line_start, (line_end - line_start));
  }

  if (error) {
    error->line = first_line;
    error->description = description;
  }
  LOG(LS_ERROR) << "Failed to parse: \"" << first_line
                << "\". Reason: " << description;
  return false;
}

// |line| is the failing line. The reason for the failure should be
// provided in the |description|.
static bool ParseFailed(const std::string& line,
                        const std::string& description,
                        SdpParseError* error) {
  return ParseFailed(line, 0, description, error);
}

// Parses failure where the failing SDP line isn't know or there are multiple
// failing lines.
static bool ParseFailed(const std::string& description,
                        SdpParseError* error) {
  return ParseFailed("", description, error);
}

// |line| is the failing line. The failure is due to the fact that |line|
// doesn't have |expected_fields| fields.
static bool ParseFailedExpectFieldNum(const std::string& line,
                                      int expected_fields,
                                      SdpParseError* error) {
  std::ostringstream description;
  description << "Expects " << expected_fields << " fields.";
  return ParseFailed(line, description.str(), error);
}

// |line| is the failing line. The failure is due to the fact that |line| has
// less than |expected_min_fields| fields.
static bool ParseFailedExpectMinFieldNum(const std::string& line,
                                         int expected_min_fields,
                                         SdpParseError* error) {
  std::ostringstream description;
  description << "Expects at least " << expected_min_fields << " fields.";
  return ParseFailed(line, description.str(), error);
}

// |line| is the failing line. The failure is due to the fact that it failed to
// get the value of |attribute|.
static bool ParseFailedGetValue(const std::string& line,
                                const std::string& attribute,
                                SdpParseError* error) {
  std::ostringstream description;
  description << "Failed to get the value of attribute: " << attribute;
  return ParseFailed(line, description.str(), error);
}

// The line starting at |line_start| of |message| is the failing line. The
// failure is due to the line type (e.g. the "m" part of the "m-line")
// not matching what is expected. The expected line type should be
// provided as |line_type|.
static bool ParseFailedExpectLine(const std::string& message,
                                  size_t line_start,
                                  const char line_type,
                                  const std::string& line_value,
                                  SdpParseError* error) {
  std::ostringstream description;
  description << "Expect line: " << line_type << "=" << line_value;
  return ParseFailed(message, line_start, description.str(), error);
}

static bool AddLine(const std::string& line, std::string* message) {
  if (!message)
    return false;

  message->append(line);
  message->append(kLineBreak);
  return true;
}

static bool GetLine(const std::string& message,
                    size_t* pos,
                    std::string* line) {
  size_t line_begin = *pos;
  size_t line_end = message.find(kNewLine, line_begin);
  if (line_end == std::string::npos) {
    return false;
  }
  // Update the new start position
  *pos = line_end + 1;
  if (line_end > 0 && (message.at(line_end - 1) == kReturn)) {
    --line_end;
  }
  *line = message.substr(line_begin, (line_end - line_begin));
  const char* cline = line->c_str();
  // RFC 4566
  // An SDP session description consists of a number of lines of text of
  // the form:
  // <type>=<value>
  // where <type> MUST be exactly one case-significant character and
  // <value> is structured text whose format depends on <type>.
  // Whitespace MUST NOT be used on either side of the "=" sign.
  if (cline[0] == kSdpDelimiterSpace ||
      cline[1] != kSdpDelimiterEqual ||
      cline[2] == kSdpDelimiterSpace) {
    *pos = line_begin;
    return false;
  }
  return true;
}

// Init |os| to "|type|=|value|".
static void InitLine(const char type,
                     const std::string& value,
                     std::ostringstream* os) {
  os->str("");
  *os << type << kSdpDelimiterEqual << value;
}

// Init |os| to "a=|attribute|".
static void InitAttrLine(const std::string& attribute, std::ostringstream* os) {
  InitLine(kLineTypeAttributes, attribute, os);
}

// Writes a SDP attribute line based on |attribute| and |value| to |message|.
static void AddAttributeLine(const std::string& attribute, int value,
                             std::string* message) {
  std::ostringstream os;
  InitAttrLine(attribute, &os);
  os << kSdpDelimiterColon << value;
  AddLine(os.str(), message);
}

// Returns the first line of the message without the line breaker.
static bool GetFirstLine(const std::string& message, std::string* line) {
  size_t pos = 0;
  if (!GetLine(message, &pos, line)) {
    // If GetLine failed, just return the full |message|.
    *line = message;
  }
  return true;
}

static bool IsLineType(const std::string& message,
                       const char type,
                       size_t line_start) {
  if (message.size() < line_start + kLinePrefixLength) {
    return false;
  }
  const char* cmessage = message.c_str();
  return (cmessage[line_start] == type &&
          cmessage[line_start + 1] == kSdpDelimiterEqual);
}

static bool IsLineType(const std::string& line,
                       const char type) {
  return IsLineType(line, type, 0);
}

static bool GetLineWithType(const std::string& message, size_t* pos,
                            std::string* line, const char type) {
  if (!IsLineType(message, type, *pos)) {
    return false;
  }

  if (!GetLine(message, pos, line))
    return false;

  return true;
}

static bool HasAttribute(const std::string& line,
                         const std::string& attribute) {
  return (line.compare(kLinePrefixLength, attribute.size(), attribute) == 0);
}

// Verifies the candiate to be of the format candidate:<blah>
static bool IsRawCandidate(const std::string& line) {
  // Checking candiadte-attribute is starting with "candidate" str.
  if (line.compare(0, strlen(kAttributeCandidate), kAttributeCandidate) != 0) {
    return false;
  }
  const size_t first_candidate = line.find(kSdpDelimiterColon);
  if (first_candidate == std::string::npos)
    return false;
  // In this format we only expecting one candiate. If any additional
  // candidates present, whole string will be discared.
  const size_t any_other = line.find(kSdpDelimiterColon, first_candidate + 1);
  return (any_other == std::string::npos);
}

static bool AddSsrcLine(uint32 ssrc_id, const std::string& attribute,
                        const std::string& value, std::string* message) {
  // RFC 5576
  // a=ssrc:<ssrc-id> <attribute>:<value>
  std::ostringstream os;
  InitAttrLine(kAttributeSsrc, &os);
  os << kSdpDelimiterColon << ssrc_id << kSdpDelimiterSpace
     << attribute << kSdpDelimiterColon << value;
  return AddLine(os.str(), message);
}

// Split the message into two parts by the first delimiter.
static bool SplitByDelimiter(const std::string& message,
                             const char delimiter,
                             std::string* field1,
                             std::string* field2) {
  // Find the first delimiter
  size_t pos = message.find(delimiter);
  if (pos == std::string::npos) {
    return false;
  }
  *field1 = message.substr(0, pos);
  // The rest is the value.
  *field2 = message.substr(pos + 1);
  return true;
}

// Get value only from <attribute>:<value>.
static bool GetValue(const std::string& message, const std::string& attribute,
                     std::string* value, SdpParseError* error) {
  std::string leftpart;
  if (!SplitByDelimiter(message, kSdpDelimiterColon, &leftpart, value)) {
    return ParseFailedGetValue(message, attribute, error);
  }
  // The left part should end with the expected attribute.
  if (leftpart.length() < attribute.length() ||
      leftpart.compare(leftpart.length() - attribute.length(),
                       attribute.length(), attribute) != 0) {
    return ParseFailedGetValue(message, attribute, error);
  }
  return true;
}

static bool CaseInsensitiveFind(std::string str1, std::string str2) {
  std::transform(str1.begin(), str1.end(), str1.begin(),
                 ::tolower);
  std::transform(str2.begin(), str2.end(), str2.begin(),
                 ::tolower);
  return str1.find(str2) != std::string::npos;
}

void CreateTracksFromSsrcInfos(const SsrcInfoVec& ssrc_infos,
                               StreamParamsVec* tracks) {
  ASSERT(tracks != NULL);
  for (SsrcInfoVec::const_iterator ssrc_info = ssrc_infos.begin();
       ssrc_info != ssrc_infos.end(); ++ssrc_info) {
    if (ssrc_info->cname.empty()) {
      continue;
    }

    std::string sync_label;
    std::string track_id;
    if (ssrc_info->msid_identifier == kDefaultMsid &&
        !ssrc_info->mslabel.empty()) {
      // If there's no msid and there's mslabel, we consider this is a sdp from
      // a older version of client that doesn't support msid.
      // In that case, we use the mslabel and label to construct the track.
      sync_label = ssrc_info->mslabel;
      track_id = ssrc_info->label;
    } else {
      sync_label = ssrc_info->msid_identifier;
      // The appdata consists of the "id" attribute of a MediaStreamTrack, which
      // is corresponding to the "id" attribute of StreamParams.
      track_id = ssrc_info->msid_appdata;
    }
    if (sync_label.empty() || track_id.empty()) {
      ASSERT(false);
      continue;
    }

    StreamParamsVec::iterator track = tracks->begin();
    for (; track != tracks->end(); ++track) {
      if (track->id == track_id) {
        break;
      }
    }
    if (track == tracks->end()) {
      // If we don't find an existing track, create a new one.
      tracks->push_back(StreamParams());
      track = tracks->end() - 1;
    }
    track->add_ssrc(ssrc_info->ssrc_id);
    track->cname = ssrc_info->cname;
    track->sync_label = sync_label;
    track->id = track_id;
  }
}

void GetMediaStreamLabels(const ContentInfo* content,
                          std::set<std::string>* labels) {
  const MediaContentDescription* media_desc =
      static_cast<const MediaContentDescription*> (
          content->description);
  const cricket::StreamParamsVec& streams =  media_desc->streams();
  for (cricket::StreamParamsVec::const_iterator it = streams.begin();
       it != streams.end(); ++it) {
    labels->insert(it->sync_label);
  }
}

// RFC 5245
// It is RECOMMENDED that default candidates be chosen based on the
// likelihood of those candidates to work with the peer that is being
// contacted.  It is RECOMMENDED that relayed > reflexive > host.
static const int kPreferenceUnknown = 0;
static const int kPreferenceHost = 1;
static const int kPreferenceReflexive = 2;
static const int kPreferenceRelayed = 3;

static int GetCandidatePreferenceFromType(const std::string& type) {
  int preference = kPreferenceUnknown;
  if (type == cricket::LOCAL_PORT_TYPE) {
    preference = kPreferenceHost;
  } else if (type == cricket::STUN_PORT_TYPE) {
    preference = kPreferenceReflexive;
  } else if (type == cricket::RELAY_PORT_TYPE) {
    preference = kPreferenceRelayed;
  } else {
    ASSERT(false);
  }
  return preference;
}

// Get ip and port of the default destination from the |candidates| with
// the given value of |component_id|.
// RFC 5245
// The value of |component_id| currently supported are 1 (RTP) and 2 (RTCP).
// TODO: Decide the default destination in webrtcsession and
// pass it down via SessionDescription.
static bool GetDefaultDestination(const std::vector<Candidate>& candidates,
    int component_id, std::string* port, std::string* ip) {
  *port = kDefaultPort;
  *ip = kDefaultAddress;
  int current_preference = kPreferenceUnknown;
  for (std::vector<Candidate>::const_iterator it = candidates.begin();
       it != candidates.end(); ++it) {
    if (it->component() != component_id) {
      continue;
    }
    const int preference = GetCandidatePreferenceFromType(it->type());
    // See if this candidate is more preferable then the current one.
    if (preference <= current_preference) {
      continue;
    }
    current_preference = preference;
    *port = it->address().PortAsString();
    *ip = it->address().ipaddr().ToString();
  }
  return true;
}

// Update the media default destination.
static void UpdateMediaDefaultDestination(
    const std::vector<Candidate>& candidates, std::string* mline) {
  // RFC 4566
  // m=<media> <port> <proto> <fmt> ...
  std::vector<std::string> fields;
  talk_base::split(*mline, kSdpDelimiterSpace, &fields);
  if (fields.size() < 3) {
    return;
  }

  bool is_rtp =
      fields[2].empty() ||
      talk_base::starts_with(fields[2].data(),
                             cricket::kMediaProtocolRtpPrefix);

  std::ostringstream os;
  std::string rtp_port, rtp_ip;
  if (GetDefaultDestination(candidates, ICE_CANDIDATE_COMPONENT_RTP,
                            &rtp_port, &rtp_ip)) {
    // Found default RTP candidate.
    // RFC 5245
    // The default candidates are added to the SDP as the default
    // destination for media.  For streams based on RTP, this is done by
    // placing the IP address and port of the RTP candidate into the c and m
    // lines, respectively.

    // Update the port in the m line.
    // If this is a m-line with port equal to 0, we don't change it.
    if (fields[1] != kMediaPortRejected) {
      mline->replace(fields[0].size() + 1,
                     fields[1].size(),
                     rtp_port);
    }
    // Add the c line.
    // RFC 4566
    // c=<nettype> <addrtype> <connection-address>
    InitLine(kLineTypeConnection, kConnectionNettype, &os);
    os << " " << kConnectionAddrtype << " " << rtp_ip;
    AddLine(os.str(), mline);
  }

  if (is_rtp) {
    std::string rtcp_port, rtcp_ip;
    if (GetDefaultDestination(candidates, ICE_CANDIDATE_COMPONENT_RTCP,
                              &rtcp_port, &rtcp_ip)) {
      // Found default RTCP candidate.
      // RFC 5245
      // If the agent is utilizing RTCP, it MUST encode the RTCP candidate
      // using the a=rtcp attribute as defined in RFC 3605.

      // RFC 3605
      // rtcp-attribute =  "a=rtcp:" port  [nettype space addrtype space
      // connection-address] CRLF
      InitAttrLine(kAttributeRtcp, &os);
      os << kSdpDelimiterColon
         << rtcp_port << " "
         << kConnectionNettype << " "
         << kConnectionAddrtype << " "
         << rtcp_ip;
      AddLine(os.str(), mline);
    }
  }
}

// Get candidates according to the mline index from SessionDescriptionInterface.
static void GetCandidatesByMindex(const SessionDescriptionInterface& desci,
                                  int mline_index,
                                  std::vector<Candidate>* candidates) {
  if (!candidates) {
    return;
  }
  const IceCandidateCollection* cc = desci.candidates(mline_index);
  for (size_t i = 0; i < cc->count(); ++i) {
    const IceCandidateInterface* candidate = cc->at(i);
    candidates->push_back(candidate->candidate());
  }
}

std::string SdpSerialize(const JsepSessionDescription& jdesc) {
  std::string sdp = SdpSerializeSessionDescription(jdesc);

  std::string sdp_with_candidates;
  size_t pos = 0;
  std::string line;
  int mline_index = -1;
  while (GetLine(sdp, &pos, &line)) {
    if (IsLineType(line, kLineTypeMedia)) {
      ++mline_index;
      std::vector<Candidate> candidates;
      GetCandidatesByMindex(jdesc, mline_index, &candidates);
      // Media line may append other lines inside the
      // UpdateMediaDefaultDestination call, so add the kLineBreak here first.
      line.append(kLineBreak);
      UpdateMediaDefaultDestination(candidates, &line);
      sdp_with_candidates.append(line);
      // Build the a=candidate lines.
      BuildCandidate(candidates, &sdp_with_candidates);
    } else {
      // Copy old line to new sdp without change.
      AddLine(line, &sdp_with_candidates);
    }
  }
  sdp = sdp_with_candidates;

  return sdp;
}

std::string SdpSerializeSessionDescription(
    const JsepSessionDescription& jdesc) {
  const cricket::SessionDescription* desc = jdesc.description();
  if (!desc) {
    return "";
  }

  std::string message;

  // Session Description.
  AddLine(kSessionVersion, &message);
  // Session Origin
  // RFC 4566
  // o=<username> <sess-id> <sess-version> <nettype> <addrtype>
  // <unicast-address>
  std::ostringstream os;
  InitLine(kLineTypeOrigin, kSessionOriginUsername, &os);
  const std::string session_id = jdesc.session_id().empty() ?
      kSessionOriginSessionId : jdesc.session_id();
  const std::string session_version = jdesc.session_version().empty() ?
      kSessionOriginSessionVersion : jdesc.session_version();
  os << " " << session_id << " " << session_version << " "
     << kSessionOriginNettype << " " << kSessionOriginAddrtype << " "
     << kSessionOriginAddress;
  AddLine(os.str(), &message);
  AddLine(kSessionName, &message);

  // Time Description.
  AddLine(kTimeDescription, &message);

  // Group
  if (desc->HasGroup(cricket::GROUP_TYPE_BUNDLE)) {
    std::string group_line = kAttrGroup;
    const cricket::ContentGroup* group =
        desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    ASSERT(group != NULL);
    const cricket::ContentNames& content_names = group->content_names();
    for (cricket::ContentNames::const_iterator it = content_names.begin();
         it != content_names.end(); ++it) {
      group_line.append(" ");
      group_line.append(*it);
    }
    AddLine(group_line, &message);
  }

  // MediaStream semantics
  InitAttrLine(kAttributeMsidSemantics, &os);
  os << kSdpDelimiterColon << " " << kMediaStreamSemantic;
  std::set<std::string> media_stream_labels;
  const ContentInfo* audio_content = GetFirstAudioContent(desc);
  if (audio_content)
    GetMediaStreamLabels(audio_content, &media_stream_labels);
  const ContentInfo* video_content = GetFirstVideoContent(desc);
  if (video_content)
    GetMediaStreamLabels(video_content, &media_stream_labels);
  for (std::set<std::string>::const_iterator it =
      media_stream_labels.begin(); it != media_stream_labels.end(); ++it) {
    os << " " << *it;
  }
  AddLine(os.str(), &message);

  if (audio_content) {
    BuildMediaDescription(audio_content,
                          desc->GetTransportInfoByName(audio_content->name),
                          cricket::MEDIA_TYPE_AUDIO, &message);
  }


  if (video_content) {
    BuildMediaDescription(video_content,
                          desc->GetTransportInfoByName(video_content->name),
                          cricket::MEDIA_TYPE_VIDEO, &message);
  }

  const ContentInfo* data_content = GetFirstDataContent(desc);
  if (data_content) {
    BuildMediaDescription(data_content,
                          desc->GetTransportInfoByName(data_content->name),
                          cricket::MEDIA_TYPE_DATA, &message);
  }


  return message;
}

// Serializes the passed in IceCandidateInterface to a SDP string.
// candidate - The candidate to be serialized.
std::string SdpSerializeCandidate(
    const IceCandidateInterface& candidate) {
  std::string message;
  std::vector<cricket::Candidate> candidates;
  candidates.push_back(candidate.candidate());
  BuildCandidate(candidates, &message);
  return message;
}

bool SdpDeserialize(const std::string& message,
                    JsepSessionDescription* jdesc,
                    SdpParseError* error) {
  std::string session_id;
  std::string session_version;
  TransportDescription session_td(NS_JINGLE_ICE_UDP, Candidates());
  RtpHeaderExtensions session_extmaps;
  cricket::SessionDescription* desc = new cricket::SessionDescription();
  std::vector<JsepIceCandidate*> candidates;
  size_t current_pos = 0;
  bool supports_msid = false;

  // Session Description
  if (!ParseSessionDescription(message, &current_pos, &session_id,
                               &session_version, &supports_msid, &session_td,
                               &session_extmaps, desc, error)) {
    delete desc;
    return false;
  }

  // Media Description
  if (!ParseMediaDescription(message, session_td, session_extmaps,
                             supports_msid, &current_pos, desc, &candidates,
                             error)) {
    delete desc;
    for (std::vector<JsepIceCandidate*>::const_iterator
         it = candidates.begin(); it != candidates.end(); ++it) {
      delete *it;
    }
    return false;
  }

  jdesc->Initialize(desc, session_id, session_version);

  for (std::vector<JsepIceCandidate*>::const_iterator
       it = candidates.begin(); it != candidates.end(); ++it) {
    jdesc->AddCandidate(*it);
    delete *it;
  }
  return true;
}

bool SdpDeserializeCandidate(const std::string& message,
                             JsepIceCandidate* jcandidate,
                             SdpParseError* error) {
  ASSERT(jcandidate != NULL);
  Candidate candidate;
  if (!ParseCandidate(message, &candidate, error, true)) {
    return false;
  }
  jcandidate->SetCandidate(candidate);
  return true;
}

bool ParseCandidate(const std::string& message, Candidate* candidate,
                    SdpParseError* error, bool is_raw) {
  ASSERT(candidate != NULL);

  // Get the first line from |message|.
  std::string first_line;
  GetFirstLine(message, &first_line);

  size_t start_pos = kLinePrefixLength;  // Starting position to parse.
  if (IsRawCandidate(first_line)) {
    // From WebRTC draft section 4.8.1.1 candidate-attribute will be
    // just candidate:<candidate> not a=candidate:<blah>CRLF
    start_pos = 0;
  } else if (!IsLineType(first_line, kLineTypeAttributes) ||
             !HasAttribute(first_line, kAttributeCandidate)) {
    // Must start with a=candidate line.
    // Expecting to be of the format a=candidate:<blah>CRLF.
    if (is_raw) {
      std::ostringstream description;
      description << "Expect line: "
                  << kAttributeCandidate
                  << ":" << "<candidate-str>";
      return ParseFailed(first_line, 0, description.str(), error);
    } else {
      return ParseFailedExpectLine(first_line, 0, kLineTypeAttributes,
                                   kAttributeCandidate, error);
    }
  }

  std::vector<std::string> fields;
  talk_base::split(first_line.substr(start_pos),
                   kSdpDelimiterSpace, &fields);
  // RFC 5245
  // a=candidate:<foundation> <component-id> <transport> <priority>
  // <connection-address> <port> typ <candidate-types>
  // [raddr <connection-address>] [rport <port>]
  // *(SP extension-att-name SP extension-att-value)
  const size_t expected_min_fields = 8;
  if (fields.size() < expected_min_fields ||
      (fields[6] != kAttributeCandidateTyp)) {
    return ParseFailedExpectMinFieldNum(first_line, expected_min_fields, error);
  }
  std::string foundation;
  if (!GetValue(fields[0], kAttributeCandidate, &foundation, error)) {
    return false;
  }
  const int component_id = talk_base::FromString<int>(fields[1]);
  const std::string transport = fields[2];
  const uint32 priority = talk_base::FromString<uint32>(fields[3]);
  const std::string connection_address = fields[4];
  const int port = talk_base::FromString<int>(fields[5]);
  SocketAddress address(connection_address, port);

  cricket::ProtocolType protocol;
  if (!StringToProto(transport.c_str(), &protocol)) {
    return ParseFailed(first_line, "Unsupported transport type.", error);
  }

  std::string candidate_type;
  const std::string type = fields[7];
  if (type == kCandidateHost) {
    candidate_type = cricket::LOCAL_PORT_TYPE;
  } else if (type == kCandidateSrflx) {
    candidate_type = cricket::STUN_PORT_TYPE;
  } else if (type == kCandidateRelay) {
    candidate_type = cricket::RELAY_PORT_TYPE;
  } else {
    return ParseFailed(first_line, "Unsupported candidate type.", error);
  }

  size_t current_position = expected_min_fields;
  SocketAddress related_address;
  // The 2 optional fields for related address
  // [raddr <connection-address>] [rport <port>]
  if (fields.size() >= (current_position + 2) &&
      fields[current_position] == kAttributeCandidateRaddr) {
    related_address.SetIP(fields[++current_position]);
    ++current_position;
  }
  if (fields.size() >= (current_position + 2) &&
      fields[current_position] == kAttributeCandidateRport) {
    related_address.SetPort(
        talk_base::FromString<int>(fields[++current_position]));
    ++current_position;
  }

  // Extension
  // Empty string as the candidate username and password.
  // Will be updated later with the ice-ufrag and ice-pwd.
  // TODO: Remove the username/password extension, which is currently
  // kept for backwards compatibility.
  std::string username;
  std::string password;
  uint32 generation = 0;
  for (size_t i = current_position; i + 1 < fields.size(); ++i) {
    // RFC 5245
    // *(SP extension-att-name SP extension-att-value)
    if (fields[i] == kAttributeCandidateGeneration) {
      generation = talk_base::FromString<uint32>(fields[++i]);
    } else if (fields[i] == kAttributeCandidateUsername) {
      username = fields[++i];
    } else if (fields[i] == kAttributeCandidatePassword) {
      password = fields[++i];
    } else {
      // Skip the unknown extension.
      ++i;
    }
  }

  // Empty string as the candidate id and network name.
  const std::string id;
  const std::string network_name;
  *candidate = Candidate(id, component_id, cricket::ProtoToString(protocol),
      address, priority, username, password, candidate_type, network_name,
      generation, foundation);
  candidate->set_related_address(related_address);
  return true;
}

bool ParseIceOptions(const std::string& line,
                     std::vector<std::string>* transport_options,
                     SdpParseError* error) {
  std::string ice_options;
  if (!GetValue(line, kAttributeIceOption, &ice_options, error)) {
    return false;
  }
  std::vector<std::string> fields;
  talk_base::split(ice_options, kSdpDelimiterSpace, &fields);
  for (size_t i = 0; i < fields.size(); ++i) {
    transport_options->push_back(fields[i]);
  }
  return true;
}

bool ParseExtmap(const std::string& line, RtpHeaderExtension* extmap,
                 SdpParseError* error) {
  // RFC 5285
  // a=extmap:<value>["/"<direction>] <URI> <extensionattributes>
  std::vector<std::string> fields;
  talk_base::split(line.substr(kLinePrefixLength),
                   kSdpDelimiterSpace, &fields);
  const size_t expected_min_fields = 2;
  if (fields.size() < expected_min_fields) {
    return ParseFailedExpectMinFieldNum(line, expected_min_fields, error);
  }
  std::string uri = fields[1];

  std::string value_direction;
  if (!GetValue(fields[0], kAttributeExtmap, &value_direction, error)) {
    return false;
  }
  std::vector<std::string> sub_fields;
  talk_base::split(value_direction, kSdpDelimiterSlash, &sub_fields);
  int value = talk_base::FromString<int>(sub_fields[0]);

  *extmap = RtpHeaderExtension(uri, value);
  return true;
}

void BuildMediaDescription(const ContentInfo* content_info,
                           const TransportInfo* transport_info,
                           const MediaType media_type,
                           std::string* message) {
  ASSERT(message != NULL);
  if (content_info == NULL || message == NULL) {
    return;
  }
  // TODO: Rethink if we should use sprintfn instead of stringstream.
  // According to the style guide, streams should only be used for logging.
  // http://google-styleguide.googlecode.com/svn/
  // trunk/cppguide.xml?showone=Streams#Streams
  std::ostringstream os;
  const MediaContentDescription* media_desc =
      static_cast<const MediaContentDescription*> (
          content_info->description);
  ASSERT(media_desc != NULL);

  bool is_sctp = (media_desc->protocol() == cricket::kMediaProtocolDtlsSctp);

  // RFC 4566
  // m=<media> <port> <proto> <fmt>
  // fmt is a list of payload type numbers that MAY be used in the session.
  const char* type = NULL;
  if (media_type == cricket::MEDIA_TYPE_AUDIO)
    type = kMediaTypeAudio;
  else if (media_type == cricket::MEDIA_TYPE_VIDEO)
    type = kMediaTypeVideo;
  else if (media_type == cricket::MEDIA_TYPE_DATA)
    type = kMediaTypeData;
  else
    ASSERT(false);

  std::string fmt;
  if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    const VideoContentDescription* video_desc =
        static_cast<const VideoContentDescription*>(media_desc);
    for (std::vector<cricket::VideoCodec>::const_iterator it =
             video_desc->codecs().begin();
         it != video_desc->codecs().end(); ++it) {
      fmt.append(" ");
      fmt.append(talk_base::ToString<int>(it->id));
    }
  } else if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    const AudioContentDescription* audio_desc =
        static_cast<const AudioContentDescription*>(media_desc);
    for (std::vector<cricket::AudioCodec>::const_iterator it =
             audio_desc->codecs().begin();
         it != audio_desc->codecs().end(); ++it) {
      fmt.append(" ");
      fmt.append(talk_base::ToString<int>(it->id));
    }
  } else if (media_type == cricket::MEDIA_TYPE_DATA) {
    if (is_sctp) {
      fmt.append(" ");
      // TODO(jiayl): Replace the hard-coded string with the fmt read out of the
      // ContentDescription.
      fmt.append(talk_base::ToString<int>(kDefaultSctpFmt));
    } else {
      const DataContentDescription* data_desc =
          static_cast<const DataContentDescription*>(media_desc);
      for (std::vector<cricket::DataCodec>::const_iterator it =
           data_desc->codecs().begin();
           it != data_desc->codecs().end(); ++it) {
        fmt.append(" ");
        fmt.append(talk_base::ToString<int>(it->id));
      }
    }
  }
  // The fmt must never be empty. If no codecs are found, set the fmt attribute
  // to 0.
  if (fmt.empty()) {
    fmt = " 0";
  }

  // The port number in the m line will be updated later when associate with
  // the candidates.
  // RFC 3264
  // To reject an offered stream, the port number in the corresponding stream in
  // the answer MUST be set to zero.
  const std::string port = content_info->rejected ?
      kMediaPortRejected : kDefaultPort;

  talk_base::SSLFingerprint* fp = (transport_info) ?
      transport_info->description.identity_fingerprint.get() : NULL;

  InitLine(kLineTypeMedia, type, &os);
  os << " " << port << " " << media_desc->protocol() << fmt;
  AddLine(os.str(), message);

  // Use the transport_info to build the media level ice-ufrag and ice-pwd.
  if (transport_info) {
    // RFC 5245
    // ice-pwd-att           = "ice-pwd" ":" password
    // ice-ufrag-att         = "ice-ufrag" ":" ufrag
    // ice-ufrag
    InitAttrLine(kAttributeIceUfrag, &os);
    os << kSdpDelimiterColon << transport_info->description.ice_ufrag;
    AddLine(os.str(), message);
    // ice-pwd
    InitAttrLine(kAttributeIcePwd, &os);
    os << kSdpDelimiterColon << transport_info->description.ice_pwd;
    AddLine(os.str(), message);

    // draft-petithuguenin-mmusic-ice-attributes-level-03
    BuildIceOptions(transport_info->description.transport_options, message);

    // RFC 4572
    // fingerprint-attribute  =
    //   "fingerprint" ":" hash-func SP fingerprint
    if (fp) {
      // Insert the fingerprint attribute.
      InitAttrLine(kAttributeFingerprint, &os);
      os << kSdpDelimiterColon
         << fp->algorithm << kSdpDelimiterSpace
         << fp->GetRfc4572Fingerprint();

      AddLine(os.str(), message);
    }
  }

  // RFC 3388
  // mid-attribute      = "a=mid:" identification-tag
  // identification-tag = token
  // Use the content name as the mid identification-tag.
  InitAttrLine(kAttributeMid, &os);
  os << kSdpDelimiterColon << content_info->name;
  AddLine(os.str(), message);

  if (is_sctp) {
    BuildSctpContentAttributes(message);
  } else {
    BuildRtpContentAttributes(media_desc, media_type, message);
  }
}

void BuildSctpContentAttributes(std::string* message) {
  cricket::DataCodec sctp_codec(kDefaultSctpFmt, kDefaultSctpFmtProtocol, 0);
  sctp_codec.SetParam(kCodecParamSctpProtocol, kDefaultSctpFmtProtocol);
  sctp_codec.SetParam(kCodecParamSctpStreams, cricket::kMaxSctpSid + 1);
  AddFmtpLine(sctp_codec, message);
}

void BuildRtpContentAttributes(
    const MediaContentDescription* media_desc,
    const MediaType media_type,
    std::string* message) {
  std::ostringstream os;
  // RFC 5285
  // a=extmap:<value>["/"<direction>] <URI> <extensionattributes>
  // The definitions MUST be either all session level or all media level. This
  // implementation uses all media level.
  for (size_t i = 0; i < media_desc->rtp_header_extensions().size(); ++i) {
    InitAttrLine(kAttributeExtmap, &os);
    os << kSdpDelimiterColon << media_desc->rtp_header_extensions()[i].id
       << kSdpDelimiterSpace << media_desc->rtp_header_extensions()[i].uri;
    AddLine(os.str(), message);
  }

  // RFC 3264
  // a=sendrecv || a=sendonly || a=sendrecv || a=inactive

  cricket::MediaContentDirection direction = media_desc->direction();
  if (media_desc->streams().empty() && direction == cricket::MD_SENDRECV) {
    direction = cricket::MD_RECVONLY;
  }

  switch (direction) {
    case cricket::MD_INACTIVE:
      InitAttrLine(kAttributeInactive, &os);
      break;
    case cricket::MD_SENDONLY:
      InitAttrLine(kAttributeSendOnly, &os);
      break;
    case cricket::MD_RECVONLY:
      InitAttrLine(kAttributeRecvOnly, &os);
      break;
    case cricket::MD_SENDRECV:
    default:
      InitAttrLine(kAttributeSendRecv, &os);
      break;
  }
  AddLine(os.str(), message);

  // RFC 4566
  // b=AS:<bandwidth>
  if (media_desc->bandwidth() >= 1000) {
    InitLine(kLineTypeSessionBandwidth, kApplicationSpecificMaximum, &os);
    os << kSdpDelimiterColon << (media_desc->bandwidth() / 1000);
    AddLine(os.str(), message);
  }

  // RFC 5761
  // a=rtcp-mux
  if (media_desc->rtcp_mux()) {
    InitAttrLine(kAttributeRtcpMux, &os);
    AddLine(os.str(), message);
  }

  // RFC 4568
  // a=crypto:<tag> <crypto-suite> <key-params> [<session-params>]
  for (std::vector<CryptoParams>::const_iterator it =
           media_desc->cryptos().begin();
       it != media_desc->cryptos().end(); ++it) {
    InitAttrLine(kAttributeCrypto, &os);
    os << kSdpDelimiterColon << it->tag << " " << it->cipher_suite << " "
       << it->key_params;
    if (!it->session_params.empty()) {
      os << " " << it->session_params;
    }
    AddLine(os.str(), message);
  }

  // RFC 4566
  // a=rtpmap:<payload type> <encoding name>/<clock rate>
  // [/<encodingparameters>]
  BuildRtpMap(media_desc, media_type, message);

  // Specify latency for buffered mode.
  // a=x-google-buffer-latency:<value>
  if (media_desc->buffered_mode_latency() != cricket::kBufferedModeDisabled) {
    std::ostringstream os;
    InitAttrLine(kAttributeXGoogleBufferLatency, &os);
    os << kSdpDelimiterColon << media_desc->buffered_mode_latency();
    AddLine(os.str(), message);
  }

  for (StreamParamsVec::const_iterator track = media_desc->streams().begin();
       track != media_desc->streams().end(); ++track) {
    // Require that the track belongs to a media stream,
    // ie the sync_label is set. This extra check is necessary since the
    // MediaContentDescription always contains a streamparam with an ssrc even
    // if no track or media stream have been created.
    if (track->sync_label.empty()) continue;

    // Build the ssrc-group lines.
    for (size_t i = 0; i < track->ssrc_groups.size(); ++i) {
      // RFC 5576
      // a=ssrc-group:<semantics> <ssrc-id> ...
      if (track->ssrc_groups[i].ssrcs.empty()) {
        continue;
      }
      std::ostringstream os;
      InitAttrLine(kAttributeSsrcGroup, &os);
      os << kSdpDelimiterColon << track->ssrc_groups[i].semantics;
      std::vector<uint32>::const_iterator ssrc =
          track->ssrc_groups[i].ssrcs.begin();
      for (; ssrc != track->ssrc_groups[i].ssrcs.end(); ++ssrc) {
        os << kSdpDelimiterSpace << talk_base::ToString<uint32>(*ssrc);
      }
      AddLine(os.str(), message);
    }
    // Build the ssrc lines for each ssrc.
    for (size_t i = 0; i < track->ssrcs.size(); ++i) {
      uint32 ssrc = track->ssrcs[i];
      // RFC 5576
      // a=ssrc:<ssrc-id> cname:<value>
      AddSsrcLine(ssrc, kSsrcAttributeCname,
                  track->cname, message);

      // draft-alvestrand-mmusic-msid-00
      // a=ssrc:<ssrc-id> msid:identifier [appdata]
      // The appdata consists of the "id" attribute of a MediaStreamTrack, which
      // is corresponding to the "name" attribute of StreamParams.
      std::string appdata = track->id;
      std::ostringstream os;
      InitAttrLine(kAttributeSsrc, &os);
      os << kSdpDelimiterColon << ssrc << kSdpDelimiterSpace
         << kSsrcAttributeMsid << kSdpDelimiterColon << track->sync_label
         << kSdpDelimiterSpace << appdata;
      AddLine(os.str(), message);

      // TODO(ronghuawu): Remove below code which is for backward compatibility.
      // draft-alvestrand-rtcweb-mid-01
      // a=ssrc:<ssrc-id> mslabel:<value>
      // The label isn't yet defined.
      // a=ssrc:<ssrc-id> label:<value>
      AddSsrcLine(ssrc, kSsrcAttributeMslabel, track->sync_label, message);
      AddSsrcLine(ssrc, kSSrcAttributeLabel, track->id, message);
    }
  }
}

void WriteFmtpHeader(int payload_type, std::ostringstream* os) {
  // fmtp header: a=fmtp:|payload_type| <parameters>
  // Add a=fmtp
  InitAttrLine(kAttributeFmtp, os);
  // Add :|payload_type|
  *os << kSdpDelimiterColon << payload_type;
}

void WriteRtcpFbHeader(int payload_type, std::ostringstream* os) {
  // rtcp-fb header: a=rtcp-fb:|payload_type|
  // <parameters>/<ccm <ccm_parameters>>
  // Add a=rtcp-fb
  InitAttrLine(kAttributeRtcpFb, os);
  // Add :
  *os << kSdpDelimiterColon;
  if (payload_type == kWildcardPayloadType) {
    *os << "*";
  } else {
    *os << payload_type;
  }
}

void WriteFmtpParameter(const std::string& parameter_name,
                        const std::string& parameter_value,
                        std::ostringstream* os) {
  // fmtp parameters: |parameter_name|=|parameter_value|
  *os << parameter_name << kSdpDelimiterEqual << parameter_value;
}

void WriteFmtpParameters(const cricket::CodecParameterMap& parameters,
                         std::ostringstream* os) {
  for (cricket::CodecParameterMap::const_iterator fmtp = parameters.begin();
       fmtp != parameters.end(); ++fmtp) {
    // Each new parameter, except the first one starts with ";" and " ".
    if (fmtp != parameters.begin()) {
      *os << kSdpDelimiterSemicolon;
    }
    *os << kSdpDelimiterSpace;
    WriteFmtpParameter(fmtp->first, fmtp->second, os);
  }
}

bool IsFmtpParam(const std::string& name) {
  const char* kFmtpParams[] = {
    kCodecParamMinPTime, kCodecParamSPropStereo,
    kCodecParamStereo, kCodecParamUseInbandFec,
    kCodecParamMaxBitrate, kCodecParamMinBitrate, kCodecParamMaxQuantization,
    kCodecParamSctpProtocol, kCodecParamSctpStreams
  };
  for (size_t i = 0; i < ARRAY_SIZE(kFmtpParams); ++i) {
    if (_stricmp(name.c_str(), kFmtpParams[i]) == 0) {
      return true;
    }
  }
  return false;
}

// Retreives fmtp parameters from |params|, which may contain other parameters
// as well, and puts them in |fmtp_parameters|.
void GetFmtpParams(const cricket::CodecParameterMap& params,
                   cricket::CodecParameterMap* fmtp_parameters) {
  for (cricket::CodecParameterMap::const_iterator iter = params.begin();
       iter != params.end(); ++iter) {
    if (IsFmtpParam(iter->first)) {
      (*fmtp_parameters)[iter->first] = iter->second;
    }
  }
}

template <class T>
void AddFmtpLine(const T& codec, std::string* message) {
  cricket::CodecParameterMap fmtp_parameters;
  GetFmtpParams(codec.params, &fmtp_parameters);
  if (fmtp_parameters.empty()) {
    // No need to add an fmtp if it will have no (optional) parameters.
    return;
  }
  std::ostringstream os;
  WriteFmtpHeader(codec.id, &os);
  WriteFmtpParameters(fmtp_parameters, &os);
  AddLine(os.str(), message);
  return;
}

template <class T>
void AddRtcpFbLines(const T& codec, std::string* message) {
  for (std::vector<cricket::FeedbackParam>::const_iterator iter =
           codec.feedback_params.params().begin();
       iter != codec.feedback_params.params().end(); ++iter) {
    std::ostringstream os;
    WriteRtcpFbHeader(codec.id, &os);
    os << " " << iter->id();
    if (!iter->param().empty()) {
      os << " " << iter->param();
    }
    AddLine(os.str(), message);
  }
}

bool GetMinValue(const std::vector<int>& values, int* value) {
  if (values.empty()) {
    return false;
  }
  std::vector<int>::const_iterator found =
      std::min_element(values.begin(), values.end());
  *value = *found;
  return true;
}

bool GetParameter(const std::string& name,
                  const cricket::CodecParameterMap& params, int* value) {
  std::map<std::string, std::string>::const_iterator found =
      params.find(name);
  if (found == params.end()) {
    return false;
  }
  *value = talk_base::FromString<int>(found->second);
  return true;
}

void BuildRtpMap(const MediaContentDescription* media_desc,
                 const MediaType media_type,
                 std::string* message) {
  ASSERT(message != NULL);
  ASSERT(media_desc != NULL);
  std::ostringstream os;
  if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    const VideoContentDescription* video_desc =
        static_cast<const VideoContentDescription*>(media_desc);
    for (std::vector<cricket::VideoCodec>::const_iterator it =
             video_desc->codecs().begin();
         it != video_desc->codecs().end(); ++it) {
      // RFC 4566
      // a=rtpmap:<payload type> <encoding name>/<clock rate>
      // [/<encodingparameters>]
      if (it->id != kWildcardPayloadType) {
        InitAttrLine(kAttributeRtpmap, &os);
        os << kSdpDelimiterColon << it->id << " " << it->name
         << "/" << kDefaultVideoClockrate;
        AddLine(os.str(), message);
      }
      AddRtcpFbLines(*it, message);
      AddFmtpLine(*it, message);
    }
  } else if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    const AudioContentDescription* audio_desc =
        static_cast<const AudioContentDescription*>(media_desc);
    std::vector<int> ptimes;
    std::vector<int> maxptimes;
    int max_minptime = 0;
    for (std::vector<cricket::AudioCodec>::const_iterator it =
             audio_desc->codecs().begin();
         it != audio_desc->codecs().end(); ++it) {
      ASSERT(!it->name.empty());
      // RFC 4566
      // a=rtpmap:<payload type> <encoding name>/<clock rate>
      // [/<encodingparameters>]
      InitAttrLine(kAttributeRtpmap, &os);
      os << kSdpDelimiterColon << it->id << " ";
      os << it->name << "/" << it->clockrate;
      if (it->channels != 1) {
        os << "/" << it->channels;
      }
      AddLine(os.str(), message);
      AddRtcpFbLines(*it, message);
      AddFmtpLine(*it, message);
      int minptime = 0;
      if (GetParameter(kCodecParamMinPTime, it->params, &minptime)) {
        max_minptime = std::max(minptime, max_minptime);
      }
      int ptime;
      if (GetParameter(kCodecParamPTime, it->params, &ptime)) {
        ptimes.push_back(ptime);
      }
      int maxptime;
      if (GetParameter(kCodecParamMaxPTime, it->params, &maxptime)) {
        maxptimes.push_back(maxptime);
      }
    }
    // Populate the maxptime attribute with the smallest maxptime of all codecs
    // under the same m-line.
    int min_maxptime = INT_MAX;
    if (GetMinValue(maxptimes, &min_maxptime)) {
      AddAttributeLine(kCodecParamMaxPTime, min_maxptime, message);
    }
    ASSERT(min_maxptime > max_minptime);
    // Populate the ptime attribute with the smallest ptime or the largest
    // minptime, whichever is the largest, for all codecs under the same m-line.
    int ptime = INT_MAX;
    if (GetMinValue(ptimes, &ptime)) {
      ptime = std::min(ptime, min_maxptime);
      ptime = std::max(ptime, max_minptime);
      AddAttributeLine(kCodecParamPTime, ptime, message);
    }
  } else if (media_type == cricket::MEDIA_TYPE_DATA) {
    const DataContentDescription* data_desc =
        static_cast<const DataContentDescription*>(media_desc);
    for (std::vector<cricket::DataCodec>::const_iterator it =
         data_desc->codecs().begin();
         it != data_desc->codecs().end(); ++it) {
      // RFC 4566
      // a=rtpmap:<payload type> <encoding name>/<clock rate>
      // [/<encodingparameters>]
      InitAttrLine(kAttributeRtpmap, &os);
      os << kSdpDelimiterColon << it->id << " "
         << it->name << "/" << it->clockrate;
      AddLine(os.str(), message);
    }
  }
}

void BuildCandidate(const std::vector<Candidate>& candidates,
                    std::string* message) {
  std::ostringstream os;

  for (std::vector<Candidate>::const_iterator it = candidates.begin();
       it != candidates.end(); ++it) {
    // RFC 5245
    // a=candidate:<foundation> <component-id> <transport> <priority>
    // <connection-address> <port> typ <candidate-types>
    // [raddr <connection-address>] [rport <port>]
    // *(SP extension-att-name SP extension-att-value)
    std::string type;
    // Map the cricket candidate type to "host" / "srflx" / "prflx" / "relay"
    if (it->type() == cricket::LOCAL_PORT_TYPE) {
      type = kCandidateHost;
    } else if (it->type() == cricket::STUN_PORT_TYPE) {
      type = kCandidateSrflx;
    } else if (it->type() == cricket::RELAY_PORT_TYPE) {
      type = kCandidateRelay;
    } else {
      ASSERT(false);
    }

    InitAttrLine(kAttributeCandidate, &os);
    os << kSdpDelimiterColon
       << it->foundation() << " " << it->component() << " "
       << it->protocol() << " " << it->priority() << " "
       << it->address().ipaddr().ToString() << " "
       << it->address().PortAsString() << " "
       << kAttributeCandidateTyp << " " << type << " ";

    // Related address
    if (!it->related_address().IsNil()) {
      os << kAttributeCandidateRaddr << " "
         << it->related_address().ipaddr().ToString() << " "
         << kAttributeCandidateRport << " "
         << it->related_address().PortAsString() << " ";
    }

    // Extensions
    os << kAttributeCandidateGeneration << " " << it->generation();

    AddLine(os.str(), message);
  }
}

void BuildIceOptions(const std::vector<std::string>& transport_options,
                     std::string* message) {
  if (!transport_options.empty()) {
    std::ostringstream os;
    InitAttrLine(kAttributeIceOption, &os);
    os << kSdpDelimiterColon << transport_options[0];
    for (size_t i = 1; i < transport_options.size(); ++i) {
      os << kSdpDelimiterSpace << transport_options[i];
    }
    AddLine(os.str(), message);
  }
}

bool ParseSessionDescription(const std::string& message, size_t* pos,
                             std::string* session_id,
                             std::string* session_version,
                             bool* supports_msid,
                             TransportDescription* session_td,
                             RtpHeaderExtensions* session_extmaps,
                             cricket::SessionDescription* desc,
                             SdpParseError* error) {
  std::string line;

  // RFC 4566
  // v=  (protocol version)
  if (!GetLineWithType(message, pos, &line, kLineTypeVersion)) {
    return ParseFailedExpectLine(message, *pos, kLineTypeVersion,
                                 std::string(), error);
  }
  // RFC 4566
  // o=<username> <sess-id> <sess-version> <nettype> <addrtype>
  // <unicast-address>
  if (!GetLineWithType(message, pos, &line, kLineTypeOrigin)) {
    return ParseFailedExpectLine(message, *pos, kLineTypeOrigin,
                                 std::string(), error);
  }
  std::vector<std::string> fields;
  talk_base::split(line.substr(kLinePrefixLength),
                   kSdpDelimiterSpace, &fields);
  const size_t expected_fields = 6;
  if (fields.size() != expected_fields) {
    return ParseFailedExpectFieldNum(line, expected_fields, error);
  }
  *session_id = fields[1];
  *session_version = fields[2];

  // RFC 4566
  // s=  (session name)
  if (!GetLineWithType(message, pos, &line, kLineTypeSessionName)) {
    return ParseFailedExpectLine(message, *pos, kLineTypeSessionName,
                                 std::string(), error);
  }

  // Optional lines
  // Those are the optional lines, so shouldn't return false if not present.
  // RFC 4566
  // i=* (session information)
  GetLineWithType(message, pos, &line, kLineTypeSessionInfo);

  // RFC 4566
  // u=* (URI of description)
  GetLineWithType(message, pos, &line, kLineTypeSessionUri);

  // RFC 4566
  // e=* (email address)
  GetLineWithType(message, pos, &line, kLineTypeSessionEmail);

  // RFC 4566
  // p=* (phone number)
  GetLineWithType(message, pos, &line, kLineTypeSessionPhone);

  // RFC 4566
  // c=* (connection information -- not required if included in
  //      all media)
  GetLineWithType(message, pos, &line, kLineTypeConnection);

  // RFC 4566
  // b=* (zero or more bandwidth information lines)
  while (GetLineWithType(message, pos, &line, kLineTypeSessionBandwidth)) {
    // By pass zero or more b lines.
  }

  // RFC 4566
  // One or more time descriptions ("t=" and "r=" lines; see below)
  // t=  (time the session is active)
  // r=* (zero or more repeat times)
  // Ensure there's at least one time description
  if (!GetLineWithType(message, pos, &line, kLineTypeTiming)) {
    return ParseFailedExpectLine(message, *pos, kLineTypeTiming, std::string(),
                                 error);
  }

  while (GetLineWithType(message, pos, &line, kLineTypeRepeatTimes)) {
    // By pass zero or more r lines.
  }

  // Go through the rest of the time descriptions
  while (GetLineWithType(message, pos, &line, kLineTypeTiming)) {
    while (GetLineWithType(message, pos, &line, kLineTypeRepeatTimes)) {
      // By pass zero or more r lines.
    }
  }

  // RFC 4566
  // z=* (time zone adjustments)
  GetLineWithType(message, pos, &line, kLineTypeTimeZone);

  // RFC 4566
  // k=* (encryption key)
  GetLineWithType(message, pos, &line, kLineTypeEncryptionKey);

  // RFC 4566
  // a=* (zero or more session attribute lines)
  while (GetLineWithType(message, pos, &line, kLineTypeAttributes)) {
    if (HasAttribute(line, kAttributeGroup)) {
      if (!ParseGroupAttribute(line, desc, error)) {
        return false;
      }
    } else if (HasAttribute(line, kAttributeIceUfrag)) {
      if (!GetValue(line, kAttributeIceUfrag,
                    &(session_td->ice_ufrag), error)) {
        return false;
      }
    } else if (HasAttribute(line, kAttributeIcePwd)) {
      if (!GetValue(line, kAttributeIcePwd, &(session_td->ice_pwd), error)) {
        return false;
      }
    } else if (HasAttribute(line, kAttributeIceLite)) {
      session_td->ice_mode = cricket::ICEMODE_LITE;
    } else if (HasAttribute(line, kAttributeIceOption)) {
      if (!ParseIceOptions(line, &(session_td->transport_options), error)) {
        return false;
      }
    } else if (HasAttribute(line, kAttributeFingerprint)) {
      if (session_td->identity_fingerprint.get()) {
        return ParseFailed(
            line,
            "Can't have multiple fingerprint attributes at the same level.",
            error);
      }
      talk_base::SSLFingerprint* fingerprint = NULL;
      if (!ParseFingerprintAttribute(line, &fingerprint, error)) {
        return false;
      }
      session_td->identity_fingerprint.reset(fingerprint);
    } else if (HasAttribute(line, kAttributeMsidSemantics)) {
      std::string semantics;
      if (!GetValue(line, kAttributeMsidSemantics, &semantics, error)) {
        return false;
      }
      *supports_msid = CaseInsensitiveFind(semantics, kMediaStreamSemantic);
    } else if (HasAttribute(line, kAttributeExtmap)) {
      RtpHeaderExtension extmap;
      if (!ParseExtmap(line, &extmap, error)) {
        return false;
      }
      session_extmaps->push_back(extmap);
    }
  }

  return true;
}

bool ParseGroupAttribute(const std::string& line,
                         cricket::SessionDescription* desc,
                         SdpParseError* error) {
  ASSERT(desc != NULL);

  // RFC 5888 and draft-holmberg-mmusic-sdp-bundle-negotiation-00
  // a=group:BUNDLE video voice
  std::vector<std::string> fields;
  talk_base::split(line.substr(kLinePrefixLength),
                   kSdpDelimiterSpace, &fields);
  std::string semantics;
  if (!GetValue(fields[0], kAttributeGroup, &semantics, error)) {
    return false;
  }
  cricket::ContentGroup group(semantics);
  for (size_t i = 1; i < fields.size(); ++i) {
    group.AddContentName(fields[i]);
  }
  desc->AddGroup(group);
  return true;
}

static bool ParseFingerprintAttribute(const std::string& line,
                                      talk_base::SSLFingerprint** fingerprint,
                                      SdpParseError* error) {
  if (!IsLineType(line, kLineTypeAttributes) ||
      !HasAttribute(line, kAttributeFingerprint)) {
    return ParseFailedExpectLine(line, 0, kLineTypeAttributes,
                                 kAttributeFingerprint, error);
  }

  std::vector<std::string> fields;
  talk_base::split(line.substr(kLinePrefixLength),
                   kSdpDelimiterSpace, &fields);
  const size_t expected_fields = 2;
  if (fields.size() != expected_fields) {
    return ParseFailedExpectFieldNum(line, expected_fields, error);
  }

  // The first field here is "fingerprint:<hash>.
  std::string algorithm;
  if (!GetValue(fields[0], kAttributeFingerprint, &algorithm, error)) {
    return false;
  }

  // Downcase the algorithm. Note that we don't need to downcase the
  // fingerprint because hex_decode can handle upper-case.
  std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(),
                 ::tolower);

  // The second field is the digest value. De-hexify it.
  *fingerprint = talk_base::SSLFingerprint::CreateFromRfc4572(
      algorithm, fields[1]);
  if (!*fingerprint) {
    return ParseFailed(line,
                       "Failed to create fingerprint from the digest.",
                       error);
  }

  return true;
}

// RFC 3551
//  PT   encoding    media type  clock rate   channels
//                      name                    (Hz)
//  0    PCMU        A            8,000       1
//  1    reserved    A
//  2    reserved    A
//  3    GSM         A            8,000       1
//  4    G723        A            8,000       1
//  5    DVI4        A            8,000       1
//  6    DVI4        A           16,000       1
//  7    LPC         A            8,000       1
//  8    PCMA        A            8,000       1
//  9    G722        A            8,000       1
//  10   L16         A           44,100       2
//  11   L16         A           44,100       1
//  12   QCELP       A            8,000       1
//  13   CN          A            8,000       1
//  14   MPA         A           90,000       (see text)
//  15   G728        A            8,000       1
//  16   DVI4        A           11,025       1
//  17   DVI4        A           22,050       1
//  18   G729        A            8,000       1
struct StaticPayloadAudioCodec {
  const char* name;
  int clockrate;
  int channels;
};
static const StaticPayloadAudioCodec kStaticPayloadAudioCodecs[] = {
  { "PCMU", 8000, 1 },
  { "reserved", 0, 0 },
  { "reserved", 0, 0 },
  { "GSM", 8000, 1 },
  { "G723", 8000, 1 },
  { "DVI4", 8000, 1 },
  { "DVI4", 16000, 1 },
  { "LPC", 8000, 1 },
  { "PCMA", 8000, 1 },
  { "G722", 8000, 1 },
  { "L16", 44100, 2 },
  { "L16", 44100, 1 },
  { "QCELP", 8000, 1 },
  { "CN", 8000, 1 },
  { "MPA", 90000, 1 },
  { "G728", 8000, 1 },
  { "DVI4", 11025, 1 },
  { "DVI4", 22050, 1 },
  { "G729", 8000, 1 },
};

void MaybeCreateStaticPayloadAudioCodecs(
    const std::vector<int>& fmts, AudioContentDescription* media_desc) {
  if (!media_desc) {
    return;
  }
  int preference = fmts.size();
  std::vector<int>::const_iterator it = fmts.begin();
  bool add_new_codec = false;
  for (; it != fmts.end(); ++it) {
    int payload_type = *it;
    if (!media_desc->HasCodec(payload_type) &&
        payload_type >= 0 &&
        payload_type < ARRAY_SIZE(kStaticPayloadAudioCodecs)) {
      std::string encoding_name = kStaticPayloadAudioCodecs[payload_type].name;
      int clock_rate = kStaticPayloadAudioCodecs[payload_type].clockrate;
      int channels = kStaticPayloadAudioCodecs[payload_type].channels;
      media_desc->AddCodec(cricket::AudioCodec(payload_type, encoding_name,
                                               clock_rate, 0, channels,
                                               preference));
      add_new_codec = true;
    }
    --preference;
  }
  if (add_new_codec) {
    media_desc->SortCodecs();
  }
}

template <class C>
static C* ParseContentDescription(const std::string& message,
                                  const MediaType media_type,
                                  int mline_index,
                                  const std::string& protocol,
                                  const std::vector<int>& codec_preference,
                                  size_t* pos,
                                  std::string* content_name,
                                  TransportDescription* transport,
                                  std::vector<JsepIceCandidate*>* candidates,
                                  webrtc::SdpParseError* error) {
  C* media_desc = new C();
  switch (media_type) {
    case cricket::MEDIA_TYPE_AUDIO:
      *content_name = cricket::CN_AUDIO;
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      *content_name = cricket::CN_VIDEO;
      break;
    case cricket::MEDIA_TYPE_DATA:
      *content_name = cricket::CN_DATA;
      break;
    default:
      ASSERT(false);
      break;
  }
  if (!ParseContent(message, media_type, mline_index, protocol,
                    codec_preference, pos, content_name,
                    media_desc, transport, candidates, error)) {
    delete media_desc;
    return NULL;
  }
  // Sort the codecs according to the m-line fmt list.
  media_desc->SortCodecs();
  return media_desc;
}

bool ParseMediaDescription(const std::string& message,
                           const TransportDescription& session_td,
                           const RtpHeaderExtensions& session_extmaps,
                           bool supports_msid,
                           size_t* pos,
                           cricket::SessionDescription* desc,
                           std::vector<JsepIceCandidate*>* candidates,
                           SdpParseError* error) {
  ASSERT(desc != NULL);
  std::string line;
  int mline_index = -1;

  // Zero or more media descriptions
  // RFC 4566
  // m=<media> <port> <proto> <fmt>
  while (GetLineWithType(message, pos, &line, kLineTypeMedia)) {
    ++mline_index;

    std::vector<std::string> fields;
    talk_base::split(line.substr(kLinePrefixLength),
                     kSdpDelimiterSpace, &fields);
    const size_t expected_min_fields = 4;
    if (fields.size() < expected_min_fields) {
      return ParseFailedExpectMinFieldNum(line, expected_min_fields, error);
    }
    bool rejected = false;
    // RFC 3264
    // To reject an offered stream, the port number in the corresponding stream
    // in the answer MUST be set to zero.
    if (fields[1] == kMediaPortRejected) {
      rejected = true;
    }

    std::string protocol = fields[2];
    bool is_sctp = (protocol == cricket::kMediaProtocolDtlsSctp);

    // <fmt>
    std::vector<int> codec_preference;
    for (size_t j = 3 ; j < fields.size(); ++j) {
      codec_preference.push_back(talk_base::FromString<int>(fields[j]));
    }

    // Make a temporary TransportDescription based on |session_td|.
    // Some of this gets overwritten by ParseContent.
    TransportDescription transport(NS_JINGLE_ICE_UDP,
                                   session_td.transport_options,
                                   session_td.ice_ufrag,
                                   session_td.ice_pwd,
                                   session_td.ice_mode,
                                   session_td.identity_fingerprint.get(),
                                   Candidates());

    talk_base::scoped_ptr<MediaContentDescription> content;
    std::string content_name;
    if (HasAttribute(line, kMediaTypeVideo)) {
      content.reset(ParseContentDescription<VideoContentDescription>(
                    message, cricket::MEDIA_TYPE_VIDEO, mline_index, protocol,
                    codec_preference, pos, &content_name,
                    &transport, candidates, error));
    } else if (HasAttribute(line, kMediaTypeAudio)) {
      content.reset(ParseContentDescription<AudioContentDescription>(
                    message, cricket::MEDIA_TYPE_AUDIO, mline_index, protocol,
                    codec_preference, pos, &content_name,
                    &transport, candidates, error));
      MaybeCreateStaticPayloadAudioCodecs(
          codec_preference,
          static_cast<AudioContentDescription*>(content.get()));
    } else if (HasAttribute(line, kMediaTypeData)) {
      content.reset(ParseContentDescription<DataContentDescription>(
                    message, cricket::MEDIA_TYPE_DATA, mline_index, protocol,
                    codec_preference, pos, &content_name,
                    &transport, candidates, error));
    } else {
      LOG(LS_WARNING) << "Unsupported media type: " << line;
      continue;
    }
    if (!content.get()) {
      // ParseContentDescription returns NULL if failed.
      return false;
    }

    if (!is_sctp) {
      // Make sure to set the media direction correctly. If the direction is not
      // MD_RECVONLY or Inactive and no streams are parsed,
      // a default MediaStream will be created to prepare for receiving media.
      if (supports_msid && content->streams().empty() &&
          content->direction() == cricket::MD_SENDRECV) {
        content->set_direction(cricket::MD_RECVONLY);
      }

      // Set the extmap.
      if (!session_extmaps.empty() &&
          !content->rtp_header_extensions().empty()) {
        return ParseFailed("",
                           "The a=extmap MUST be either all session level or "
                           "all media level.",
                           error);
      }
      for (size_t i = 0; i < session_extmaps.size(); ++i) {
        content->AddRtpHeaderExtension(session_extmaps[i]);
      }
    }
    content->set_protocol(protocol);
    desc->AddContent(content_name,
                     is_sctp ? cricket::NS_JINGLE_DRAFT_SCTP :
                               cricket::NS_JINGLE_RTP,
                     rejected,
                     content.release());
    // Create TransportInfo with the media level "ice-pwd" and "ice-ufrag".
    TransportInfo transport_info(content_name, transport);

    if (!desc->AddTransportInfo(transport_info)) {
      std::ostringstream description;
      description << "Failed to AddTransportInfo with content name: "
                  << content_name;
      return ParseFailed("", description.str(), error);
    }
  }
  return true;
}

bool VerifyCodec(const cricket::Codec& codec) {
  // Codec has not been populated correctly unless the name has been set. This
  // can happen if an SDP has an fmtp or rtcp-fb with a payload type but doesn't
  // have a corresponding "rtpmap" line.
  cricket::Codec default_codec;
  return default_codec.name != codec.name;
}

bool VerifyAudioCodecs(const AudioContentDescription* audio_desc) {
  const std::vector<cricket::AudioCodec>& codecs = audio_desc->codecs();
  for (std::vector<cricket::AudioCodec>::const_iterator iter = codecs.begin();
       iter != codecs.end(); ++iter) {
    if (!VerifyCodec(*iter)) {
      return false;
    }
  }
  return true;
}

bool VerifyVideoCodecs(const VideoContentDescription* video_desc) {
  const std::vector<cricket::VideoCodec>& codecs = video_desc->codecs();
  for (std::vector<cricket::VideoCodec>::const_iterator iter = codecs.begin();
       iter != codecs.end(); ++iter) {
    if (!VerifyCodec(*iter)) {
      return false;
    }
  }
  return true;
}

void AddParameters(const cricket::CodecParameterMap& parameters,
                   cricket::Codec* codec) {
  for (cricket::CodecParameterMap::const_iterator iter =
           parameters.begin(); iter != parameters.end(); ++iter) {
    codec->SetParam(iter->first, iter->second);
  }
}

void AddFeedbackParameter(const cricket::FeedbackParam& feedback_param,
                          cricket::Codec* codec) {
  codec->AddFeedbackParam(feedback_param);
}

void AddFeedbackParameters(const cricket::FeedbackParams& feedback_params,
                           cricket::Codec* codec) {
  for (std::vector<cricket::FeedbackParam>::const_iterator iter =
           feedback_params.params().begin();
       iter != feedback_params.params().end(); ++iter) {
    codec->AddFeedbackParam(*iter);
  }
}

// Gets the current codec setting associated with |payload_type|. If there
// is no AudioCodec associated with that payload type it returns an empty codec
// with that payload type.
template <class T>
T GetCodec(const std::vector<T>& codecs, int payload_type) {
  for (typename std::vector<T>::const_iterator codec = codecs.begin();
       codec != codecs.end(); ++codec) {
    if (codec->id == payload_type) {
      return *codec;
    }
  }
  T ret_val = T();
  ret_val.id = payload_type;
  return ret_val;
}

// Updates or creates a new codec entry in the audio description.
template <class T, class U>
void AddOrReplaceCodec(MediaContentDescription* content_desc, const U& codec) {
  T* desc = static_cast<T*>(content_desc);
  std::vector<U> codecs = desc->codecs();
  bool found = false;

  typename std::vector<U>::iterator iter;
  for (iter = codecs.begin(); iter != codecs.end(); ++iter) {
    if (iter->id == codec.id) {
      *iter = codec;
      found = true;
      break;
    }
  }
  if (!found) {
    desc->AddCodec(codec);
    return;
  }
  desc->set_codecs(codecs);
}

// Adds or updates existing codec corresponding to |payload_type| according
// to |parameters|.
template <class T, class U>
void UpdateCodec(MediaContentDescription* content_desc, int payload_type,
                 const cricket::CodecParameterMap& parameters) {
  // Codec might already have been populated (from rtpmap).
  U new_codec = GetCodec(static_cast<T*>(content_desc)->codecs(), payload_type);
  AddParameters(parameters, &new_codec);
  AddOrReplaceCodec<T, U>(content_desc, new_codec);
}

// Adds or updates existing codec corresponding to |payload_type| according
// to |feedback_param|.
template <class T, class U>
void UpdateCodec(MediaContentDescription* content_desc, int payload_type,
                 const cricket::FeedbackParam& feedback_param) {
  // Codec might already have been populated (from rtpmap).
  U new_codec = GetCodec(static_cast<T*>(content_desc)->codecs(), payload_type);
  AddFeedbackParameter(feedback_param, &new_codec);
  AddOrReplaceCodec<T, U>(content_desc, new_codec);
}

bool PopWildcardCodec(std::vector<cricket::VideoCodec>* codecs,
                      cricket::VideoCodec* wildcard_codec) {
  for (std::vector<cricket::VideoCodec>::iterator iter = codecs->begin();
       iter != codecs->end(); ++iter) {
    if (iter->id == kWildcardPayloadType) {
      *wildcard_codec = *iter;
      codecs->erase(iter);
      return true;
    }
  }
  return false;
}

void UpdateFromWildcardVideoCodecs(VideoContentDescription* video_desc) {
  std::vector<cricket::VideoCodec> codecs = video_desc->codecs();
  cricket::VideoCodec wildcard_codec;
  if (!PopWildcardCodec(&codecs, &wildcard_codec)) {
    return;
  }
  for (std::vector<cricket::VideoCodec>::iterator iter = codecs.begin();
       iter != codecs.end(); ++iter) {
    cricket::VideoCodec& codec = *iter;
    AddFeedbackParameters(wildcard_codec.feedback_params, &codec);
  }
  video_desc->set_codecs(codecs);
}

void AddAudioAttribute(const std::string& name, const std::string& value,
                       AudioContentDescription* audio_desc) {
  if (value.empty()) {
    return;
  }
  std::vector<cricket::AudioCodec> codecs = audio_desc->codecs();
  for (std::vector<cricket::AudioCodec>::iterator iter = codecs.begin();
       iter != codecs.end(); ++iter) {
    iter->params[name] = value;
  }
  audio_desc->set_codecs(codecs);
}

bool ParseContent(const std::string& message,
                  const MediaType media_type,
                  int mline_index,
                  const std::string& protocol,
                  const std::vector<int>& codec_preference,
                  size_t* pos,
                  std::string* content_name,
                  MediaContentDescription* media_desc,
                  TransportDescription* transport,
                  std::vector<JsepIceCandidate*>* candidates,
                  SdpParseError* error) {
  ASSERT(media_desc != NULL);
  ASSERT(content_name != NULL);
  ASSERT(transport != NULL);

  // The media level "ice-ufrag" and "ice-pwd".
  // The candidates before update the media level "ice-pwd" and "ice-ufrag".
  Candidates candidates_orig;
  std::string line;
  std::string mline_id;
  // Tracks created out of the ssrc attributes.
  StreamParamsVec tracks;
  SsrcInfoVec ssrc_infos;
  SsrcGroupVec ssrc_groups;
  std::string maxptime_as_string;
  std::string ptime_as_string;

  bool is_rtp =
      protocol.empty() ||
      talk_base::starts_with(protocol.data(),
                             cricket::kMediaProtocolRtpPrefix);

  // Loop until the next m line
  while (!IsLineType(message, kLineTypeMedia, *pos)) {
    if (!GetLine(message, pos, &line)) {
      if (*pos >= message.size()) {
        break;  // Done parsing
      } else {
        return ParseFailed(message, *pos, "Can't find valid SDP line.", error);
      }
    }

    if (IsLineType(line, kLineTypeSessionBandwidth)) {
      std::string bandwidth;
      if (HasAttribute(line, kApplicationSpecificMaximum)) {
        if (!GetValue(line, kApplicationSpecificMaximum, &bandwidth, error)) {
          return false;
        } else {
          media_desc->set_bandwidth(
              talk_base::FromString<int>(bandwidth) * 1000);
        }
      }
      continue;
    }

    // RFC 4566
    // b=* (zero or more bandwidth information lines)
    if (IsLineType(line, kLineTypeSessionBandwidth)) {
      std::string bandwidth;
      if (HasAttribute(line, kApplicationSpecificMaximum)) {
        if (!GetValue(line, kApplicationSpecificMaximum, &bandwidth, error)) {
          return false;
        } else {
          media_desc->set_bandwidth(
              talk_base::FromString<int>(bandwidth) * 1000);
        }
      }
      continue;
    }

    if (!IsLineType(line, kLineTypeAttributes)) {
      // TODO: Handle other lines if needed.
      LOG(LS_INFO) << "Ignored line: " << line;
      continue;
    }

    // Handle attributes common to SCTP and RTP.
    if (HasAttribute(line, kAttributeMid)) {
      // RFC 3388
      // mid-attribute      = "a=mid:" identification-tag
      // identification-tag = token
      // Use the mid identification-tag as the content name.
      if (!GetValue(line, kAttributeMid, &mline_id, error)) {
        return false;
      }
      *content_name = mline_id;
    } else if (HasAttribute(line, kAttributeCandidate)) {
      Candidate candidate;
      if (!ParseCandidate(line, &candidate, error, false)) {
        return false;
      }
      candidates_orig.push_back(candidate);
    } else if (HasAttribute(line, kAttributeIceUfrag)) {
      if (!GetValue(line, kAttributeIceUfrag, &transport->ice_ufrag, error)) {
        return false;
      }
    } else if (HasAttribute(line, kAttributeIcePwd)) {
      if (!GetValue(line, kAttributeIcePwd, &transport->ice_pwd, error)) {
        return false;
      }
    } else if (HasAttribute(line, kAttributeIceOption)) {
      if (!ParseIceOptions(line, &transport->transport_options, error)) {
        return false;
      }
    } else if (HasAttribute(line, kAttributeFmtp)) {
      if (!ParseFmtpAttributes(line, media_type, media_desc, error)) {
        return false;
      }
    } else if (HasAttribute(line, kAttributeFingerprint)) {
      talk_base::SSLFingerprint* fingerprint = NULL;

      if (!ParseFingerprintAttribute(line, &fingerprint, error)) {
        return false;
      }
      transport->identity_fingerprint.reset(fingerprint);
    } else if (is_rtp) {
      //
      // RTP specific attrubtes
      //
      if (HasAttribute(line, kAttributeRtcpMux)) {
        media_desc->set_rtcp_mux(true);
      } else if (HasAttribute(line, kAttributeSsrcGroup)) {
        if (!ParseSsrcGroupAttribute(line, &ssrc_groups, error)) {
          return false;
        }
      } else if (HasAttribute(line, kAttributeSsrc)) {
        if (!ParseSsrcAttribute(line, &ssrc_infos, error)) {
          return false;
        }
      } else if (HasAttribute(line, kAttributeCrypto)) {
        if (!ParseCryptoAttribute(line, media_desc, error)) {
          return false;
        }
      } else if (HasAttribute(line, kAttributeRtpmap)) {
        if (!ParseRtpmapAttribute(line, media_type, codec_preference,
                                  media_desc, error)) {
          return false;
        }
      } else if (HasAttribute(line, kCodecParamMaxPTime)) {
        if (!GetValue(line, kCodecParamMaxPTime, &maxptime_as_string, error)) {
          return false;
        }
      } else if (HasAttribute(line, kAttributeRtcpFb)) {
        if (!ParseRtcpFbAttribute(line, media_type, media_desc, error)) {
          return false;
        }
      } else if (HasAttribute(line, kCodecParamPTime)) {
        if (!GetValue(line, kCodecParamPTime, &ptime_as_string, error)) {
          return false;
        }
      } else if (HasAttribute(line, kAttributeSendOnly)) {
        media_desc->set_direction(cricket::MD_SENDONLY);
      } else if (HasAttribute(line, kAttributeRecvOnly)) {
        media_desc->set_direction(cricket::MD_RECVONLY);
      } else if (HasAttribute(line, kAttributeInactive)) {
        media_desc->set_direction(cricket::MD_INACTIVE);
      } else if (HasAttribute(line, kAttributeSendRecv)) {
        media_desc->set_direction(cricket::MD_SENDRECV);
      } else if (HasAttribute(line, kAttributeExtmap)) {
        RtpHeaderExtension extmap;
        if (!ParseExtmap(line, &extmap, error)) {
          return false;
        }
        media_desc->AddRtpHeaderExtension(extmap);
      } else if (HasAttribute(line, kAttributeXGoogleFlag)) {
        // Experimental attribute.  Conference mode activates more aggressive
        // AEC and NS settings.
        // TODO: expose API to set these directly.
        std::string flag_value;
        if (!GetValue(line, kAttributeXGoogleFlag, &flag_value, error)) {
          return false;
        }
        if (flag_value.compare(kValueConference) == 0)
          media_desc->set_conference_mode(true);
      } else if (HasAttribute(line, kAttributeXGoogleBufferLatency)) {
        // Experimental attribute.
        // TODO: expose API to set this directly.
        std::string flag_value;
        if (!GetValue(line, kAttributeXGoogleBufferLatency, &flag_value,
                      error)) {
          return false;
        }
        int buffer_latency = 0;
        if (!talk_base::FromString(flag_value, &buffer_latency) ||
            buffer_latency < 0) {
          return ParseFailed(message, "Invalid buffer latency.", error);
        }
        media_desc->set_buffered_mode_latency(buffer_latency);
      }
    } else {
      // Only parse lines that we are interested of.
      LOG(LS_INFO) << "Ignored line: " << line;
      continue;
    }
  }

  // Create tracks from the |ssrc_infos|.
  CreateTracksFromSsrcInfos(ssrc_infos, &tracks);

  // Add the ssrc group to the track.
  for (SsrcGroupVec::iterator ssrc_group = ssrc_groups.begin();
       ssrc_group != ssrc_groups.end(); ++ssrc_group) {
    if (ssrc_group->ssrcs.empty()) {
      continue;
    }
    uint32 ssrc = ssrc_group->ssrcs.front();
    for (StreamParamsVec::iterator track = tracks.begin();
         track != tracks.end(); ++track) {
      if (track->has_ssrc(ssrc)) {
        track->ssrc_groups.push_back(*ssrc_group);
      }
    }
  }

  // Add the new tracks to the |media_desc|.
  for (StreamParamsVec::iterator track = tracks.begin();
       track != tracks.end(); ++track) {
    media_desc->AddStream(*track);
  }

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    AudioContentDescription* audio_desc =
        static_cast<AudioContentDescription*>(media_desc);
    // Verify audio codec ensures that no audio codec has been populated with
    // only fmtp.
    if (!VerifyAudioCodecs(audio_desc)) {
      return ParseFailed("Failed to parse audio codecs correctly.", error);
    }
    AddAudioAttribute(kCodecParamMaxPTime, maxptime_as_string, audio_desc);
    AddAudioAttribute(kCodecParamPTime, ptime_as_string, audio_desc);
  }

  if (media_type == cricket::MEDIA_TYPE_VIDEO) {
      VideoContentDescription* video_desc =
          static_cast<VideoContentDescription*>(media_desc);
      UpdateFromWildcardVideoCodecs(video_desc);
      // Verify video codec ensures that no video codec has been populated with
      // only rtcp-fb.
      if (!VerifyVideoCodecs(video_desc)) {
        return ParseFailed("Failed to parse video codecs correctly.", error);
      }
  }

  // RFC 5245
  // Update the candidates with the media level "ice-pwd" and "ice-ufrag".
  for (Candidates::iterator it = candidates_orig.begin();
       it != candidates_orig.end(); ++it) {
    ASSERT((*it).username().empty());
    (*it).set_username(transport->ice_ufrag);
    ASSERT((*it).password().empty());
    (*it).set_password(transport->ice_pwd);
    candidates->push_back(
        new JsepIceCandidate(mline_id, mline_index, *it));
  }
  return true;
}

bool ParseSsrcAttribute(const std::string& line, SsrcInfoVec* ssrc_infos,
                        SdpParseError* error) {
  ASSERT(ssrc_infos != NULL);
  // RFC 5576
  // a=ssrc:<ssrc-id> <attribute>
  // a=ssrc:<ssrc-id> <attribute>:<value>
  std::string field1, field2;
  if (!SplitByDelimiter(line.substr(kLinePrefixLength),
                        kSdpDelimiterSpace,
                        &field1,
                        &field2)) {
    const size_t expected_fields = 2;
    return ParseFailedExpectFieldNum(line, expected_fields, error);
  }

  // ssrc:<ssrc-id>
  std::string ssrc_id_s;
  if (!GetValue(field1, kAttributeSsrc, &ssrc_id_s, error)) {
    return false;
  }
  uint32 ssrc_id = talk_base::FromString<uint32>(ssrc_id_s);

  std::string attribute;
  std::string value;
  if (!SplitByDelimiter(field2, kSdpDelimiterColon,
                        &attribute, &value)) {
    std::ostringstream description;
    description << "Failed to get the ssrc attribute value from " << field2
                << ". Expected format <attribute>:<value>.";
    return ParseFailed(line, description.str(), error);
  }

  // Check if there's already an item for this |ssrc_id|. Create a new one if
  // there isn't.
  SsrcInfoVec::iterator ssrc_info = ssrc_infos->begin();
  for (; ssrc_info != ssrc_infos->end(); ++ssrc_info) {
    if (ssrc_info->ssrc_id == ssrc_id) {
      break;
    }
  }
  if (ssrc_info == ssrc_infos->end()) {
    SsrcInfo info;
    info.ssrc_id = ssrc_id;
    ssrc_infos->push_back(info);
    ssrc_info = ssrc_infos->end() - 1;
  }

  // Store the info to the |ssrc_info|.
  if (attribute == kSsrcAttributeCname) {
    // RFC 5576
    // cname:<value>
    ssrc_info->cname = value;
  } else if (attribute == kSsrcAttributeMsid) {
    // draft-alvestrand-mmusic-msid-00
    // "msid:" identifier [ " " appdata ]
    std::vector<std::string> fields;
    talk_base::split(value, kSdpDelimiterSpace, &fields);
    if (fields.size() < 1 || fields.size() > 2) {
      return ParseFailed(line,
                         "Expected format \"msid:<identifier>[ <appdata>]\".",
                         error);
    }
    ssrc_info->msid_identifier = fields[0];
    if (fields.size() == 2) {
      ssrc_info->msid_appdata = fields[1];
    }
  } else if (attribute == kSsrcAttributeMslabel) {
    // draft-alvestrand-rtcweb-mid-01
    // mslabel:<value>
    ssrc_info->mslabel = value;
  } else if (attribute == kSSrcAttributeLabel) {
    // The label isn't defined.
    // label:<value>
    ssrc_info->label = value;
  }
  return true;
}

bool ParseSsrcGroupAttribute(const std::string& line,
                             SsrcGroupVec* ssrc_groups,
                             SdpParseError* error) {
  ASSERT(ssrc_groups != NULL);
  // RFC 5576
  // a=ssrc-group:<semantics> <ssrc-id> ...
  std::vector<std::string> fields;
  talk_base::split(line.substr(kLinePrefixLength),
                   kSdpDelimiterSpace, &fields);
  const size_t expected_min_fields = 2;
  if (fields.size() < expected_min_fields) {
    return ParseFailedExpectMinFieldNum(line, expected_min_fields, error);
  }
  std::string semantics;
  if (!GetValue(fields[0], kAttributeSsrcGroup, &semantics, error)) {
    return false;
  }
  std::vector<uint32> ssrcs;
  for (size_t i = 1; i < fields.size(); ++i) {
    uint32 ssrc = talk_base::FromString<uint32>(fields[i]);
    ssrcs.push_back(ssrc);
  }
  ssrc_groups->push_back(SsrcGroup(semantics, ssrcs));
  return true;
}

bool ParseCryptoAttribute(const std::string& line,
                          MediaContentDescription* media_desc,
                          SdpParseError* error) {
  std::vector<std::string> fields;
  talk_base::split(line.substr(kLinePrefixLength),
                   kSdpDelimiterSpace, &fields);
  // RFC 4568
  // a=crypto:<tag> <crypto-suite> <key-params> [<session-params>]
  const size_t expected_min_fields = 3;
  if (fields.size() < expected_min_fields) {
    return ParseFailedExpectMinFieldNum(line, expected_min_fields, error);
  }
  std::string tag_value;
  if (!GetValue(fields[0], kAttributeCrypto, &tag_value, error)) {
    return false;
  }
  int tag = talk_base::FromString<int>(tag_value);
  const std::string crypto_suite = fields[1];
  const std::string key_params = fields[2];
  std::string session_params;
  if (fields.size() > 3) {
    session_params = fields[3];
  }
  media_desc->AddCrypto(CryptoParams(tag, crypto_suite, key_params,
                                     session_params));
  return true;
}

// Updates or creates a new codec entry in the audio description with according
// to |name|, |clockrate|, |bitrate|, |channels| and |preference|.
void UpdateCodec(int payload_type, const std::string& name, int clockrate,
                 int bitrate, int channels, int preference,
                 AudioContentDescription* audio_desc) {
  // Codec may already be populated with (only) optional parameters
  // (from an fmtp).
  cricket::AudioCodec codec = GetCodec(audio_desc->codecs(), payload_type);
  codec.name = name;
  codec.clockrate = clockrate;
  codec.bitrate = bitrate;
  codec.channels = channels;
  codec.preference = preference;
  AddOrReplaceCodec<AudioContentDescription, cricket::AudioCodec>(audio_desc,
                                                                  codec);
}

// Updates or creates a new codec entry in the video description according to
// |name|, |width|, |height|, |framerate| and |preference|.
void UpdateCodec(int payload_type, const std::string& name, int width,
                 int height, int framerate, int preference,
                 VideoContentDescription* video_desc) {
  // Codec may already be populated with (only) optional parameters
  // (from an fmtp).
  cricket::VideoCodec codec = GetCodec(video_desc->codecs(), payload_type);
  codec.name = name;
  codec.width = width;
  codec.height = height;
  codec.framerate = framerate;
  codec.preference = preference;
  AddOrReplaceCodec<VideoContentDescription, cricket::VideoCodec>(video_desc,
                                                                  codec);
}

bool ParseRtpmapAttribute(const std::string& line,
                          const MediaType media_type,
                          const std::vector<int>& codec_preference,
                          MediaContentDescription* media_desc,
                          SdpParseError* error) {
  std::vector<std::string> fields;
  talk_base::split(line.substr(kLinePrefixLength),
                   kSdpDelimiterSpace, &fields);
  // RFC 4566
  // a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encodingparameters>]
  const size_t expected_min_fields = 2;
  if (fields.size() < expected_min_fields) {
    return ParseFailedExpectMinFieldNum(line, expected_min_fields, error);
  }
  std::string payload_type_value;
  if (!GetValue(fields[0], kAttributeRtpmap, &payload_type_value, error)) {
    return false;
  }
  const int payload_type = talk_base::FromString<int>(payload_type_value);

  // Set the preference order depending on the order of the pl type in the
  // <fmt> of the m-line.
  const int preference = codec_preference.end() -
      std::find(codec_preference.begin(), codec_preference.end(),
                payload_type);
  if (preference == 0) {
    LOG(LS_WARNING) << "Ignore rtpmap line that did not appear in the "
                    << "<fmt> of the m-line: " << line;
    return true;
  }
  const std::string encoder = fields[1];
  std::vector<std::string> codec_params;
  talk_base::split(encoder, '/', &codec_params);
  // <encoding name>/<clock rate>[/<encodingparameters>]
  // 2 mandatory fields
  if (codec_params.size() < 2 || codec_params.size() > 3) {
    return ParseFailed(line,
                       "Expected format \"<encoding name>/<clock rate>"
                       "[/<encodingparameters>]\".",
                       error);
  }
  const std::string encoding_name = codec_params[0];
  const int clock_rate = talk_base::FromString<int>(codec_params[1]);
  if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    VideoContentDescription* video_desc =
        static_cast<VideoContentDescription*>(media_desc);
    // TODO: We will send resolution in SDP. For now use
    // JsepSessionDescription::kMaxVideoCodecWidth and kMaxVideoCodecHeight.
    UpdateCodec(payload_type, encoding_name,
                JsepSessionDescription::kMaxVideoCodecWidth,
                JsepSessionDescription::kMaxVideoCodecHeight,
                JsepSessionDescription::kDefaultVideoCodecFramerate,
                preference, video_desc);
  } else if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    // RFC 4566
    // For audio streams, <encoding parameters> indicates the number
    // of audio channels.  This parameter is OPTIONAL and may be
    // omitted if the number of channels is one, provided that no
    // additional parameters are needed.
    int channels = 1;
    if (codec_params.size() == 3) {
      channels = talk_base::FromString<int>(codec_params[2]);
    }
    int bitrate = 0;
    // The default behavior for ISAC (bitrate == 0) in webrtcvoiceengine.cc
    // (specifically FindWebRtcCodec) is bandwidth-adaptive variable bitrate.
    // The bandwidth adaptation doesn't always work well, so this code
    // sets a fixed target bitrate instead.
    if (_stricmp(encoding_name.c_str(), kIsacCodecName) == 0) {
      if (clock_rate <= 16000) {
        bitrate = kIsacWbDefaultRate;
      } else {
        bitrate = kIsacSwbDefaultRate;
      }
    }
    AudioContentDescription* audio_desc =
        static_cast<AudioContentDescription*>(media_desc);
    UpdateCodec(payload_type, encoding_name, clock_rate, bitrate, channels,
                preference, audio_desc);
  } else if (media_type == cricket::MEDIA_TYPE_DATA) {
    DataContentDescription* data_desc =
        static_cast<DataContentDescription*>(media_desc);
    data_desc->AddCodec(cricket::DataCodec(payload_type, encoding_name,
                                           preference));
  }
  return true;
}

void PruneRight(const char delimiter, std::string* message) {
  size_t trailing = message->find(delimiter);
  if (trailing != std::string::npos) {
    *message = message->substr(0, trailing);
  }
}

bool ParseFmtpParam(const std::string& line, std::string* parameter,
                    std::string* value, SdpParseError* error) {
  if (!SplitByDelimiter(line, kSdpDelimiterEqual, parameter, value)) {
    ParseFailed(line, "Unable to parse fmtp parameter. \'=\' missing.", error);
    return false;
  }
  // a=fmtp:<payload_type> <param1>=<value1>; <param2>=<value2>; ...
  // When parsing the values the trailing ";" gets picked up. Remove them.
  PruneRight(kSdpDelimiterSemicolon, value);
  return true;
}

bool ParseFmtpAttributes(const std::string& line, const MediaType media_type,
                         MediaContentDescription* media_desc,
                         SdpParseError* error) {
  if (media_type != cricket::MEDIA_TYPE_AUDIO &&
      media_type != cricket::MEDIA_TYPE_VIDEO) {
    return true;
  }
  std::vector<std::string> fields;
  talk_base::split(line.substr(kLinePrefixLength),
                   kSdpDelimiterSpace, &fields);

  // RFC 5576
  // a=fmtp:<format> <format specific parameters>
  // At least two fields, whereas the second one is any of the optional
  // parameters.
  if (fields.size() < 2) {
    ParseFailedExpectMinFieldNum(line, 2, error);
    return false;
  }

  std::string payload_type;
  if (!GetValue(fields[0], kAttributeFmtp, &payload_type, error)) {
    return false;
  }

  cricket::CodecParameterMap codec_params;
  for (std::vector<std::string>::const_iterator iter = fields.begin() + 1;
       iter != fields.end(); ++iter) {
    std::string name;
    std::string value;
    if (iter->find(kSdpDelimiterEqual) == std::string::npos) {
      // Only fmtps with equals are currently supported. Other fmtp types
      // should be ignored. Unknown fmtps do not constitute an error.
      continue;
    }
    if (!ParseFmtpParam(*iter, &name, &value, error)) {
      return false;
    }
    codec_params[name] = value;
  }

  int int_payload_type = talk_base::FromString<int>(payload_type);
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    UpdateCodec<AudioContentDescription, cricket::AudioCodec>(
        media_desc, int_payload_type, codec_params);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    UpdateCodec<VideoContentDescription, cricket::VideoCodec>(
        media_desc, int_payload_type, codec_params);
  }
  return true;
}

bool ParseRtcpFbAttribute(const std::string& line, const MediaType media_type,
                          MediaContentDescription* media_desc,
                          SdpParseError* error) {
  if (media_type != cricket::MEDIA_TYPE_AUDIO &&
      media_type != cricket::MEDIA_TYPE_VIDEO) {
    return true;
  }
  std::vector<std::string> rtcp_fb_fields;
  talk_base::split(line.c_str(), kSdpDelimiterSpace, &rtcp_fb_fields);
  if (rtcp_fb_fields.size() < 2) {
    return ParseFailedGetValue(line, kAttributeRtcpFb, error);
  }
  std::string payload_type_string;
  if (!GetValue(rtcp_fb_fields[0], kAttributeRtcpFb, &payload_type_string,
                error)) {
    return false;
  }
  int payload_type = (payload_type_string == "*") ?
      kWildcardPayloadType : talk_base::FromString<int>(payload_type_string);
  std::string id = rtcp_fb_fields[1];
  std::string param = "";
  for (std::vector<std::string>::iterator iter = rtcp_fb_fields.begin() + 2;
       iter != rtcp_fb_fields.end(); ++iter) {
    param.append(*iter);
  }
  const cricket::FeedbackParam feedback_param(id, param);

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    UpdateCodec<AudioContentDescription, cricket::AudioCodec>(media_desc,
                                                              payload_type,
                                                              feedback_param);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    UpdateCodec<VideoContentDescription, cricket::VideoCodec>(media_desc,
                                                              payload_type,
                                                              feedback_param);
  }
  return true;
}

}  // namespace webrtc
