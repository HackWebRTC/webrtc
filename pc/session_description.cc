/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/session_description.h"

#include <algorithm>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "pc/media_protocol_names.h"
#include "rtc_base/checks.h"

namespace cricket {
namespace {

ContentInfo* FindContentInfoByName(ContentInfos* contents,
                                   const std::string& name) {
  RTC_DCHECK(contents);
  for (ContentInfo& content : *contents) {
    if (content.name == name) {
      return &content;
    }
  }
  return nullptr;
}

}  // namespace

const ContentInfo* FindContentInfoByName(const ContentInfos& contents,
                                         const std::string& name) {
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); ++content) {
    if (content->name == name) {
      return &(*content);
    }
  }
  return NULL;
}

const ContentInfo* FindContentInfoByType(const ContentInfos& contents,
                                         MediaProtocolType type) {
  for (const auto& content : contents) {
    if (content.type == type) {
      return &content;
    }
  }
  return nullptr;
}

ContentGroup::ContentGroup(const std::string& semantics)
    : semantics_(semantics) {}

ContentGroup::ContentGroup(const ContentGroup&) = default;
ContentGroup::ContentGroup(ContentGroup&&) = default;
ContentGroup& ContentGroup::operator=(const ContentGroup&) = default;
ContentGroup& ContentGroup::operator=(ContentGroup&&) = default;
ContentGroup::~ContentGroup() = default;

const std::string* ContentGroup::FirstContentName() const {
  return (!content_names_.empty()) ? &(*content_names_.begin()) : NULL;
}

bool ContentGroup::HasContentName(const std::string& content_name) const {
  return absl::c_linear_search(content_names_, content_name);
}

void ContentGroup::AddContentName(const std::string& content_name) {
  if (!HasContentName(content_name)) {
    content_names_.push_back(content_name);
  }
}

bool ContentGroup::RemoveContentName(const std::string& content_name) {
  ContentNames::iterator iter = absl::c_find(content_names_, content_name);
  if (iter == content_names_.end()) {
    return false;
  }
  content_names_.erase(iter);
  return true;
}

SessionDescription::SessionDescription() = default;
SessionDescription::SessionDescription(const SessionDescription&) = default;

SessionDescription::~SessionDescription() {
  for (ContentInfos::iterator content = contents_.begin();
       content != contents_.end(); ++content) {
    delete content->description;
  }
}

std::unique_ptr<SessionDescription> SessionDescription::Clone() const {
  // Copy the non-special portions using the private copy constructor.
  auto copy = absl::WrapUnique(new SessionDescription(*this));
  // Copy all ContentDescriptions.
  for (ContentInfos::iterator content = copy->contents_.begin();
       content != copy->contents().end(); ++content) {
    content->description = content->description->Copy();
  }
  return copy;
}

SessionDescription* SessionDescription::Copy() const {
  return Clone().release();
}

const ContentInfo* SessionDescription::GetContentByName(
    const std::string& name) const {
  return FindContentInfoByName(contents_, name);
}

ContentInfo* SessionDescription::GetContentByName(const std::string& name) {
  return FindContentInfoByName(&contents_, name);
}

const MediaContentDescription* SessionDescription::GetContentDescriptionByName(
    const std::string& name) const {
  const ContentInfo* cinfo = FindContentInfoByName(contents_, name);
  if (cinfo == NULL) {
    return NULL;
  }

  return cinfo->media_description();
}

MediaContentDescription* SessionDescription::GetContentDescriptionByName(
    const std::string& name) {
  ContentInfo* cinfo = FindContentInfoByName(&contents_, name);
  if (cinfo == NULL) {
    return NULL;
  }

  return cinfo->media_description();
}

const ContentInfo* SessionDescription::FirstContentByType(
    MediaProtocolType type) const {
  return FindContentInfoByType(contents_, type);
}

const ContentInfo* SessionDescription::FirstContent() const {
  return (contents_.empty()) ? NULL : &(*contents_.begin());
}

void SessionDescription::AddContent(const std::string& name,
                                    MediaProtocolType type,
                                    MediaContentDescription* description) {
  ContentInfo content(type);
  content.name = name;
  content.description = description;
  AddContent(&content);
}

