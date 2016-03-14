/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import "webrtc/api/objc/RTCIceCandidate.h"
#import "webrtc/api/objc/RTCSessionDescription.h"

typedef enum {
  kARDSignalingMessageTypeCandidate,
  kARDSignalingMessageTypeOffer,
  kARDSignalingMessageTypeAnswer,
  kARDSignalingMessageTypeBye,
} ARDSignalingMessageType;

@interface ARDSignalingMessage : NSObject

@property(nonatomic, readonly) ARDSignalingMessageType type;

+ (ARDSignalingMessage *)messageFromJSONString:(NSString *)jsonString;
- (NSData *)JSONData;

@end

@interface ARDICECandidateMessage : ARDSignalingMessage

@property(nonatomic, readonly) RTCIceCandidate *candidate;

- (instancetype)initWithCandidate:(RTCIceCandidate *)candidate;

@end

@interface ARDSessionDescriptionMessage : ARDSignalingMessage

@property(nonatomic, readonly) RTCSessionDescription *sessionDescription;

- (instancetype)initWithDescription:(RTCSessionDescription *)description;

@end

@interface ARDByeMessage : ARDSignalingMessage
@end
