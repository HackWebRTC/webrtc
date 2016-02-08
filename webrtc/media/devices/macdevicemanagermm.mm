/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// support GCC compiler
#ifndef __has_feature
#define __has_feature(x) 0
#endif

#include "webrtc/media/devices/devicemanager.h"

#import <assert.h>
#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
  #import <AVFoundation/AVFoundation.h>
#endif
#endif
#import <QTKit/QTKit.h>

#include "webrtc/base/logging.h"

@interface DeviceWatcherImpl : NSObject {
 @private
  cricket::DeviceManagerInterface* manager_;
}
- (id)init:(cricket::DeviceManagerInterface*)manager;
- (void)onDevicesChanged:(NSNotification*)notification;
@end

@implementation DeviceWatcherImpl
- (id)init:(cricket::DeviceManagerInterface*)manager {
  if ((self = [super init])) {
    assert(manager != NULL);
    manager_ = manager;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onDevicesChanged:)
               name:QTCaptureDeviceWasConnectedNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onDevicesChanged:)
               name:QTCaptureDeviceWasDisconnectedNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
#if !__has_feature(objc_arc)
  [super dealloc];
#endif
}
- (void)onDevicesChanged:(NSNotification*)notification {
  manager_->SignalDevicesChange();
}
@end

namespace cricket {

DeviceWatcherImpl* CreateDeviceWatcherCallback(
    DeviceManagerInterface* manager) {
  DeviceWatcherImpl* impl;
#if !__has_feature(objc_arc)
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
#else
  @autoreleasepool
#endif
  { impl = [[DeviceWatcherImpl alloc] init:manager]; }
#if !__has_feature(objc_arc)
  [pool drain];
#endif
  return impl;
}

void ReleaseDeviceWatcherCallback(DeviceWatcherImpl* watcher) {
#if !__has_feature(objc_arc)
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  [watcher release];
  [pool drain];
#endif
}

bool GetQTKitVideoDevices(std::vector<Device>* devices) {
#if !__has_feature(objc_arc)
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
#else
  @autoreleasepool
#endif
  {
    NSArray* qt_capture_devices =
        [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
    NSUInteger count = [qt_capture_devices count];
    LOG(LS_INFO) << count << " capture device(s) found:";
    for (QTCaptureDevice* qt_capture_device in qt_capture_devices) {
      static NSString* const kFormat = @"localizedDisplayName: \"%@\", "
          @"modelUniqueID: \"%@\", uniqueID \"%@\", isConnected: %d, "
          @"isOpen: %d, isInUseByAnotherApplication: %d";
      NSString* info = [NSString
          stringWithFormat:kFormat,
                           [qt_capture_device localizedDisplayName],
                           [qt_capture_device modelUniqueID],
                           [qt_capture_device uniqueID],
                           [qt_capture_device isConnected],
                           [qt_capture_device isOpen],
                           [qt_capture_device isInUseByAnotherApplication]];
      LOG(LS_INFO) << [info UTF8String];

      std::string name([[qt_capture_device localizedDisplayName] UTF8String]);
      devices->push_back(
          Device(name, [[qt_capture_device uniqueID] UTF8String]));
    }
  }
#if !__has_feature(objc_arc)
  [pool drain];
#endif
  return true;
}

bool GetAVFoundationVideoDevices(std::vector<Device>* devices) {
#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED
#if __MAC_OS_X_VERSION_MAX_ALLOWED >=1070
  if (![AVCaptureDevice class]) {
    // Fallback to using QTKit if AVFoundation is not available
    return GetQTKitVideoDevices(devices);
  }
#if !__has_feature(objc_arc)
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
#else
  @autoreleasepool
#endif
  {
    NSArray* capture_devices = [AVCaptureDevice devices];
    LOG(LS_INFO) << [capture_devices count] << " capture device(s) found:";
    for (AVCaptureDevice* capture_device in capture_devices) {
      if ([capture_device hasMediaType:AVMediaTypeVideo] ||
          [capture_device hasMediaType:AVMediaTypeMuxed]) {
        static NSString* const kFormat = @"localizedName: \"%@\", "
            @"modelID: \"%@\", uniqueID \"%@\", isConnected: %d, "
            @"isInUseByAnotherApplication: %d";
        NSString* info = [NSString
            stringWithFormat:kFormat,
                             [capture_device localizedName],
                             [capture_device modelID],
                             [capture_device uniqueID],
                             [capture_device isConnected],
                             [capture_device isInUseByAnotherApplication]];
        LOG(LS_INFO) << [info UTF8String];

        std::string name([[capture_device localizedName] UTF8String]);
        devices->push_back(
            Device(name, [[capture_device uniqueID] UTF8String]));
      }
    }
  }
#if !__has_feature(objc_arc)
  [pool drain];
#endif
  return true;
#else  // __MAC_OS_X_VERSION_MAX_ALLOWED >=1070
  return GetQTKitVideoDevices(devices);
#endif  // __MAC_OS_X_VERSION_MAX_ALLOWED >=1070
#else  // __MAC_OS_X_VERSION_MAX_ALLOWED
  return GetQTKitVideoDevices(devices);
#endif  // __MAC_OS_X_VERSION_MAX_ALLOWED
}

}  // namespace cricket
