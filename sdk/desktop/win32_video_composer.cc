#include "sdk/desktop/win32_video_composer.h"
#include "api/units/time_delta.h"

namespace AvConf {

Win32VideoComposer::Win32VideoComposer(void* wnd, int frame_interval_ms)
    : wnd_(reinterpret_cast<HWND>(wnd)),
      frame_interval_ms_(frame_interval_ms),
      compose_thread_(rtc::Thread::Create()) {
  ::InitializeCriticalSection(&renderers_lock_);
  compose_thread_->SetName("composer_thread", nullptr);
  RTC_CHECK(compose_thread_->Start()) << "Failed to start composer thread";
}

Win32VideoComposer::~Win32VideoComposer() {
  ::DeleteCriticalSection(&renderers_lock_);
}
void Win32VideoComposer::AddRenderer(Win32VideoRenderer* renderer) {
  AutoLock<Win32VideoComposer> lock(this);
  renderers_.push_back(renderer);

  compose_thread_->PostTask([this]() { composer_loop(); });
}

void Win32VideoComposer::RemoveRenderer(Win32VideoRenderer* renderer) {
  AutoLock<Win32VideoComposer> lock(this);
  std::vector<Win32VideoRenderer*>::const_iterator iter = std::find_if(
      renderers_.begin(), renderers_.end(),
      [renderer](const Win32VideoRenderer* r) { return r == renderer; });
  if (iter != renderers_.end()) {
    renderers_.erase(iter);
  }
}

void Win32VideoComposer::UpdateHwnd(void* wnd) {
  AutoLock<Win32VideoComposer> lock(this);
  wnd_ = reinterpret_cast<HWND>(wnd);
}

void Win32VideoComposer::composer_loop() {
  if (compose()) {
    compose_thread_->PostDelayedTask([this]() { composer_loop(); }, webrtc::TimeDelta::Millis(frame_interval_ms_));
  }
}

bool Win32VideoComposer::compose() {
  AutoLock<Win32VideoComposer> lock(this);
  if (renderers_.size() == 0) {
    return false;
  }

  for (auto renderer : renderers_) {
    renderer->Lock();
  }

  HDC hdc;
  hdc = GetDC(wnd_);

  HDC dc_mem = ::CreateCompatibleDC(hdc);
  ::SetStretchBltMode(dc_mem, HALFTONE);

  RECT canvas_rect_device;
  ::GetClientRect(wnd_, &canvas_rect_device);

  POINT canvas_lt_logical = {canvas_rect_device.left, canvas_rect_device.top};
  DPtoLP(hdc, &canvas_lt_logical, 1);
  POINT canvas_rb_logical = {canvas_rect_device.right,
                             canvas_rect_device.bottom};
  DPtoLP(hdc, &canvas_rb_logical, 1);
  RECT canvas_rect_logical = {canvas_lt_logical.x, canvas_lt_logical.y,
                              canvas_rb_logical.x, canvas_rb_logical.y};

  HBITMAP bmp_mem = ::CreateCompatibleBitmap(
      hdc, canvas_rect_logical.right - canvas_rect_logical.left,
      canvas_rect_logical.bottom - canvas_rect_logical.top);
  HGDIOBJ bmp_old = ::SelectObject(dc_mem, bmp_mem);

  HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
  ::FillRect(dc_mem, &canvas_rect_logical, brush);
  ::DeleteObject(brush);

  for (auto renderer : renderers_) {
    compose_one_renderer(dc_mem, renderer);
  }

  BitBlt(hdc, canvas_rect_logical.left, canvas_rect_logical.top,
         canvas_rect_logical.right - canvas_rect_logical.left,
         canvas_rect_logical.bottom - canvas_rect_logical.top, dc_mem, 0, 0,
         SRCCOPY);

  ::SelectObject(dc_mem, bmp_old);
  ::DeleteObject(bmp_mem);
  ::DeleteDC(dc_mem);

  ReleaseDC(wnd_, hdc);

  for (auto renderer : renderers_) {
    renderer->Unlock();
  }

  return true;
}
void Win32VideoComposer::compose_one_renderer(
    HDC dc_mem,
    Win32VideoRenderer* renderer) {
  const BITMAPINFO& bmi = renderer->bmi();
  int img_height = abs(bmi.bmiHeader.biHeight);
  int img_width = bmi.bmiHeader.biWidth;

  int width = renderer->width();
  int height = renderer->height();
  float render_ratio = width / (float)height;
  float img_ratio = img_width / (float)img_height;

  int dst_x = 0;
  int dst_y = 0;
  int dst_width = width;
  int dst_height = height;
  int src_x = 0;
  int src_y = 0;
  int src_width = img_width;
  int src_height = img_height;

  switch (renderer->scale_type()) {
    case SCALE_TYPE_CENTER_INSIDE:
      // center inside: draw full image, shrink render area left and right.
      // we use render area as reference, so shrink render area needn't
      // consider scale ratio
      if (render_ratio > img_ratio) {
        // image width is shorter
        dst_x = (int)(width - img_ratio * height) / 2;
        dst_width -= 2 * dst_x;
      } else {
        dst_y = (int)(height - width / img_ratio) / 2;
        dst_height -= 2 * dst_y;
      }
      break;
    case SCALE_TYPE_CENTER_CROP:
    default:
      // center crop: fill full render area, shrink image top and bottom.
      // we use render area as reference, so shrink image area need consider
      // scale ratio, we shrink image area to render area by multiply ratio,
      // so image shrink size need devide scale ratio
      if (render_ratio > img_ratio) {
        // image width is shorter
        float scale_ratio = width / (float)img_width;
        src_y = (int)(width / img_ratio - height) / 2 / scale_ratio;
        src_height -= 2 * src_y;
      } else {
        // image height is shorter
        float scale_ratio = height / (float)img_height;
        src_x = (int)(height * img_ratio - width) / 2 / scale_ratio;
        src_width -= 2 * src_x;
      }
      break;
  }
  StretchDIBits(dc_mem, dst_x + renderer->left(), dst_y + renderer->top(),
                dst_width, dst_height, src_x, src_y, src_width, src_height,
                renderer->image(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}
}  // namespace AvConf
