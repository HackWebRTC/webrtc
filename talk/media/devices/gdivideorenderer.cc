// libjingle
// Copyright 2004 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Implementation of GdiVideoRenderer on Windows

#ifdef WIN32

#include "talk/media/devices/gdivideorenderer.h"

#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/base/win32window.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/base/videoframe.h"

namespace cricket {

/////////////////////////////////////////////////////////////////////////////
// Definition of private class VideoWindow. We use a worker thread to manage
// the window.
/////////////////////////////////////////////////////////////////////////////
class GdiVideoRenderer::VideoWindow : public talk_base::Win32Window {
 public:
  VideoWindow(int x, int y, int width, int height);
  virtual ~VideoWindow();

  // Called when the video size changes. If it is called the first time, we
  // create and start the thread. Otherwise, we send kSetSizeMsg to the thread.
  // Context: non-worker thread.
  bool SetSize(int width, int height);

  // Called when a new frame is available. Upon this call, we send
  // kRenderFrameMsg to the window thread. Context: non-worker thread. It may be
  // better to pass RGB bytes to VideoWindow. However, we pass VideoFrame to put
  // all the thread synchronization within VideoWindow.
  bool RenderFrame(const VideoFrame* frame);

 protected:
  // Override virtual method of talk_base::Win32Window. Context: worker Thread.
  virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
                         LRESULT& result);

 private:
  enum { kSetSizeMsg = WM_USER, kRenderFrameMsg};

  class WindowThread : public talk_base::Thread {
   public:
    explicit WindowThread(VideoWindow* window) : window_(window) {}

    virtual ~WindowThread() {
      Stop();
    }

    // Override virtual method of talk_base::Thread. Context: worker Thread.
    virtual void Run() {
      // Initialize the window
      if (!window_ || !window_->Initialize()) {
        return;
      }
      // Run the message loop
      MSG msg;
      while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

  private:
    VideoWindow* window_;
  };

  // Context: worker Thread.
  bool Initialize();
  void OnPaint();
  void OnSize(int width, int height, bool frame_changed);
  void OnRenderFrame(const VideoFrame* frame);

