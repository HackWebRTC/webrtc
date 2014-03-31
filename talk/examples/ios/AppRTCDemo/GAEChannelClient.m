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

#import "GAEChannelClient.h"

#import "RTCPeerConnectionFactory.h"

@interface GAEChannelClient ()

@property(nonatomic, assign) id<GAEMessageHandler> delegate;
@property(nonatomic, strong) UIWebView* webView;

@end

@implementation GAEChannelClient

- (id)initWithToken:(NSString*)token delegate:(id<GAEMessageHandler>)delegate {
  self = [super init];
  if (self) {
    _webView = [[UIWebView alloc] init];
    _webView.delegate = self;
    _delegate = delegate;
    NSString* htmlPath =
        [[NSBundle mainBundle] pathForResource:@"ios_channel" ofType:@"html"];
    NSURL* htmlUrl = [NSURL fileURLWithPath:htmlPath];
    NSString* path = [NSString
        stringWithFormat:@"%@?token=%@", [htmlUrl absoluteString], token];

    [_webView
        loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:path]]];
  }
  return self;
}

- (void)dealloc {
  _webView.delegate = nil;
  [_webView stopLoading];
}

#pragma mark - UIWebViewDelegate method

+ (NSDictionary*)jsonStringToDictionary:(NSString*)str {
  NSData* data = [str dataUsingEncoding:NSUTF8StringEncoding];
  NSError* error;
  NSDictionary* dict =
      [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
  NSAssert(!error, @"Invalid JSON? %@", str);
  return dict;
}

- (BOOL)webView:(UIWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request
                navigationType:(UIWebViewNavigationType)navigationType {
  NSString* scheme = [request.URL scheme];
  NSAssert(scheme, @"scheme is nil: %@", request);
  if (![scheme isEqualToString:@"js-frame"]) {
    return YES;
  }

  dispatch_async(dispatch_get_main_queue(), ^(void) {
      NSString* queuedMessage = [webView
          stringByEvaluatingJavaScriptFromString:@"popQueuedMessage();"];
      NSAssert([queuedMessage length], @"Empty queued message from JS");

      NSDictionary* queuedMessageDict =
          [GAEChannelClient jsonStringToDictionary:queuedMessage];
      NSString* method = queuedMessageDict[@"type"];
      NSAssert(method, @"Missing method: %@", queuedMessageDict);
      NSDictionary* payload = queuedMessageDict[@"payload"];  // May be nil.

      if ([method isEqualToString:@"onopen"]) {
        [self.delegate onOpen];
      } else if ([method isEqualToString:@"onmessage"]) {
        NSDictionary* payloadData =
            [GAEChannelClient jsonStringToDictionary:payload[@"data"]];
        [self.delegate onMessage:payloadData];
      } else if ([method isEqualToString:@"onclose"]) {
        [self.delegate onClose];
      } else if ([method isEqualToString:@"onerror"]) {
        NSNumber* codeNumber = payload[@"code"];
        int code = [codeNumber intValue];
        NSAssert([codeNumber isEqualToNumber:[NSNumber numberWithInt:code]],
                 @"Unexpected non-integral code: %@", payload);
        [self.delegate onError:code withDescription:payload[@"description"]];
      } else {
        NSAssert(NO, @"Invalid message sent from UIWebView: %@", queuedMessage);
      }
  });
  return NO;
}

@end
