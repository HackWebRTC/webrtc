/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PEERCONNECTION_SAMPLES_CLIENT_DEFAULTS_H_
#define PEERCONNECTION_SAMPLES_CLIENT_DEFAULTS_H_
#pragma once

#include <string>

#include "talk/base/basictypes.h"

extern const char kAudioLabel[];
extern const char kVideoLabel[];
extern const char kStreamLabel[];
extern const uint16 kDefaultServerPort;

std::string GetEnvVarOrDefault(const char* env_var_name,
                               const char* default_value);
std::string GetPeerConnectionString();
std::string GetDefaultServerName();
std::string GetPeerName();

#endif  // PEERCONNECTION_SAMPLES_CLIENT_DEFAULTS_H_
