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

#import "RTCMediaStreamTrack+internal.h"
#import "RTCEnumConverter.h"

@implementation RTCMediaStreamTrack {
  talk_base::scoped_refptr<webrtc::MediaStreamTrackInterface> _mediaTrack;
}

@synthesize label;

- (BOOL)isEqual:(id)other {
  // Equality is purely based on the label just like the C++ implementation.
  if (self == other) return YES;
  if (![other isKindOfClass:[self class]] ||
      ![self isKindOfClass:[other class]]) {
    return NO;
  }
  RTCMediaStreamTrack *otherMediaStream = (RTCMediaStreamTrack *)other;
  return [self.label isEqual:otherMediaStream.label];
}

- (NSUInteger)hash {
  return [self.label hash];
}

- (NSString *)kind {
  return @(self.mediaTrack->kind().c_str());
}

- (NSString *)label {
  return @(self.mediaTrack->id().c_str());
}

- (BOOL)isEnabled {
  return self.mediaTrack->enabled();
}

- (BOOL)setEnabled:(BOOL)enabled {
  return self.mediaTrack->set_enabled(enabled);
}

- (RTCTrackState)state {
  return [RTCEnumConverter convertTrackStateToObjC:self.mediaTrack->state()];
}

- (BOOL)setState:(RTCTrackState)state {
  return self.mediaTrack->set_state(
      [RTCEnumConverter convertTrackStateToNative:state]);
}

@end

@implementation RTCMediaStreamTrack (Internal)

- (id)initWithMediaTrack:(
    talk_base::scoped_refptr<webrtc::MediaStreamTrackInterface>)mediaTrack {
  if (!mediaTrack) {
    NSAssert(NO, @"nil arguments not allowed");
    self = nil;
    return nil;
  }
  if ((self = [super init])) {
    _mediaTrack = mediaTrack;
    label = @(mediaTrack->id().c_str());
  }
  return self;
}

- (talk_base::scoped_refptr<webrtc::MediaStreamTrackInterface>)mediaTrack {
  return _mediaTrack;
}

@end
