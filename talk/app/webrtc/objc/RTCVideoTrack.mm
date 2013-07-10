/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "RTCVideoTrack+internal.h"

#import "RTCMediaStreamTrack+internal.h"
#import "RTCVideoRenderer+internal.h"

@implementation RTCVideoTrack {
  NSMutableArray *_rendererArray;
}

- (id)initWithMediaTrack:(
    talk_base::scoped_refptr<webrtc::MediaStreamTrackInterface>)mediaTrack {
  if (self = [super initWithMediaTrack:mediaTrack]) {
    _rendererArray = [NSMutableArray array];
  }
  return self;
}

- (void)addRenderer:(RTCVideoRenderer *)renderer {
  NSAssert1(![self.renderers containsObject:renderer],
            @"renderers already contains object [%@]",
            [renderer description]);
  [_rendererArray addObject:renderer];
  self.videoTrack->AddRenderer(renderer.videoRenderer);
}

- (void)removeRenderer:(RTCVideoRenderer *)renderer {
  NSUInteger index = [self.renderers indexOfObjectIdenticalTo:renderer];
  if (index != NSNotFound) {
    [_rendererArray removeObjectAtIndex:index];
    self.videoTrack->RemoveRenderer(renderer.videoRenderer);
  }
}

- (NSArray *)renderers {
  return [_rendererArray copy];
}

@end

@implementation RTCVideoTrack (Internal)

- (talk_base::scoped_refptr<webrtc::VideoTrackInterface>)videoTrack {
  return static_cast<webrtc::VideoTrackInterface *>(self.mediaTrack.get());
}

@end
