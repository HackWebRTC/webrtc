/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "CFCaptureController.h"

#import "base/RTCLogging.h"
#import "components/capturer/RTCCameraVideoCapturer.h"

const Float64 kFramerateLimit = 30.0;

// stay sync with ConfConfig.kt
#define CAMERA_FACE_FRONT 0
#define CAMERA_FACE_BACK 1

@implementation CFCaptureController {
  RTCCameraVideoCapturer *_capturer;
  int32_t _width;
  int32_t _height;
  BOOL _usingFrontCamera;
}

- (instancetype)initWithCapturer:(RTCCameraVideoCapturer *)capturer
                      cameraFace:(int32_t)cameraFace
                           width:(int32_t)width
                          height:(int32_t)height {
    if ((self = [super init])) {
        _capturer = capturer;
        _width = width;
        _height = height;
        _usingFrontCamera = cameraFace == CAMERA_FACE_FRONT;
    }

    return self;
}

- (void)startCapture:(nullable void (^)(NSError *))completionHandler {
  AVCaptureDevicePosition position =
      _usingFrontCamera ? AVCaptureDevicePositionFront : AVCaptureDevicePositionBack;
  AVCaptureDevice *device = [self findDeviceForPosition:position];
  AVCaptureDeviceFormat *format = [self selectFormatForDevice:device];

  if (format == nil) {
    RTCLogError(@"No valid formats for device %@", device);
    NSAssert(NO, @"");

    return;
  }

  NSInteger fps = [self selectFpsForFormat:format];

  [_capturer startCaptureWithDevice:device format:format fps:fps completionHandler:completionHandler];
}

- (void)stopCapture {
  [_capturer stopCapture];
}

- (void)switchCamera:(nullable void (^)(NSError *))completionHandler {
  _usingFrontCamera = !_usingFrontCamera;
  [self startCapture:completionHandler];
}

#pragma mark - Private

- (AVCaptureDevice *)findDeviceForPosition:(AVCaptureDevicePosition)position {
  NSArray<AVCaptureDevice *> *captureDevices = [RTCCameraVideoCapturer captureDevices];
  for (AVCaptureDevice *device in captureDevices) {
    if (device.position == position) {
      return device;
    }
  }
  return captureDevices[0];
}

- (AVCaptureDeviceFormat *)selectFormatForDevice:(AVCaptureDevice *)device {
  NSArray<AVCaptureDeviceFormat *> *formats =
      [RTCCameraVideoCapturer supportedFormatsForDevice:device];
  AVCaptureDeviceFormat *selectedFormat = nil;
  int currentDiff = INT_MAX;

  for (AVCaptureDeviceFormat *format in formats) {
    CMVideoDimensions dimension = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    FourCharCode pixelFormat = CMFormatDescriptionGetMediaSubType(format.formatDescription);
    int diff = abs(_width - dimension.width) + abs(_height - dimension.height);
    if (diff < currentDiff) {
      selectedFormat = format;
      currentDiff = diff;
    } else if (diff == currentDiff && pixelFormat == [_capturer preferredOutputPixelFormat]) {
      selectedFormat = format;
    }
  }

  return selectedFormat;
}

- (NSInteger)selectFpsForFormat:(AVCaptureDeviceFormat *)format {
  Float64 maxSupportedFramerate = 0;
  for (AVFrameRateRange *fpsRange in format.videoSupportedFrameRateRanges) {
    maxSupportedFramerate = fmax(maxSupportedFramerate, fpsRange.maxFrameRate);
  }
  return fmin(maxSupportedFramerate, kFramerateLimit);
}

@end
