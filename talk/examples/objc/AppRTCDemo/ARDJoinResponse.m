/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#import "ARDJoinResponse+Internal.h"

#import "ARDSignalingMessage.h"
#import "ARDUtilities.h"
#import "RTCICEServer+JSON.h"

static NSString const *kARDJoinResultKey = @"result";
static NSString const *kARDJoinResultParamsKey = @"params";
static NSString const *kARDJoinInitiatorKey = @"is_initiator";
static NSString const *kARDJoinRoomIdKey = @"room_id";
static NSString const *kARDJoinClientIdKey = @"client_id";
static NSString const *kARDJoinMessagesKey = @"messages";
static NSString const *kARDJoinWebSocketURLKey = @"wss_url";
static NSString const *kARDJoinWebSocketRestURLKey = @"wss_post_url";

@implementation ARDJoinResponse

@synthesize result = _result;
@synthesize isInitiator = _isInitiator;
@synthesize roomId = _roomId;
@synthesize clientId = _clientId;
@synthesize messages = _messages;
@synthesize webSocketURL = _webSocketURL;
@synthesize webSocketRestURL = _webSocketRestURL;

+ (ARDJoinResponse *)responseFromJSONData:(NSData *)data {
  NSDictionary *responseJSON = [NSDictionary dictionaryWithJSONData:data];
  if (!responseJSON) {
    return nil;
  }
  ARDJoinResponse *response = [[ARDJoinResponse alloc] init];
  NSString *resultString = responseJSON[kARDJoinResultKey];
  response.result = [[self class] resultTypeFromString:resultString];
  NSDictionary *params = responseJSON[kARDJoinResultParamsKey];

  response.isInitiator = [params[kARDJoinInitiatorKey] boolValue];
  response.roomId = params[kARDJoinRoomIdKey];
  response.clientId = params[kARDJoinClientIdKey];

  // Parse messages.
  NSArray *messages = params[kARDJoinMessagesKey];
  NSMutableArray *signalingMessages =
      [NSMutableArray arrayWithCapacity:messages.count];
  for (NSString *message in messages) {
    ARDSignalingMessage *signalingMessage =
        [ARDSignalingMessage messageFromJSONString:message];
    [signalingMessages addObject:signalingMessage];
  }
  response.messages = signalingMessages;

  // Parse websocket urls.
  NSString *webSocketURLString = params[kARDJoinWebSocketURLKey];
  response.webSocketURL = [NSURL URLWithString:webSocketURLString];
  NSString *webSocketRestURLString = params[kARDJoinWebSocketRestURLKey];
  response.webSocketRestURL = [NSURL URLWithString:webSocketRestURLString];

  return response;
}

#pragma mark - Private

+ (ARDJoinResultType)resultTypeFromString:(NSString *)resultString {
  ARDJoinResultType result = kARDJoinResultTypeUnknown;
  if ([resultString isEqualToString:@"SUCCESS"]) {
    result = kARDJoinResultTypeSuccess;
  } else if ([resultString isEqualToString:@"FULL"]) {
    result = kARDJoinResultTypeFull;
  }
  return result;
}

@end
