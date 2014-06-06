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

#if TARGET_OS_IPHONE

#import <UIKit/UIKit.h>

@interface GAEChannelClient () <UIWebViewDelegate>

@property(nonatomic, strong) UIWebView* webView;

#else

#import <WebKit/WebKit.h>

@interface GAEChannelClient ()

@property(nonatomic, strong) WebView* webView;

#endif

@end

@implementation GAEChannelClient

- (instancetype)initWithToken:(NSString*)token
                     delegate:(id<GAEMessageHandler>)delegate {
  NSParameterAssert([token length] > 0);
  NSParameterAssert(delegate);
  self = [super init];
  if (self) {
#if TARGET_OS_IPHONE
    _webView = [[UIWebView alloc] init];
    _webView.delegate = self;
#else
    _webView = [[WebView alloc] init];
    _webView.policyDelegate = self;
#endif
    _delegate = delegate;
    NSString* htmlPath =
        [[NSBundle mainBundle] pathForResource:@"channel" ofType:@"html"];
    NSURL* htmlUrl = [NSURL fileURLWithPath:htmlPath];
    NSString* path = [NSString
        stringWithFormat:@"%@?token=%@", [htmlUrl absoluteString], token];
#if TARGET_OS_IPHONE
    [_webView
#else
    [[_webView mainFrame]
#endif
        loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:path]]];
  }
  return self;
}

- (void)dealloc {
#if TARGET_OS_IPHONE
  _webView.delegate = nil;
  [_webView stopLoading];
#else
  _webView.policyDelegate = nil;
  [[_webView mainFrame] stopLoading];
#endif
}

#if TARGET_OS_IPHONE
#pragma mark - UIWebViewDelegate

- (BOOL)webView:(UIWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request
                navigationType:(UIWebViewNavigationType)navigationType {
#else
// WebPolicyDelegate is an informal delegate.
#pragma mark - WebPolicyDelegate

- (void)webView:(WebView*)webView
    decidePolicyForNavigationAction:(NSDictionary*)actionInformation
                            request:(NSURLRequest*)request
                              frame:(WebFrame*)frame
                   decisionListener:(id<WebPolicyDecisionListener>)listener {
#endif
  NSString* scheme = [request.URL scheme];
  NSAssert(scheme, @"scheme is nil: %@", request);
  if (![scheme isEqualToString:@"js-frame"]) {
#if TARGET_OS_IPHONE
    return YES;
#else
    [listener use];
    return;
#endif
  }
  dispatch_async(dispatch_get_main_queue(), ^{
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
#if TARGET_OS_IPHONE
  return NO;
#else
  [listener ignore];
  return;
#endif
}

#pragma mark - Private

+ (NSDictionary*)jsonStringToDictionary:(NSString*)str {
  NSData* data = [str dataUsingEncoding:NSUTF8StringEncoding];
  NSError* error;
  NSDictionary* dict =
      [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
  NSAssert(!error, @"Invalid JSON? %@", str);
  return dict;
}

@end
