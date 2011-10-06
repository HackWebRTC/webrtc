/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "util.h"

#include <stdarg.h>
#include <cstdio>

#include "google/gflags.h"

DEFINE_bool(verbose, true, "Verbose mode. Prints a lot of debugging info. "
            "Suitable for tracking progress but not for capturing output. "
            "Default: enabled");

int log(const char *format, ...) {
  int result = 0;
  if (FLAGS_verbose) {
    va_list args;
    va_start(args, format);
    result = vprintf(format, args);
    va_end(args);
  }
  return result;
}

