/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include <windows.h>

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_frame_win.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/differ.h"
#include "webrtc/modules/desktop_capture/mouse_cursor_shape.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/screen_capturer_helper.h"
#include "webrtc/modules/desktop_capture/win/desktop.h"
#include "webrtc/modules/desktop_capture/win/scoped_thread_desktop.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {

namespace {

// Constants from dwmapi.h.
const UINT DWM_EC_DISABLECOMPOSITION = 0;
const UINT DWM_EC_ENABLECOMPOSITION = 1;

typedef HRESULT (WINAPI * DwmEnableCompositionFunc)(UINT);

const wchar_t kDwmapiLibraryName[] = L"dwmapi.dll";

// Pixel colors used when generating cursor outlines.
const uint32_t kPixelBgraBlack = 0xff000000;
const uint32_t kPixelBgraWhite = 0xffffffff;
const uint32_t kPixelBgraTransparent = 0x00000000;

uint8_t AlphaMul(uint8_t v, uint8_t alpha) {
  return (static_cast<uint16_t>(v) * alpha) >> 8;
}

// ScreenCapturerWin captures 32bit RGB using GDI.
//
// ScreenCapturerWin is double-buffered as required by ScreenCapturer.
class ScreenCapturerWin : public ScreenCapturer {
 public:
  ScreenCapturerWin(bool disable_aero);
  virtual ~ScreenCapturerWin();

  // Overridden from ScreenCapturer:
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE;

 private:
  // Make sure that the device contexts match the screen configuration.
  void PrepareCaptureResources();

  // Captures the current screen contents into the current buffer.
  void CaptureImage();

  // Expand the cursor shape to add a white outline for visibility against
  // dark backgrounds.
  void AddCursorOutline(int width, int height, uint32_t* dst);

  // Capture the current cursor shape.
  void CaptureCursor();

  Callback* callback_;
  MouseShapeObserver* mouse_shape_observer_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // Snapshot of the last cursor bitmap we sent to the client. This is used
  // to diff against the current cursor so we only send a cursor-change
  // message when the shape has changed.
  MouseCursorShape last_cursor_;

  ScopedThreadDesktop desktop_;

  // GDI resources used for screen capture.
  HDC desktop_dc_;
  HDC memory_dc_;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue queue_;

  // Rectangle describing the bounds of the desktop device context.
  DesktopRect desktop_dc_rect_;

  // Class to calculate the difference between two screen bitmaps.
  scoped_ptr<Differ> differ_;

  HMODULE dwmapi_library_;
  DwmEnableCompositionFunc composition_func_;

  // Used to suppress duplicate logging of SetThreadExecutionState errors.
  bool set_thread_execution_state_failed_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWin);
};

ScreenCapturerWin::ScreenCapturerWin(bool disable_aero)
    : callback_(NULL),
      mouse_shape_observer_(NULL),
      desktop_dc_(NULL),
      memory_dc_(NULL),
      dwmapi_library_(NULL),
      composition_func_(NULL),
      set_thread_execution_state_failed_(false) {
  if (disable_aero) {
    // Load dwmapi.dll dynamically since it is not available on XP.
    if (!dwmapi_library_)
      dwmapi_library_ = LoadLibrary(kDwmapiLibraryName);

    if (dwmapi_library_) {
      composition_func_ = reinterpret_cast<DwmEnableCompositionFunc>(
          GetProcAddress(dwmapi_library_, "DwmEnableComposition"));
    }
  }
}

ScreenCapturerWin::~ScreenCapturerWin() {
  if (desktop_dc_)
    ReleaseDC(NULL, desktop_dc_);
  if (memory_dc_)
    DeleteDC(memory_dc_);

  // Restore Aero.
  if (composition_func_)
    (*composition_func_)(DWM_EC_ENABLECOMPOSITION);

  if (dwmapi_library_)
    FreeLibrary(dwmapi_library_);
}

