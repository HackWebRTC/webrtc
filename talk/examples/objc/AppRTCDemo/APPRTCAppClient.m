/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "APPRTCAppClient.h"

#import <dispatch/dispatch.h>

#import "GAEChannelClient.h"
#import "RTCICEServer.h"
#import "RTCMediaConstraints.h"
#import "RTCPair.h"

@implementation APPRTCAppClient {
  dispatch_queue_t _backgroundQueue;
  GAEChannelClient* _gaeChannel;
  NSURL* _postMessageURL;
  BOOL _verboseLogging;
  __weak id<GAEMessageHandler> _messageHandler;
}

- (instancetype)initWithDelegate:(id<APPRTCAppClientDelegate>)delegate
                  messageHandler:(id<GAEMessageHandler>)handler {
  if (self = [super init]) {
    _delegate = delegate;
    _messageHandler = handler;
    _backgroundQueue = dispatch_queue_create("RTCBackgroundQueue",
                                             DISPATCH_QUEUE_SERIAL);
    // Uncomment to see Request/Response logging.
    // _verboseLogging = YES;
  }
  return self;
}

- (void)connectToRoom:(NSURL*)url {
  NSString* urlString =
      [[url absoluteString] stringByAppendingString:@"&t=json"];
  NSURL* requestURL = [NSURL URLWithString:urlString];
  NSURLRequest* request = [NSURLRequest requestWithURL:requestURL];
  [self sendURLRequest:request
      completionHandler:^(NSError* error,
                          NSHTTPURLResponse* httpResponse,
                          NSData* responseData) {
    int statusCode = [httpResponse statusCode];
    [self logVerbose:[NSString stringWithFormat:
        @"Response received\nURL\n%@\nStatus [%d]\nHeaders\n%@",
        [httpResponse URL],
        statusCode,
        [httpResponse allHeaderFields]]];
    NSAssert(statusCode == 200,
             @"Invalid response of %d received while connecting to: %@",
             statusCode,
             urlString);
    if (statusCode != 200) {
      return;
    }
    [self handleResponseData:responseData
              forRoomRequest:request];
  }];
}

- (void)sendData:(NSData*)data {
  NSParameterAssert([data length] > 0);
  NSString* message = [NSString stringWithUTF8String:[data bytes]];
  [self logVerbose:[NSString stringWithFormat:@"Send message:\n%@", message]];
  if (!_postMessageURL) {
    return;
  }
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:_postMessageURL];
  request.HTTPMethod = @"POST";
  [request setHTTPBody:data];
  [self sendURLRequest:request
     completionHandler:^(NSError* error,
                         NSHTTPURLResponse* httpResponse,
                         NSData* responseData) {
    int status = [httpResponse statusCode];
    NSString* response = [responseData length] > 0 ?
        [NSString stringWithUTF8String:[responseData bytes]] :
        nil;
    NSAssert(status == 200,
             @"Bad response [%d] to message: %@\n\n%@",
             status,
             message,
             response);
  }];
}

#pragma mark - Private

- (void)logVerbose:(NSString*)message {
  if (_verboseLogging) {
    NSLog(@"%@", message);
  }
}

- (void)handleResponseData:(NSData*)responseData
            forRoomRequest:(NSURLRequest*)request {
  NSDictionary* roomJSON = [self parseJSONData:responseData];
  [self logVerbose:[NSString stringWithFormat:@"Room JSON:\n%@", roomJSON]];
  NSParameterAssert(roomJSON);
  if (roomJSON[@"error"]) {
    NSArray* errorMessages = roomJSON[@"error_messages"];
    NSMutableString* message = [NSMutableString string];
    for (NSString* errorMessage in errorMessages) {
      [message appendFormat:@"%@\n", errorMessage];
    }
    [self.delegate appClient:self didErrorWithMessage:message];
    return;
  }
  NSString* pcConfig = roomJSON[@"pc_config"];
  NSData* pcConfigData = [pcConfig dataUsingEncoding:NSUTF8StringEncoding];
  NSDictionary* pcConfigJSON = [self parseJSONData:pcConfigData];
  [self logVerbose:[NSString stringWithFormat:@"PCConfig JSON:\n%@",
                                              pcConfigJSON]];
  NSParameterAssert(pcConfigJSON);

  NSArray* iceServers = [self parseICEServersForPCConfigJSON:pcConfigJSON];
  [self requestTURNServerForICEServers:iceServers
                         turnServerUrl:roomJSON[@"turn_url"]];

  _initiator = [roomJSON[@"initiator"] boolValue];
  [self logVerbose:[NSString stringWithFormat:@"Initiator: %d", _initiator]];
  _postMessageURL = [self parsePostMessageURLForRoomJSON:roomJSON
                                                 request:request];
  [self logVerbose:[NSString stringWithFormat:@"POST message URL:\n%@",
                                              _postMessageURL]];
  _videoConstraints = [self parseVideoConstraintsForRoomJSON:roomJSON];
  [self logVerbose:[NSString stringWithFormat:@"Media constraints:\n%@",
                                              _videoConstraints]];
  NSString* token = roomJSON[@"token"];
  [self logVerbose:
      [NSString stringWithFormat:@"About to open GAE with token:  %@",
                                 token]];
  _gaeChannel =
      [[GAEChannelClient alloc] initWithToken:token
                                     delegate:_messageHandler];
}

