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

@interface APPRTCAppClient ()

@property(nonatomic, weak, readonly) id<GAEMessageHandler> messageHandler;
@property(nonatomic, copy) NSString* baseURL;
@property(nonatomic, strong) GAEChannelClient* gaeChannel;
@property(nonatomic, copy) NSString* postMessageUrl;
@property(nonatomic, copy) NSString* pcConfig;
@property(nonatomic, strong) NSMutableString* roomHtml;
@property(atomic, strong) NSMutableArray* sendQueue;
@property(nonatomic, copy) NSString* token;

@property(nonatomic, assign) BOOL verboseLogging;

@end

@implementation APPRTCAppClient {
  dispatch_queue_t _backgroundQueue;
}

- (id)initWithDelegate:(id<APPRTCAppClientDelegate>)delegate
        messageHandler:(id<GAEMessageHandler>)handler {
  if (self = [super init]) {
    _delegate = delegate;
    _messageHandler = handler;
    _backgroundQueue = dispatch_queue_create("RTCBackgroundQueue",
                                             DISPATCH_QUEUE_SERIAL);
    _sendQueue = [NSMutableArray array];
    // Uncomment to see Request/Response logging.
    // _verboseLogging = YES;
  }
  return self;
}

#pragma mark - Public methods

- (void)connectToRoom:(NSURL*)url {
  self.roomHtml = [NSMutableString stringWithCapacity:20000];
  NSURLRequest* request = [NSURLRequest requestWithURL:url];
  [NSURLConnection connectionWithRequest:request delegate:self];
}

- (void)sendData:(NSData*)data {
  [self maybeLogMessage:@"Send message"];

  dispatch_async(_backgroundQueue, ^{
    [self.sendQueue addObject:[data copy]];

    if ([self.postMessageUrl length] < 1) {
      return;
    }
    for (NSData* data in self.sendQueue) {
      NSString* url =
          [NSString stringWithFormat:@"%@/%@",
                    self.baseURL, self.postMessageUrl];
      [self sendData:data withUrl:url];
    }
    [self.sendQueue removeAllObjects];
  });
}

#pragma mark - Internal methods

- (NSString*)findVar:(NSString*)name strippingQuotes:(BOOL)strippingQuotes {
  NSError* error;
  NSString* pattern =
      [NSString stringWithFormat:@".*\n *var %@ = ([^\n]*);\n.*", name];
  NSRegularExpression* regexp =
      [NSRegularExpression regularExpressionWithPattern:pattern
                                                options:0
                                                  error:&error];
  NSAssert(!error,
           @"Unexpected error compiling regex: ",
           error.localizedDescription);

  NSRange fullRange = NSMakeRange(0, [self.roomHtml length]);
  NSArray* matches =
      [regexp matchesInString:self.roomHtml options:0 range:fullRange];
  if ([matches count] != 1) {
    NSString* format = @"%lu matches for %@ in %@";
    NSString* message = [NSString stringWithFormat:format,
        (unsigned long)[matches count], name, self.roomHtml];
    [self.delegate appClient:self didErrorWithMessage:message];
    return nil;
  }
  NSRange matchRange = [matches[0] rangeAtIndex:1];
  NSString* value = [self.roomHtml substringWithRange:matchRange];
  if (strippingQuotes) {
    NSAssert([value length] > 2,
             @"Can't strip quotes from short string: [%@]",
             value);
    NSAssert(([value characterAtIndex:0] == '\'' &&
              [value characterAtIndex:[value length] - 1] == '\''),
             @"Can't strip quotes from unquoted string: [%@]",
             value);
    value = [value substringWithRange:NSMakeRange(1, [value length] - 2)];
  }
  return value;
}

- (void)maybeLogMessage:(NSString*)message {
  if (self.verboseLogging) {
    NSLog(@"%@", message);
  }
}

