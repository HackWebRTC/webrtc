/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDSettingsModel+Private.h"
#import "ARDSettingsStore.h"
#import "WebRTC/RTCMediaConstraints.h"

NS_ASSUME_NONNULL_BEGIN
static NSArray<NSString *> *videoResolutionsStaticValues() {
  return @[ @"640x480", @"960x540", @"1280x720" ];
}

static NSArray<NSString *> *videoCodecsStaticValues() {
  return @[ @"H264", @"VP8", @"VP9" ];
}

@interface ARDSettingsModel () {
  ARDSettingsStore *_settingsStore;
}
@end

@implementation ARDSettingsModel

- (NSArray<NSString *> *)availableVideoResolutions {
  return videoResolutionsStaticValues();
}

- (NSString *)currentVideoResolutionSettingFromStore {
  NSString *resolution = [[self settingsStore] videoResolution];
  if (!resolution) {
    resolution = [self defaultVideoResolutionSetting];
    // To ensure consistency add the default to the store.
    [[self settingsStore] setVideoResolution:resolution];
  }
  return resolution;
}

- (BOOL)storeVideoResolutionSetting:(NSString *)resolution {
  if (![[self availableVideoResolutions] containsObject:resolution]) {
    return NO;
  }
  [[self settingsStore] setVideoResolution:resolution];
  return YES;
}

- (NSArray<NSString *> *)availableVideoCodecs {
  return videoCodecsStaticValues();
}

- (NSString *)currentVideoCodecSettingFromStore {
  NSString *videoCodec = [[self settingsStore] videoCodec];
  if (!videoCodec) {
    videoCodec = [self defaultVideoCodecSetting];
    [[self settingsStore] setVideoCodec:videoCodec];
  }
  return videoCodec;
}

- (BOOL)storeVideoCodecSetting:(NSString *)videoCodec {
  if (![[self availableVideoCodecs] containsObject:videoCodec]) {
    return NO;
  }
  [[self settingsStore] setVideoCodec:videoCodec];
  return YES;
}

- (nullable NSNumber *)currentMaxBitrateSettingFromStore {
  return [[self settingsStore] maxBitrate];
}

- (void)storeMaxBitrateSetting:(nullable NSNumber *)bitrate {
  [[self settingsStore] setMaxBitrate:bitrate];
}

#pragma mark - Testable

- (ARDSettingsStore *)settingsStore {
  if (!_settingsStore) {
    _settingsStore = [[ARDSettingsStore alloc] init];
  }
  return _settingsStore;
}

- (int)currentVideoResolutionWidthFromStore {
  NSString *resolution = [self currentVideoResolutionSettingFromStore];

  return [self videoResolutionComponentAtIndex:0 inString:resolution];
}

- (int)currentVideoResolutionHeightFromStore {
  NSString *resolution = [self currentVideoResolutionSettingFromStore];
  return [self videoResolutionComponentAtIndex:1 inString:resolution];
}

#pragma mark -

- (NSString *)defaultVideoResolutionSetting {
  return videoResolutionsStaticValues()[0];
}

- (int)videoResolutionComponentAtIndex:(int)index inString:(NSString *)resolution {
  if (index != 0 && index != 1) {
    return 0;
  }
  NSArray<NSString *> *components = [resolution componentsSeparatedByString:@"x"];
  if (components.count != 2) {
    return 0;
  }
  return components[index].intValue;
}

- (NSString *)defaultVideoCodecSetting {
  return videoCodecsStaticValues()[0];
}

@end
NS_ASSUME_NONNULL_END
