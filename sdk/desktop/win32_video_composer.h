#pragma once

#include <vector>

#include "sdk/desktop/win32_video_renderer.h"

#include "rtc_base/thread.h"
#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif  // WEBRTC_WIN

namespace AvConf {

class Win32VideoComposer {
 public:
  Win32VideoComposer(void* wnd, int frame_interval_ms);
  ~Win32VideoComposer();

  void AddRenderer(Win32VideoRenderer* renderer);

  void RemoveRenderer(Win32VideoRenderer* renderer);

  void Lock() { ::EnterCriticalSection(&renderers_lock_); }

  void Unlock() { ::LeaveCriticalSection(&renderers_lock_); }

  void UpdateHwnd(void* wnd);

 private:
  void composer_loop();

  bool compose();

  void compose_one_renderer(HDC dc_mem, Win32VideoRenderer* renderer);

  HWND wnd_;
  int frame_interval_ms_;
  std::unique_ptr<rtc::Thread> compose_thread_;

  CRITICAL_SECTION renderers_lock_;
  std::vector<Win32VideoRenderer*> renderers_;
};

}  // namespace AvConf
