/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_x11_channel.h"

#include "critical_section_wrapper.h"
#include "trace.h"


namespace webrtc {

#define DISP_MAX 128

static Display *dispArray[DISP_MAX];
static int dispCount = 0;


VideoX11Channel::VideoX11Channel(WebRtc_Word32 id) :
    _crit(*CriticalSectionWrapper::CreateCriticalSection()),
            _videoInterpolator(NULL), _display(NULL), _xvport(), _shminfo(),
            _image(NULL), _window(NULL), _width(DEFAULT_RENDER_FRAME_WIDTH),
            _height(DEFAULT_RENDER_FRAME_HEIGHT), _outWidth(0), _outHeight(0),
            _xPos(0), _yPos(0), _prepared(false), _dispCount(0), _buffer(NULL),
            _Id(id)
{
}

VideoX11Channel::~VideoX11Channel()
{
    if (_prepared)
    {
        _crit.Enter();
        RemoveRenderer();
        _crit.Leave();
    }
    delete &_crit;

    if (_videoInterpolator)
    {
        delete _videoInterpolator;
    }
}

WebRtc_Word32 VideoX11Channel::RenderFrame(const WebRtc_UWord32 streamId,
                                               VideoFrame& videoFrame)
{
    CriticalSectionScoped cs(_crit);
    if (_width != (WebRtc_Word32) videoFrame.Width() || _height
            != (WebRtc_Word32) videoFrame.Height())
    {
        if (FrameSizeChange(videoFrame.Width(), videoFrame.Height(), 1) == -1)
        {
            return -1;
        }
    }
    return DeliverFrame(videoFrame.Buffer(), videoFrame.Length(),
                        videoFrame.TimeStamp());
}

WebRtc_Word32 VideoX11Channel::FrameSizeChange(WebRtc_Word32 width,
                                                   WebRtc_Word32 height,
                                                   WebRtc_Word32 /*numberOfStreams */)
{
    CriticalSectionScoped cs(_crit);
    if (_prepared)
    {
        RemoveRenderer();
    }

    if (CreateLocalRenderer(width, height) == -1)
    {
        return -1;
    }

    return 0;
}

WebRtc_Word32 VideoX11Channel::DeliverFrame(unsigned char* buffer,
                                                WebRtc_Word32 bufferSize,
                                                unsigned WebRtc_Word32 /*timeStamp90kHz*/)
{
    CriticalSectionScoped cs(_crit);
    if (!_prepared)
    {
        return 0;
    }

    if (!dispArray[_dispCount])
    {
        return -1;
    }

    unsigned char *pBuf = buffer;
    // convert to RGB32
    ConvertI420ToARGB(pBuf, _buffer, _width, _height, 0);

    // put image in window
    XShmPutImage(_display, _window, _gc, _image, 0, 0, _xPos, _yPos, _width,
                 _height, True);

    // very important for the image to update properly!
    XSync(_display, false);

    return 0;

}

WebRtc_Word32 VideoX11Channel::GetFrameSize(WebRtc_Word32& width,
                                                WebRtc_Word32& height)
{
    width = _width;
    height = _height;

    return 0;
}

WebRtc_Word32 VideoX11Channel::Init(Window window, float left, float top,
                                        float right, float bottom)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(_crit);

    _window = window;
    _left = left;
    _right = _right;
    _top = top;
    _bottom = _bottom;

    _display = XOpenDisplay(NULL); // Use default display
    if (!_window || !_display)
    {
        return -1;
    }

    if (dispCount < DISP_MAX)
    {
        dispArray[dispCount] = _display;
        _dispCount = dispCount;
        dispCount++;
    }
    else
    {
        return -1;
    }

    if ((1 < left || left < 0) || (1 < top || top < 0) || (1 < right || right
            < 0) || (1 < bottom || bottom < 0))
    {
        return -1;
    }

