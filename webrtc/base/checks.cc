/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace rtc {

void Fatal(const char* file, int line, const char* format, ...) {
  char msg[256];

  va_list arguments;
  va_start(arguments, format);
  vsnprintf(msg, sizeof(msg), format, arguments);
  va_end(arguments);

  LOG(LS_ERROR) << "\n\n#\n# Fatal error in " << file
                << ", line " << line << "\n# " << msg
                << "\n#\n";
  abort();
}

}  // namespace rtc
