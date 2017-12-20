/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SESSIONDESCRIPTION_H_
#define PC_SESSIONDESCRIPTION_H_

#include <string>
#include <vector>

#include "api/cryptoparams.h"
#include "api/rtpparameters.h"
#include "api/rtptransceiverinterface.h"
#include "media/base/codec.h"
#include "media/base/mediachannel.h"
#include "media/base/streamparams.h"
#include "p2p/base/transportinfo.h"
#include "rtc_base/constructormagic.h"

namespace cricket {

typedef std::vector<AudioCodec> AudioCodecs;
typedef std::vector<VideoCodec> VideoCodecs;
typedef std::vector<DataCodec> DataCodecs;
typedef std::vector<CryptoParams> CryptoParamsVec;
typedef std::vector<webrtc::RtpExtension> RtpHeaderExtensions;

// RTC4585 RTP/AVPF
extern const char kMediaProtocolAvpf[];
// RFC5124 RTP/SAVPF
extern const char kMediaProtocolSavpf[];

extern const char kMediaProtocolDtlsSavpf[];

extern const char kMediaProtocolRtpPrefix[];

extern const char kMediaProtocolSctp[];
extern const char kMediaProtocolDtlsSctp[];
extern const char kMediaProtocolUdpDtlsSctp[];
extern const char kMediaProtocolTcpDtlsSctp[];

// Options to control how session descriptions are generated.
const int kAutoBandwidth = -1;

// Describes a session content. Individual content types inherit from
// this class.  Analagous to a <jingle><content><description> or
// <session><description>.
class ContentDescription {
 public:
  virtual ~ContentDescription() {}
  virtual ContentDescription* Copy() const = 0;
};

// "content" (as used in XEP-0166) descriptions for voice and video.
class MediaContentDescription : public ContentDescription {
 public:
  MediaContentDescription() {}

  virtual MediaType type() const = 0;
  virtual bool has_codecs() const = 0;

  // |protocol| is the expected media transport protocol, such as RTP/AVPF,
  // RTP/SAVPF or SCTP/DTLS.
  std::string protocol() const { return protocol_; }
  void set_protocol(const std::string& protocol) { protocol_ = protocol; }

  webrtc::RtpTransceiverDirection direction() const { return direction_; }
  void set_direction(webrtc::RtpTransceiverDirection direction) {
    direction_ = direction;
  }

  bool rtcp_mux() const { return rtcp_mux_; }
  void set_rtcp_mux(bool mux) { rtcp_mux_ = mux; }

  bool rtcp_reduced_size() const { return rtcp_reduced_size_; }
  void set_rtcp_reduced_size(bool reduced_size) {
    rtcp_reduced_size_ = reduced_size;
  }

  int bandwidth() const { return bandwidth_; }
  void set_bandwidth(int bandwidth) { bandwidth_ = bandwidth; }

  const std::vector<CryptoParams>& cryptos() const { return cryptos_; }
  void AddCrypto(const CryptoParams& params) { cryptos_.push_back(params); }
  void set_cryptos(const std::vector<CryptoParams>& cryptos) {
    cryptos_ = cryptos;
  }

  const RtpHeaderExtensions& rtp_header_extensions() const {
    return rtp_header_extensions_;
  }
  void set_rtp_header_extensions(const RtpHeaderExtensions& extensions) {
    rtp_header_extensions_ = extensions;
    rtp_header_extensions_set_ = true;
  }
  void AddRtpHeaderExtension(const webrtc::RtpExtension& ext) {
    rtp_header_extensions_.push_back(ext);
    rtp_header_extensions_set_ = true;
  }
  void AddRtpHeaderExtension(const cricket::RtpHeaderExtension& ext) {
    webrtc::RtpExtension webrtc_extension;
    webrtc_extension.uri = ext.uri;
    webrtc_extension.id = ext.id;
    rtp_header_extensions_.push_back(webrtc_extension);
    rtp_header_extensions_set_ = true;
  }
  void ClearRtpHeaderExtensions() {
    rtp_header_extensions_.clear();
    rtp_header_extensions_set_ = true;
  }
  // We can't always tell if an empty list of header extensions is
  // because the other side doesn't support them, or just isn't hooked up to
  // signal them. For now we assume an empty list means no signaling, but
  // provide the ClearRtpHeaderExtensions method to allow "no support" to be
  // clearly indicated (i.e. when derived from other information).
  bool rtp_header_extensions_set() const { return rtp_header_extensions_set_; }
  const StreamParamsVec& streams() const { return streams_; }
  // TODO(pthatcher): Remove this by giving mediamessage.cc access
  // to MediaContentDescription
  StreamParamsVec& mutable_streams() { return streams_; }
  void AddStream(const StreamParams& stream) { streams_.push_back(stream); }
  // Legacy streams have an ssrc, but nothing else.
  void AddLegacyStream(uint32_t ssrc) {
    streams_.push_back(StreamParams::CreateLegacy(ssrc));
  }
  void AddLegacyStream(uint32_t ssrc, uint32_t fid_ssrc) {
    StreamParams sp = StreamParams::CreateLegacy(ssrc);
    sp.AddFidSsrc(ssrc, fid_ssrc);
    streams_.push_back(sp);
  }
  // Sets the CNAME of all StreamParams if it have not been set.
  void SetCnameIfEmpty(const std::string& cname) {
    for (cricket::StreamParamsVec::iterator it = streams_.begin();
         it != streams_.end(); ++it) {
      if (it->cname.empty())
        it->cname = cname;
    }
  }
  uint32_t first_ssrc() const {
    if (streams_.empty()) {
      return 0;
    }
    return streams_[0].first_ssrc();
  }
  bool has_ssrcs() const {
    if (streams_.empty()) {
      return false;
    }
    return streams_[0].has_ssrcs();
  }