  BITMAPINFO bmi_;
  talk_base::scoped_array<uint8> image_;
  talk_base::scoped_ptr<WindowThread> window_thread_;
  // The initial position of the window.
  int initial_x_;
  int initial_y_;
};

/////////////////////////////////////////////////////////////////////////////
// Implementation of class VideoWindow
/////////////////////////////////////////////////////////////////////////////
GdiVideoRenderer::VideoWindow::VideoWindow(
    int x, int y, int width, int height)
    : initial_x_(x),
      initial_y_(y) {
  memset(&bmi_.bmiHeader, 0, sizeof(bmi_.bmiHeader));
  bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi_.bmiHeader.biPlanes = 1;
  bmi_.bmiHeader.biBitCount = 32;
  bmi_.bmiHeader.biCompression = BI_RGB;
  bmi_.bmiHeader.biWidth = width;
  bmi_.bmiHeader.biHeight = -height;
  bmi_.bmiHeader.biSizeImage = width * height * 4;

  image_.reset(new uint8[bmi_.bmiHeader.biSizeImage]);
}

GdiVideoRenderer::VideoWindow::~VideoWindow() {
  // Context: caller Thread. We cannot call Destroy() since the window was
  // created by another thread. Instead, we send WM_CLOSE message.
  if (handle()) {
    SendMessage(handle(), WM_CLOSE, 0, 0);
  }
}

bool GdiVideoRenderer::VideoWindow::SetSize(int width, int height) {
  if (!window_thread_.get()) {
    // Create and start the window thread.
    window_thread_.reset(new WindowThread(this));
    return window_thread_->Start();
  } else if (width != bmi_.bmiHeader.biWidth ||
      height != -bmi_.bmiHeader.biHeight) {
    SendMessage(handle(), kSetSizeMsg, 0, MAKELPARAM(width, height));
  }
  return true;
}

bool GdiVideoRenderer::VideoWindow::RenderFrame(const VideoFrame* frame) {
  if (!handle()) {
    return false;
  }

  SendMessage(handle(), kRenderFrameMsg, reinterpret_cast<WPARAM>(frame), 0);
  return true;
}

bool GdiVideoRenderer::VideoWindow::OnMessage(UINT uMsg, WPARAM wParam,
                                              LPARAM lParam, LRESULT& result) {
  switch (uMsg) {
    case WM_PAINT:
      OnPaint();
      return true;

    case WM_DESTROY:
      PostQuitMessage(0);  // post WM_QUIT to end the message loop in Run()
      return false;

    case WM_SIZE:  // The window UI was resized.
      OnSize(LOWORD(lParam), HIWORD(lParam), false);
      return true;

    case kSetSizeMsg:  // The video resolution changed.
      OnSize(LOWORD(lParam), HIWORD(lParam), true);
      return true;

    case kRenderFrameMsg:
      OnRenderFrame(reinterpret_cast<const VideoFrame*>(wParam));
      return true;
  }
  return false;
}

bool GdiVideoRenderer::VideoWindow::Initialize() {
  if (!talk_base::Win32Window::Create(
      NULL, L"Video Renderer",
      WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
      WS_EX_APPWINDOW,
      initial_x_, initial_y_,
      bmi_.bmiHeader.biWidth, -bmi_.bmiHeader.biHeight)) {
        return false;
  }
  OnSize(bmi_.bmiHeader.biWidth, -bmi_.bmiHeader.biHeight, false);
  return true;
}

void GdiVideoRenderer::VideoWindow::OnPaint() {
  RECT rcClient;
  GetClientRect(handle(), &rcClient);
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(handle(), &ps);
  StretchDIBits(hdc,
    0, 0, rcClient.right, rcClient.bottom,  // destination rect
    0, 0, bmi_.bmiHeader.biWidth, -bmi_.bmiHeader.biHeight,  // source rect
    image_.get(), &bmi_, DIB_RGB_COLORS, SRCCOPY);
  EndPaint(handle(), &ps);
}

void GdiVideoRenderer::VideoWindow::OnSize(int width, int height,
                                           bool frame_changed) {
  // Get window and client sizes
  RECT rcClient, rcWindow;
  GetClientRect(handle(), &rcClient);
  GetWindowRect(handle(), &rcWindow);

  // Find offset between window size and client size
  POINT ptDiff;
  ptDiff.x = (rcWindow.right - rcWindow.left) - rcClient.right;
  ptDiff.y = (rcWindow.bottom - rcWindow.top) - rcClient.bottom;

  // Resize client
  MoveWindow(handle(), rcWindow.left, rcWindow.top,
             width + ptDiff.x, height + ptDiff.y, false);
  UpdateWindow(handle());
  ShowWindow(handle(), SW_SHOW);

  if (frame_changed && (width != bmi_.bmiHeader.biWidth ||
    height != -bmi_.bmiHeader.biHeight)) {
    // Update the bmi and image buffer
    bmi_.bmiHeader.biWidth = width;
    bmi_.bmiHeader.biHeight = -height;
    bmi_.bmiHeader.biSizeImage = width * height * 4;
    image_.reset(new uint8[bmi_.bmiHeader.biSizeImage]);
  }
}

void GdiVideoRenderer::VideoWindow::OnRenderFrame(const VideoFrame* frame) {
  if (!frame) {
    return;
  }
  // Convert frame to ARGB format, which is accepted by GDI
  frame->ConvertToRgbBuffer(cricket::FOURCC_ARGB, image_.get(),
                            bmi_.bmiHeader.biSizeImage,
                            bmi_.bmiHeader.biWidth * 4);
  InvalidateRect(handle(), 0, 0);
}

/////////////////////////////////////////////////////////////////////////////
// Implementation of class GdiVideoRenderer
/////////////////////////////////////////////////////////////////////////////
GdiVideoRenderer::GdiVideoRenderer(int x, int y)
    : initial_x_(x),
      initial_y_(y) {
}
GdiVideoRenderer::~GdiVideoRenderer() {}

bool GdiVideoRenderer::SetSize(int width, int height, int reserved) {
  if (!window_.get()) {  // Create the window for the first frame
    window_.reset(new VideoWindow(initial_x_, initial_y_, width, height));
  }
  return window_->SetSize(width, height);
}

bool GdiVideoRenderer::RenderFrame(const VideoFrame* frame) {
  if (!frame || !window_.get()) {
    return false;
  }
  return window_->RenderFrame(frame);
}

}  // namespace cricket
#endif  // WIN32
