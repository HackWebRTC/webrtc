/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "webrtc/base/objc/RTCMacros.h"

RTC_EXPORT void RTCSetupInternalTracer();
/** Starts capture to specified file. Must be a valid writable path.
 *  Returns YES if capture starts.
 */
RTC_EXPORT BOOL RTCStartInternalCapture(NSString *filePath);
RTC_EXPORT void RTCStopInternalCapture();
RTC_EXPORT void RTCShutdownInternalTracer();
