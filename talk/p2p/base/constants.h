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

#ifndef TALK_P2P_BASE_CONSTANTS_H_
#define TALK_P2P_BASE_CONSTANTS_H_

#include <string>
#include "talk/xmllite/qname.h"

// This file contains constants related to signaling that are used in various
// classes in this directory.

namespace cricket {

// NS_ == namespace
// QN_ == buzz::QName (namespace + name)
// LN_ == "local name" == QName::LocalPart()
//   these are useful when you need to find a tag
//   that has different namespaces (like <description> or <transport>)

extern const char NS_EMPTY[];
extern const char NS_JINGLE[];
extern const char NS_JINGLE_DRAFT[];
extern const char NS_GINGLE[];

enum SignalingProtocol {
  PROTOCOL_JINGLE,
  PROTOCOL_GINGLE,
  PROTOCOL_HYBRID,
};

// actions (aka Gingle <session> or Jingle <jingle>)
extern const buzz::StaticQName QN_ACTION;
extern const char LN_INITIATOR[];
extern const buzz::StaticQName QN_INITIATOR;
extern const buzz::StaticQName QN_CREATOR;

extern const buzz::StaticQName QN_JINGLE;
extern const buzz::StaticQName QN_JINGLE_CONTENT;
extern const buzz::StaticQName QN_JINGLE_CONTENT_NAME;
extern const buzz::StaticQName QN_JINGLE_CONTENT_MEDIA;
extern const buzz::StaticQName QN_JINGLE_REASON;
extern const buzz::StaticQName QN_JINGLE_DRAFT_GROUP;
extern const buzz::StaticQName QN_JINGLE_DRAFT_GROUP_TYPE;
extern const char JINGLE_CONTENT_MEDIA_AUDIO[];
extern const char JINGLE_CONTENT_MEDIA_VIDEO[];
extern const char JINGLE_CONTENT_MEDIA_DATA[];
extern const char JINGLE_ACTION_SESSION_INITIATE[];
extern const char JINGLE_ACTION_SESSION_INFO[];
extern const char JINGLE_ACTION_SESSION_ACCEPT[];
extern const char JINGLE_ACTION_SESSION_TERMINATE[];
extern const char JINGLE_ACTION_TRANSPORT_INFO[];
extern const char JINGLE_ACTION_TRANSPORT_ACCEPT[];
extern const char JINGLE_ACTION_DESCRIPTION_INFO[];

extern const buzz::StaticQName QN_GINGLE_SESSION;
extern const char GINGLE_ACTION_INITIATE[];
extern const char GINGLE_ACTION_INFO[];
extern const char GINGLE_ACTION_ACCEPT[];
extern const char GINGLE_ACTION_REJECT[];
extern const char GINGLE_ACTION_TERMINATE[];
extern const char GINGLE_ACTION_CANDIDATES[];
extern const char GINGLE_ACTION_UPDATE[];

extern const char LN_ERROR[];
extern const buzz::StaticQName QN_GINGLE_REDIRECT;
extern const char STR_REDIRECT_PREFIX[];

// Session Contents (aka Gingle <session><description>
//                   or Jingle <content><description>)
extern const char LN_DESCRIPTION[];
extern const char LN_PAYLOADTYPE[];
extern const buzz::StaticQName QN_ID;
extern const buzz::StaticQName QN_SID;
extern const buzz::StaticQName QN_NAME;
extern const buzz::StaticQName QN_CLOCKRATE;
extern const buzz::StaticQName QN_BITRATE;
extern const buzz::StaticQName QN_CHANNELS;
extern const buzz::StaticQName QN_PARAMETER;
extern const char LN_NAME[];
extern const char LN_VALUE[];
extern const buzz::StaticQName QN_PAYLOADTYPE_PARAMETER_NAME;
extern const buzz::StaticQName QN_PAYLOADTYPE_PARAMETER_VALUE;
extern const char PAYLOADTYPE_PARAMETER_BITRATE[];
extern const char PAYLOADTYPE_PARAMETER_HEIGHT[];
extern const char PAYLOADTYPE_PARAMETER_WIDTH[];
extern const char PAYLOADTYPE_PARAMETER_FRAMERATE[];
extern const char LN_BANDWIDTH[];

// CN_ == "content name".  When we initiate a session, we choose the
// name, and when we receive a Gingle session, we provide default
// names (since Gingle has no content names).  But when we receive a
// Jingle call, the content name can be anything, so don't rely on
// these values being the same as the ones received.
extern const char CN_AUDIO[];
extern const char CN_VIDEO[];
extern const char CN_DATA[];
extern const char CN_OTHER[];
// other SDP related strings
// GN stands for group name
extern const char GROUP_TYPE_BUNDLE[];

extern const char NS_JINGLE_RTP[];
extern const buzz::StaticQName QN_JINGLE_RTP_CONTENT;
extern const buzz::StaticQName QN_SSRC;
extern const buzz::StaticQName QN_JINGLE_RTP_PAYLOADTYPE;
extern const buzz::StaticQName QN_JINGLE_RTP_BANDWIDTH;
extern const buzz::StaticQName QN_JINGLE_RTCP_MUX;
extern const buzz::StaticQName QN_JINGLE_RTCP_FB;
extern const buzz::StaticQName QN_SUBTYPE;
extern const buzz::StaticQName QN_JINGLE_RTP_HDREXT;
extern const buzz::StaticQName QN_URI;

extern const char NS_JINGLE_DRAFT_SCTP[];
extern const buzz::StaticQName QN_JINGLE_DRAFT_SCTP_CONTENT;
extern const buzz::StaticQName QN_JINGLE_DRAFT_SCTP_STREAM;

extern const char NS_GINGLE_AUDIO[];
extern const buzz::StaticQName QN_GINGLE_AUDIO_CONTENT;
extern const buzz::StaticQName QN_GINGLE_AUDIO_PAYLOADTYPE;
extern const buzz::StaticQName QN_GINGLE_AUDIO_SRCID;
extern const char NS_GINGLE_VIDEO[];
extern const buzz::StaticQName QN_GINGLE_VIDEO_CONTENT;
extern const buzz::StaticQName QN_GINGLE_VIDEO_PAYLOADTYPE;
extern const buzz::StaticQName QN_GINGLE_VIDEO_SRCID;
extern const buzz::StaticQName QN_GINGLE_VIDEO_BANDWIDTH;

// Crypto support.
extern const buzz::StaticQName QN_ENCRYPTION;
extern const buzz::StaticQName QN_ENCRYPTION_REQUIRED;
extern const buzz::StaticQName QN_CRYPTO;
extern const buzz::StaticQName QN_GINGLE_AUDIO_CRYPTO_USAGE;
extern const buzz::StaticQName QN_GINGLE_VIDEO_CRYPTO_USAGE;
extern const buzz::StaticQName QN_CRYPTO_SUITE;
extern const buzz::StaticQName QN_CRYPTO_KEY_PARAMS;
extern const buzz::StaticQName QN_CRYPTO_TAG;
extern const buzz::StaticQName QN_CRYPTO_SESSION_PARAMS;

// Transports and candidates.
extern const char LN_TRANSPORT[];
extern const char LN_CANDIDATE[];
extern const buzz::StaticQName QN_JINGLE_P2P_TRANSPORT;
extern const buzz::StaticQName QN_JINGLE_P2P_CANDIDATE;
extern const buzz::StaticQName QN_UFRAG;
extern const buzz::StaticQName QN_COMPONENT;
extern const buzz::StaticQName QN_PWD;
extern const buzz::StaticQName QN_IP;
extern const buzz::StaticQName QN_PORT;
extern const buzz::StaticQName QN_NETWORK;
extern const buzz::StaticQName QN_GENERATION;
extern const buzz::StaticQName QN_PRIORITY;
extern const buzz::StaticQName QN_PROTOCOL;
extern const char ICE_CANDIDATE_TYPE_PEER_STUN[];
extern const char ICE_CANDIDATE_TYPE_SERVER_STUN[];
extern const int ICE_UFRAG_LENGTH;
extern const int ICE_PWD_LENGTH;
extern const int ICE_CANDIDATE_COMPONENT_RTP;
extern const int ICE_CANDIDATE_COMPONENT_RTCP;
extern const int ICE_CANDIDATE_COMPONENT_DEFAULT;

extern const buzz::StaticQName QN_FINGERPRINT;
extern const buzz::StaticQName QN_FINGERPRINT_ALGORITHM;
extern const buzz::StaticQName QN_FINGERPRINT_DIGEST;

extern const char NS_JINGLE_ICE_UDP[];

extern const char ICE_OPTION_GICE[];
extern const char NS_GINGLE_P2P[];
extern const buzz::StaticQName QN_GINGLE_P2P_TRANSPORT;
extern const buzz::StaticQName QN_GINGLE_P2P_CANDIDATE;
extern const buzz::StaticQName QN_GINGLE_P2P_UNKNOWN_CHANNEL_NAME;
extern const buzz::StaticQName QN_GINGLE_CANDIDATE;
extern const buzz::StaticQName QN_ADDRESS;
extern const buzz::StaticQName QN_USERNAME;
extern const buzz::StaticQName QN_PASSWORD;
extern const buzz::StaticQName QN_PREFERENCE;
extern const char GINGLE_CANDIDATE_TYPE_STUN[];
extern const char GICE_CHANNEL_NAME_RTP[];
extern const char GICE_CHANNEL_NAME_RTCP[];
extern const char GICE_CHANNEL_NAME_VIDEO_RTP[];
extern const char GICE_CHANNEL_NAME_VIDEO_RTCP[];
extern const char GICE_CHANNEL_NAME_DATA_RTP[];
extern const char GICE_CHANNEL_NAME_DATA_RTCP[];

extern const char NS_GINGLE_RAW[];
extern const buzz::StaticQName QN_GINGLE_RAW_TRANSPORT;
extern const buzz::StaticQName QN_GINGLE_RAW_CHANNEL;

// terminate reasons and errors: see http://xmpp.org/extensions/xep-0166.html
extern const char JINGLE_ERROR_BAD_REQUEST[];  // like parse error
// got transport-info before session-initiate, for example
extern const char JINGLE_ERROR_OUT_OF_ORDER[];
extern const char JINGLE_ERROR_UNKNOWN_SESSION[];

// Call terminate reasons from XEP-166
extern const char STR_TERMINATE_DECLINE[];  // polite reject
extern const char STR_TERMINATE_SUCCESS[];  // polite hangup
extern const char STR_TERMINATE_ERROR[];  // something bad happened
extern const char STR_TERMINATE_INCOMPATIBLE_PARAMETERS[];  // no codecs?

// Old terminate reasons used by cricket
extern const char STR_TERMINATE_CALL_ENDED[];
extern const char STR_TERMINATE_RECIPIENT_UNAVAILABLE[];
extern const char STR_TERMINATE_RECIPIENT_BUSY[];
extern const char STR_TERMINATE_INSUFFICIENT_FUNDS[];
extern const char STR_TERMINATE_NUMBER_MALFORMED[];
extern const char STR_TERMINATE_NUMBER_DISALLOWED[];
extern const char STR_TERMINATE_PROTOCOL_ERROR[];
extern const char STR_TERMINATE_INTERNAL_SERVER_ERROR[];
extern const char STR_TERMINATE_UNKNOWN_ERROR[];

// Draft view and notify messages.
extern const char STR_JINGLE_DRAFT_CONTENT_NAME_VIDEO[];
extern const char STR_JINGLE_DRAFT_CONTENT_NAME_AUDIO[];
extern const buzz::StaticQName QN_NICK;
extern const buzz::StaticQName QN_TYPE;
extern const buzz::StaticQName QN_JINGLE_DRAFT_VIEW;
extern const char STR_JINGLE_DRAFT_VIEW_TYPE_NONE[];
extern const char STR_JINGLE_DRAFT_VIEW_TYPE_STATIC[];
extern const buzz::StaticQName QN_JINGLE_DRAFT_PARAMS;
extern const buzz::StaticQName QN_WIDTH;
extern const buzz::StaticQName QN_HEIGHT;
extern const buzz::StaticQName QN_FRAMERATE;
extern const buzz::StaticQName QN_JINGLE_DRAFT_STREAM;
extern const buzz::StaticQName QN_JINGLE_DRAFT_STREAMS;
extern const buzz::StaticQName QN_DISPLAY;
extern const buzz::StaticQName QN_CNAME;
extern const buzz::StaticQName QN_JINGLE_DRAFT_SSRC;
extern const buzz::StaticQName QN_JINGLE_DRAFT_SSRC_GROUP;
extern const buzz::StaticQName QN_SEMANTICS;
extern const buzz::StaticQName QN_JINGLE_LEGACY_NOTIFY;
extern const buzz::StaticQName QN_JINGLE_LEGACY_SOURCE;

// old stuff
#ifdef FEATURE_ENABLE_VOICEMAIL
extern const char NS_VOICEMAIL[];
extern const buzz::StaticQName QN_VOICEMAIL_REGARDING;
#endif

}  // namespace cricket

#endif  // TALK_P2P_BASE_CONSTANTS_H_