- (void)sendData:(NSData*)data withUrl:(NSString*)url {
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:url]];
  request.HTTPMethod = @"POST";
  [request setHTTPBody:data];
  NSURLResponse* response;
  NSError* error;
  NSData* responseData = [NSURLConnection sendSynchronousRequest:request
                                               returningResponse:&response
                                                           error:&error];
  NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
  int status = [httpResponse statusCode];
  NSAssert(status == 200,
           @"Bad response [%d] to message: %@\n\n%@",
           status,
           [NSString stringWithUTF8String:[data bytes]],
           [NSString stringWithUTF8String:[responseData bytes]]);
}

- (void)updateICEServers:(NSMutableArray*)ICEServers
          withTurnServer:(NSString*)turnServerUrl {
  if ([turnServerUrl length] < 1) {
    [self.delegate appClient:self didReceiveICEServers:ICEServers];
    return;
  }
  dispatch_async(_backgroundQueue, ^(void) {
      NSMutableURLRequest* request = [NSMutableURLRequest
          requestWithURL:[NSURL URLWithString:turnServerUrl]];
      [request addValue:@"Mozilla/5.0" forHTTPHeaderField:@"user-agent"];
      [request addValue:@"https://apprtc.appspot.com"
          forHTTPHeaderField:@"origin"];
      NSURLResponse* response;
      NSError* error;
      NSData* responseData = [NSURLConnection sendSynchronousRequest:request
                                                   returningResponse:&response
                                                               error:&error];
      if (!error) {
        NSDictionary* json =
            [NSJSONSerialization JSONObjectWithData:responseData
                                            options:0
                                              error:&error];
        NSAssert(!error, @"Unable to parse.  %@", error.localizedDescription);
        NSString* username = json[@"username"];
        NSString* password = json[@"password"];
        NSArray* uris = json[@"uris"];
        for (int i = 0; i < [uris count]; ++i) {
          NSString* turnServer = [uris objectAtIndex:i];
          RTCICEServer* ICEServer =
              [[RTCICEServer alloc] initWithURI:[NSURL URLWithString:turnServer]
                                       username:username
                                       password:password];
          NSLog(@"Added ICE Server: %@", ICEServer);
          [ICEServers addObject:ICEServer];
        }
      } else {
        NSLog(@"Unable to get TURN server.  Error: %@", error.description);
      }

      dispatch_async(dispatch_get_main_queue(), ^(void) {
          [self.delegate appClient:self didReceiveICEServers:ICEServers];
      });
  });
}

#pragma mark - NSURLConnectionDataDelegate methods

- (void)connection:(NSURLConnection*)connection didReceiveData:(NSData*)data {
  NSString* roomHtml = [NSString stringWithUTF8String:[data bytes]];
  NSString* message =
      [NSString stringWithFormat:@"Received %lu chars",
                                  (unsigned long)[roomHtml length]];
  [self maybeLogMessage:message];
  [self.roomHtml appendString:roomHtml];
}

- (void)connection:(NSURLConnection*)connection
    didReceiveResponse:(NSURLResponse*)response {
  NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
  int statusCode = [httpResponse statusCode];
  [self
      maybeLogMessage:
          [NSString stringWithFormat:
                        @"Response received\nURL\n%@\nStatus [%d]\nHeaders\n%@",
                        [httpResponse URL],
                        statusCode,
                        [httpResponse allHeaderFields]]];
  NSAssert(statusCode == 200, @"Invalid response  of %d received.", statusCode);
}

