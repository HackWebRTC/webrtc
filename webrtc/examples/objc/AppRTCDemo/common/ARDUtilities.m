/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDUtilities.h"

#import "RTCLogging.h"

@implementation NSDictionary (ARDUtilites)

+ (NSDictionary *)dictionaryWithJSONString:(NSString *)jsonString {
  NSParameterAssert(jsonString.length > 0);
  NSData *data = [jsonString dataUsingEncoding:NSUTF8StringEncoding];
  NSError *error = nil;
  NSDictionary *dict =
      [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
  if (error) {
    RTCLogError(@"Error parsing JSON: %@", error.localizedDescription);
  }
  return dict;
}

+ (NSDictionary *)dictionaryWithJSONData:(NSData *)jsonData {
  NSError *error = nil;
  NSDictionary *dict =
      [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:&error];
  if (error) {
    RTCLogError(@"Error parsing JSON: %@", error.localizedDescription);
  }
  return dict;
}

@end

@implementation NSURLConnection (ARDUtilities)

+ (void)sendAsyncRequest:(NSURLRequest *)request
       completionHandler:(void (^)(NSURLResponse *response,
                                   NSData *data,
                                   NSError *error))completionHandler {
  // Kick off an async request which will call back on main thread.
  [NSURLConnection sendAsynchronousRequest:request
                                     queue:[NSOperationQueue mainQueue]
                         completionHandler:^(NSURLResponse *response,
                                             NSData *data,
                                             NSError *error) {
    if (completionHandler) {
      completionHandler(response, data, error);
    }
  }];
}

// Posts data to the specified URL.
+ (void)sendAsyncPostToURL:(NSURL *)url
                  withData:(NSData *)data
         completionHandler:(void (^)(BOOL succeeded,
                                     NSData *data))completionHandler {
  NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
  request.HTTPMethod = @"POST";
  request.HTTPBody = data;
  [[self class] sendAsyncRequest:request
                completionHandler:^(NSURLResponse *response,
                                    NSData *data,
                                    NSError *error) {
    if (error) {
      RTCLogError(@"Error posting data: %@", error.localizedDescription);
      if (completionHandler) {
        completionHandler(NO, data);
      }
      return;
    }
    NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
    if (httpResponse.statusCode != 200) {
      NSString *serverResponse = data.length > 0 ?
          [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] :
          nil;
      RTCLogError(@"Received bad response: %@", serverResponse);
      if (completionHandler) {
        completionHandler(NO, data);
      }
      return;
    }
    if (completionHandler) {
      completionHandler(YES, data);
    }
  }];
}

@end
