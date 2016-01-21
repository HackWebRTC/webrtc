/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoTrack.h"

#import "webrtc/api/objc/RTCMediaStreamTrack+Private.h"
#import "webrtc/api/objc/RTCPeerConnectionFactory+Private.h"
#import "webrtc/api/objc/RTCVideoRendererAdapter+Private.h"
#import "webrtc/api/objc/RTCVideoSource+Private.h"
#import "webrtc/api/objc/RTCVideoTrack+Private.h"
#import "webrtc/base/objc/NSString+StdString.h"

@implementation RTCVideoTrack {
  NSMutableArray *_adapters;
}

@synthesize source = _source;

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                         source:(RTCVideoSource *)source
                        trackId:(NSString *)trackId {
  NSParameterAssert(factory);
  NSParameterAssert(source);
  NSParameterAssert(trackId.length);
  std::string nativeId = [NSString stdStringForString:trackId];
  rtc::scoped_refptr<webrtc::VideoTrackInterface> track =
      factory.nativeFactory->CreateVideoTrack(nativeId,
                                              source.nativeVideoSource);
  return [self initWithNativeTrack:track type:RTCMediaStreamTrackTypeVideo];
}

- (instancetype)initWithNativeMediaTrack:
    (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeMediaTrack
                                    type:(RTCMediaStreamTrackType)type {
  NSParameterAssert(nativeMediaTrack);
  NSParameterAssert(type == RTCMediaStreamTrackTypeVideo);
  if (self = [super initWithNativeTrack:nativeMediaTrack type:type]) {
    _adapters = [NSMutableArray array];
    rtc::scoped_refptr<webrtc::VideoSourceInterface> source =
        self.nativeVideoTrack->GetSource();
    if (source) {
      _source = [[RTCVideoSource alloc] initWithNativeVideoSource:source.get()];
    }
  }
  return self;
}

- (void)dealloc {
  for (RTCVideoRendererAdapter *adapter in _adapters) {
    self.nativeVideoTrack->RemoveRenderer(adapter.nativeVideoRenderer);
  }
}

- (void)addRenderer:(id<RTCVideoRenderer>)renderer {
  // Make sure we don't have this renderer yet.
  for (RTCVideoRendererAdapter *adapter in _adapters) {
    // Getting around unused variable error
    if (adapter.videoRenderer != renderer) {
      NSAssert(NO, @"|renderer| is already attached to this track");
    }
  }
  // Create a wrapper that provides a native pointer for us.
  RTCVideoRendererAdapter* adapter =
      [[RTCVideoRendererAdapter alloc] initWithNativeRenderer:renderer];
  [_adapters addObject:adapter];
  self.nativeVideoTrack->AddRenderer(adapter.nativeVideoRenderer);
}

- (void)removeRenderer:(id<RTCVideoRenderer>)renderer {
  RTCVideoRendererAdapter *adapter;
  __block NSUInteger indexToRemove = NSNotFound;
  [_adapters enumerateObjectsUsingBlock:^(RTCVideoRendererAdapter *adapter,
                                          NSUInteger idx,
                                          BOOL *stop) {
    if (adapter.videoRenderer == renderer) {
      indexToRemove = idx;
      *stop = YES;
    }
  }];
  if (indexToRemove == NSNotFound) {
    return;
  }
  self.nativeVideoTrack->RemoveRenderer(adapter.nativeVideoRenderer);
  [_adapters removeObjectAtIndex:indexToRemove];
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::VideoTrackInterface>)nativeVideoTrack {
  return static_cast<webrtc::VideoTrackInterface *>(self.nativeTrack.get());
}

@end
