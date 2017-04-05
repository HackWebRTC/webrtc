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
#import <XCTest/XCTest.h>

#import "WebRTC/RTCMediaConstraints.h"

#import "ARDSettingsModel+Private.h"
#import "ARDSettingsStore.h"


@interface ARDSettingsModelTests : XCTestCase {
  ARDSettingsModel *_model;
}
@end

@implementation ARDSettingsModelTests

- (id)setupMockStoreWithVideoResolution:(NSString *)videoResolution {
  id storeMock = [OCMockObject mockForClass:[ARDSettingsStore class]];
  [([[storeMock stub] andReturn:videoResolution])videoResolution];

  id partialMock = [OCMockObject partialMockForObject:_model];
  [[[partialMock stub] andReturn:storeMock] settingsStore];

  return storeMock;
}

- (void)setUp {
  _model = [[ARDSettingsModel alloc] init];
}

- (void)testDefaultMediaFromStore {
  id storeMock = [self setupMockStoreWithVideoResolution:nil];
  [[storeMock expect] setVideoResolution:@"640x480"];

  NSString *string = [_model currentVideoResolutionSettingFromStore];

  XCTAssertEqualObjects(string, @"640x480");
  [storeMock verify];
}

- (void)testStoringInvalidConstraintReturnsNo {
  __unused id storeMock = [self setupMockStoreWithVideoResolution:@"960x480"];
  XCTAssertFalse([_model storeVideoResolutionSetting:@"960x480"]);
}

- (void)testWidthConstraintFromStore {
  [self setupMockStoreWithVideoResolution:@"1270x480"];
  int width = [_model currentVideoResolutionWidthFromStore];

  XCTAssertEqual(width, 1270);
}

- (void)testHeightConstraintFromStore {
  [self setupMockStoreWithVideoResolution:@"960x540"];
  int height = [_model currentVideoResolutionHeightFromStore];

  XCTAssertEqual(height, 540);
}

- (void)testConstraintComponentIsNilWhenInvalidConstraintString {
  [self setupMockStoreWithVideoResolution:@"invalid"];
  int width = [_model currentVideoResolutionWidthFromStore];

  XCTAssertEqual(width, 0);
}
@end
