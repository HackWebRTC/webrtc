/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "engine_configurations.h"

#if defined(COCOA_RENDERING)
#import "cocoa_render_view.h"
#include "vie_autotest_mac_cocoa.h"
#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "vie_autotest_main.h"

@implementation TestCocoaUi

// TODO(phoglund): This file probably leaks memory like crazy. Find someone
// who understands objective-c memory management and fix it.

- (void)prepareToCreateWindowsWithSize:(AutoTestRect)window1Size
                               andSize:(AutoTestRect)window2Size
                             withTitle:(void*)window1_title
                              andTitle:(void*)window2_title {
  window1Size_ = window1Size;
  window2Size_ = window2Size;
  window1Title_ = window1_title;
  window2Title_ = window2_title;
}

- (void)createWindows:(NSObject*)ignored {
  NSRect window1Frame = NSMakeRect(
      window1Size_.origin.x, window1Size_.origin.y,
      window1Size_.size.width, window1Size_.size.height);

  window1_ = [[NSWindow alloc]
               initWithContentRect:window1Frame
                         styleMask:NSTitledWindowMask
                           backing:NSBackingStoreBuffered
                             defer:NO];
  [window1_ orderOut:nil];

  NSRect render_view1_frame = NSMakeRect(
      0, 0, window1Size_.size.width, window1Size_.size.height);
  cocoaRenderView1_ =
      [[CocoaRenderView alloc] initWithFrame:render_view1_frame];

  [[window1_ contentView] addSubview:(NSView*)cocoaRenderView1_];
  [window1_ setTitle:[NSString stringWithFormat:@"%s", window1Title_]];
  [window1_ makeKeyAndOrderFront:NSApp];

  NSRect window2_frame = NSMakeRect(
      window2Size_.origin.x, window2Size_.origin.y,
      window2Size_.size.width, window2Size_.size.height);

  window2_ = [[NSWindow alloc]
               initWithContentRect:window2_frame
                         styleMask:NSTitledWindowMask
                           backing:NSBackingStoreBuffered
                             defer:NO];
  [window2_ orderOut:nil];

  NSRect render_view2_frame = NSMakeRect(
      0, 0, window1Size_.size.width, window1Size_.size.height);
  cocoaRenderView2_ =
      [[CocoaRenderView alloc] initWithFrame:render_view2_frame];
  [[window2_ contentView] addSubview:(NSView*)cocoaRenderView2_];
  [window2_ setTitle:[NSString stringWithFormat:@"%s", window2Title_]];
  [window2_ makeKeyAndOrderFront:NSApp];
}

- (NSWindow*)window1 {
  return window1_;
}

- (NSWindow*)window2 {
  return window2_;
}

- (CocoaRenderView*)cocoaRenderView1 {
  return cocoaRenderView1_;
}

- (CocoaRenderView*)cocoaRenderView2 {
  return cocoaRenderView2_;
}

@end

ViEAutoTestWindowManager::ViEAutoTestWindowManager() {
  cocoa_ui_ = [[TestCocoaUi alloc] init];
}

ViEAutoTestWindowManager::~ViEAutoTestWindowManager() {
  [cocoa_ui_ release];
}

int ViEAutoTestWindowManager::CreateWindows(AutoTestRect window1Size,
                                            AutoTestRect window2Size,
                                            void* window1_title,
                                            void* window2_title) {
    [cocoa_ui_ prepareToCreateWindowsWithSize:window1Size
                                      andSize:window2Size
                                    withTitle:window1_title
                                     andTitle:window2_title];
    [cocoa_ui_ performSelectorOnMainThread:@selector(createWindows:)
                                withObject:nil
                             waitUntilDone:YES];
    return 0;
}

int ViEAutoTestWindowManager::TerminateWindows() {
    [[cocoa_ui_ window1] close];
    [[cocoa_ui_ window2] close];
    return 0;
}

void* ViEAutoTestWindowManager::GetWindow1() {
    return [cocoa_ui_ cocoaRenderView1];
}

void* ViEAutoTestWindowManager::GetWindow2() {
    return [cocoa_ui_ cocoaRenderView2];
}

bool ViEAutoTestWindowManager::SetTopmostWindow() {
    return true;
}

int main(int argc, char * argv[]) {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    [NSApplication sharedApplication];

    int result = 0;
    AutoTestInWorkerThread* tests = [[AutoTestInWorkerThread alloc] init];

    [tests setArgc:argc argv:argv];
    [tests setDone:false];
    [NSThread detachNewThreadSelector:@selector(autoTestWithArg:)
                             toTarget:tests
                           withObject:nil];

    NSRunLoop* main_run_loop = [NSRunLoop mainRunLoop];
    NSDate *loop_until = [NSDate dateWithTimeIntervalSinceNow:0.1];
    bool runloop_ok = true;
    while (![tests done] && runloop_ok) {
      runloop_ok = [main_run_loop runMode:NSDefaultRunLoopMode
                               beforeDate:loop_until];
      loop_until = [NSDate dateWithTimeIntervalSinceNow:0.1];
    }

    result = [tests result];

    [pool release];
    return result;
}

@implementation AutoTestInWorkerThread

- (void)setDone:(bool)done {
  done_ = done;
}

- (bool)done {
  return done_;
}

- (void)setArgc:(int)argc argv:(char**)argv {
  argc_ = argc;
  argv_ = argv;
}

- (void)autoTestWithArg:(NSObject*)ignored {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    ViEAutoTestMain auto_test;

    result_ = auto_test.RunTests(argc_, argv_);
    done_ = true;

    [pool release];
    return;
}

- (int)result {
  return result_;
}

@end

#endif

