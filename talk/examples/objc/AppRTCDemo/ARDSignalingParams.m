/*
 * libjingle
 * Copyright 2014, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "ARDSignalingParams.h"

#import "ARDUtilities.h"
#import "RTCICEServer+JSON.h"
#import "RTCMediaConstraints+JSON.h"

static NSString const *kARDSignalingParamsErrorKey = @"error";
static NSString const *kARDSignalingParamsErrorMessagesKey = @"error_messages";
static NSString const *kARDSignalingParamsInitiatorKey = @"initiator";
static NSString const *kARDSignalingParamsPeerConnectionConfigKey =
    @"pc_config";
static NSString const *kARDSignalingParamsICEServersKey = @"iceServers";
static NSString const *kARDSignalingParamsMediaConstraintsKey =
    @"media_constraints";
static NSString const *kARDSignalingParamsMediaConstraintsVideoKey =
    @"video";
static NSString const *kARDSignalingParamsTokenKey = @"token";
static NSString const *kARDSignalingParamsTurnRequestUrlKey = @"turn_url";

@interface ARDSignalingParams ()

@property(nonatomic, strong) NSArray *errorMessages;
@property(nonatomic, strong) RTCMediaConstraints *offerConstraints;
@property(nonatomic, strong) RTCMediaConstraints *mediaConstraints;
@property(nonatomic, strong) NSMutableArray *iceServers;
@property(nonatomic, strong) NSURL *signalingServerURL;
@property(nonatomic, strong) NSURL *turnRequestURL;
@property(nonatomic, strong) NSString *channelToken;

@end

@implementation ARDSignalingParams

@synthesize errorMessages = _errorMessages;
@synthesize isInitiator = _isInitiator;
@synthesize offerConstraints = _offerConstraints;
@synthesize mediaConstraints = _mediaConstraints;
@synthesize iceServers = _iceServers;
@synthesize signalingServerURL = _signalingServerURL;

+ (ARDSignalingParams *)paramsFromJSONData:(NSData *)data {
  NSDictionary *paramsJSON = [NSDictionary dictionaryWithJSONData:data];
  if (!paramsJSON) {
    return nil;
  }
  ARDSignalingParams *params = [[ARDSignalingParams alloc] init];

  // Parse errors.
  BOOL hasError = NO;
  NSArray *errorMessages = paramsJSON[kARDSignalingParamsErrorMessagesKey];
  if (errorMessages.count > 0) {
    params.errorMessages = errorMessages;
    return params;
  }

  // Parse ICE servers.
  NSString *peerConnectionConfigString =
      paramsJSON[kARDSignalingParamsPeerConnectionConfigKey];
  NSDictionary *peerConnectionConfig =
      [NSDictionary dictionaryWithJSONString:peerConnectionConfigString];
  NSArray *iceServerJSONArray =
      peerConnectionConfig[kARDSignalingParamsICEServersKey];
  NSMutableArray *iceServers = [NSMutableArray array];
  for (NSDictionary *iceServerJSON in iceServerJSONArray) {
    RTCICEServer *iceServer =
        [RTCICEServer serverFromJSONDictionary:iceServerJSON];
    [iceServers addObject:iceServer];
  }
  params.iceServers = iceServers;

  // Parse initiator.
  BOOL isInitiator = [paramsJSON[kARDSignalingParamsInitiatorKey] boolValue];
  params.isInitiator = isInitiator;

  // Parse video constraints.
  RTCMediaConstraints *videoConstraints = nil;
  NSString *mediaConstraintsJSONString =
      paramsJSON[kARDSignalingParamsMediaConstraintsKey];
  NSDictionary *mediaConstraintsJSON =
      [NSDictionary dictionaryWithJSONString:mediaConstraintsJSONString];
  id videoJSON =
      mediaConstraintsJSON[kARDSignalingParamsMediaConstraintsVideoKey];
  if ([videoJSON isKindOfClass:[NSDictionary class]]) {
    videoConstraints =
        [RTCMediaConstraints constraintsFromJSONDictionary:videoJSON];
  } else if ([videoJSON isKindOfClass:[NSNumber class]] &&
             [videoJSON boolValue]) {
    videoConstraints = [[RTCMediaConstraints alloc] init];
  }
  params.mediaConstraints = videoConstraints;

  // Parse channel token.
  NSString *token = paramsJSON[kARDSignalingParamsTokenKey];
  params.channelToken = token;

  // Parse turn request url.
  params.turnRequestURL =
      [NSURL URLWithString:paramsJSON[kARDSignalingParamsTurnRequestUrlKey]];

  return params;
}

@end
