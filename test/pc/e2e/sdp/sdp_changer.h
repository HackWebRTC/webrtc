/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_SDP_SDP_CHANGER_H_
#define TEST_PC_E2E_SDP_SDP_CHANGER_H_

#include <map>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/jsep.h"
#include "api/rtp_parameters.h"
#include "media/base/rid_description.h"
#include "pc/session_description.h"
#include "pc/simulcast_description.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Creates list of capabilities, which can be set on RtpTransceiverInterface via
// RtpTransceiverInterface::SetCodecPreferences(...) to negotiate use of codec
// from list of |supported_codecs| with specified |codec_name| and parameters,
// which contains all of |codec_required_params|. If flags |ulpfec| or |flexfec|
// set to true corresponding FEC codec will be added. FEC and RTX codecs will be
// added after required codecs.
//
// All codecs will be added only if they exists in the list of
// |supported_codecs|. If multiple codecs from this list will have |codec_name|
// and |codec_required_params|, then all of them will be added to the output
// vector and they will be added in the same order, as they were in
// |supported_codecs|.
std::vector<RtpCodecCapability> FilterCodecCapabilities(
    absl::string_view codec_name,
    const std::map<std::string, std::string>& codec_required_params,
    bool use_rtx,
    bool use_ulpfec,
    bool use_flexfec,
    std::vector<RtpCodecCapability> supported_codecs);

struct LocalAndRemoteSdp {
  LocalAndRemoteSdp(std::unique_ptr<SessionDescriptionInterface> local_sdp,
                    std::unique_ptr<SessionDescriptionInterface> remote_sdp)
      : local_sdp(std::move(local_sdp)), remote_sdp(std::move(remote_sdp)) {}

  // Sdp, that should be as local description on the peer, that created it.
  std::unique_ptr<SessionDescriptionInterface> local_sdp;
  // Sdp, that should be set as remote description on the peer opposite to the
  // one, who created it.
  std::unique_ptr<SessionDescriptionInterface> remote_sdp;
};

class SignalingInterceptor {
 public:
  LocalAndRemoteSdp PatchOffer(
      std::unique_ptr<SessionDescriptionInterface> offer);
  LocalAndRemoteSdp PatchAnswer(
      std::unique_ptr<SessionDescriptionInterface> offer);

  std::vector<std::unique_ptr<IceCandidateInterface>> PatchOffererIceCandidates(
      rtc::ArrayView<const IceCandidateInterface* const> candidates);
  std::vector<std::unique_ptr<IceCandidateInterface>>
  PatchAnswererIceCandidates(
      rtc::ArrayView<const IceCandidateInterface* const> candidates);

 private:
  // Contains information about simulcast section, that is required to perform
  // modified offer/answer and ice candidates exchange.
  struct SimulcastSectionInfo {
    SimulcastSectionInfo(const std::string& mid,
                         cricket::MediaProtocolType media_protocol_type,
                         const std::vector<cricket::RidDescription>& rids_desc);

    const std::string mid;
    const cricket::MediaProtocolType media_protocol_type;
    std::vector<std::string> rids;
    cricket::SimulcastDescription simulcast_description;
    webrtc::RtpExtension mid_extension;
    webrtc::RtpExtension rid_extension;
    webrtc::RtpExtension rrid_extension;
    cricket::TransportDescription transport_description;
  };

  struct SignalingContext {
    SignalingContext() = default;
    // SignalingContext is not copyable and movable.
    SignalingContext(SignalingContext&) = delete;
    SignalingContext& operator=(SignalingContext&) = delete;
    SignalingContext(SignalingContext&&) = delete;
    SignalingContext& operator=(SignalingContext&&) = delete;

    void AddSimulcastInfo(const SimulcastSectionInfo& info);
    bool HasSimulcast() const { return !simulcast_infos.empty(); }

    std::vector<SimulcastSectionInfo> simulcast_infos;
    std::map<std::string, SimulcastSectionInfo*> simulcast_infos_by_mid;
    std::map<std::string, SimulcastSectionInfo*> simulcast_infos_by_rid;

    std::vector<std::string> mids_order;
  };

  void FillContext(SessionDescriptionInterface* offer);
  std::unique_ptr<cricket::SessionDescription> RestoreMediaSectionsOrder(
      std::unique_ptr<cricket::SessionDescription> source);

  SignalingContext context_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_SDP_SDP_CHANGER_H_
