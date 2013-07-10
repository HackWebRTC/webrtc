/*
 * libjingle
 * Copyright 2004--2010, Google Inc.
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

// support GCC compiler
#ifndef __has_feature
#  define __has_feature(x) 0
#endif

#include "talk/media/devices/devicemanager.h"

#import <assert.h>
#import <QTKit/QTKit.h>

#include "talk/base/logging.h"

@interface DeviceWatcherImpl : NSObject {
 @private
  cricket::DeviceManagerInterface* manager_;
}
- (id)init:(cricket::DeviceManagerInterface*)manager;
- (void)onDevicesChanged:(NSNotification *)notification;
@end

@implementation DeviceWatcherImpl
- (id)init:(cricket::DeviceManagerInterface*)manager {
  if ((self = [super init])) {
    assert(manager != NULL);
    manager_ = manager;
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(onDevicesChanged:)
        name:QTCaptureDeviceWasConnectedNotification
        object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
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
- (void)onDevicesChanged:(NSNotification *)notification {
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
  {
    impl = [[DeviceWatcherImpl alloc] init:manager];
  }
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
      NSString* info = [NSString stringWithFormat:kFormat,
          [qt_capture_device localizedDisplayName],
          [qt_capture_device modelUniqueID],
          [qt_capture_device uniqueID],
          [qt_capture_device isConnected],
          [qt_capture_device isOpen],
          [qt_capture_device isInUseByAnotherApplication]];
      LOG(LS_INFO) << [info UTF8String];

      std::string name([[qt_capture_device localizedDisplayName]
                           UTF8String]);
      devices->push_back(Device(name,
         [[qt_capture_device uniqueID]
             UTF8String]));
    }
  }
#if !__has_feature(objc_arc)
  [pool drain];
#endif
  return true;
}

}  // namespace cricket