void ScreenCapturerWin::Capture(const DesktopRegion& region) {
  TickTime capture_start_time = TickTime::Now();

  queue_.MoveToNextFrame();

  // Request that the system not power-down the system, or the display hardware.
  if (!SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED)) {
    if (!set_thread_execution_state_failed_) {
      set_thread_execution_state_failed_ = true;
      LOG_F(LS_WARNING) << "Failed to make system & display power assertion: "
                        << GetLastError();
    }
  }

  // Make sure the GDI capture resources are up-to-date.
  PrepareCaptureResources();

  // Copy screen bits to the current buffer.
  CaptureImage();

  const DesktopFrame* current_frame = queue_.current_frame();
  const DesktopFrame* last_frame = queue_.previous_frame();
  if (last_frame) {
    // Make sure the differencer is set up correctly for these previous and
    // current screens.
    if (!differ_.get() ||
        (differ_->width() != current_frame->size().width()) ||
        (differ_->height() != current_frame->size().height()) ||
        (differ_->bytes_per_row() != current_frame->stride())) {
      differ_.reset(new Differ(current_frame->size().width(),
                               current_frame->size().height(),
                               DesktopFrame::kBytesPerPixel,
                               current_frame->stride()));
    }

    // Calculate difference between the two last captured frames.
    DesktopRegion region;
    differ_->CalcDirtyRegion(last_frame->data(), current_frame->data(),
                             &region);
    helper_.InvalidateRegion(region);
  } else {
    // No previous frame is available. Invalidate the whole screen.
    helper_.InvalidateScreen(current_frame->size());
  }

  helper_.set_size_most_recent(current_frame->size());

  // Emit the current frame.
  DesktopFrame* frame = queue_.current_frame()->Share();
  frame->set_dpi(DesktopVector(
      GetDeviceCaps(desktop_dc_, LOGPIXELSX),
      GetDeviceCaps(desktop_dc_, LOGPIXELSY)));
  frame->mutable_updated_region()->Clear();
  helper_.TakeInvalidRegion(frame->mutable_updated_region());
  frame->set_capture_time_ms(
      (TickTime::Now() - capture_start_time).Milliseconds());
  callback_->OnCaptureCompleted(frame);

  // Check for cursor shape update.
  CaptureCursor();
}

void ScreenCapturerWin::SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) {
  assert(!mouse_shape_observer_);
  assert(mouse_shape_observer);

  mouse_shape_observer_ = mouse_shape_observer;
}

void ScreenCapturerWin::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;

  // Vote to disable Aero composited desktop effects while capturing. Windows
  // will restore Aero automatically if the process exits. This has no effect
  // under Windows 8 or higher.  See crbug.com/124018.
  if (composition_func_)
    (*composition_func_)(DWM_EC_DISABLECOMPOSITION);
}

void ScreenCapturerWin::PrepareCaptureResources() {
  // Switch to the desktop receiving user input if different from the current
  // one.
  scoped_ptr<Desktop> input_desktop(Desktop::GetInputDesktop());
  if (input_desktop.get() != NULL && !desktop_.IsSame(*input_desktop)) {
    // Release GDI resources otherwise SetThreadDesktop will fail.
    if (desktop_dc_) {
      ReleaseDC(NULL, desktop_dc_);
      desktop_dc_ = NULL;
    }

    if (memory_dc_) {
      DeleteDC(memory_dc_);
      memory_dc_ = NULL;
    }

    // If SetThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from the wrong desktop.
    desktop_.SetThreadDesktop(input_desktop.release());

    // Re-assert our vote to disable Aero.
    // See crbug.com/124018 and crbug.com/129906.
    if (composition_func_ != NULL) {
      (*composition_func_)(DWM_EC_DISABLECOMPOSITION);
    }
  }

  // If the display bounds have changed then recreate GDI resources.
  // TODO(wez): Also check for pixel format changes.
  DesktopRect screen_rect(DesktopRect::MakeXYWH(
      GetSystemMetrics(SM_XVIRTUALSCREEN),
      GetSystemMetrics(SM_YVIRTUALSCREEN),
      GetSystemMetrics(SM_CXVIRTUALSCREEN),
      GetSystemMetrics(SM_CYVIRTUALSCREEN)));
  if (!screen_rect.equals(desktop_dc_rect_)) {
    if (desktop_dc_) {
      ReleaseDC(NULL, desktop_dc_);
      desktop_dc_ = NULL;
    }
    if (memory_dc_) {
      DeleteDC(memory_dc_);
      memory_dc_ = NULL;
    }
    desktop_dc_rect_ = DesktopRect();
  }

  if (desktop_dc_ == NULL) {
    assert(memory_dc_ == NULL);

    // Create GDI device contexts to capture from the desktop into memory.
    desktop_dc_ = GetDC(NULL);
    if (!desktop_dc_)
      abort();
    memory_dc_ = CreateCompatibleDC(desktop_dc_);
    if (!memory_dc_)
      abort();
    desktop_dc_rect_ = screen_rect;

    // Make sure the frame buffers will be reallocated.
    queue_.Reset();

    helper_.ClearInvalidRegion();
  }
}

