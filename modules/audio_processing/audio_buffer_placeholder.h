/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_PLACEHOLDER_H_
#define MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_PLACEHOLDER_H_

// TODO(peah): Remove this file once the audio buffer has been moved to the
// audio_buffer build target. The purpose of this file is to ensure that the
// audio_buffer build target is not empty as that causes the compiler to
// complain.
struct AudioBufferPlaceholder {
 public:
  int dummy;

 private:
};

#endif  // MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_PLACEHOLDER_H_
