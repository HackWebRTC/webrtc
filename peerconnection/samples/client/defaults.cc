/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "peerconnection/samples/client/defaults.h"

const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const uint16 kDefaultServerPort = 8888;

std::string GetEnvVarOrDefault(const char* env_var_name,
                               const char* default_value) {
  std::string value;
  const char* env_var = getenv(env_var_name);
  if (env_var)
    value = env_var;

  if (value.empty())
    value = default_value;

  return value;
}

std::string GetPeerConnectionString() {
  return GetEnvVarOrDefault("WEBRTC_CONNECT", "STUN stun.l.google.com:19302");
}

std::string GetDefaultServerName() {
  return GetEnvVarOrDefault("WEBRTC_SERVER", "localhost");
}

std::string GetPeerName() {
  char computer_name[MAX_PATH] = {0}, user_name[MAX_PATH] = {0};
  DWORD size = ARRAYSIZE(computer_name);
  ::GetComputerNameA(computer_name, &size);
  size = ARRAYSIZE(user_name);
  ::GetUserNameA(user_name, &size);
  std::string ret(user_name);
  ret += '@';
  ret += computer_name;
  return ret;
}
