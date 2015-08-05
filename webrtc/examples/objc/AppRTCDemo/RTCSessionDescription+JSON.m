/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCSessionDescription+JSON.h"

static NSString const *kRTCSessionDescriptionTypeKey = @"type";
static NSString const *kRTCSessionDescriptionSdpKey = @"sdp";

@implementation RTCSessionDescription (JSON)

+ (RTCSessionDescription *)descriptionFromJSONDictionary:
    (NSDictionary *)dictionary {
  NSString *type = dictionary[kRTCSessionDescriptionTypeKey];
  NSString *sdp = dictionary[kRTCSessionDescriptionSdpKey];
  return [[RTCSessionDescription alloc] initWithType:type sdp:sdp];
}

- (NSData *)JSONData {
  NSDictionary *json = @{
    kRTCSessionDescriptionTypeKey : self.type,
    kRTCSessionDescriptionSdpKey : self.description
  };
  return [NSJSONSerialization dataWithJSONObject:json options:0 error:nil];
}

@end
