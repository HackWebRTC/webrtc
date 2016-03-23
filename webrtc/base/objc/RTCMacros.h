/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_OBJC_RTC_MACROS_H_
#define WEBRTC_BASE_OBJC_RTC_MACROS_H_

#if defined(__cplusplus)
#define RTC_EXPORT extern "C"
#else
#define RTC_EXPORT extern
#endif

#ifdef __OBJC__
#define RTC_FWD_DECL_OBJC_CLASS(classname) @class classname
#else
#define RTC_FWD_DECL_OBJC_CLASS(classname) typedef struct objc_object classname
#endif

#endif  // WEBRTC_BASE_OBJC_RTC_MACROS_H_
