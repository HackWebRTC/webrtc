/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#include "talk/p2p/base/constants.h"

#include <string>

#include "talk/xmllite/qname.h"

namespace cricket {

const char NS_EMPTY[] = "";
const char NS_JINGLE[] = "urn:xmpp:jingle:1";
const char NS_JINGLE_DRAFT[] = "google:jingle";
const char NS_GINGLE[] = "http://www.google.com/session";

// actions (aka <session> or <jingle>)
const buzz::StaticQName QN_ACTION = { NS_EMPTY, "action" };
const char LN_INITIATOR[] = "initiator";
const buzz::StaticQName QN_INITIATOR = { NS_EMPTY, LN_INITIATOR };
const buzz::StaticQName QN_CREATOR = { NS_EMPTY, "creator" };

const buzz::StaticQName QN_JINGLE = { NS_JINGLE, "jingle" };
const buzz::StaticQName QN_JINGLE_CONTENT = { NS_JINGLE, "content" };
const buzz::StaticQName QN_JINGLE_CONTENT_NAME = { NS_EMPTY, "name" };
const buzz::StaticQName QN_JINGLE_CONTENT_MEDIA = { NS_EMPTY, "media" };
const buzz::StaticQName QN_JINGLE_REASON = { NS_JINGLE, "reason" };
const buzz::StaticQName QN_JINGLE_DRAFT_GROUP = { NS_JINGLE_DRAFT, "group" };
const buzz::StaticQName QN_JINGLE_DRAFT_GROUP_TYPE = { NS_EMPTY, "type" };
const char JINGLE_CONTENT_MEDIA_AUDIO[] = "audio";
const char JINGLE_CONTENT_MEDIA_VIDEO[] = "video";
const char JINGLE_CONTENT_MEDIA_DATA[] = "data";
const char JINGLE_ACTION_SESSION_INITIATE[] = "session-initiate";
const char JINGLE_ACTION_SESSION_INFO[] = "session-info";
const char JINGLE_ACTION_SESSION_ACCEPT[] = "session-accept";
const char JINGLE_ACTION_SESSION_TERMINATE[] = "session-terminate";
const char JINGLE_ACTION_TRANSPORT_INFO[] = "transport-info";
const char JINGLE_ACTION_TRANSPORT_ACCEPT[] = "transport-accept";
const char JINGLE_ACTION_DESCRIPTION_INFO[] = "description-info";

const buzz::StaticQName QN_GINGLE_SESSION = { NS_GINGLE, "session" };
const char GINGLE_ACTION_INITIATE[] = "initiate";
const char GINGLE_ACTION_INFO[] = "info";
const char GINGLE_ACTION_ACCEPT[] = "accept";
const char GINGLE_ACTION_REJECT[] = "reject";
const char GINGLE_ACTION_TERMINATE[] = "terminate";
const char GINGLE_ACTION_CANDIDATES[] = "candidates";
const char GINGLE_ACTION_UPDATE[] = "update";

const char LN_ERROR[] = "error";
const buzz::StaticQName QN_GINGLE_REDIRECT = { NS_GINGLE, "redirect" };
const char STR_REDIRECT_PREFIX[] = "xmpp:";

// Session Contents (aka Gingle <session><description>
//                   or Jingle <content><description>)
const char LN_DESCRIPTION[] = "description";
const char LN_PAYLOADTYPE[] = "payload-type";
const buzz::StaticQName QN_ID = { NS_EMPTY, "id" };
const buzz::StaticQName QN_SID = { NS_EMPTY, "sid" };
const buzz::StaticQName QN_NAME = { NS_EMPTY, "name" };
const buzz::StaticQName QN_CLOCKRATE = { NS_EMPTY, "clockrate" };
const buzz::StaticQName QN_BITRATE = { NS_EMPTY, "bitrate" };
const buzz::StaticQName QN_CHANNELS = { NS_EMPTY, "channels" };
const buzz::StaticQName QN_WIDTH = { NS_EMPTY, "width" };
const buzz::StaticQName QN_HEIGHT = { NS_EMPTY, "height" };
const buzz::StaticQName QN_FRAMERATE = { NS_EMPTY, "framerate" };
const char LN_NAME[] = "name";
const char LN_VALUE[] = "value";
const buzz::StaticQName QN_PAYLOADTYPE_PARAMETER_NAME = { NS_EMPTY, LN_NAME };
const buzz::StaticQName QN_PAYLOADTYPE_PARAMETER_VALUE = { NS_EMPTY, LN_VALUE };
const char PAYLOADTYPE_PARAMETER_BITRATE[] = "bitrate";
const char PAYLOADTYPE_PARAMETER_HEIGHT[] = "height";
const char PAYLOADTYPE_PARAMETER_WIDTH[] = "width";
const char PAYLOADTYPE_PARAMETER_FRAMERATE[] = "framerate";
const char LN_BANDWIDTH[] = "bandwidth";

const char CN_AUDIO[] = "audio";
const char CN_VIDEO[] = "video";
const char CN_DATA[] = "data";
const char CN_OTHER[] = "main";
// other SDP related strings
const char GROUP_TYPE_BUNDLE[] = "BUNDLE";

const char NS_JINGLE_RTP[] = "urn:xmpp:jingle:apps:rtp:1";
const buzz::StaticQName QN_JINGLE_RTP_CONTENT =
    { NS_JINGLE_RTP, LN_DESCRIPTION };
const buzz::StaticQName QN_SSRC = { NS_EMPTY, "ssrc" };
const buzz::StaticQName QN_JINGLE_RTP_PAYLOADTYPE =
    { NS_JINGLE_RTP, LN_PAYLOADTYPE };
const buzz::StaticQName QN_JINGLE_RTP_BANDWIDTH =
    { NS_JINGLE_RTP, LN_BANDWIDTH };
const buzz::StaticQName QN_JINGLE_RTCP_MUX = { NS_JINGLE_RTP, "rtcp-mux" };
const buzz::StaticQName QN_JINGLE_RTCP_FB = { NS_JINGLE_RTP, "rtcp-fb" };
const buzz::StaticQName QN_SUBTYPE = { NS_EMPTY, "subtype" };
const buzz::StaticQName QN_PARAMETER = { NS_JINGLE_RTP, "parameter" };
const buzz::StaticQName QN_JINGLE_RTP_HDREXT =
    { NS_JINGLE_RTP, "rtp-hdrext" };
const buzz::StaticQName QN_URI = { NS_EMPTY, "uri" };

const char NS_JINGLE_DRAFT_SCTP[] = "google:jingle:sctp";
const buzz::StaticQName QN_JINGLE_DRAFT_SCTP_CONTENT =
    { NS_JINGLE_DRAFT_SCTP, LN_DESCRIPTION };
const buzz::StaticQName QN_JINGLE_DRAFT_SCTP_STREAM =
    { NS_JINGLE_DRAFT_SCTP, "stream" };

const char NS_GINGLE_AUDIO[] = "http://www.google.com/session/phone";
const buzz::StaticQName QN_GINGLE_AUDIO_CONTENT =
    { NS_GINGLE_AUDIO, LN_DESCRIPTION };
const buzz::StaticQName QN_GINGLE_AUDIO_PAYLOADTYPE =
    { NS_GINGLE_AUDIO, LN_PAYLOADTYPE };
const buzz::StaticQName QN_GINGLE_AUDIO_SRCID = { NS_GINGLE_AUDIO, "src-id" };
const char NS_GINGLE_VIDEO[] = "http://www.google.com/session/video";
const buzz::StaticQName QN_GINGLE_VIDEO_CONTENT =
    { NS_GINGLE_VIDEO, LN_DESCRIPTION };
const buzz::StaticQName QN_GINGLE_VIDEO_PAYLOADTYPE =
    { NS_GINGLE_VIDEO, LN_PAYLOADTYPE };
const buzz::StaticQName QN_GINGLE_VIDEO_SRCID = { NS_GINGLE_VIDEO, "src-id" };
const buzz::StaticQName QN_GINGLE_VIDEO_BANDWIDTH =
    { NS_GINGLE_VIDEO, LN_BANDWIDTH };

// Crypto support.
const buzz::StaticQName QN_ENCRYPTION = { NS_JINGLE_RTP, "encryption" };
const buzz::StaticQName QN_ENCRYPTION_REQUIRED = { NS_EMPTY, "required" };
const buzz::StaticQName QN_CRYPTO = { NS_JINGLE_RTP, "crypto" };
const buzz::StaticQName QN_GINGLE_AUDIO_CRYPTO_USAGE =
    { NS_GINGLE_AUDIO, "usage" };
const buzz::StaticQName QN_GINGLE_VIDEO_CRYPTO_USAGE =
    { NS_GINGLE_VIDEO, "usage" };
const buzz::StaticQName QN_CRYPTO_SUITE = { NS_EMPTY, "crypto-suite" };
const buzz::StaticQName QN_CRYPTO_KEY_PARAMS = { NS_EMPTY, "key-params" };
const buzz::StaticQName QN_CRYPTO_TAG = { NS_EMPTY, "tag" };
const buzz::StaticQName QN_CRYPTO_SESSION_PARAMS =
    { NS_EMPTY, "session-params" };

// Transports and candidates.
const char LN_TRANSPORT[] = "transport";
const char LN_CANDIDATE[] = "candidate";
const buzz::StaticQName QN_UFRAG = { cricket::NS_EMPTY, "ufrag" };
const buzz::StaticQName QN_PWD = { cricket::NS_EMPTY, "pwd" };
const buzz::StaticQName QN_COMPONENT = { cricket::NS_EMPTY, "component" };
const buzz::StaticQName QN_IP = { cricket::NS_EMPTY, "ip" };
const buzz::StaticQName QN_PORT = { cricket::NS_EMPTY, "port" };
const buzz::StaticQName QN_NETWORK = { cricket::NS_EMPTY, "network" };
const buzz::StaticQName QN_GENERATION = { cricket::NS_EMPTY, "generation" };
const buzz::StaticQName QN_PRIORITY = { cricket::NS_EMPTY, "priority" };
const buzz::StaticQName QN_PROTOCOL = { cricket::NS_EMPTY, "protocol" };
const char ICE_CANDIDATE_TYPE_PEER_STUN[] = "prflx";
const char ICE_CANDIDATE_TYPE_SERVER_STUN[] = "srflx";
// Minimum ufrag length is 4 characters as per RFC5245. We chose 16 because
// some internal systems expect username to be 16 bytes.
const int ICE_UFRAG_LENGTH = 16;
// Minimum password length of 22 characters as per RFC5245. We chose 24 because
// some internal systems expect password to be multiple of 4.
const int ICE_PWD_LENGTH = 24;
// TODO: This is media-specific, so might belong
// somewhere like media/base/constants.h
const int ICE_CANDIDATE_COMPONENT_RTP = 1;
const int ICE_CANDIDATE_COMPONENT_RTCP = 2;
const int ICE_CANDIDATE_COMPONENT_DEFAULT = 1;

const buzz::StaticQName QN_FINGERPRINT = { cricket::NS_EMPTY, "fingerprint" };
const buzz::StaticQName QN_FINGERPRINT_ALGORITHM =
    { cricket::NS_EMPTY, "algorithm" };
const buzz::StaticQName QN_FINGERPRINT_DIGEST = { cricket::NS_EMPTY, "digest" };

const char NS_JINGLE_ICE_UDP[] = "urn:xmpp:jingle:transports:ice-udp:1";

const char ICE_OPTION_GICE[] = "google-ice";
const char NS_GINGLE_P2P[] = "http://www.google.com/transport/p2p";
const buzz::StaticQName QN_GINGLE_P2P_TRANSPORT =
    { NS_GINGLE_P2P, LN_TRANSPORT };
const buzz::StaticQName QN_GINGLE_P2P_CANDIDATE =
    { NS_GINGLE_P2P, LN_CANDIDATE };
const buzz::StaticQName QN_GINGLE_P2P_UNKNOWN_CHANNEL_NAME =
    { NS_GINGLE_P2P, "unknown-channel-name" };
const buzz::StaticQName QN_GINGLE_CANDIDATE = { NS_GINGLE, LN_CANDIDATE };
const buzz::StaticQName QN_ADDRESS = { cricket::NS_EMPTY, "address" };
const buzz::StaticQName QN_USERNAME = { cricket::NS_EMPTY, "username" };
const buzz::StaticQName QN_PASSWORD = { cricket::NS_EMPTY, "password" };
const buzz::StaticQName QN_PREFERENCE = { cricket::NS_EMPTY, "preference" };
const char GICE_CANDIDATE_TYPE_STUN[] = "stun";
const char GICE_CHANNEL_NAME_RTP[] = "rtp";
const char GICE_CHANNEL_NAME_RTCP[] = "rtcp";
const char GICE_CHANNEL_NAME_VIDEO_RTP[] = "video_rtp";
const char GICE_CHANNEL_NAME_VIDEO_RTCP[] = "video_rtcp";
const char GICE_CHANNEL_NAME_DATA_RTP[] = "data_rtp";
const char GICE_CHANNEL_NAME_DATA_RTCP[] = "data_rtcp";

// terminate reasons and errors
const char JINGLE_ERROR_BAD_REQUEST[] = "bad-request";
const char JINGLE_ERROR_OUT_OF_ORDER[] = "out-of-order";
const char JINGLE_ERROR_UNKNOWN_SESSION[] = "unknown-session";

// Call terminate reasons from XEP-166
const char STR_TERMINATE_DECLINE[] = "decline";
const char STR_TERMINATE_SUCCESS[] = "success";
const char STR_TERMINATE_ERROR[] = "general-error";
const char STR_TERMINATE_INCOMPATIBLE_PARAMETERS[] = "incompatible-parameters";

// Old terminate reasons used by cricket
const char STR_TERMINATE_CALL_ENDED[] = "call-ended";
const char STR_TERMINATE_RECIPIENT_UNAVAILABLE[] = "recipient-unavailable";
const char STR_TERMINATE_RECIPIENT_BUSY[] = "recipient-busy";
const char STR_TERMINATE_INSUFFICIENT_FUNDS[] = "insufficient-funds";
const char STR_TERMINATE_NUMBER_MALFORMED[] = "number-malformed";
const char STR_TERMINATE_NUMBER_DISALLOWED[] = "number-disallowed";
const char STR_TERMINATE_PROTOCOL_ERROR[] = "protocol-error";
const char STR_TERMINATE_INTERNAL_SERVER_ERROR[] = "internal-server-error";
const char STR_TERMINATE_UNKNOWN_ERROR[] = "unknown-error";

// Draft view and notify messages.
const char STR_JINGLE_DRAFT_CONTENT_NAME_VIDEO[] = "video";
const char STR_JINGLE_DRAFT_CONTENT_NAME_AUDIO[] = "audio";
const buzz::StaticQName QN_NICK = { cricket::NS_EMPTY, "nick" };
const buzz::StaticQName QN_TYPE = { cricket::NS_EMPTY, "type" };
const buzz::StaticQName QN_JINGLE_DRAFT_VIEW = { NS_JINGLE_DRAFT, "view" };
const char STR_JINGLE_DRAFT_VIEW_TYPE_NONE[] = "none";
const char STR_JINGLE_DRAFT_VIEW_TYPE_STATIC[] = "static";
const buzz::StaticQName QN_JINGLE_DRAFT_PARAMS = { NS_JINGLE_DRAFT, "params" };
const buzz::StaticQName QN_JINGLE_DRAFT_STREAMS = { NS_JINGLE_DRAFT, "streams" };
const buzz::StaticQName QN_JINGLE_DRAFT_STREAM = { NS_JINGLE_DRAFT, "stream" };
const buzz::StaticQName QN_DISPLAY = { cricket::NS_EMPTY, "display" };
const buzz::StaticQName QN_CNAME = { cricket::NS_EMPTY, "cname" };
const buzz::StaticQName QN_JINGLE_DRAFT_SSRC = { NS_JINGLE_DRAFT, "ssrc" };
const buzz::StaticQName QN_JINGLE_DRAFT_SSRC_GROUP =
    { NS_JINGLE_DRAFT, "ssrc-group" };
const buzz::StaticQName QN_SEMANTICS = { cricket::NS_EMPTY, "semantics" };
const buzz::StaticQName QN_JINGLE_LEGACY_NOTIFY = { NS_JINGLE_DRAFT, "notify" };
const buzz::StaticQName QN_JINGLE_LEGACY_SOURCE = { NS_JINGLE_DRAFT, "source" };

const char NS_GINGLE_RAW[] = "http://www.google.com/transport/raw-udp";
const buzz::StaticQName QN_GINGLE_RAW_TRANSPORT = { NS_GINGLE_RAW, "transport" };
const buzz::StaticQName QN_GINGLE_RAW_CHANNEL = { NS_GINGLE_RAW, "channel" };

// old stuff
#ifdef FEATURE_ENABLE_VOICEMAIL
const char NS_VOICEMAIL[] = "http://www.google.com/session/voicemail";
const buzz::StaticQName QN_VOICEMAIL_REGARDING = { NS_VOICEMAIL, "regarding" };
#endif

}  // namespace cricket