  void set_conference_mode(bool enable) { conference_mode_ = enable; }
  bool conference_mode() const { return conference_mode_; }

  // https://tools.ietf.org/html/rfc4566#section-5.7
  // May be present at the media or session level of SDP. If present at both
  // levels, the media-level attribute overwrites the session-level one.
  void set_connection_address(const rtc::SocketAddress& address) {
    connection_address_ = address;
  }
  const rtc::SocketAddress& connection_address() const {
    return connection_address_;
  }

 protected:
  bool rtcp_mux_ = false;
  bool rtcp_reduced_size_ = false;
  int bandwidth_ = kAutoBandwidth;
  std::string protocol_;
  std::vector<CryptoParams> cryptos_;
  std::vector<webrtc::RtpExtension> rtp_header_extensions_;
  bool rtp_header_extensions_set_ = false;
  StreamParamsVec streams_;
  bool conference_mode_ = false;
  webrtc::RtpTransceiverDirection direction_ =
      webrtc::RtpTransceiverDirection::kSendRecv;
  rtc::SocketAddress connection_address_;
};

template <class C>
class MediaContentDescriptionImpl : public MediaContentDescription {
 public:
  typedef C CodecType;

  // Codecs should be in preference order (most preferred codec first).
  const std::vector<C>& codecs() const { return codecs_; }
  void set_codecs(const std::vector<C>& codecs) { codecs_ = codecs; }
  virtual bool has_codecs() const { return !codecs_.empty(); }
  bool HasCodec(int id) {
    bool found = false;
    for (typename std::vector<C>::iterator iter = codecs_.begin();
         iter != codecs_.end(); ++iter) {
      if (iter->id == id) {
        found = true;
        break;
      }
    }
    return found;
  }
  void AddCodec(const C& codec) { codecs_.push_back(codec); }
  void AddOrReplaceCodec(const C& codec) {
    for (typename std::vector<C>::iterator iter = codecs_.begin();
         iter != codecs_.end(); ++iter) {
      if (iter->id == codec.id) {
        *iter = codec;
        return;
      }
    }
    AddCodec(codec);
  }
  void AddCodecs(const std::vector<C>& codecs) {
    typename std::vector<C>::const_iterator codec;
    for (codec = codecs.begin(); codec != codecs.end(); ++codec) {
      AddCodec(*codec);
    }
  }

 private:
  std::vector<C> codecs_;
};

class AudioContentDescription : public MediaContentDescriptionImpl<AudioCodec> {
 public:
  AudioContentDescription() {}

  virtual ContentDescription* Copy() const {
    return new AudioContentDescription(*this);
  }
  virtual MediaType type() const { return MEDIA_TYPE_AUDIO; }
};

class VideoContentDescription : public MediaContentDescriptionImpl<VideoCodec> {
 public:
  virtual ContentDescription* Copy() const {
    return new VideoContentDescription(*this);
  }
  virtual MediaType type() const { return MEDIA_TYPE_VIDEO; }
};

class DataContentDescription : public MediaContentDescriptionImpl<DataCodec> {
 public:
  DataContentDescription() {}

  virtual ContentDescription* Copy() const {
    return new DataContentDescription(*this);
  }
  virtual MediaType type() const { return MEDIA_TYPE_DATA; }

  bool use_sctpmap() const { return use_sctpmap_; }
  void set_use_sctpmap(bool enable) { use_sctpmap_ = enable; }

