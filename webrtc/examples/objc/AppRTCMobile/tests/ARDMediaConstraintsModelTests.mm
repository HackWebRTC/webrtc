/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <OCMock/OCMock.h>
#import "ARDMediaConstraintsModel+Private.h"
#import "ARDMediaConstraintsSettingsStore.h"
#import "WebRTC/RTCMediaConstraints.h"
#include "webrtc/base/gunit.h"


@interface ARDMediaConstraintsModelTests : NSObject {
  ARDMediaConstraintsModel *_model;
}

- (void)testStoringInavlidConstraintReturnsNo;
- (void)testDefaultMediaFromStore;
- (void)testWidthConstraintFromStore;
- (void)testHeightConstraintFromStore;

@end

@implementation ARDMediaConstraintsModelTests

- (instancetype)init {
  self = [super init];
  if (self) {
    _model = [[ARDMediaConstraintsModel alloc] init];
  }
  return self;
}

- (id)setupMockStoreWithMediaConstraintString:(NSString *)constraintString {
  id storeMock = [OCMockObject mockForClass:[ARDMediaConstraintsSettingsStore class]];
  [([[storeMock stub] andReturn:constraintString]) videoResolutionConstraintsSetting];

  id partialMock = [OCMockObject partialMockForObject:_model];
  [[[partialMock stub] andReturn:storeMock] settingsStore];

  return storeMock;
}

- (void)testDefaultMediaFromStore {
  // given
  id storeMock = [self setupMockStoreWithMediaConstraintString:nil];

  [[storeMock expect] setVideoResolutionConstraintsSetting:@"640x480"];

  // when
  NSString *string = [_model currentVideoResoultionConstraintFromStore];

  // then
  EXPECT_TRUE([string isEqualToString:@"640x480"]);
  [storeMock verify];
}

- (void)testStoringInavlidConstraintReturnsNo {
  // given
  id storeMock = [self setupMockStoreWithMediaConstraintString:@"960x480"];

  // when
  BOOL result = [_model storeVideoResoultionConstraint:@"960x480"];

  // then
  EXPECT_TRUE(result);
}

- (void)testWidthConstraintFromStore {
  // given
  [self setupMockStoreWithMediaConstraintString:@"1270x480"];

  // when
  NSString *width = [_model currentVideoResolutionWidthFromStore];

  // then
  EXPECT_TRUE([width isEqualToString:@"1270"]);
}

- (void)testHeightConstraintFromStore {
  // given
  [self setupMockStoreWithMediaConstraintString:@"960x540"];
  // when
  NSString *height = [_model currentVideoResolutionHeightFromStore];

  // then
  EXPECT_TRUE([height isEqualToString:@"540"]);
}

- (void)testConstraintComponentIsNilWhenInvalidConstraintString {
  // given
  [self setupMockStoreWithMediaConstraintString:@"invalid"];

  // when
  NSString *width = [_model currentVideoResolutionWidthFromStore];

  // then
  EXPECT_TRUE(width == nil);
}

- (void)testConstraintsDictionaryIsNilWhenInvalidConstraintString {
  // given
  [self setupMockStoreWithMediaConstraintString:@"invalid"];

  // when
  NSDictionary *constraintsDictionary = [_model currentMediaConstraintFromStoreAsRTCDictionary];

  // then
  EXPECT_TRUE(constraintsDictionary == nil);
}
@end

class ARDMediaConstraintsModelTest : public ::testing::Test {
 protected:
  ARDMediaConstraintsModelTests *test;
  ARDMediaConstraintsModelTest() { test = [[ARDMediaConstraintsModelTests alloc] init]; }
};

TEST_F(ARDMediaConstraintsModelTest, DefaultMediaFromStore) {
  @autoreleasepool {
    [test testDefaultMediaFromStore];
  }
}

TEST_F(ARDMediaConstraintsModelTest, StoringInvalidConstraintsReturnsNo) {
  @autoreleasepool {
    [test testStoringInavlidConstraintReturnsNo];
  }
}

TEST_F(ARDMediaConstraintsModelTest, WidthConstraintFromStore) {
  @autoreleasepool {
    [test testWidthConstraintFromStore];
  }
}

TEST_F(ARDMediaConstraintsModelTest, HeightConstraintFromStore) {
  @autoreleasepool {
    [test testHeightConstraintFromStore];
  }
}

TEST_F(ARDMediaConstraintsModelTest, ConstratintIsNil) {
  @autoreleasepool {
    [test testConstraintComponentIsNilWhenInvalidConstraintString];
  }
}

TEST_F(ARDMediaConstraintsModelTest, DictionaryIsNil) {
  @autoreleasepool {
    [test testConstraintsDictionaryIsNilWhenInvalidConstraintString];
  }
}
