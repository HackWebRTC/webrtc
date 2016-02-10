/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#import <Foundation/Foundation.h>

#include "webrtc/base/gunit.h"

#import "webrtc/api/objc/RTCMediaConstraints.h"
#import "webrtc/api/objc/RTCMediaConstraints+Private.h"
#import "webrtc/base/objc/NSString+StdString.h"

@interface RTCMediaConstraintsTest : NSObject
- (void)testMediaConstraints;
@end

@implementation RTCMediaConstraintsTest

- (void)testMediaConstraints {
  NSDictionary *mandatory = @{@"key1": @"value1", @"key2": @"value2"};
  NSDictionary *optional = @{@"key3": @"value3", @"key4": @"value4"};

  RTCMediaConstraints *constraints = [[RTCMediaConstraints alloc]
      initWithMandatoryConstraints:mandatory
               optionalConstraints:optional];
  rtc::scoped_ptr<webrtc::MediaConstraints> nativeConstraints =
      [constraints nativeConstraints];

  webrtc::MediaConstraintsInterface::Constraints nativeMandatory =
      nativeConstraints->GetMandatory();
  [self expectConstraints:mandatory inNativeConstraints:nativeMandatory];

  webrtc::MediaConstraintsInterface::Constraints nativeOptional =
      nativeConstraints->GetOptional();
  [self expectConstraints:optional inNativeConstraints:nativeOptional];
}

- (void)expectConstraints:(NSDictionary *)constraints
      inNativeConstraints:
    (webrtc::MediaConstraintsInterface::Constraints)nativeConstraints {
  EXPECT_EQ(constraints.count, nativeConstraints.size());

  for (NSString *key in constraints) {
    NSString *value = constraints[key];

    std::string nativeValue;
    bool found = nativeConstraints.FindFirst(key.stdString, &nativeValue);
    EXPECT_TRUE(found);
    EXPECT_EQ(value.stdString, nativeValue);
  }
}

@end

TEST(RTCMediaConstraintsTest, MediaConstraintsTest) {
  @autoreleasepool {
    RTCMediaConstraintsTest *test = [[RTCMediaConstraintsTest alloc] init];
    [test testMediaConstraints];
  }
}
