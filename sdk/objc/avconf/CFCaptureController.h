/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

@class RTCCameraVideoCapturer;

NS_ASSUME_NONNULL_BEGIN

// Controls the camera. Handles starting the capture, switching cameras etc.
RTC_OBJC_EXPORT
@interface CFCaptureController : NSObject

- (instancetype)initWithCapturer:(RTCCameraVideoCapturer *)capturer
                      cameraFace:(int32_t)cameraFace
                           width:(int32_t)width
                          height:(int32_t)height;
- (void)startCapture:(nullable void (^)(NSError *))completionHandler;
- (void)stopCapture;
- (void)switchCamera:(nullable void (^)(NSError *))completionHandler;

@end

NS_ASSUME_NONNULL_END