    // calculate position and size of rendered video
    int x, y;
    unsigned int winWidth, winHeight, borderwidth, depth;
    Window rootret;
    if (XGetGeometry(_display, _window, &rootret, &x, &y, &winWidth,
                     &winHeight, &borderwidth, &depth) == 0)
    {
        return -1;
    }

    _xPos = (WebRtc_Word32) (winWidth * left);
    _yPos = (WebRtc_Word32) (winHeight * top);
    _outWidth = (WebRtc_Word32) (winWidth * (right - left));
    _outHeight = (WebRtc_Word32) (winHeight * (bottom - top));
    if (_outWidth % 2)
        _outWidth++; // the renderer want's sizes that are multiples of two
    if (_outHeight % 2)
        _outHeight++;

    if (CreateLocalRenderer(winWidth, winHeight) == -1)
    {
        return -1;
    }
    return 0;

}

WebRtc_Word32 VideoX11Channel::ChangeWindow(Window window)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(_crit);

    // Stop the rendering, if we are rendering...
    RemoveRenderer();
    _window = window;

    // calculate position and size of rendered video
    int x, y;
    unsigned int winWidth, winHeight, borderwidth, depth;
    Window rootret;
    if (XGetGeometry(_display, _window, &rootret, &x, &y, &winWidth,
                     &winHeight, &borderwidth, &depth) == -1)
    {
        return -1;
    }
    _xPos = (int) (winWidth * _left);
    _yPos = (int) (winHeight * _top);
    _outWidth = (int) (winWidth * (_right - _left));
    _outHeight = (int) (winHeight * (_bottom - _top));
    if (_outWidth % 2)
        _outWidth++; // the renderer want's sizes that are multiples of two
    if (_outHeight % 2)
        _outHeight++;

    // Prepare rendering using the
    if (CreateLocalRenderer(_width, _height) == -1)
    {
        return -1;
    }
    return 0;
}

WebRtc_Word32 VideoX11Channel::ReleaseWindow()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(_crit);

    return RemoveRenderer();

}

WebRtc_Word32 VideoX11Channel::CreateLocalRenderer(WebRtc_Word32 width,
                                                       WebRtc_Word32 height)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(_crit);

    if (!_window || !_display)
    {
        return -1;
    }

    if (_prepared)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _Id,
                     "Renderer already prepared, exits.");
        return -1;
    }

    _width = width;
    _height = height;

    // create a graphics context in the window
    _gc = XCreateGC(_display, _window, 0, 0);

    // create shared memory image
    _image = XShmCreateImage(_display, CopyFromParent, 24, ZPixmap, NULL,
                             &_shminfo, _width, _height); // this parameter needs to be the same for some reason.
    _shminfo.shmid = shmget(IPC_PRIVATE, (_image->bytes_per_line
            * _image->height), IPC_CREAT | 0777);
    _shminfo.shmaddr = _image->data = (char*) shmat(_shminfo.shmid, 0, 0);
    _buffer = (unsigned char*) _image->data;
    _shminfo.readOnly = False;

    // attach image to display
    if (!XShmAttach(_display, &_shminfo))
    {
        //printf("XShmAttach failed !\n");
        return -1;
    }

    _prepared = true;
    return 0;
}

WebRtc_Word32 VideoX11Channel::RemoveRenderer()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);

    if (!_prepared)
    {
        return 0;
    }
    _prepared = false;

    // free and closse Xwindow and XShm
    XShmDetach(_display, &_shminfo);
    XDestroyImage( _image );
    shmdt(_shminfo.shmaddr);

    return 0;
}

WebRtc_Word32 VideoX11Channel::GetStreamProperties(WebRtc_UWord32& zOrder,
                                                       float& left, float& top,
                                                       float& right,
                                                       float& bottom) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);

    zOrder = 0; // no z-order support yet
    left = _left;
    top = _top;
    right = _right;
    bottom = _bottom;

    return 0;
}


} //namespace webrtc