void ScreenCapturerWin::CaptureImage() {
  // If the current buffer is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame()) {
    assert(desktop_dc_ != NULL);
    assert(memory_dc_ != NULL);

    DesktopSize size = DesktopSize(
        desktop_dc_rect_.width(), desktop_dc_rect_.height());

    size_t buffer_size = size.width() * size.height() *
        DesktopFrame::kBytesPerPixel;
    SharedMemory* shared_memory =
        callback_->CreateSharedMemory(buffer_size);
    scoped_ptr<DesktopFrameWin> buffer(
        DesktopFrameWin::Create(size, shared_memory, desktop_dc_));
    queue_.ReplaceCurrentFrame(buffer.release());
  }

  // Select the target bitmap into the memory dc and copy the rect from desktop
  // to memory.
  DesktopFrameWin* current = static_cast<DesktopFrameWin*>(
      queue_.current_frame()->GetUnderlyingFrame());
  HGDIOBJ previous_object = SelectObject(memory_dc_, current->bitmap());
  if (previous_object != NULL) {
    BitBlt(memory_dc_,
           0, 0, desktop_dc_rect_.width(), desktop_dc_rect_.height(),
           desktop_dc_,
           desktop_dc_rect_.left(), desktop_dc_rect_.top(),
           SRCCOPY | CAPTUREBLT);

    // Select back the previously selected object to that the device contect
    // could be destroyed independently of the bitmap if needed.
    SelectObject(memory_dc_, previous_object);
  }
}

void ScreenCapturerWin::AddCursorOutline(int width,
                                         int height,
                                         uint32_t* dst) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      // If this is a transparent pixel (bgr == 0 and alpha = 0), check the
      // neighbor pixels to see if this should be changed to an outline pixel.
      if (*dst == kPixelBgraTransparent) {
        // Change to white pixel if any neighbors (top, bottom, left, right)
        // are black.
        if ((y > 0 && dst[-width] == kPixelBgraBlack) ||
            (y < height - 1 && dst[width] == kPixelBgraBlack) ||
            (x > 0 && dst[-1] == kPixelBgraBlack) ||
            (x < width - 1 && dst[1] == kPixelBgraBlack)) {
          *dst = kPixelBgraWhite;
        }
      }
      dst++;
    }
  }
}