- (void)connectionDidFinishLoading:(NSURLConnection*)connection {
  NSString* message =
      [NSString stringWithFormat:@"finished loading %lu chars",
                                  (unsigned long)[self.roomHtml length]];
  [self maybeLogMessage:message];
  NSRegularExpression* fullRegex =
      [NSRegularExpression regularExpressionWithPattern:@"room is full"
                                                options:0
                                                  error:nil];
  if ([fullRegex
          numberOfMatchesInString:self.roomHtml
                          options:0
                            range:NSMakeRange(0, [self.roomHtml length])]) {
    NSString* message = @"Room full, dropping peerconnection.";
    [self.delegate appClient:self didErrorWithMessage:message];
    return;
  }

  NSString* fullUrl = [[[connection originalRequest] URL] absoluteString];
  NSRange queryRange = [fullUrl rangeOfString:@"?"];
  self.baseURL = [fullUrl substringToIndex:queryRange.location];
  [self maybeLogMessage:[NSString
                            stringWithFormat:@"Base URL: %@", self.baseURL]];

  self.initiator = [[self findVar:@"initiator" strippingQuotes:NO] boolValue];
  self.token = [self findVar:@"channelToken" strippingQuotes:YES];
  if (!self.token)
    return;
  [self maybeLogMessage:[NSString stringWithFormat:@"Token: %@", self.token]];

  NSString* roomKey = [self findVar:@"roomKey" strippingQuotes:YES];
  NSString* me = [self findVar:@"me" strippingQuotes:YES];
  if (!roomKey || !me)
    return;
  self.postMessageUrl =
      [NSString stringWithFormat:@"/message?r=%@&u=%@", roomKey, me];
  [self maybeLogMessage:[NSString stringWithFormat:@"POST message URL: %@",
                                                   self.postMessageUrl]];

  NSString* pcConfig = [self findVar:@"pcConfig" strippingQuotes:NO];
  if (!pcConfig)
    return;
  [self maybeLogMessage:[NSString
                            stringWithFormat:@"PC Config JSON: %@", pcConfig]];

  NSString* turnServerUrl = [self findVar:@"turnUrl" strippingQuotes:YES];
  if (turnServerUrl) {
    [self maybeLogMessage:[NSString
                              stringWithFormat:@"TURN server request URL: %@",
                                               turnServerUrl]];
  }

  NSError* error;
  NSData* pcData = [pcConfig dataUsingEncoding:NSUTF8StringEncoding];
  NSDictionary* json =
      [NSJSONSerialization JSONObjectWithData:pcData options:0 error:&error];
  NSAssert(!error, @"Unable to parse.  %@", error.localizedDescription);
  NSArray* servers = [json objectForKey:@"iceServers"];
  NSMutableArray* ICEServers = [NSMutableArray array];
  for (NSDictionary* server in servers) {
    NSString* url = [server objectForKey:@"urls"];
    NSString* username = json[@"username"];
    NSString* credential = [server objectForKey:@"credential"];
    if (!username) {
      username = @"";
    }
    if (!credential) {
      credential = @"";
    }
    [self maybeLogMessage:[NSString
                              stringWithFormat:@"url [%@] - credential [%@]",
                                               url,
                                               credential]];
    RTCICEServer* ICEServer =
        [[RTCICEServer alloc] initWithURI:[NSURL URLWithString:url]
                                 username:username
                                 password:credential];
    NSLog(@"Added ICE Server: %@", ICEServer);
    [ICEServers addObject:ICEServer];
  }
  [self updateICEServers:ICEServers withTurnServer:turnServerUrl];

  NSString* mc = [self findVar:@"mediaConstraints" strippingQuotes:NO];
  if (mc) {
    error = nil;
    NSData* mcData = [mc dataUsingEncoding:NSUTF8StringEncoding];
    json =
        [NSJSONSerialization JSONObjectWithData:mcData options:0 error:&error];
    NSAssert(!error, @"Unable to parse.  %@", error.localizedDescription);
    id video = json[@"video"];
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
      _videoConstraints =
          [[RTCMediaConstraints alloc]
              initWithMandatoryConstraints:mandatoryContraints
                       optionalConstraints:nil];
    } else if ([video isKindOfClass:[NSNumber class]] && [video boolValue]) {
      _videoConstraints = [[RTCMediaConstraints alloc] init];
    }
  }

  [self
      maybeLogMessage:[NSString
                          stringWithFormat:@"About to open GAE with token:  %@",
                                           self.token]];
  self.gaeChannel =
      [[GAEChannelClient alloc] initWithToken:self.token
                                     delegate:self.messageHandler];
}

@end
