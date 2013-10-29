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
@property(nonatomic, strong) UIWebView *webView;

@end

@implementation GAEChannelClient

@synthesize delegate = _delegate;
@synthesize webView = _webView;

- (id)initWithToken:(NSString *)token delegate:(id<GAEMessageHandler>)delegate {
  self = [super init];
  if (self) {
    _webView = [[UIWebView alloc] init];
    _webView.delegate = self;
    _delegate = delegate;
    NSString *htmlPath =
        [[NSBundle mainBundle] pathForResource:@"ios_channel" ofType:@"html"];
    NSURL *htmlUrl = [NSURL fileURLWithPath:htmlPath];
    NSString *path = [NSString stringWithFormat:@"%@?token=%@",
                      [htmlUrl absoluteString],
                      token];

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

- (BOOL)webView:(UIWebView *)webView
    shouldStartLoadWithRequest:(NSURLRequest *)request
                navigationType:(UIWebViewNavigationType)navigationType {
  NSString *scheme = [request.URL scheme];
  if ([scheme compare:@"js-frame"] != NSOrderedSame) {
    return YES;
  }
  NSString *resourceSpecifier = [request.URL resourceSpecifier];
  NSRange range = [resourceSpecifier rangeOfString:@":"];
  NSString *method;
  NSString *message;
  if (range.length == 0 && range.location == NSNotFound) {
    method = resourceSpecifier;
  } else {
    method = [resourceSpecifier substringToIndex:range.location];
    message = [resourceSpecifier substringFromIndex:range.location + 1];
  }
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    if ([method compare:@"onopen"] == NSOrderedSame) {
      [self.delegate onOpen];
    } else if ([method compare:@"onmessage"] == NSOrderedSame) {
      [self.delegate onMessage:message];
    } else if ([method compare:@"onclose"] == NSOrderedSame) {
      [self.delegate onClose];
    } else if ([method compare:@"onerror"] == NSOrderedSame) {
      // TODO(hughv): Get error.
      int code = -1;
      NSString *description = message;
      [self.delegate onError:code withDescription:description];
    } else {
      NSAssert(NO, @"Invalid message sent from UIWebView: %@",
               resourceSpecifier);
    }
  });
  return YES;
}

@end
