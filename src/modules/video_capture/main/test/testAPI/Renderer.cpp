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

Renderer::Renderer(bool preview) :
    _renderModule(NULL), _quiting(false), _renderWindow(NULL)

{
#ifndef _WIN32	
    if (-1 == WebRtcCreateWindow((void**)(&_renderWindow), 0, 352, 288))
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
        WebRtcCreateWindow((void**)&_renderWindow, 0, 352, 288);
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


