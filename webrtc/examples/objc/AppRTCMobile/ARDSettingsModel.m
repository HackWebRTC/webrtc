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
#import "WebRTC/RTCCameraVideoCapturer.h"
#import "WebRTC/RTCMediaConstraints.h"

NS_ASSUME_NONNULL_BEGIN

static NSArray<NSString *> *videoCodecsStaticValues() {
  return @[ @"H264", @"VP8", @"VP9" ];
}

@interface ARDSettingsModel () {
  ARDSettingsStore *_settingsStore;
}
@end

@implementation ARDSettingsModel

- (NSArray<NSString *> *)availableVideoResolutions {
  NSMutableSet<NSArray<NSNumber *> *> *resolutions =
      [[NSMutableSet<NSArray<NSNumber *> *> alloc] init];
  for (AVCaptureDevice *device in [RTCCameraVideoCapturer captureDevices]) {
    for (AVCaptureDeviceFormat *format in
         [RTCCameraVideoCapturer supportedFormatsForDevice:device]) {
      CMVideoDimensions resolution =
          CMVideoFormatDescriptionGetDimensions(format.formatDescription);
      NSArray<NSNumber *> *resolutionObject = @[ @(resolution.width), @(resolution.height) ];
      [resolutions addObject:resolutionObject];
    }
  }

  NSArray<NSArray<NSNumber *> *> *sortedResolutions =
      [[resolutions allObjects] sortedArrayUsingComparator:^NSComparisonResult(
                                    NSArray<NSNumber *> *obj1, NSArray<NSNumber *> *obj2) {
        return obj1.firstObject > obj2.firstObject;
      }];

  NSMutableArray<NSString *> *resolutionStrings = [[NSMutableArray<NSString *> alloc] init];
  for (NSArray<NSNumber *> *resolution in sortedResolutions) {
    NSString *resolutionString =
        [NSString stringWithFormat:@"%@x%@", resolution.firstObject, resolution.lastObject];
    [resolutionStrings addObject:resolutionString];
  }

  return [resolutionStrings copy];
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
  return [self availableVideoResolutions][0];
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
