/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <GLKit/GLKit.h>

@class RTCVideoFrame;

NS_ASSUME_NONNULL_BEGIN

@interface RTCNV12TextureCache : NSObject

@property(nonatomic, readonly) GLuint yTexture;
@property(nonatomic, readonly) GLuint uvTexture;

- (nullable instancetype)initWithContext:(EAGLContext *)context;

- (BOOL)uploadFrameToTextures:(RTCVideoFrame *)frame;

- (void)releaseTextures;

@end

NS_ASSUME_NONNULL_END
