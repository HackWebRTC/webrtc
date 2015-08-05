/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCICECandidate+JSON.h"

#import "RTCLogging.h"

static NSString const *kRTCICECandidateTypeKey = @"type";
static NSString const *kRTCICECandidateTypeValue = @"candidate";
static NSString const *kRTCICECandidateMidKey = @"id";
static NSString const *kRTCICECandidateMLineIndexKey = @"label";
static NSString const *kRTCICECandidateSdpKey = @"candidate";

@implementation RTCICECandidate (JSON)

+ (RTCICECandidate *)candidateFromJSONDictionary:(NSDictionary *)dictionary {
  NSString *mid = dictionary[kRTCICECandidateMidKey];
  NSString *sdp = dictionary[kRTCICECandidateSdpKey];
  NSNumber *num = dictionary[kRTCICECandidateMLineIndexKey];
  NSInteger mLineIndex = [num integerValue];
  return [[RTCICECandidate alloc] initWithMid:mid index:mLineIndex sdp:sdp];
}

- (NSData *)JSONData {
  NSDictionary *json = @{
    kRTCICECandidateTypeKey : kRTCICECandidateTypeValue,
    kRTCICECandidateMLineIndexKey : @(self.sdpMLineIndex),
    kRTCICECandidateMidKey : self.sdpMid,
    kRTCICECandidateSdpKey : self.sdp
  };
  NSError *error = nil;
  NSData *data =
      [NSJSONSerialization dataWithJSONObject:json
                                      options:NSJSONWritingPrettyPrinted
                                        error:&error];
  if (error) {
    RTCLogError(@"Error serializing JSON: %@", error);
    return nil;
  }
  return data;
}

@end
