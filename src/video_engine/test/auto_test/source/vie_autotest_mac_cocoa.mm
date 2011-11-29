/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include	"engine_configurations.h"

#if defined(COCOA_RENDERING)
#include	"vie_autotest_mac_cocoa.h"
#include	"vie_autotest_defines.h"
#include	"vie_autotest.h"
#include	"vie_autotest_main.h"

ViEAutoTestWindowManager::ViEAutoTestWindowManager() :
    _cocoaRenderView1(nil), _cocoaRenderView2(nil)
{

}

ViEAutoTestWindowManager::~ViEAutoTestWindowManager()
{
    if (_cocoaRenderView1)
    {
        [    _cocoaRenderView1 release];
    }
    if(_cocoaRenderView2)
    {
        [_cocoaRenderView2 release];
    }
}

int ViEAutoTestWindowManager::CreateWindows(AutoTestRect window1Size,
                                            AutoTestRect window2Size,
                                            void* window1Title,
                                            void* window2Title)
{
    NSRect outWindow1Frame = NSMakeRect(window1Size.origin.x,
                                        window1Size.origin.y,
                                        window1Size.size.width,
                                        window1Size.size.height);
    NSWindow* outWindow1 = [[NSWindow alloc] initWithContentRect:outWindow1Frame
                            styleMask:NSTitledWindowMask
                            backing:NSBackingStoreBuffered defer:NO];
    [outWindow1 orderOut:nil];
    NSRect cocoaRenderView1Frame = NSMakeRect(0, 0, window1Size.size.width,
                                              window1Size.size.height);
    _cocoaRenderView1 = [[CocoaRenderView alloc]
                          initWithFrame:cocoaRenderView1Frame];
    [[outWindow1 contentView] addSubview:(NSView*)_cocoaRenderView1];
    [outWindow1 setTitle:[NSString stringWithFormat:@"%s", window1Title]];
    [outWindow1 makeKeyAndOrderFront:NSApp];

    NSRect outWindow2Frame = NSMakeRect(window2Size.origin.x,
                                        window2Size.origin.y,
                                        window2Size.size.width,
                                        window2Size.size.height);
    NSWindow* outWindow2 = [[NSWindow alloc] initWithContentRect:outWindow2Frame
                            styleMask:NSTitledWindowMask
                            backing:NSBackingStoreBuffered defer:NO];
    [outWindow2 orderOut:nil];
    NSRect cocoaRenderView2Frame = NSMakeRect(0, 0, window2Size.size.width,
                                              window2Size.size.height);
    _cocoaRenderView2 = [[CocoaRenderView alloc]
                          initWithFrame:cocoaRenderView2Frame];
    [[outWindow2 contentView] addSubview:(NSView*)_cocoaRenderView2];
    [outWindow2 setTitle:[NSString stringWithFormat:@"%s", window2Title]];
    [outWindow2 makeKeyAndOrderFront:NSApp];

    return 0;
}

int ViEAutoTestWindowManager::TerminateWindows()
{
    return 0;
}

void* ViEAutoTestWindowManager::GetWindow1()
{
    return _cocoaRenderView1;
}

void* ViEAutoTestWindowManager::GetWindow2()
{
    return _cocoaRenderView2;
}

bool ViEAutoTestWindowManager::SetTopmostWindow()
{
    return true;
}

int main(int argc, const char * argv[])
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    [NSApplication sharedApplication];

#if defined(MAC_COCOA_USE_NSRUNLOOP)
    // we have to run the test in a secondary thread because we need to run a
    // runloop, which blocks
    if (argc > 1)
    {
        AutoTestClass    * autoTestClass = [[AutoTestClass alloc]init];
        [NSThread detachNewThreadSelector:@selector(autoTestWithArg:)
         toTarget:autoTestClass withObject:[NSString stringWithFormat:@"%s",
                                            argv[1]]];
    }
    else
    {
        AutoTestClass* autoTestClass = [[AutoTestClass alloc]init];
        [NSThread detachNewThreadSelector:@selector(autoTestWithArg:)
         toTarget:autoTestClass withObject:nil];
    }

// process OS events. Blocking call
[[NSRunLoop mainRunLoop]run];

#else

    ViEAutoTestMain autoTest;
    int success = autoTest.BeginOSIndependentTesting();
#endif
[pool release];
}

@implementation AutoTestClass

-(void)autoTestWithArg:(NSString*)answerFile
{

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    ViEAutoTestMain autoTest;

    if(NSOrderedSame != [answerFile compare:@""])
    {
        char answerFileUTF8[1024] = "";
        strcpy(answerFileUTF8, (char*)[answerFileUTF8 UTF8]);
        autoTest.UseAnswerFile(answerFileUTF8);
    }

    int success = autoTest.BeginOSIndependentTesting();

    [pool release];
    return;
}

@end

#endif