void SessionDescription::AddContent(const std::string& name,
                                    MediaProtocolType type,
                                    bool rejected,
                                    MediaContentDescription* description) {
  ContentInfo content(type);
  content.name = name;
  content.rejected = rejected;
  content.description = description;
  AddContent(&content);
}

void SessionDescription::AddContent(const std::string& name,
                                    MediaProtocolType type,
                                    bool rejected,
                                    bool bundle_only,
                                    MediaContentDescription* description) {
  ContentInfo content(type);
  content.name = name;
  content.rejected = rejected;
  content.bundle_only = bundle_only;
  content.description = description;
  AddContent(&content);
}

void SessionDescription::AddContent(ContentInfo* content) {
  // Unwrap the as_data shim layer before using.
  auto* description = content->media_description();
  bool should_delete = false;
  if (description->as_rtp_data()) {
    if (description->as_rtp_data() != description) {
      content->set_media_description(
          description->deprecated_as_data()->Unshim(&should_delete));
    }
  }
  if (description->as_sctp()) {
    if (description->as_sctp() != description) {
      content->set_media_description(
          description->deprecated_as_data()->Unshim(&should_delete));
    }
  }
  if (should_delete) {
    delete description;
  }
  if (extmap_allow_mixed()) {
    // Mixed support on session level overrides setting on media level.
    content->description->set_extmap_allow_mixed_enum(
        MediaContentDescription::kSession);
  }
  contents_.push_back(std::move(*content));
}

bool SessionDescription::RemoveContentByName(const std::string& name) {
  for (ContentInfos::iterator content = contents_.begin();
       content != contents_.end(); ++content) {
    if (content->name == name) {
      delete content->description;
      contents_.erase(content);
      return true;
    }
  }

  return false;
}

void SessionDescription::AddTransportInfo(const TransportInfo& transport_info) {
  transport_infos_.push_back(transport_info);
}

bool SessionDescription::RemoveTransportInfoByName(const std::string& name) {
  for (TransportInfos::iterator transport_info = transport_infos_.begin();
       transport_info != transport_infos_.end(); ++transport_info) {
    if (transport_info->content_name == name) {
      transport_infos_.erase(transport_info);
      return true;
    }
  }
  return false;
}

const TransportInfo* SessionDescription::GetTransportInfoByName(
    const std::string& name) const {
  for (TransportInfos::const_iterator iter = transport_infos_.begin();
       iter != transport_infos_.end(); ++iter) {
    if (iter->content_name == name) {
      return &(*iter);
    }
  }
  return NULL;
}

TransportInfo* SessionDescription::GetTransportInfoByName(
    const std::string& name) {
  for (TransportInfos::iterator iter = transport_infos_.begin();
       iter != transport_infos_.end(); ++iter) {
    if (iter->content_name == name) {
      return &(*iter);
    }
  }
  return NULL;
}

void SessionDescription::RemoveGroupByName(const std::string& name) {
  for (ContentGroups::iterator iter = content_groups_.begin();
       iter != content_groups_.end(); ++iter) {
    if (iter->semantics() == name) {
      content_groups_.erase(iter);
      break;
    }
  }
}

bool SessionDescription::HasGroup(const std::string& name) const {
  for (ContentGroups::const_iterator iter = content_groups_.begin();
       iter != content_groups_.end(); ++iter) {
    if (iter->semantics() == name) {
      return true;
    }
  }
  return false;
}

const ContentGroup* SessionDescription::GetGroupByName(
    const std::string& name) const {
  for (ContentGroups::const_iterator iter = content_groups_.begin();
       iter != content_groups_.end(); ++iter) {
    if (iter->semantics() == name) {
      return &(*iter);
    }
  }
  return NULL;
}

// DataContentDescription shim creation
DataContentDescription* RtpDataContentDescription::deprecated_as_data() {
  if (!shim_) {
    shim_.reset(new DataContentDescription(this));
  }
  return shim_.get();
}

DataContentDescription* RtpDataContentDescription::as_data() {
  return deprecated_as_data();
}

const DataContentDescription* RtpDataContentDescription::as_data() const {
  return const_cast<RtpDataContentDescription*>(this)->as_data();
}

