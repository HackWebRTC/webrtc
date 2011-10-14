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
#include "thread_wrapper.h"
#include "tick_util.h"

#if defined _WIN32
#include <tchar.h>
#endif

namespace webrtc
{
#if defined _WIN32
#define SLEEP_10_SEC ::Sleep(10000)
#define GET_TIME_IN_MS timeGetTime

LRESULT CALLBACK WinProc( HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_DESTROY:
        break;
        case WM_COMMAND:
        break;
    }
    return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

int WebRtcCreateWindow(HWND &hwndMain,int winNum, int width, int height)
{
    HINSTANCE hinst = GetModuleHandle(0);
    WNDCLASSEX wcx;
    wcx.hInstance = hinst;
    wcx.lpszClassName = _T(" test camera delay");
    wcx.lpfnWndProc = (WNDPROC)WinProc;
    wcx.style = CS_DBLCLKS;
    wcx.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    wcx.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
    wcx.hCursor = LoadCursor (NULL, IDC_ARROW);
    wcx.lpszMenuName = NULL;
    wcx.cbSize = sizeof (WNDCLASSEX);
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.hbrBackground = GetSysColorBrush(COLOR_3DFACE);

    // Register our window class with the operating system.
    // If there is an error, exit program.
    if ( !RegisterClassEx (&wcx) )
    {
        //MessageBox( 0, TEXT("Failed to register window class!"),TEXT("Error!"), MB_OK|MB_ICONERROR ) ;
        //return 0;
    }

    // Create the main window.
    hwndMain = CreateWindowEx(
        0, // no extended styles
        wcx.lpszClassName, // class name
        _T("Test Camera Delay"), // window name
        WS_OVERLAPPED |WS_THICKFRAME, // overlapped window
        0, // horizontal position
        0, // vertical position
        width, // width
        height, // height
        (HWND) NULL, // no parent or owner window
        (HMENU) NULL, // class menu used
        hinst, // instance handle
        NULL); // no window creation data

    if (!hwndMain)
    {
        int error = GetLastError();
        return -1;
    }

    // Show the window using the flag specified by the program
    // that started the application, and send the application
    // a WM_PAINT message.

    ShowWindow(hwndMain, SW_SHOWDEFAULT);
    UpdateWindow(hwndMain);
    return 0;
}

void SetWindowPos(HWND &hwndMain, int x, int y, int width, int height, bool onTop)
{
    /*   if(onTop)
     {
     SetWindowPos(hwndMain,HWND_TOPMOST,x,y,width,height,0);
     }
     else*/
    {
        SetWindowPos(hwndMain,HWND_TOP,x,y,width,height,0);
    }

}

#elif defined(WEBRTC_MAC_INTEL)
static int _screen = 0;
int WebRtcCreateWindow(CocoaRenderer*& cocoaRenderer,int winNum, int width, int height)
{
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

    NSWindow* outWindow = [[NSWindow alloc] initWithContentRect:outWindowFrame styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO screen:screen];
    [outWindow orderOut:nil];
    [outWindow setTitle:@"Cocoa Renderer"];
    [outWindow setBackgroundColor:[NSColor blueColor]];
    [[outWindow contentView] setAutoresizesSubviews:YES];

    //		// ***** TODO: test screen positioning *****
    //		// set to the appropriate screen
    //		NSArray* screens = [NSScreen screens];
    //		printf("TODO: test positioning to correct screen\n");
    //		switch(winNum){
    //			case 0:
    //				if([screens count] >= 1){
    //					[outWindow constrainFrameRect:outWindowFrame toScreen:(NSScreen*)[screens objectAtIndex:0]];
    //				}
    //				break;
    //			case 1:
    //				if([screens count] >= 2){
    //					[outWindow constrainFrameRect:outWindowFrame toScreen:(NSScreen*)[screens objectAtIndex:1]];
    //				}
    //				break;
    //			case 2:
    //				if([screens count] >= 3){
    //					[outWindow constrainFrameRect:outWindowFrame toScreen:(NSScreen*)[screens objectAtIndex:2]];
    //				}
    //				break;
    //			case 3:
    //				if([screens count] >= 4){
    //					[outWindow constrainFrameRect:outWindowFrame toScreen:(NSScreen*)[screens objectAtIndex:3]];
    //				}
    //				break;
    //			default:
    //				break;
    //
    //		}//

    // create renderer and attach to window
    NSRect cocoaRendererFrame = NSMakeRect(10, 10, width, height);
    cocoaRenderer = [[CocoaRenderer alloc] initWithFrame:cocoaRendererFrame];
    [[outWindow contentView] addSubview:cocoaRenderer];

    // must tell Cocoa to draw the window, but any GUI work must be done on the main thread.
    [outWindow performSelector:@selector(display) onThread:[NSThread mainThread] withObject:nil waitUntilDone:YES];
    [outWindow makeKeyAndOrderFront:NSApp];

    [pool release];
    return 0;

}

void SetWindowPos(CocoaRenderer*& cocoaRenderer, int x, int y, int width, int height, bool onTop)
{
    NSWindow* ownerWindow = [cocoaRenderer window];
    NSRect ownerNewRect = NSMakeRect(x, y, width, height);
    [ownerWindow setFrame:ownerNewRect display:YES];

    //NSRect cocoaRendererNewRect = {0, 0, 500, 500};
    //NSArray* screens = [NSScreen screens];
    //NSScreen* screen = (NSScreen*)[screens objectAtIndex:_screen];
    //cocoaRendererNewRect = [cocoaRenderer constrainFrameRect:cocoaRendererNewRect toScreen:screen];

    //[cocoaRenderer setFrame:cocoaRendererNewRect];
    //[cocoaRenderer performSelector:@selector(display) onThread:[NSThread mainThread] withObject:nil waitUntilDone:YES];


    //[cocoaRenderer performSelector:@selector(drawRect) onThread:[NSThread mainThread] withObject:cocoaRendererNewRect waitUntilDone:YES];


    // *** Setting onTop is toooo on top. Can't get to anything underneath.
    //		[ownerWindow setLevel:NSNormalWindowLevel];
    //		if(YES == onTop){
    //			[ownerWindow setLevel:NSScreenSaverWindowLevel];
    //		}
    //		else{
    //			[ownerWindow setLevel:NSNormalWindowLevel];
    //		}
}
#elif defined(WEBRTC_ANDROID)
#define nil NULL
#define NO false
jobject Renderer::g_renderWindow=NULL;

int WebRtcCreateWindow(jobject &renderWin,int /*winNum*/, int /*width*/, int /*height*/)
{
    renderWin=Renderer::g_renderWindow;
    return 0;
}
void SetWindowPos(void *& /*hwndMain*/, int /*x*/, int /*y*/, int /*width*/, int /*height*/, bool /*onTop*/)
{

}

void Renderer::SetRenderWindow(jobject renderWindow)
{
    __android_log_print(ANDROID_LOG_DEBUG, "VideoCaptureModule -testAPI", "Renderer::SetRenderWindow");
    g_renderWindow=renderWindow;
}

#elif defined(WEBRTC_LINUX)

int WebRtcCreateWindow(HWND &hwndMain,int winNum, int width, int height)
{
    return 0;
}
void SetWindowPos(HWND &hwndMain, int x, int y, int width, int height, bool onTop)
{
}
#endif

Renderer::Renderer(bool preview) :
    _renderModule(NULL), _quiting(false), _renderWindow(NULL)

{
#ifndef _WIN32	
    if (-1 == WebRtcCreateWindow(_renderWindow, 0, 352, 288))
    {
        printf("ERROR** INVALID SCREEN\n");
    }
#endif // In Windows the thread running the message loop needs to create the window.
    _messageThread = ThreadWrapper::CreateThread(RenderThread, this,
                                                 kLowPriority, "RenderThread");
    unsigned int threadId;
    _messageThread->Start(threadId);
    while (!_renderWindow)
    {
        SLEEP(10);
    }// Wait until messageThread has created the window


    _renderModule = VideoRender::CreateVideoRender(0, (void*) _renderWindow,
                                                   false);

#if defined(WEBRTC_MAC_INTEL)
    _pool = [[NSAutoreleasePool alloc] init];
#endif

    _renderProvider = _renderModule->AddIncomingRenderStream(0, 0, 0.0f, 0.0f,
                                                             1.0f, 1.0f);

    WebRtc_UWord32 width;
    WebRtc_UWord32 height;

    _renderModule->GetScreenResolution(width, height);
#ifdef _WIN32
    // GetScreenResolution is currently not implemented
    RECT screenRect;
    GetWindowRect(GetDesktopWindow(), &screenRect);
    width=screenRect.right;
    height=screenRect.bottom;
#endif

    if (!preview)
    {
#if defined(_WIN32)
        SetWindowPos(_renderWindow,0,height/2,width,height/2,true);
#elif defined(WEBRTC_MAC_INTEL)
        SetWindowPos(_renderWindow, 0, height, width, height, true);
#elif defined(WEBRTC_LINUX)

#endif

        _videoFrame.VerifyAndAllocate(_frameWidth * _frameHeight * 3 / 2);
        _videoFrame.SetHeight(_frameHeight);
        _videoFrame.SetWidth(_frameWidth);
        _videoFrame.SetLength(_videoFrame.Size());

        memset(_videoFrame.Buffer(), 0, _videoFrame.Size());
    }
    else // Preview window
    {

#if defined(_WIN32)
        SetWindowPos(_renderWindow,width/2,0,width/2,height/2,false);
#elif defined(WEBRTC_MAC_INTEL)
        SetWindowPos(_renderWindow, 0, height, width, height, false);
#elif defined(WEBRTC_LINUX)

#endif
    }

    _renderModule->StartRender(0);

}

Renderer::~Renderer(void)
{
    VideoRender::DestroyVideoRender(_renderModule);
    _quiting = true;
    while (_renderWindow)
    {
        SLEEP(20);
    }
    _messageThread->Stop();

#if defined(WEBRTC_MAC_INTEL)
    [_pool release];
#endif
    delete _messageThread;

}

bool Renderer::RenderThread(ThreadObj obj)
{
    return static_cast<Renderer*> (obj)->RenderThreadProcess();
}

bool Renderer::RenderThreadProcess()
{

    if (_quiting == false && _renderWindow == NULL) // Create the render window
    {
        WebRtcCreateWindow(_renderWindow, 0, 352, 288);
    }

#ifdef _WIN32
    MSG msg;
    if(PeekMessage(&msg, NULL, 0, 0,PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
    if (_quiting == true)
    {
#if defined _WIN32     
        ::DestroyWindow(_renderWindow);
#endif
        _renderWindow = NULL;
    }

    SLEEP(50);
    return true;

}

void Renderer::PaintGreen()
{

    _videoFrame.VerifyAndAllocate(_frameWidth * _frameHeight * 3 / 2);
    _videoFrame.SetHeight(_frameHeight);
    _videoFrame.SetWidth(_frameWidth);
    _videoFrame.SetLength(_videoFrame.Size());

    memset(_videoFrame.Buffer(), 127, _videoFrame.Size());
    memset(_videoFrame.Buffer() + _videoFrame.Width() * _videoFrame.Height(),
           0, _videoFrame.Width() * _videoFrame.Height() / 2);

    _videoFrame.SetRenderTime(TickTime::MillisecondTimestamp());    
    _renderProvider->RenderFrame(0,_videoFrame);

}

void Renderer::RenderFrame(VideoFrame& videoFrame)
{
    _renderProvider->RenderFrame(0, videoFrame);
}

void Renderer::PaintBlue()
{

    _videoFrame.VerifyAndAllocate(_frameWidth * _frameHeight * 3 / 2);
    _videoFrame.SetHeight(_frameHeight);
    _videoFrame.SetWidth(_frameWidth);
    _videoFrame.SetLength(_videoFrame.Size());

    memset(_videoFrame.Buffer(), 127, _videoFrame.Size());
    memset(_videoFrame.Buffer() + _videoFrame.Width() * _videoFrame.Height(),
           255, _videoFrame.Width() * _videoFrame.Height() / 2);
    _videoFrame.SetRenderTime(TickTime::MillisecondTimestamp());
    _renderProvider->RenderFrame(0, _videoFrame);

}
void* Renderer::GetWindow()
{
    return (void*) _renderWindow;
}

} // namespace webrtc


