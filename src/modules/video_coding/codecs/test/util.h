/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef SRC_MODULES_VIDEO_CODING_CODECS_TEST_UTIL_H_
#define SRC_MODULES_VIDEO_CODING_CODECS_TEST_UTIL_H_

// Custom log method that only prints if the verbose flag is given
// Supports all the standard printf parameters and formatting (just forwarded)
int log(const char *format, ...);

#endif  // SRC_MODULES_VIDEO_CODING_CODECS_TEST_UTIL_H_