DataContentDescription* SctpDataContentDescription::deprecated_as_data() {
  if (!shim_) {
    shim_.reset(new DataContentDescription(this));
  }
  return shim_.get();
}

DataContentDescription* SctpDataContentDescription::as_data() {
  return deprecated_as_data();
}

const DataContentDescription* SctpDataContentDescription::as_data() const {
  return const_cast<SctpDataContentDescription*>(this)->deprecated_as_data();
}

DataContentDescription::DataContentDescription() {
  // In this case, we will initialize |owned_description_| as soon as
  // we are told what protocol to use via set_protocol or another function
  // calling CreateShimTarget.
}

DataContentDescription::DataContentDescription(
    SctpDataContentDescription* wrapped)
    : real_description_(wrapped) {
  // SctpDataContentDescription doesn't contain codecs, but code
  // using DataContentDescription expects to see one.
  Super::AddCodec(
      cricket::DataCodec(kGoogleSctpDataCodecPlType, kGoogleSctpDataCodecName));
}

DataContentDescription::DataContentDescription(
    RtpDataContentDescription* wrapped)
    : real_description_(wrapped) {}

DataContentDescription::DataContentDescription(
    const DataContentDescription* o) {
  if (o->real_description_) {
    owned_description_ = absl::WrapUnique(o->real_description_->Copy());
    real_description_ = owned_description_.get();
  } else {
    // Copy all information collected so far, including codecs.
    Super::operator=(*o);
  }
}

void DataContentDescription::CreateShimTarget(bool is_sctp) {
  RTC_LOG(LS_INFO) << "Creating shim target, is_sctp is " << is_sctp;
  RTC_CHECK(!owned_description_.get());
  if (is_sctp) {
    owned_description_ = absl::make_unique<SctpDataContentDescription>();
    // Copy all information collected so far, except codecs.
    owned_description_->MediaContentDescription::operator=(*this);
  } else {
    owned_description_ = absl::make_unique<RtpDataContentDescription>();
    // Copy all information collected so far, including codecs.
    owned_description_->as_rtp_data()
        ->MediaContentDescriptionImpl<RtpDataCodec>::operator=(*this);
  }
  real_description_ = owned_description_.get();
}

MediaContentDescription* DataContentDescription::Unshim(bool* should_delete) {
  // If protocol isn't decided at this point, we have a problem.
  RTC_CHECK(real_description_);
  if (owned_description_) {
    // Pass ownership to caller, and remove myself.
    // Since caller can't know if I was owner or owned, tell them.
    MediaContentDescription* to_return = owned_description_.release();
    *should_delete = true;
    return to_return;
  }
  // Real object is owner, and presumably referenced from elsewhere.
  *should_delete = false;
  return real_description_;
}

void DataContentDescription::set_protocol(const std::string& protocol) {
  if (!real_description_) {
    CreateShimTarget(IsSctpProtocol(protocol));
  }
  real_description_->set_protocol(protocol);
}

bool DataContentDescription::IsSctp() const {
  return (real_description_ && real_description_->as_sctp());
}

void DataContentDescription::EnsureIsRtp() {
  RTC_CHECK(real_description_);
  RTC_CHECK(real_description_->as_rtp_data());
}

RtpDataContentDescription* DataContentDescription::as_rtp_data() {
  if (real_description_) {
    return real_description_->as_rtp_data();
  }
  return nullptr;
}

SctpDataContentDescription* DataContentDescription::as_sctp() {
  if (real_description_) {
    return real_description_->as_sctp();
  }
  return nullptr;
}

// Override all methods defined in MediaContentDescription.
bool DataContentDescription::has_codecs() const {
  if (!real_description_) {
    return Super::has_codecs();
  }
  return real_description_->has_codecs();
}
std::string DataContentDescription::protocol() const {
  if (!real_description_) {
    return Super::protocol();
  }
  return real_description_->protocol();
}

