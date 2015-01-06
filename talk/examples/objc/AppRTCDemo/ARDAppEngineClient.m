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

#import "ARDAppEngineClient.h"

#import "ARDMessageResponse.h"
#import "ARDRegisterResponse.h"
#import "ARDSignalingMessage.h"
#import "ARDUtilities.h"

// TODO(tkchin): move these to a configuration object.
static NSString *kARDRoomServerHostUrl =
    @"https://apprtc.appspot.com";
static NSString *kARDRoomServerRegisterFormat =
    @"https://apprtc.appspot.com/register/%@";
static NSString *kARDRoomServerMessageFormat =
    @"https://apprtc.appspot.com/message/%@/%@";
static NSString *kARDRoomServerByeFormat =
    @"https://apprtc.appspot.com/bye/%@/%@";

static NSString *kARDAppEngineClientErrorDomain = @"ARDAppEngineClient";
static NSInteger kARDAppEngineClientErrorBadResponse = -1;

@implementation ARDAppEngineClient

#pragma mark - ARDRoomServerClient

- (void)registerForRoomId:(NSString *)roomId
    completionHandler:(void (^)(ARDRegisterResponse *response,
                                NSError *error))completionHandler {
  NSParameterAssert(roomId.length);

  NSString *urlString =
      [NSString stringWithFormat:kARDRoomServerRegisterFormat, roomId];
  NSURL *roomURL = [NSURL URLWithString:urlString];
  NSLog(@"Registering with room server.");
  NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:roomURL];
  request.HTTPMethod = @"POST";
  __weak ARDAppEngineClient *weakSelf = self;
  [NSURLConnection sendAsyncRequest:request
                  completionHandler:^(NSURLResponse *response,
                                      NSData *data,
                                      NSError *error) {
    ARDAppEngineClient *strongSelf = weakSelf;
    if (error) {
      if (completionHandler) {
        completionHandler(nil, error);
      }
      return;
    }
    ARDRegisterResponse *registerResponse =
        [ARDRegisterResponse responseFromJSONData:data];
    if (!registerResponse) {
      if (completionHandler) {
        NSError *error = [[self class] badResponseError];
        completionHandler(nil, error);
      }
      return;
    }
    if (completionHandler) {
      completionHandler(registerResponse, nil);
    }
  }];
}

- (void)sendMessage:(ARDSignalingMessage *)message
            forRoomId:(NSString *)roomId
             clientId:(NSString *)clientId
    completionHandler:(void (^)(ARDMessageResponse *response,
                                NSError *error))completionHandler {
  NSParameterAssert(message);
  NSParameterAssert(roomId.length);
  NSParameterAssert(clientId.length);

  NSData *data = [message JSONData];
  NSString *urlString =
      [NSString stringWithFormat:
          kARDRoomServerMessageFormat, roomId, clientId];
  NSURL *url = [NSURL URLWithString:urlString];
  NSLog(@"C->RS POST: %@", message);
  NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
  request.HTTPMethod = @"POST";
  request.HTTPBody = data;
  __weak ARDAppEngineClient *weakSelf = self;
  [NSURLConnection sendAsyncRequest:request
                  completionHandler:^(NSURLResponse *response,
                                      NSData *data,
                                      NSError *error) {
    ARDAppEngineClient *strongSelf = weakSelf;
    if (error) {
      if (completionHandler) {
        completionHandler(nil, error);
      }
      return;
    }
    ARDMessageResponse *messageResponse =
        [ARDMessageResponse responseFromJSONData:data];
    if (!messageResponse) {
      if (completionHandler) {
        NSError *error = [[self class] badResponseError];
        completionHandler(nil, error);
      }
      return;
    }
    if (completionHandler) {
      completionHandler(messageResponse, nil);
    }
  }];
}

- (void)deregisterForRoomId:(NSString *)roomId
                   clientId:(NSString *)clientId
          completionHandler:(void (^)(NSError *error))completionHandler {
  NSParameterAssert(roomId.length);
  NSParameterAssert(clientId.length);

  NSString *urlString =
      [NSString stringWithFormat:kARDRoomServerByeFormat, roomId, clientId];
  NSURL *url = [NSURL URLWithString:urlString];
  NSURLRequest *request = [NSURLRequest requestWithURL:url];
  NSURLResponse *response = nil;
  NSError *error = nil;
  // We want a synchronous request so that we know that we're unregistered from
  // room server before we do any further unregistration.
  NSLog(@"C->RS: BYE");
  [NSURLConnection sendSynchronousRequest:request
                        returningResponse:&response
                                    error:&error];
  if (error) {
    NSLog(@"Error unregistering from room server: %@", error);
    if (completionHandler) {
      completionHandler(error);
    }
    return;
  }
  NSLog(@"Unregistered from room server.");
  if (completionHandler) {
    completionHandler(nil);
  }
}

#pragma mark - Private

+ (NSError *)badResponseError {
  NSError *error =
      [[NSError alloc] initWithDomain:kARDAppEngineClientErrorDomain
                                 code:kARDAppEngineClientErrorBadResponse
                             userInfo:@{
    NSLocalizedDescriptionKey: @"Error parsing response.",
  }];
  return error;
}

@end
