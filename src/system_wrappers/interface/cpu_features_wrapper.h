/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CPU_FEATURES_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CPU_FEATURES_WRAPPER_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

// list of features.
typedef enum {
  kSSE2,
  kSSE3
} CPUFeature;

typedef int (*WebRtc_CPUInfo)(CPUFeature feature);
// returns true if the CPU supports the feature.
extern WebRtc_CPUInfo WebRtc_GetCPUInfo;
// No CPU feature is available => straight C path.
extern WebRtc_CPUInfo WebRtc_GetCPUInfoNoASM;

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CPU_FEATURES_WRAPPER_H_