webrtc::RtpTransceiverDirection DataContentDescription::direction() const {
  if (!real_description_) {
    return Super::direction();
  }
  return real_description_->direction();
}
void DataContentDescription::set_direction(
    webrtc::RtpTransceiverDirection direction) {
  if (!real_description_) {
    return Super::set_direction(direction);
  }
  return real_description_->set_direction(direction);
}
bool DataContentDescription::rtcp_mux() const {
  if (!real_description_) {
    return Super::rtcp_mux();
  }
  return real_description_->rtcp_mux();
}
void DataContentDescription::set_rtcp_mux(bool mux) {
  if (!real_description_) {
    Super::set_rtcp_mux(mux);
    return;
  }
  real_description_->set_rtcp_mux(mux);
}
bool DataContentDescription::rtcp_reduced_size() const {
  if (!real_description_) {
    return Super::rtcp_reduced_size();
  }
  return real_description_->rtcp_reduced_size();
}
void DataContentDescription::set_rtcp_reduced_size(bool reduced_size) {
  if (!real_description_) {
    return Super::set_rtcp_reduced_size(reduced_size);
  }

  return real_description_->set_rtcp_reduced_size(reduced_size);
}
int DataContentDescription::bandwidth() const {
  if (!real_description_) {
    return Super::bandwidth();
  }

  return real_description_->bandwidth();
}
void DataContentDescription::set_bandwidth(int bandwidth) {
  if (!real_description_) {
    return Super::set_bandwidth(bandwidth);
  }

  return real_description_->set_bandwidth(bandwidth);
}
const std::vector<CryptoParams>& DataContentDescription::cryptos() const {
  if (!real_description_) {
    return Super::cryptos();
  }

  return real_description_->cryptos();
}
void DataContentDescription::AddCrypto(const CryptoParams& params) {
  if (!real_description_) {
    return Super::AddCrypto(params);
  }

  return real_description_->AddCrypto(params);
}
void DataContentDescription::set_cryptos(
    const std::vector<CryptoParams>& cryptos) {
  if (!real_description_) {
    return Super::set_cryptos(cryptos);
  }

  return real_description_->set_cryptos(cryptos);
}
const RtpHeaderExtensions& DataContentDescription::rtp_header_extensions()
    const {
  if (!real_description_) {
    return Super::rtp_header_extensions();
  }

  return real_description_->rtp_header_extensions();
}
void DataContentDescription::set_rtp_header_extensions(
    const RtpHeaderExtensions& extensions) {
  if (!real_description_) {
    return Super::set_rtp_header_extensions(extensions);
  }

  return real_description_->set_rtp_header_extensions(extensions);
}
void DataContentDescription::AddRtpHeaderExtension(
    const webrtc::RtpExtension& ext) {
  if (!real_description_) {
    return Super::AddRtpHeaderExtension(ext);
  }
  return real_description_->AddRtpHeaderExtension(ext);
}
void DataContentDescription::AddRtpHeaderExtension(
    const cricket::RtpHeaderExtension& ext) {
  if (!real_description_) {
    return Super::AddRtpHeaderExtension(ext);
  }
  return real_description_->AddRtpHeaderExtension(ext);
}
void DataContentDescription::ClearRtpHeaderExtensions() {
  if (!real_description_) {
    return Super::ClearRtpHeaderExtensions();
  }
  return real_description_->ClearRtpHeaderExtensions();
}
bool DataContentDescription::rtp_header_extensions_set() const {
  if (!real_description_) {
    return Super::rtp_header_extensions_set();
  }
  return real_description_->rtp_header_extensions_set();
}
const StreamParamsVec& DataContentDescription::streams() const {
  if (!real_description_) {
    return Super::streams();
  }
  return real_description_->streams();
}
StreamParamsVec& DataContentDescription::mutable_streams() {
  if (!real_description_) {
    return Super::mutable_streams();
  }
  return real_description_->mutable_streams();
}
void DataContentDescription::AddStream(const StreamParams& stream) {
  if (!real_description_) {
    return Super::AddStream(stream);
  }
  return real_description_->AddStream(stream);
}
void DataContentDescription::SetCnameIfEmpty(const std::string& cname) {
  if (!real_description_) {
    return Super::SetCnameIfEmpty(cname);
  }
  return real_description_->SetCnameIfEmpty(cname);
}
uint32_t DataContentDescription::first_ssrc() const {
  if (!real_description_) {
    return Super::first_ssrc();
  }
  return real_description_->first_ssrc();
}
bool DataContentDescription::has_ssrcs() const {
  if (!real_description_) {
    return Super::has_ssrcs();
  }
  return real_description_->has_ssrcs();
}
void DataContentDescription::set_conference_mode(bool enable) {
  if (!real_description_) {
    return Super::set_conference_mode(enable);
  }
  return real_description_->set_conference_mode(enable);
}
bool DataContentDescription::conference_mode() const {
  if (!real_description_) {
    return Super::conference_mode();
  }
  return real_description_->conference_mode();
}
void DataContentDescription::set_connection_address(
    const rtc::SocketAddress& address) {
  if (!real_description_) {
    return Super::set_connection_address(address);
  }
  return real_description_->set_connection_address(address);
}
const rtc::SocketAddress& DataContentDescription::connection_address() const {
  if (!real_description_) {
    return Super::connection_address();
  }
  return real_description_->connection_address();
}
void DataContentDescription::set_extmap_allow_mixed_enum(
    ExtmapAllowMixed mixed) {
  if (!real_description_) {
    return Super::set_extmap_allow_mixed_enum(mixed);
  }
  return real_description_->set_extmap_allow_mixed_enum(mixed);
}
MediaContentDescription::ExtmapAllowMixed
DataContentDescription::extmap_allow_mixed_enum() const {
  if (!real_description_) {
    return Super::extmap_allow_mixed_enum();
  }
  return real_description_->extmap_allow_mixed_enum();
}
bool DataContentDescription::HasSimulcast() const {
  if (!real_description_) {
    return Super::HasSimulcast();
  }
  return real_description_->HasSimulcast();
}
SimulcastDescription& DataContentDescription::simulcast_description() {
  if (!real_description_) {
    return Super::simulcast_description();
  }
  return real_description_->simulcast_description();
}
const SimulcastDescription& DataContentDescription::simulcast_description()
    const {
  if (!real_description_) {
    return Super::simulcast_description();
  }
  return real_description_->simulcast_description();
}
void DataContentDescription::set_simulcast_description(
    const SimulcastDescription& simulcast) {
  if (!real_description_) {
    return Super::set_simulcast_description(simulcast);
  }
  return real_description_->set_simulcast_description(simulcast);
}

