/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoCapturer.h"

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@protocol CFYuvFrameCallbackInternal <NSObject>

- (void)onYuvFrameWidth:(int)width height:(int)height buffer:(int8_t*)buffer length:(int)length;

@end

RTC_OBJC_EXPORT
@protocol CFJpegFrameCallbackInternal <NSObject>

- (void)onJpegFrame:(NSString*)path;

@end

RTC_OBJC_EXPORT
@interface CFHijackCapturerDelegate : NSObject<RTCVideoCapturerDelegate>

- (instancetype)initWithRealDelegate:(id<RTCVideoCapturerDelegate>)delegate;

- (void)toggleMute:(bool)muted;

- (void)togglePause:(bool)pause;

- (void)setYuvFrameCallback:(int64_t)intervalMs callback:(nullable id<CFYuvFrameCallbackInternal>)callback;

- (void)toggleSendLastFrame:(nullable NSString*)path callback:(nullable id<CFJpegFrameCallbackInternal>)callback;

- (void)dispose;

@end

NS_ASSUME_NONNULL_END
