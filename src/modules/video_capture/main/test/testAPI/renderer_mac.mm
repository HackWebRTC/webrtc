/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "Renderer.h"

#include <stdio.h>

#import <AppKit/AppKit.h>
#import "cocoa_renderer.h"

#include "thread_wrapper.h"
#include "tick_util.h"

static int _screen = 0;

namespace webrtc {

int WebRtcCreateWindow(void** os_specific_handle, int winNum, int width, int height)
{
    CocoaRenderer** cocoaRenderer = reinterpret_cast<CocoaRenderer**> (os_specific_handle);

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc]init];

    _screen = winNum = 0;

    // In Cocoa, rendering is not done directly to a window like in Windows and Linux.
    // It is rendererd to a Subclass of NSOpenGLView

    // create cocoa container window
    NSRect outWindowFrame = NSMakeRect(200, 800, width + 20, height + 20);

    NSArray* screens = [NSScreen screens];
    if(_screen >= [screens count])
    {
        // requesting screen
        return -1;
    }
    NSScreen* screen = (NSScreen*)[screens objectAtIndex:_screen];

    NSWindow* outWindow = [[NSWindow alloc] initWithContentRect:outWindowFrame 
                                                      styleMask:NSTitledWindowMask 
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO screen:screen];
    [outWindow orderOut:nil];
    [outWindow setTitle:@"Cocoa Renderer"];
    [outWindow setBackgroundColor:[NSColor blueColor]];
    [[outWindow contentView] setAutoresizesSubviews:YES];

    // create renderer and attach to window
    NSRect cocoaRendererFrame = NSMakeRect(10, 10, width, height);
    *cocoaRenderer = [[CocoaRenderer alloc] initWithFrame:cocoaRendererFrame];
    [[outWindow contentView] addSubview:*cocoaRenderer];

    // must tell Cocoa to draw the window, but any GUI work must be done on the main thread.
    [outWindow performSelector:@selector(display) 
                      onThread:[NSThread mainThread] 
                    withObject:nil 
                 waitUntilDone:YES];
    [outWindow makeKeyAndOrderFront:NSApp];

    [pool release];
    return 0;
}

void SetWindowPos(void* os_specific_handle, int x, int y, int width, int height, bool onTop)
{
    CocoaRenderer* cocoaRenderer = (CocoaRenderer*)os_specific_handle;
    NSWindow* ownerWindow = [cocoaRenderer window];
    NSRect ownerNewRect = NSMakeRect(x, y, width, height);
    [ownerWindow setFrame:ownerNewRect display:YES];
}

}  // namespace webrtc