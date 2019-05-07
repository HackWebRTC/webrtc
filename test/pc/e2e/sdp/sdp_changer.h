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
#include "api/rtp_parameters.h"

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
    bool ulpfec,
    bool flexfec,
    std::vector<RtpCodecCapability> supported_codecs);

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_SDP_SDP_CHANGER_H_