void ScreenCapturerWin::CaptureCursor() {
  CURSORINFO cursor_info;
  cursor_info.cbSize = sizeof(CURSORINFO);
  if (!GetCursorInfo(&cursor_info)) {
    LOG_F(LS_INFO) << "Unable to get cursor info. Error = " << GetLastError();
    return;
  }

  // Note that this does not need to be freed.
  HCURSOR hcursor = cursor_info.hCursor;
  ICONINFO iinfo;
  if (!GetIconInfo(hcursor, &iinfo)) {
    LOG_F(LS_INFO) << "Unable to get cursor icon info. Error = "
                    << GetLastError();
    return;
  }
  int hotspot_x = iinfo.xHotspot;
  int hotspot_y = iinfo.yHotspot;

  // Get the cursor bitmap.
  HBITMAP hbitmap;
  BITMAP bitmap;
  bool color_bitmap;
  if (iinfo.hbmColor) {
    // Color cursor bitmap.
    color_bitmap = true;
    hbitmap = reinterpret_cast<HBITMAP>(
        CopyImage(iinfo.hbmColor, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    if (!hbitmap) {
      LOG_F(LS_INFO) << "Unable to copy color cursor image. Error = "
                       << GetLastError();
      return;
    }

    // Free the color and mask bitmaps since we only need our copy.
    DeleteObject(iinfo.hbmColor);
    DeleteObject(iinfo.hbmMask);
  } else {
    // Black and white (xor) cursor.
    color_bitmap = false;
    hbitmap = iinfo.hbmMask;
  }

  if (!GetObject(hbitmap, sizeof(BITMAP), &bitmap)) {
    LOG_F(LS_INFO) << "Unable to get cursor bitmap. Error = " << GetLastError();
    DeleteObject(hbitmap);
    return;
  }

  int width = bitmap.bmWidth;
  int height = bitmap.bmHeight;
  // For non-color cursors, the mask contains both an AND and an XOR mask and
  // the height includes both. Thus, the width is correct, but we need to
  // divide by 2 to get the correct mask height.
  if (!color_bitmap) {
    height /= 2;
  }
  int data_size = height * width * DesktopFrame::kBytesPerPixel;

  scoped_ptr<MouseCursorShape> cursor(new MouseCursorShape());
  cursor->data.resize(data_size);
  uint8_t* cursor_dst_data =
      reinterpret_cast<uint8_t*>(&*(cursor->data.begin()));

  // Copy/convert cursor bitmap into format needed by chromotocol.
  int row_bytes = bitmap.bmWidthBytes;
  if (color_bitmap) {
    if (bitmap.bmPlanes != 1 || bitmap.bmBitsPixel != 32) {
      LOG_F(LS_INFO) << "Unsupported color cursor format. Error = "
                     << GetLastError();
      DeleteObject(hbitmap);
      return;
    }

    // Copy across colour cursor imagery.
    // MouseCursorShape stores imagery top-down, and premultiplied
    // by the alpha channel, whereas windows stores them bottom-up
    // and not premultiplied.
    uint8_t* cursor_src_data = reinterpret_cast<uint8_t*>(bitmap.bmBits);
    uint8_t* src = cursor_src_data + ((height - 1) * row_bytes);
    uint8_t* dst = cursor_dst_data;
    for (int row = 0; row < height; ++row) {
      for (int column = 0; column < width; ++column) {
        dst[0] = AlphaMul(src[0], src[3]);
        dst[1] = AlphaMul(src[1], src[3]);
        dst[2] = AlphaMul(src[2], src[3]);
        dst[3] = src[3];
        dst += DesktopFrame::kBytesPerPixel;
        src += DesktopFrame::kBytesPerPixel;
      }
      src -= row_bytes + (width * DesktopFrame::kBytesPerPixel);
    }
  } else {
    if (bitmap.bmPlanes != 1 || bitmap.bmBitsPixel != 1) {
      LOG(LS_VERBOSE) << "Unsupported cursor mask format. Error = "
                      << GetLastError();
      DeleteObject(hbitmap);
      return;
    }

    // x2 because there are 2 masks in the bitmap: AND and XOR.
    int mask_bytes = height * row_bytes * 2;
    scoped_array<uint8_t> mask(new uint8_t[mask_bytes]);
    if (!GetBitmapBits(hbitmap, mask_bytes, mask.get())) {
      LOG(LS_VERBOSE) << "Unable to get cursor mask bits. Error = "
                      << GetLastError();
      DeleteObject(hbitmap);
      return;
    }
    uint8_t* and_mask = mask.get();
    uint8_t* xor_mask = mask.get() + height * row_bytes;
    uint8_t* dst = cursor_dst_data;
    bool add_outline = false;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        int byte = y * row_bytes + x / 8;
        int bit = 7 - x % 8;
        int and_bit = and_mask[byte] & (1 << bit);
        int xor_bit = xor_mask[byte] & (1 << bit);

        // The two cursor masks combine as follows:
        //  AND  XOR   Windows Result  Our result   RGB  Alpha
        //   0    0    Black           Black         00    ff
        //   0    1    White           White         ff    ff
        //   1    0    Screen          Transparent   00    00
        //   1    1    Reverse-screen  Black         00    ff
        // Since we don't support XOR cursors, we replace the "Reverse Screen"
        // with black. In this case, we also add an outline around the cursor
        // so that it is visible against a dark background.
        int rgb = (!and_bit && xor_bit) ? 0xff : 0x00;
        int alpha = (and_bit && !xor_bit) ? 0x00 : 0xff;
        *dst++ = rgb;
        *dst++ = rgb;
        *dst++ = rgb;
        *dst++ = alpha;
        if (and_bit && xor_bit) {
          add_outline = true;
        }
      }
    }
    if (add_outline) {
      AddCursorOutline(width, height,
                       reinterpret_cast<uint32_t*>(cursor_dst_data));
    }
  }

  DeleteObject(hbitmap);

  cursor->size.set(width, height);
  cursor->hotspot.set(hotspot_x, hotspot_y);

  // Compare the current cursor with the last one we sent to the client. If
  // they're the same, then don't bother sending the cursor again.
  if (last_cursor_.size.equals(cursor->size) &&
      last_cursor_.hotspot.equals(cursor->hotspot) &&
      last_cursor_.data == cursor->data) {
    return;
  }

  LOG(LS_VERBOSE) << "Sending updated cursor: " << width << "x" << height;

  // Record the last cursor image that we sent to the client.
  last_cursor_ = *cursor;

  if (mouse_shape_observer_)
    mouse_shape_observer_->OnCursorShapeChanged(cursor.release());
}

}  // namespace

// static
ScreenCapturer* ScreenCapturer::Create() {
  return CreateWithDisableAero(true);
}

// static
ScreenCapturer* ScreenCapturer::CreateWithDisableAero(bool disable_aero) {
  return new ScreenCapturerWin(disable_aero);
}

}  // namespace webrtc
