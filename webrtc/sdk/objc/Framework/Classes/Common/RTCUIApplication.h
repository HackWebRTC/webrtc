/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_UI_RTCUIAPPLICATION_H_
#define WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_UI_RTCUIAPPLICATION_H_

#include "WebRTC/RTCMacros.h"

#if defined(WEBRTC_IOS)
/** Convenience function to get UIApplicationState from C++. */
RTC_EXTERN bool RTCIsUIApplicationActive();
#endif  // WEBRTC_IOS

#endif  // WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_UI_RTCUIAPPLICATION_H_
