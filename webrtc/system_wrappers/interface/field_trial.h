//
// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_FIELD_TRIAL_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_FIELD_TRIAL_H_

#include <string>

#include "webrtc/common_types.h"

// Field trials allow a client of webrtc (such as Chrome) to turn on feature
// code in binraries out in the field and gather information with that.
// WebRTC field trials are designed to be wired up directly to chrome field
// trials and speed up the time developers need to get features out there by
// spending less time wiring up APIs to control whether the feature is on/off.
// Note, that not every feature is candidate to be controlled by them as it may
// require proper negotiation between involved parties (e.g. SDP negotiation).
//
// E.g.: To experiment with a new method that could lead to a different
// trade-off between CPU/bandwidth:
//
// 1 - Develop the feature with default behaviour off:
//
//   if (FieldTrial::FindFullName("WebRTCExperimenMethod2") == "Enabled")
//     method2();
//   else
//     method1();
//
// 2 - Once the changes are rolled to chrome, the new code path can be executed
// by running chrome with --force-fieldtrials=WebRTCExperimentMethod2/Enabled/
// or controled by finch studies.
//
// 3 - Evaluate the new feature and clean the code paths.
//
// TODO(andresp): find out how to get bots and unit tests to run with field
// trials enabled.

namespace webrtc {
namespace field_trial {

typedef std::string (*FindFullNameMethod)(const std::string&);

// Returns the group name chosen for the named trial, or the empty string
// if the trial does not exists.
//
// Note: To keep things tidy append all the trial names with WebRTC.
std::string FindFullName(const std::string& name);

// WebRTC clients MUST call this method to setup field trials.
// Failing to do so will crash the first time code tries to access a field
// trial.
//
// This method must be called before any WebRTC methods. Functions
// provided should be thread-safe.
WEBRTC_DLLEXPORT void Init(FindFullNameMethod find);

}  // namespace field_trial
}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_FIELD_TRIAL_H_
