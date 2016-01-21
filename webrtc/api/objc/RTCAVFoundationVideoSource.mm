/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAVFoundationVideoSource+Private.h"

#import "webrtc/api/objc/RTCMediaConstraints+Private.h"
#import "webrtc/api/objc/RTCPeerConnectionFactory+Private.h"
#import "webrtc/api/objc/RTCVideoSource+Private.h"

@implementation RTCAVFoundationVideoSource

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                    constraints:(RTCMediaConstraints *)constraints {
  NSParameterAssert(factory);
  rtc::scoped_ptr<webrtc::AVFoundationVideoCapturer> capturer;
  capturer.reset(new webrtc::AVFoundationVideoCapturer());
  rtc::scoped_refptr<webrtc::VideoSourceInterface> source =
      factory.nativeFactory->CreateVideoSource(capturer.release(),
          constraints.nativeConstraints.get());
  return [super initWithNativeVideoSource:source];
}

- (BOOL)useBackCamera {
  return self.capturer->GetUseBackCamera();
}

- (void)setUseBackCamera:(BOOL)useBackCamera {
  self.capturer->SetUseBackCamera(useBackCamera);
}

- (AVCaptureSession *)captureSession {
  return self.capturer->GetCaptureSession();
}

- (webrtc::AVFoundationVideoCapturer *)capturer {
  cricket::VideoCapturer *capturer = self.nativeVideoSource->GetVideoCapturer();
  // This should be safe because no one should have changed the underlying video
  // source.
  webrtc::AVFoundationVideoCapturer *foundationCapturer =
      static_cast<webrtc::AVFoundationVideoCapturer *>(capturer);
  return foundationCapturer;
}

@end