 private:
  bool use_sctpmap_ = true;
};

// Analagous to a <jingle><content> or <session><description>.
// name = name of <content name="...">
// type = xmlns of <content>
struct ContentInfo {
  ContentInfo() {}
  ContentInfo(const std::string& name,
              const std::string& type,
              ContentDescription* description)
      : name(name), type(type), description(description) {}
  ContentInfo(const std::string& name,
              const std::string& type,
              bool rejected,
              ContentDescription* description)
      : name(name), type(type), rejected(rejected), description(description) {}
  ContentInfo(const std::string& name,
              const std::string& type,
              bool rejected,
              bool bundle_only,
              ContentDescription* description)
      : name(name),
        type(type),
        rejected(rejected),
        bundle_only(bundle_only),
        description(description) {}
  std::string name;
  std::string type;
  bool rejected = false;
  bool bundle_only = false;
  ContentDescription* description = nullptr;
};

typedef std::vector<std::string> ContentNames;

// This class provides a mechanism to aggregate different media contents into a
// group. This group can also be shared with the peers in a pre-defined format.
// GroupInfo should be populated only with the |content_name| of the
// MediaDescription.
class ContentGroup {
 public:
  explicit ContentGroup(const std::string& semantics);
  ContentGroup(const ContentGroup&);
  ContentGroup(ContentGroup&&);
  ContentGroup& operator=(const ContentGroup&);
  ContentGroup& operator=(ContentGroup&&);
  ~ContentGroup();

  const std::string& semantics() const { return semantics_; }
  const ContentNames& content_names() const { return content_names_; }

  const std::string* FirstContentName() const;
  bool HasContentName(const std::string& content_name) const;
  void AddContentName(const std::string& content_name);
  bool RemoveContentName(const std::string& content_name);

 private:
  std::string semantics_;
  ContentNames content_names_;
};

typedef std::vector<ContentInfo> ContentInfos;
typedef std::vector<ContentGroup> ContentGroups;

const ContentInfo* FindContentInfoByName(const ContentInfos& contents,
                                         const std::string& name);
const ContentInfo* FindContentInfoByType(const ContentInfos& contents,
                                         const std::string& type);

// Describes a collection of contents, each with its own name and
// type.  Analogous to a <jingle> or <session> stanza.  Assumes that
// contents are unique be name, but doesn't enforce that.
class SessionDescription {
 public:
  SessionDescription();
  explicit SessionDescription(const ContentInfos& contents);
  SessionDescription(const ContentInfos& contents, const ContentGroups& groups);
  SessionDescription(const ContentInfos& contents,
                     const TransportInfos& transports,
                     const ContentGroups& groups);
  ~SessionDescription();

  SessionDescription* Copy() const;

  // Content accessors.
  const ContentInfos& contents() const { return contents_; }
  ContentInfos& contents() { return contents_; }
  const ContentInfo* GetContentByName(const std::string& name) const;
  ContentInfo* GetContentByName(const std::string& name);
  const ContentDescription* GetContentDescriptionByName(
      const std::string& name) const;
  ContentDescription* GetContentDescriptionByName(const std::string& name);
  const ContentInfo* FirstContentByType(const std::string& type) const;
  const ContentInfo* FirstContent() const;

  // Content mutators.
  // Adds a content to this description. Takes ownership of ContentDescription*.
  void AddContent(const std::string& name,
                  const std::string& type,
                  ContentDescription* description);
  void AddContent(const std::string& name,
                  const std::string& type,
                  bool rejected,
                  ContentDescription* description);
  void AddContent(const std::string& name,
                  const std::string& type,
                  bool rejected,
                  bool bundle_only,
                  ContentDescription* description);
  bool RemoveContentByName(const std::string& name);

  // Transport accessors.
  const TransportInfos& transport_infos() const { return transport_infos_; }
  TransportInfos& transport_infos() { return transport_infos_; }
  const TransportInfo* GetTransportInfoByName(const std::string& name) const;
  TransportInfo* GetTransportInfoByName(const std::string& name);
  const TransportDescription* GetTransportDescriptionByName(
      const std::string& name) const {
    const TransportInfo* tinfo = GetTransportInfoByName(name);
    return tinfo ? &tinfo->description : NULL;
  }

  // Transport mutators.
  void set_transport_infos(const TransportInfos& transport_infos) {
    transport_infos_ = transport_infos;
  }
  // Adds a TransportInfo to this description.
  // Returns false if a TransportInfo with the same name already exists.
  bool AddTransportInfo(const TransportInfo& transport_info);
  bool RemoveTransportInfoByName(const std::string& name);

  // Group accessors.
  const ContentGroups& groups() const { return content_groups_; }
  const ContentGroup* GetGroupByName(const std::string& name) const;
  bool HasGroup(const std::string& name) const;

  // Group mutators.
  void AddGroup(const ContentGroup& group) { content_groups_.push_back(group); }
  // Remove the first group with the same semantics specified by |name|.
  void RemoveGroupByName(const std::string& name);

  // Global attributes.
  void set_msid_supported(bool supported) { msid_supported_ = supported; }
  bool msid_supported() const { return msid_supported_; }

 private:
  SessionDescription(const SessionDescription&);

  ContentInfos contents_;
  TransportInfos transport_infos_;
  ContentGroups content_groups_;
  bool msid_supported_ = true;
};

// Indicates whether a ContentDescription was sent by the local client
// or received from the remote client.
enum ContentSource { CS_LOCAL, CS_REMOTE };

}  // namespace cricket

#endif  // PC_SESSIONDESCRIPTION_H_
