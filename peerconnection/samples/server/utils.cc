/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "peerconnection/samples/server/utils.h"

#include <stdio.h>

std::string int2str(int i) {
  char buffer[11] = {0};
  sprintf(buffer, "%d", i);  // NOLINT
  return buffer;
}

std::string size_t2str(size_t i) {
  char buffer[32] = {0};
#ifdef WIN32
  // %zu isn't supported on Windows.
  sprintf(buffer, "%Iu", i);  // NOLINT
#else
  sprintf(buffer, "%zu", i);  // NOLINT
#endif
  return buffer;
}