- (NSDictionary*)parseJSONData:(NSData*)data {
  NSError* error = nil;
  NSDictionary* json =
      [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
  NSAssert(!error, @"Unable to parse.  %@", error.localizedDescription);
  return json;
}

- (NSArray*)parseICEServersForPCConfigJSON:(NSDictionary*)pcConfigJSON {
  NSMutableArray* result = [NSMutableArray array];
  NSArray* iceServers = pcConfigJSON[@"iceServers"];
  for (NSDictionary* iceServer in iceServers) {
    NSString* url = iceServer[@"urls"];
    NSString* username = pcConfigJSON[@"username"];
    NSString* credential = iceServer[@"credential"];
    username = username ? username : @"";
    credential = credential ? credential : @"";
    [self logVerbose:[NSString stringWithFormat:@"url [%@] - credential [%@]",
                                                url,
                                                credential]];
    RTCICEServer* server =
        [[RTCICEServer alloc] initWithURI:[NSURL URLWithString:url]
                                 username:username
                                 password:credential];
    [result addObject:server];
  }
  return result;
}

- (NSURL*)parsePostMessageURLForRoomJSON:(NSDictionary*)roomJSON
                                 request:(NSURLRequest*)request {
  NSString* requestUrl = [[request URL] absoluteString];
  NSRange queryRange = [requestUrl rangeOfString:@"?"];
  NSString* baseUrl = [requestUrl substringToIndex:queryRange.location];
  NSString* roomKey = roomJSON[@"room_key"];
  NSParameterAssert([roomKey length] > 0);
  NSString* me = roomJSON[@"me"];
  NSParameterAssert([me length] > 0);
  NSString* postMessageUrl =
      [NSString stringWithFormat:@"%@/message?r=%@&u=%@", baseUrl, roomKey, me];
  return [NSURL URLWithString:postMessageUrl];
}

- (RTCMediaConstraints*)parseVideoConstraintsForRoomJSON:
    (NSDictionary*)roomJSON {
  NSString* mediaConstraints = roomJSON[@"media_constraints"];
  RTCMediaConstraints* constraints = nil;
  if ([mediaConstraints length] > 0) {
    NSData* constraintsData =
        [mediaConstraints dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* constraintsJSON = [self parseJSONData:constraintsData];
    id video = constraintsJSON[@"video"];
    if ([video isKindOfClass:[NSDictionary class]]) {
      NSDictionary* mandatory = video[@"mandatory"];
      NSMutableArray* mandatoryContraints =
          [NSMutableArray arrayWithCapacity:[mandatory count]];
      [mandatory enumerateKeysAndObjectsUsingBlock:^(
          id key, id obj, BOOL* stop) {
        [mandatoryContraints addObject:[[RTCPair alloc] initWithKey:key
                                                              value:obj]];
      }];
      // TODO(tkchin): figure out json formats for optional constraints.
      constraints =
          [[RTCMediaConstraints alloc]
              initWithMandatoryConstraints:mandatoryContraints
                       optionalConstraints:nil];
    } else if ([video isKindOfClass:[NSNumber class]] && [video boolValue]) {
      constraints = [[RTCMediaConstraints alloc] init];
    }
  }
  return constraints;
}

- (void)requestTURNServerWithUrl:(NSString*)turnServerUrl
               completionHandler:
    (void (^)(RTCICEServer* turnServer))completionHandler {
  NSURL* turnServerURL = [NSURL URLWithString:turnServerUrl];
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:turnServerURL];
  [request addValue:@"Mozilla/5.0" forHTTPHeaderField:@"user-agent"];
  [request addValue:@"https://apprtc.appspot.com"
      forHTTPHeaderField:@"origin"];
  [self sendURLRequest:request
     completionHandler:^(NSError* error,
                         NSHTTPURLResponse* response,
                         NSData* responseData) {
    if (error) {
      NSLog(@"Unable to get TURN server.");
      completionHandler(nil);
      return;
    }
    NSDictionary* json = [self parseJSONData:responseData];
    NSString* username = json[@"username"];
    NSString* password = json[@"password"];
    NSArray* uris = json[@"uris"];
    NSParameterAssert([uris count] > 0);
    RTCICEServer* turnServer =
        [[RTCICEServer alloc] initWithURI:[NSURL URLWithString:uris[0]]
                                 username:username
                                 password:password];
    completionHandler(turnServer);
  }];
}

- (void)requestTURNServerForICEServers:(NSArray*)iceServers
                         turnServerUrl:(NSString*)turnServerUrl {
  BOOL isTurnPresent = NO;
  for (RTCICEServer* iceServer in iceServers) {
    if ([[iceServer.URI scheme] isEqualToString:@"turn"]) {
      isTurnPresent = YES;
      break;
    }
  }
  if (!isTurnPresent) {
    [self requestTURNServerWithUrl:turnServerUrl
                 completionHandler:^(RTCICEServer* turnServer) {
      NSArray* servers = iceServers;
      if (turnServer) {
        servers = [servers arrayByAddingObject:turnServer];
      }
      NSLog(@"ICE servers:\n%@", servers);
      [self.delegate appClient:self didReceiveICEServers:servers];
    }];
  } else {
    NSLog(@"ICE servers:\n%@", iceServers);
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.delegate appClient:self didReceiveICEServers:iceServers];
    });
  }
}

- (void)sendURLRequest:(NSURLRequest*)request
     completionHandler:(void (^)(NSError* error,
                                 NSHTTPURLResponse* httpResponse,
                                 NSData* responseData))completionHandler {
  dispatch_async(_backgroundQueue, ^{
    NSError* error = nil;
    NSURLResponse* response = nil;
    NSData* responseData = [NSURLConnection sendSynchronousRequest:request
                                                 returningResponse:&response
                                                             error:&error];
    NSParameterAssert(!response ||
                      [response isKindOfClass:[NSHTTPURLResponse class]]);
    if (error) {
      NSLog(@"Failed URL request for:%@\nError:%@", request, error);
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
      completionHandler(error, httpResponse, responseData);
    });
  });
}

@end
