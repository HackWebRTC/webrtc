/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include "video_render.h"
#include "testDefines.h"

#ifdef _WIN32

#include <windows.h>
#elif defined(WEBRTC_MAC_INTEL)
#import <AppKit/AppKit.h>
//#import "CocoaWindow.h"
#import "cocoa_renderer.h"
#elif defined (WEBRTC_ANDROID)
#include <JNI.h>
#elif defined(WEBRTC_LINUX)
typedef void* HWND;
#endif

#include "thread_wrapper.h"

namespace webrtc
{

class Renderer
{
public:
    Renderer(bool preview = false);
    ~Renderer(void);

    void RenderFrame(VideoFrame& videoFrame);
    void PaintGreen();
    void PaintBlue();
    void* GetWindow();

#if defined (WEBRTC_ANDROID)
    static void SetRenderWindow(jobject renderWindow);
#endif

private:

    static bool RenderThread(ThreadObj);
    bool RenderThreadProcess();

    VideoRender* _renderModule;
    VideoRenderCallback* _renderProvider;
    VideoFrame _videoFrame;
    bool _quiting;
    ThreadWrapper* _messageThread;
    static int _screen;
    static const WebRtc_UWord32 _frameWidth = 352;
    static const WebRtc_UWord32 _frameHeight = 288;

#if defined(_WIN32)
    HWND _renderWindow;
#elif defined(WEBRTC_MAC_INTEL)
    NSAutoreleasePool* _pool;
    CocoaRenderer* _renderWindow;
#elif defined (WEBRTC_ANDROID)
    jobject _renderWindow; //this is a glsurface.
public:
    static jobject g_renderWindow;
#elif defined(WEBRTC_LINUX)
    typedef void* HWND;
    HWND _renderWindow;
#endif
};
} // namespace webrtc
