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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "APPRTCAppDelegate.h"

#import "APPRTCViewController.h"
#import "RTCPeerConnectionFactory.h"

@interface APPRTCAppDelegate () <NSWindowDelegate>
@end

@implementation APPRTCAppDelegate {
  APPRTCViewController* _viewController;
  NSWindow* _window;
}

#pragma mark - NSApplicationDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  [RTCPeerConnectionFactory initializeSSL];
  NSScreen* screen = [NSScreen mainScreen];
  NSRect visibleRect = [screen visibleFrame];
  NSRect windowRect = NSMakeRect(NSMidX(visibleRect),
                                 NSMidY(visibleRect),
                                 1320,
                                 1140);
  NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask;
  _window = [[NSWindow alloc] initWithContentRect:windowRect
                                        styleMask:styleMask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  _window.delegate = self;
  [_window makeKeyAndOrderFront:self];
  [_window makeMainWindow];
  _viewController = [[APPRTCViewController alloc] initWithNibName:nil
                                                           bundle:nil];
  [_window setContentView:[_viewController view]];
}

#pragma mark - NSWindow

- (void)windowWillClose:(NSNotification*)notification {
  [_viewController windowWillClose:notification];
  [RTCPeerConnectionFactory deinitializeSSL];
  [NSApp terminate:self];
}

@end

