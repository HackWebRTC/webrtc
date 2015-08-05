/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaConstraints+JSON.h"

#import "RTCPair.h"

static NSString const *kRTCMediaConstraintsMandatoryKey = @"mandatory";

@implementation RTCMediaConstraints (JSON)

+ (RTCMediaConstraints *)constraintsFromJSONDictionary:
    (NSDictionary *)dictionary {
  NSDictionary *mandatory = dictionary[kRTCMediaConstraintsMandatoryKey];
  NSMutableArray *mandatoryContraints =
      [NSMutableArray arrayWithCapacity:[mandatory count]];
  [mandatory enumerateKeysAndObjectsUsingBlock:^(
      id key, id obj, BOOL *stop) {
    [mandatoryContraints addObject:[[RTCPair alloc] initWithKey:key
                                                          value:obj]];
  }];
  // TODO(tkchin): figure out json formats for optional constraints.
  RTCMediaConstraints *constraints =
      [[RTCMediaConstraints alloc]
          initWithMandatoryConstraints:mandatoryContraints
                   optionalConstraints:nil];
  return constraints;
}

@end
