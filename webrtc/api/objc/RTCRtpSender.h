/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "webrtc/api/objc/RTCMediaStreamTrack.h"
#import "webrtc/api/objc/RTCRtpParameters.h"
#import "webrtc/base/objc/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

RTC_EXPORT
@protocol RTCRtpSender <NSObject>

/** The currently active RTCRtpParameters, as defined in
 *  https://www.w3.org/TR/webrtc/#idl-def-RTCRtpParameters.
 */
@property(nonatomic, readonly) RTCRtpParameters *parameters;

/** The RTCMediaStreamTrack associated with the sender.
 *  Note: reading this property returns a new instance of
 *  RTCMediaStreamTrack. Use isEqual: instead of == to compare
 *  RTCMediaStreamTrack instances.
 */
@property(nonatomic, readonly) RTCMediaStreamTrack *track;

/** Set the new RTCRtpParameters to be used by the sender.
 *  Returns YES if the new parameters were applied, NO otherwise.
 */
- (BOOL)setParameters:(RTCRtpParameters *)parameters;

@end

RTC_EXPORT
@interface RTCRtpSender : NSObject <RTCRtpSender>

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
