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

#import "ARDSignalingParams.h"
#import "ARDUtilities.h"
#import "GAEChannelClient.h"
#import "RTCICEServer.h"
#import "RTCICEServer+JSON.h"
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
  NSString *urlString =
      [[url absoluteString] stringByAppendingString:@"&t=json"];
  NSURL *requestURL = [NSURL URLWithString:urlString];
  NSURLRequest *request = [NSURLRequest requestWithURL:requestURL];
  [NSURLConnection sendAsynchronousRequest:request
                         completionHandler:^(NSURLResponse *response,
                                             NSData *data,
                                             NSError *error) {
    NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
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
    [self handleResponseData:data forRoomRequest:request];
  }];
}

- (void)sendData:(NSData*)data {
  NSParameterAssert([data length] > 0);
  NSString *message = [NSString stringWithUTF8String:[data bytes]];
  [self logVerbose:[NSString stringWithFormat:@"Send message:\n%@", message]];
  if (!_postMessageURL) {
    return;
  }
  NSMutableURLRequest *request =
      [NSMutableURLRequest requestWithURL:_postMessageURL];
  request.HTTPMethod = @"POST";
  [request setHTTPBody:data];
  [NSURLConnection sendAsynchronousRequest:request
                         completionHandler:^(NSURLResponse *response,
                                             NSData *data,
                                             NSError *error) {
    NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
    int status = [httpResponse statusCode];
    NSString *responseString = [data length] > 0 ?
        [NSString stringWithUTF8String:[data bytes]] :
        nil;
    NSAssert(status == 200,
             @"Bad response [%d] to message: %@\n\n%@",
             status,
             message,
             responseString);
  }];
}

#pragma mark - Private

- (void)logVerbose:(NSString *)message {
  if (_verboseLogging) {
    NSLog(@"%@", message);
  }
}

- (void)handleResponseData:(NSData *)responseData
            forRoomRequest:(NSURLRequest *)request {
  ARDSignalingParams *params =
      [ARDSignalingParams paramsFromJSONData:responseData];
  if (params.errorMessages.count > 0) {
    NSMutableString *message = [NSMutableString string];
    for (NSString *errorMessage in params.errorMessages) {
      [message appendFormat:@"%@\n", errorMessage];
    }
    [self.delegate appClient:self didErrorWithMessage:message];
    return;
  }
  [self requestTURNServerForICEServers:params.iceServers
                         turnServerUrl:[params.turnRequestURL absoluteString]];
  NSString *token = params.channelToken;
  [self logVerbose:
      [NSString stringWithFormat:@"About to open GAE with token:  %@",
                                 token]];
  _gaeChannel =
      [[GAEChannelClient alloc] initWithToken:token
                                     delegate:_messageHandler];
  _params = params;
  // Generate URL for posting data.
  NSDictionary *roomJSON = [NSDictionary dictionaryWithJSONData:responseData];
  _postMessageURL = [self parsePostMessageURLForRoomJSON:roomJSON
                                                 request:request];
  [self logVerbose:[NSString stringWithFormat:@"POST message URL:\n%@",
                                              _postMessageURL]];
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

- (void)requestTURNServerWithUrl:(NSString *)turnServerUrl
               completionHandler:
    (void (^)(RTCICEServer *turnServer))completionHandler {
  NSURL *turnServerURL = [NSURL URLWithString:turnServerUrl];
  NSMutableURLRequest *request =
      [NSMutableURLRequest requestWithURL:turnServerURL];
  [request addValue:@"Mozilla/5.0" forHTTPHeaderField:@"user-agent"];
  [request addValue:@"https://apprtc.appspot.com"
      forHTTPHeaderField:@"origin"];
  [NSURLConnection sendAsynchronousRequest:request
                         completionHandler:^(NSURLResponse *response,
                                             NSData *data,
                                             NSError *error) {
    if (error) {
      NSLog(@"Unable to get TURN server.");
      completionHandler(nil);
      return;
    }
    NSDictionary *json = [NSDictionary dictionaryWithJSONData:data];
    RTCICEServer *turnServer = [RTCICEServer serverFromCEODJSONDictionary:json];
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

@end