// Methods defined in MediaContentDescriptionImpl.
// For SCTP, we implement codec handling.
// For RTP, we pass the codecs.
// In the cases where type hasn't been decided yet, we return dummies.

const std::vector<DataCodec>& DataContentDescription::codecs() const {
  if (IsSctp() || !real_description_) {
    return Super::codecs();
  }
  return real_description_->as_rtp_data()->codecs();
}

void DataContentDescription::set_codecs(const std::vector<DataCodec>& codecs) {
  if (IsSctp() || !real_description_) {
    Super::set_codecs(codecs);
  } else {
    EnsureIsRtp();
    real_description_->as_rtp_data()->set_codecs(codecs);
  }
}

bool DataContentDescription::HasCodec(int id) {
  if (IsSctp() || !real_description_) {
    return Super::HasCodec(id);
  }
  return real_description_->as_rtp_data()->HasCodec(id);
}

void DataContentDescription::AddCodec(const DataCodec& codec) {
  if (IsSctp() || !real_description_) {
    Super::AddCodec(codec);
  } else {
    EnsureIsRtp();
    real_description_->as_rtp_data()->AddCodec(codec);
  }
}

void DataContentDescription::AddOrReplaceCodec(const DataCodec& codec) {
  if (IsSctp() || real_description_) {
    Super::AddOrReplaceCodec(codec);
  } else {
    EnsureIsRtp();
    real_description_->as_rtp_data()->AddOrReplaceCodec(codec);
  }
}

void DataContentDescription::AddCodecs(const std::vector<DataCodec>& codecs) {
  if (IsSctp() || !real_description_) {
    Super::AddCodecs(codecs);
  } else {
    EnsureIsRtp();
    real_description_->as_rtp_data()->AddCodecs(codecs);
  }
}

}  // namespace cricket
