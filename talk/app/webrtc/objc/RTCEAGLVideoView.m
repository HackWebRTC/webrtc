/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#import "RTCEAGLVideoView.h"

#import <GLKit/GLKit.h>

#import "RTCI420Frame.h"
#import "RTCOpenGLVideoRenderer.h"

// RTCDisplayLinkTimer wraps a CADisplayLink and is set to fire every two screen
// refreshes, which should be 30fps. We wrap the display link in order to avoid
// a retain cycle since CADisplayLink takes a strong reference onto its target.
// The timer is paused by default.
@interface RTCDisplayLinkTimer : NSObject

@property(nonatomic) BOOL isPaused;

- (instancetype)initWithTimerHandler:(void (^)(void))timerHandler;
- (void)invalidate;

@end

@implementation RTCDisplayLinkTimer {
  CADisplayLink* _displayLink;
  void (^_timerHandler)(void);
}

- (instancetype)initWithTimerHandler:(void (^)(void))timerHandler {
  NSParameterAssert(timerHandler);
  if (self = [super init]) {
    _timerHandler = timerHandler;
    _displayLink =
        [CADisplayLink displayLinkWithTarget:self
                                    selector:@selector(displayLinkDidFire:)];
    _displayLink.paused = YES;
    // Set to half of screen refresh, which should be 30fps.
    [_displayLink setFrameInterval:2];
    [_displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                       forMode:NSRunLoopCommonModes];
  }
  return self;
}

- (void)dealloc {
  [self invalidate];
}

- (BOOL)isPaused {
  return _displayLink.paused;
}

- (void)setIsPaused:(BOOL)isPaused {
  _displayLink.paused = isPaused;
}

- (void)invalidate {
  [_displayLink invalidate];
}

- (void)displayLinkDidFire:(CADisplayLink*)displayLink {
  _timerHandler();
}

@end

@interface RTCEAGLVideoView () <GLKViewDelegate>
// |i420Frame| is set when we receive a frame from a worker thread and is read
// from the display link callback so atomicity is required.
@property(atomic, strong) RTCI420Frame* i420Frame;
@property(nonatomic, readonly) GLKView* glkView;
@property(nonatomic, readonly) RTCOpenGLVideoRenderer* glRenderer;
@end

@implementation RTCEAGLVideoView {
  RTCDisplayLinkTimer* _timer;
  GLKView* _glkView;
  RTCOpenGLVideoRenderer* _glRenderer;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self configure];
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder {
  if (self = [super initWithCoder:aDecoder]) {
    [self configure];
  }
  return self;
}

- (void)configure {
  EAGLContext* glContext =
    [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
  _glRenderer = [[RTCOpenGLVideoRenderer alloc] initWithContext:glContext];

  // GLKView manages a framebuffer for us.
  _glkView = [[GLKView alloc] initWithFrame:CGRectZero
                                    context:glContext];
  _glkView.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
  _glkView.drawableDepthFormat = GLKViewDrawableDepthFormatNone;
  _glkView.drawableStencilFormat = GLKViewDrawableStencilFormatNone;
  _glkView.drawableMultisample = GLKViewDrawableMultisampleNone;
  _glkView.delegate = self;
  _glkView.layer.masksToBounds = YES;
  [self addSubview:_glkView];

  // Listen to application state in order to clean up OpenGL before app goes
  // away.
  NSNotificationCenter* notificationCenter =
    [NSNotificationCenter defaultCenter];
  [notificationCenter addObserver:self
                         selector:@selector(willResignActive)
                             name:UIApplicationWillResignActiveNotification
                           object:nil];
  [notificationCenter addObserver:self
                         selector:@selector(didBecomeActive)
                             name:UIApplicationDidBecomeActiveNotification
                           object:nil];

  // Frames are received on a separate thread, so we poll for current frame
  // using a refresh rate proportional to screen refresh frequency. This
  // occurs on the main thread.
  __weak RTCEAGLVideoView* weakSelf = self;
  _timer = [[RTCDisplayLinkTimer alloc] initWithTimerHandler:^{
      RTCEAGLVideoView* strongSelf = weakSelf;
      // Don't render if frame hasn't changed.
      if (strongSelf.glRenderer.lastDrawnFrame == strongSelf.i420Frame) {
        return;
      }
      // This tells the GLKView that it's dirty, which will then call the
      // GLKViewDelegate method implemented below.
      [strongSelf.glkView setNeedsDisplay];
    }];
  [self setupGL];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  UIApplicationState appState =
      [UIApplication sharedApplication].applicationState;
  if (appState == UIApplicationStateActive) {
    [self teardownGL];
  }
  [_timer invalidate];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  _glkView.frame = self.bounds;
}

#pragma mark - GLKViewDelegate

// This method is called when the GLKView's content is dirty and needs to be
// redrawn. This occurs on main thread.
- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect {
  // The renderer will draw the frame to the framebuffer corresponding to the
  // one used by |view|.
  [_glRenderer drawFrame:self.i420Frame];
}

#pragma mark - RTCVideoRenderer

// These methods may be called on non-main thread.
- (void)setSize:(CGSize)size {
  __weak RTCEAGLVideoView* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    RTCEAGLVideoView* strongSelf = weakSelf;
    [strongSelf.delegate videoView:strongSelf didChangeVideoSize:size];
  });
}

- (void)renderFrame:(RTCI420Frame*)frame {
  self.i420Frame = frame;
}

#pragma mark - Private

- (void)setupGL {
  self.i420Frame = nil;
  [_glRenderer setupGL];
  _timer.isPaused = NO;
}

- (void)teardownGL {
  self.i420Frame = nil;
  _timer.isPaused = YES;
  [_glkView deleteDrawable];
  [_glRenderer teardownGL];
}

- (void)didBecomeActive {
  [self setupGL];
}

- (void)willResignActive {
  [self teardownGL];
}

@end
