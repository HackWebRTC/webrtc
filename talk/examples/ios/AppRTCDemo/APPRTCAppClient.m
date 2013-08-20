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

#import "APPRTCAppClient.h"

#import <dispatch/dispatch.h>

#import "GAEChannelClient.h"
#import "RTCICEServer.h"

@interface APPRTCAppClient ()

@property(nonatomic, strong) dispatch_queue_t backgroundQueue;
@property(nonatomic, copy) NSString *baseURL;
@property(nonatomic, strong) GAEChannelClient *gaeChannel;
@property(nonatomic, copy) NSString *postMessageUrl;
@property(nonatomic, copy) NSString *pcConfig;
@property(nonatomic, strong) NSMutableString *roomHtml;
@property(atomic, strong) NSMutableArray *sendQueue;
@property(nonatomic, copy) NSString *token;

@property(nonatomic, assign) BOOL verboseLogging;

@end

@implementation APPRTCAppClient

- (id)init {
  if (self = [super init]) {
    _backgroundQueue = dispatch_queue_create("RTCBackgroundQueue", NULL);
    _sendQueue = [NSMutableArray array];
    // Uncomment to see Request/Response logging.
    // _verboseLogging = YES;
  }
  return self;
}

#pragma mark - Public methods

- (void)connectToRoom:(NSURL *)url {
  NSURLRequest *request = [self getRequestFromUrl:url];
  [NSURLConnection connectionWithRequest:request delegate:self];
}

- (void)sendData:(NSData *)data {
  @synchronized(self) {
    [self maybeLogMessage:@"Send message"];
    [self.sendQueue addObject:[data copy]];
  }
  [self requestQueueDrainInBackground];
}

#pragma mark - Internal methods

- (NSString*)findVar:(NSString*)name
     strippingQuotes:(BOOL)strippingQuotes {
  NSError* error;
  NSString* pattern =
      [NSString stringWithFormat:@".*\n *var %@ = ([^\n]*);\n.*", name];
  NSRegularExpression *regexp =
      [NSRegularExpression regularExpressionWithPattern:pattern
                                                options:0
                                                  error:&error];
  NSAssert(!error, @"Unexpected error compiling regex: ",
           error.localizedDescription);

  NSRange fullRange = NSMakeRange(0, [self.roomHtml length]);
  NSArray *matches =
      [regexp matchesInString:self.roomHtml options:0 range:fullRange];
  if ([matches count] != 1) {
    [self showMessage:[NSString stringWithFormat:@"%d matches for %@ in %@",
                                [matches count], name, self.roomHtml]];
    return nil;
  }
  NSRange matchRange = [matches[0] rangeAtIndex:1];
  NSString* value = [self.roomHtml substringWithRange:matchRange];
  if (strippingQuotes) {
    NSAssert([value length] > 2,
             @"Can't strip quotes from short string: [%@]", value);
    NSAssert(([value characterAtIndex:0] == '\'' &&
              [value characterAtIndex:[value length] - 1] == '\''),
             @"Can't strip quotes from unquoted string: [%@]", value);
    value = [value substringWithRange:NSMakeRange(1, [value length] - 2)];
  }
  return value;
}

- (NSURLRequest *)getRequestFromUrl:(NSURL *)url {
  self.roomHtml = [NSMutableString stringWithCapacity:20000];
  NSString *path =
      [NSString stringWithFormat:@"https:%@", [url resourceSpecifier]];
  NSURLRequest *request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:path]];
  return request;
}

- (void)maybeLogMessage:(NSString *)message {
  if (self.verboseLogging) {
    NSLog(@"%@", message);
  }
}

- (void)requestQueueDrainInBackground {
  dispatch_async(self.backgroundQueue, ^(void) {
    // TODO(hughv): This can block the UI thread.  Fix.
    @synchronized(self) {
      if ([self.postMessageUrl length] < 1) {
        return;
      }
      for (NSData *data in self.sendQueue) {
        NSString *url = [NSString stringWithFormat:@"%@/%@",
                         self.baseURL,
                         self.postMessageUrl];
        [self sendData:data withUrl:url];
      }
      [self.sendQueue removeAllObjects];
    }
  });
}

- (void)sendData:(NSData *)data withUrl:(NSString *)url {
  NSMutableURLRequest *request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:url]];
  request.HTTPMethod = @"POST";
  [request setHTTPBody:data];
  NSURLResponse *response;
  NSError *error;
  NSData *responseData = [NSURLConnection sendSynchronousRequest:request
                                               returningResponse:&response
                                                           error:&error];
  NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
  int status = [httpResponse statusCode];
  NSAssert(status == 200,
           @"Bad response [%d] to message: %@\n\n%@",
           status,
           [NSString stringWithUTF8String:[data bytes]],
           [NSString stringWithUTF8String:[responseData bytes]]);
}

- (void)showMessage:(NSString *)message {
  NSLog(@"%@", message);
  UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:@"Unable to join"
                                                      message:message
                                                     delegate:nil
                                            cancelButtonTitle:@"OK"
                                            otherButtonTitles:nil];
  [alertView show];
}

