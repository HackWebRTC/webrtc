/*
 * libjingle
 * Copyright 2014, Google Inc.
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

#import "APPRTCViewController.h"

#import <AVFoundation/AVFoundation.h>
#import "APPRTCConnectionManager.h"
#import "RTCNSGLVideoView.h"

static NSUInteger const kContentWidth = 1280;
static NSUInteger const kContentHeight = 720;
static NSUInteger const kRoomFieldWidth = 80;
static NSUInteger const kLogViewHeight = 280;

@class APPRTCMainView;
@protocol APPRTCMainViewDelegate

- (void)appRTCMainView:(APPRTCMainView*)mainView
        didEnterRoomId:(NSString*)roomId;

@end

@interface APPRTCMainView : NSView

@property(nonatomic, weak) id<APPRTCMainViewDelegate> delegate;
@property(nonatomic, readonly) RTCNSGLVideoView* localVideoView;
@property(nonatomic, readonly) RTCNSGLVideoView* remoteVideoView;

- (void)displayLogMessage:(NSString*)message;

@end

@interface APPRTCMainView () <NSTextFieldDelegate, RTCNSGLVideoViewDelegate>
@end
@implementation APPRTCMainView  {
  NSScrollView* _scrollView;
  NSTextField* _roomLabel;
  NSTextField* _roomField;
  NSTextView* _logView;
  RTCNSGLVideoView* _localVideoView;
  RTCNSGLVideoView* _remoteVideoView;
  CGSize _localVideoSize;
  CGSize _remoteVideoSize;
}

+ (BOOL)requiresConstraintBasedLayout {
  return YES;
}

- (instancetype)initWithFrame:(NSRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self setupViews];
  }
  return self;
}

- (void)updateConstraints {
  NSParameterAssert(
      _roomField != nil && _scrollView != nil && _remoteVideoView != nil);
  [self removeConstraints:[self constraints]];
  NSDictionary* viewsDictionary =
      NSDictionaryOfVariableBindings(_roomLabel,
                                     _roomField,
                                     _scrollView,
                                     _remoteVideoView);

  NSSize remoteViewSize = [self remoteVideoViewSize];
  NSDictionary* metrics = @{
    @"kLogViewHeight" : @(kLogViewHeight),
    @"kRoomFieldWidth" : @(kRoomFieldWidth),
    @"remoteViewWidth" : @(remoteViewSize.width),
    @"remoteViewHeight" : @(remoteViewSize.height),
  };
  // Declare this separately to avoid compiler warning about splitting string
  // within an NSArray expression.
  NSString* verticalConstraint =
      @"V:|-[_roomLabel]-[_roomField]-[_scrollView(kLogViewHeight)]"
       "-[_remoteVideoView(remoteViewHeight)]-|";
  NSArray* constraintFormats = @[
      verticalConstraint,
      @"|-[_roomLabel]",
      @"|-[_roomField(kRoomFieldWidth)]",
      @"|-[_scrollView(remoteViewWidth)]-|",
      @"|-[_remoteVideoView(remoteViewWidth)]-|",
  ];
  for (NSString* constraintFormat in constraintFormats) {
    NSArray* constraints =
        [NSLayoutConstraint constraintsWithVisualFormat:constraintFormat
                                                options:0
                                                metrics:metrics
                                                  views:viewsDictionary];
    for (NSLayoutConstraint* constraint in constraints) {
      [self addConstraint:constraint];
    }
  }
  [super updateConstraints];
}

- (void)displayLogMessage:(NSString*)message {
  _logView.string =
      [NSString stringWithFormat:@"%@%@\n", _logView.string, message];
  NSRange range = NSMakeRange([_logView.string length], 0);
  [_logView scrollRangeToVisible:range];
}

#pragma mark - NSControl delegate

- (void)controlTextDidEndEditing:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  NSInteger textMovement = [userInfo[@"NSTextMovement"] intValue];
  if (textMovement == NSReturnTextMovement) {
    [self.delegate appRTCMainView:self didEnterRoomId:_roomField.stringValue];
  }
}

#pragma mark - RTCNSGLVideoViewDelegate

- (void)videoView:(RTCNSGLVideoView*)videoView
    didChangeVideoSize:(NSSize)size {
  if (videoView == _remoteVideoView) {
    _remoteVideoSize = size;
  } else if (videoView == _localVideoView) {
    _localVideoSize = size;
  } else {
    return;
  }
  [self setNeedsUpdateConstraints:YES];
}

#pragma mark - Private

- (void)setupViews {
  NSParameterAssert([[self subviews] count] == 0);

  _roomLabel = [[NSTextField alloc] initWithFrame:NSZeroRect];
  [_roomLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_roomLabel setBezeled:NO];
  [_roomLabel setDrawsBackground:NO];
  [_roomLabel setEditable:NO];
  [_roomLabel setStringValue:@"Enter AppRTC room id:"];
  [self addSubview:_roomLabel];

  _roomField = [[NSTextField alloc] initWithFrame:NSZeroRect];
  [_roomField setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self addSubview:_roomField];
  [_roomField setEditable:YES];
  [_roomField setDelegate:self];

  _logView = [[NSTextView alloc] initWithFrame:NSZeroRect];
  [_logView setMinSize:NSMakeSize(0, kLogViewHeight)];
  [_logView setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
  [_logView setVerticallyResizable:YES];
  [_logView setAutoresizingMask:NSViewWidthSizable];
  NSTextContainer* textContainer = [_logView textContainer];
  NSSize containerSize = NSMakeSize(kContentWidth, FLT_MAX);
  [textContainer setContainerSize:containerSize];
  [textContainer setWidthTracksTextView:YES];
  [_logView setEditable:NO];

  _scrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  [_scrollView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_scrollView setHasVerticalScroller:YES];
  [_scrollView setDocumentView:_logView];
  [self addSubview:_scrollView];

  NSOpenGLPixelFormatAttribute attributes[] = {
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFADepthSize, 24,
    NSOpenGLPFAOpenGLProfile,
    NSOpenGLProfileVersion3_2Core,
    0
  };
  NSOpenGLPixelFormat* pixelFormat =
      [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
  _remoteVideoView = [[RTCNSGLVideoView alloc] initWithFrame:NSZeroRect
                                                 pixelFormat:pixelFormat];
  [_remoteVideoView setTranslatesAutoresizingMaskIntoConstraints:NO];
  _remoteVideoView.delegate = self;
  [self addSubview:_remoteVideoView];

  // TODO(tkchin): create local video view.
  // https://code.google.com/p/webrtc/issues/detail?id=3417.
}

- (NSSize)remoteVideoViewSize {
  if (_remoteVideoSize.width > 0 && _remoteVideoSize.height > 0) {
    return _remoteVideoSize;
  } else {
    return NSMakeSize(kContentWidth, kContentHeight);
  }
}

- (NSSize)localVideoViewSize {
  return NSZeroSize;
}

@end

@interface APPRTCViewController ()
    <APPRTCConnectionManagerDelegate, APPRTCMainViewDelegate, APPRTCLogger>
@property(nonatomic, readonly) APPRTCMainView* mainView;
@end

@implementation APPRTCViewController {
  APPRTCConnectionManager* _connectionManager;
}

- (instancetype)initWithNibName:(NSString*)nibName
                         bundle:(NSBundle*)bundle {
  if (self = [super initWithNibName:nibName bundle:bundle]) {
    _connectionManager =
        [[APPRTCConnectionManager alloc] initWithDelegate:self
                                                   logger:self];
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)loadView {
  APPRTCMainView* view = [[APPRTCMainView alloc] initWithFrame:NSZeroRect];
  [view setTranslatesAutoresizingMaskIntoConstraints:NO];
  view.delegate = self;
  self.view = view;
}

- (void)windowWillClose:(NSNotification*)notification {
  [self disconnect];
}

#pragma mark - APPRTCConnectionManagerDelegate

- (void)connectionManager:(APPRTCConnectionManager*)manager
    didReceiveLocalVideoTrack:(RTCVideoTrack*)localVideoTrack {
  self.mainView.localVideoView.videoTrack = localVideoTrack;
}

- (void)connectionManager:(APPRTCConnectionManager*)manager
    didReceiveRemoteVideoTrack:(RTCVideoTrack*)remoteVideoTrack {
  self.mainView.remoteVideoView.videoTrack = remoteVideoTrack;
}

- (void)connectionManagerDidReceiveHangup:(APPRTCConnectionManager*)manager {
  [self showAlertWithMessage:@"Remote closed connection"];
  [self disconnect];
}

- (void)connectionManager:(APPRTCConnectionManager*)manager
      didErrorWithMessage:(NSString*)message {
  [self showAlertWithMessage:message];
  [self disconnect];
}

#pragma mark - APPRTCLogger

- (void)logMessage:(NSString*)message {
  [self.mainView displayLogMessage:message];
}

#pragma mark - APPRTCMainViewDelegate

- (void)appRTCMainView:(APPRTCMainView*)mainView
        didEnterRoomId:(NSString*)roomId {
  NSString* urlString =
      [NSString stringWithFormat:@"https://apprtc.appspot.com/?r=%@", roomId];
  [_connectionManager connectToRoomWithURL:[NSURL URLWithString:urlString]];
}

#pragma mark - Private

- (APPRTCMainView*)mainView {
  return (APPRTCMainView*)self.view;
}

- (void)showAlertWithMessage:(NSString*)message {
  NSAlert* alert = [[NSAlert alloc] init];
  [alert setMessageText:message];
  [alert runModal];
}

- (void)disconnect {
  self.mainView.remoteVideoView.videoTrack = nil;
  [_connectionManager disconnect];
}

@end
