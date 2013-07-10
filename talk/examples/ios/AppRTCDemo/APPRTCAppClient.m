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
#import "RTCIceServer.h"

@interface APPRTCAppClient ()

@property(nonatomic, strong) dispatch_queue_t backgroundQueue;
@property(nonatomic, copy) NSString *baseURL;
@property(nonatomic, strong) GAEChannelClient *gaeChannel;
@property(nonatomic, copy) NSString *postMessageUrl;
@property(nonatomic, copy) NSString *pcConfig;
@property(nonatomic, strong) NSMutableString *receivedData;
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
    //_verboseLogging = YES;
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

- (NSTextCheckingResult *)findMatch:(NSString *)regexpPattern
                         withString:(NSString *)string
                       errorMessage:(NSString *)errorMessage {
  NSError *error;
  NSRegularExpression *regexp =
      [NSRegularExpression regularExpressionWithPattern:regexpPattern
                                                options:0
                                                  error:&error];
  if (error) {
    [self maybeLogMessage:
            [NSString stringWithFormat:@"Failed to create regexp - %@",
                [error description]]];
    return nil;
  }
  NSRange fullRange = NSMakeRange(0, [string length]);
  NSArray *matches = [regexp matchesInString:string options:0 range:fullRange];
  if ([matches count] == 0) {
    if ([errorMessage length] > 0) {
      [self maybeLogMessage:string];
      [self showMessage:
              [NSString stringWithFormat:@"Missing %@ in HTML.", errorMessage]];
    }
    return nil;
  } else if ([matches count] > 1) {
    if ([errorMessage length] > 0) {
      [self maybeLogMessage:string];
      [self showMessage:[NSString stringWithFormat:@"Too many %@s in HTML.",
                         errorMessage]];
    }
    return nil;
  }
  return matches[0];
}

- (NSURLRequest *)getRequestFromUrl:(NSURL *)url {
  self.receivedData = [NSMutableString stringWithCapacity:20000];
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

- (void)updateIceServers:(NSMutableArray *)iceServers
          withTurnServer:(NSString *)turnServerUrl {
  if ([turnServerUrl length] < 1) {
    [self.iceServerDelegate onIceServers:iceServers];
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
      RTCIceServer *iceServer =
          [[RTCIceServer alloc] initWithUri:[NSURL URLWithString:fullUrl]
                                   password:password];
      [iceServers addObject:iceServer];
    } else {
      NSLog(@"Unable to get TURN server.  Error: %@", error.description);
    }

    dispatch_async(dispatch_get_main_queue(), ^(void) {
      [self.iceServerDelegate onIceServers:iceServers];
    });
  });
}

#pragma mark - NSURLConnectionDataDelegate methods

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
  NSString *roomHtml = [NSString stringWithUTF8String:[data bytes]];
  [self maybeLogMessage:
          [NSString stringWithFormat:@"Received %d chars", [roomHtml length]]];
  [self.receivedData appendString:roomHtml];
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
                         [self.receivedData length]]];
  NSTextCheckingResult *result =
      [self findMatch:@".*\n *Sorry, this room is full\\..*"
            withString:self.receivedData
          errorMessage:nil];
  if (result) {
    [self showMessage:@"Room full"];
    return;
  }

  NSString *fullUrl = [[[connection originalRequest] URL] absoluteString];
  NSRange queryRange = [fullUrl rangeOfString:@"?"];
  self.baseURL = [fullUrl substringToIndex:queryRange.location];
  [self maybeLogMessage:[NSString stringWithFormat:@"URL\n%@", self.baseURL]];

  result = [self findMatch:@".*\n *openChannel\\('([^']*)'\\);\n.*"
                withString:self.receivedData
              errorMessage:@"channel token"];
  if (!result) {
    return;
  }
  self.token = [self.receivedData substringWithRange:[result rangeAtIndex:1]];
  [self maybeLogMessage:[NSString stringWithFormat:@"Token\n%@", self.token]];

  result =
      [self findMatch:@".*\n *path = '/(message\\?r=.+)' \\+ '(&u=[0-9]+)';\n.*"
            withString:self.receivedData
          errorMessage:@"postMessage URL"];
  if (!result) {
    return;
  }
  self.postMessageUrl =
      [NSString stringWithFormat:@"%@%@",
          [self.receivedData substringWithRange:[result rangeAtIndex:1]],
          [self.receivedData substringWithRange:[result rangeAtIndex:2]]];
  [self maybeLogMessage:[NSString stringWithFormat:@"POST message URL\n%@",
                         self.postMessageUrl]];

  result = [self findMatch:@".*\n *var pc_config = (\\{[^\n]*\\});\n.*"
                withString:self.receivedData
              errorMessage:@"pc_config"];
  if (!result) {
    return;
  }
  NSString *pcConfig =
      [self.receivedData substringWithRange:[result rangeAtIndex:1]];
  [self maybeLogMessage:
          [NSString stringWithFormat:@"PC Config JSON\n%@", pcConfig]];

  result = [self findMatch:@".*\n *requestTurn\\('([^\n]*)'\\);\n.*"
                withString:self.receivedData
              errorMessage:@"channel token"];
  NSString *turnServerUrl;
  if (result) {
    turnServerUrl =
        [self.receivedData substringWithRange:[result rangeAtIndex:1]];
    [self maybeLogMessage:
            [NSString stringWithFormat:@"TURN server request URL\n%@",
                turnServerUrl]];
  }

  NSError *error;
  NSData *pcData = [pcConfig dataUsingEncoding:NSUTF8StringEncoding];
  NSDictionary *json =
      [NSJSONSerialization JSONObjectWithData:pcData options:0 error:&error];
  NSAssert(!error, @"Unable to parse.  %@", error.localizedDescription);
  NSArray *servers = [json objectForKey:@"iceServers"];
  NSMutableArray *iceServers = [NSMutableArray array];
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
    RTCIceServer *iceServer =
        [[RTCIceServer alloc] initWithUri:[NSURL URLWithString:url]
                                 password:credential];
    [iceServers addObject:iceServer];
  }
  [self updateIceServers:iceServers withTurnServer:turnServerUrl];

  [self maybeLogMessage:
          [NSString stringWithFormat:@"About to open GAE with token:  %@",
              self.token]];
  self.gaeChannel =
      [[GAEChannelClient alloc] initWithToken:self.token
                                     delegate:self.messageHandler];
}

@end