- (void)updateICEServers:(NSMutableArray *)ICEServers
          withTurnServer:(NSString *)turnServerUrl {
  if ([turnServerUrl length] < 1) {
    [self.ICEServerDelegate onICEServers:ICEServers];
    return;
  }
  dispatch_async(self.backgroundQueue, ^(void) {
    NSMutableURLRequest *request = [NSMutableURLRequest
        requestWithURL:[NSURL URLWithString:turnServerUrl]];
    [request addValue:@"Mozilla/5.0" forHTTPHeaderField:@"user-agent"];
    [request addValue:@"https://apprtc.appspot.com"
        forHTTPHeaderField:@"origin"];
    NSURLResponse *response;
    NSError *error;
    NSData *responseData = [NSURLConnection sendSynchronousRequest:request
                                                 returningResponse:&response
                                                             error:&error];
    if (!error) {
      NSDictionary *json = [NSJSONSerialization JSONObjectWithData:responseData
                                                           options:0
                                                             error:&error];
      NSAssert(!error, @"Unable to parse.  %@", error.localizedDescription);
      NSString *username = json[@"username"];
      NSString *turnServer = json[@"turn"];
      NSString *password = json[@"password"];
      NSString *fullUrl =
          [NSString stringWithFormat:@"turn:%@@%@", username, turnServer];
      RTCICEServer *ICEServer =
          [[RTCICEServer alloc] initWithURI:[NSURL URLWithString:fullUrl]
                                   password:password];
      [ICEServers addObject:ICEServer];
    } else {
      NSLog(@"Unable to get TURN server.  Error: %@", error.description);
    }

    dispatch_async(dispatch_get_main_queue(), ^(void) {
      [self.ICEServerDelegate onICEServers:ICEServers];
    });
  });
}

#pragma mark - NSURLConnectionDataDelegate methods

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
  NSString *roomHtml = [NSString stringWithUTF8String:[data bytes]];
  [self maybeLogMessage:
          [NSString stringWithFormat:@"Received %d chars", [roomHtml length]]];
  [self.roomHtml appendString:roomHtml];
}

- (void)connection:(NSURLConnection *)connection
    didReceiveResponse:(NSURLResponse *)response {
  NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
  int statusCode = [httpResponse statusCode];
  [self maybeLogMessage:
          [NSString stringWithFormat:
                  @"Response received\nURL\n%@\nStatus [%d]\nHeaders\n%@",
              [httpResponse URL],
              statusCode,
              [httpResponse allHeaderFields]]];
  NSAssert(statusCode == 200, @"Invalid response  of %d received.", statusCode);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
  [self maybeLogMessage:[NSString stringWithFormat:@"finished loading %d chars",
                         [self.roomHtml length]]];
  NSRegularExpression* fullRegex =
    [NSRegularExpression regularExpressionWithPattern:@"room is full"
                                              options:0
                                                error:nil];
  if ([fullRegex numberOfMatchesInString:self.roomHtml
                                 options:0
                                   range:NSMakeRange(0, [self.roomHtml length])]) {
    [self showMessage:@"Room full"];
    return;
  }


  NSString *fullUrl = [[[connection originalRequest] URL] absoluteString];
  NSRange queryRange = [fullUrl rangeOfString:@"?"];
  self.baseURL = [fullUrl substringToIndex:queryRange.location];
  [self maybeLogMessage:[NSString stringWithFormat:@"Base URL: %@", self.baseURL]];

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
  [self maybeLogMessage:
          [NSString stringWithFormat:@"PC Config JSON: %@", pcConfig]];

  NSString *turnServerUrl = [self findVar:@"turnUrl" strippingQuotes:YES];
  if (turnServerUrl) {
    [self maybeLogMessage:
            [NSString stringWithFormat:@"TURN server request URL: %@",
                turnServerUrl]];
  }

  NSError *error;
  NSData *pcData = [pcConfig dataUsingEncoding:NSUTF8StringEncoding];
  NSDictionary *json =
      [NSJSONSerialization JSONObjectWithData:pcData options:0 error:&error];
  NSAssert(!error, @"Unable to parse.  %@", error.localizedDescription);
  NSArray *servers = [json objectForKey:@"ICEServers"];
  NSMutableArray *ICEServers = [NSMutableArray array];
  for (NSDictionary *server in servers) {
    NSString *url = [server objectForKey:@"url"];
    NSString *credential = [server objectForKey:@"credential"];
    if (!credential) {
      credential = @"";
    }
    [self maybeLogMessage:
            [NSString stringWithFormat:@"url [%@] - credential [%@]",
                url,
                credential]];
    RTCICEServer *ICEServer =
        [[RTCICEServer alloc] initWithURI:[NSURL URLWithString:url]
                                 password:credential];
    [ICEServers addObject:ICEServer];
  }
  [self updateICEServers:ICEServers withTurnServer:turnServerUrl];

  [self maybeLogMessage:
          [NSString stringWithFormat:@"About to open GAE with token:  %@",
              self.token]];
  self.gaeChannel =
      [[GAEChannelClient alloc] initWithToken:self.token
                                     delegate:self.messageHandler];
}

@end
