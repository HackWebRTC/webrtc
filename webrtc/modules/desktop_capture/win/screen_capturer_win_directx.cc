/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"

#include <string.h>

#include <comdef.h>
#include <wincodec.h>
#include <wingdi.h>
#include <DXGI.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/win/screen_capture_utils.h"
#include "webrtc/system_wrappers/include/atomic32.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

using Microsoft::WRL::ComPtr;

namespace {

// Timeout for AcquireNextFrame() call.
const int kAcquireTimeoutMs = 10;

// Wait time between two DuplicateOutput operations, DuplicateOutput may fail if
// display mode is changing.
const int kDuplicateOutputWaitMs = 50;

// How many times we attempt to DuplicateOutput before returning an error to
// upstream components.
const int kDuplicateOutputAttempts = 10;

rtc::GlobalLockPod g_initialize_lock;

// A container of all the objects we need to call Windows API. Note, one
// application can only have one IDXGIOutputDuplication instance, that's the
// reason the container is singleton.
struct DxgiContainer {
  rtc::CriticalSection duplication_lock;
  rtc::CriticalSection acquire_lock;
  bool initialize_result GUARDED_BY(g_initialize_lock) = false;
  ID3D11Device* device GUARDED_BY(g_initialize_lock) = nullptr;
  ID3D11DeviceContext* context GUARDED_BY(g_initialize_lock) = nullptr;
  IDXGIOutput1* output1 GUARDED_BY(g_initialize_lock) = nullptr;
  ComPtr<IDXGIOutputDuplication> duplication
      GUARDED_BY(duplication_lock);
  DXGI_OUTDUPL_DESC duplication_desc;
  std::vector<uint8_t> metadata GUARDED_BY(acquire_lock);
};

DxgiContainer* g_container GUARDED_BY(g_initialize_lock);

}  // namespace

// A pair of an ID3D11Texture2D and an IDXGISurface. We need an
// ID3D11Texture2D instance to copy GPU texture to RAM, but an IDXGISurface to
// map the texture into a bitmap buffer. These two instances are always
// pointing to a same object.
// This class also has two DesktopRegions, one is the updated region from
// returned from Windows API, the other is the region intersects with the
// updated region of last frame.
//
// This class is not thread safe.
class ScreenCapturerWinDirectx::Texture {
 public:
  // Copy a frame represented by frame_info and resource. Returns false if
  // anything wrong.
  bool CopyFrom(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                IDXGIResource* resource,
                const DesktopRegion& last_updated_region) {
    if (!resource || frame_info.AccumulatedFrames == 0) {
      // Nothing updated, but current data is still valid.
      return false;
    }

    ComPtr<ID3D11Texture2D> texture;
    _com_error error = resource->QueryInterface(
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(texture.GetAddressOf()));
    if (error.Error() != S_OK || !texture) {
      LOG(LS_ERROR) << "Failed to convert IDXGIResource to ID3D11Texture2D, "
                       "error "
                    << error.ErrorMessage() << ", code " << error.Error();
      return false;
    }

    // AcquireNextFrame returns a CPU inaccessible IDXGIResource, so we need to
    // make a copy.
    if (!InitializeStage(texture.Get())) {
      return false;
    }

    updated_region_.Clear();
    if (!DetectUpdatedRegion(frame_info, &updated_region_)) {
      updated_region_.SetRect(DesktopRect::MakeSize(size()));
    }
    // We need to copy changed area in both this frame and last frame, since
    // currently this frame stores the bitmap of the one before last frame.
    copied_region_.Clear();
    copied_region_.AddRegion(updated_region_);
    copied_region_.AddRegion(last_updated_region);
    copied_region_.IntersectWith(DesktopRect::MakeSize(size()));

    for (DesktopRegion::Iterator it(copied_region_);
         !it.IsAtEnd();
         it.Advance()) {
      D3D11_BOX box;
      box.left = it.rect().left();
      box.top = it.rect().top();
      box.right = it.rect().right();
      box.bottom = it.rect().bottom();
      box.front = 0;
      box.back = 1;
      g_container->context->CopySubresourceRegion(
          static_cast<ID3D11Resource*>(stage_.Get()),
          0, it.rect().left(), it.rect().top(), 0,
          static_cast<ID3D11Resource*>(texture.Get()),
          0, &box);
    }

    rect_ = {0};
    error = _com_error(surface_->Map(&rect_, DXGI_MAP_READ));
    if (error.Error() != S_OK) {
      rect_ = {0};
      LOG(LS_ERROR) << "Failed to map the IDXGISurface to a bitmap, error "
                    << error.ErrorMessage() << ", code " << error.Error();
      return false;
    }

    // surface_->Unmap() will be called next time we capture an image to avoid
    // memory copy without shared_memory.
    return true;
  }

  uint8_t* bits() const { return static_cast<uint8_t*>(rect_.pBits); }
  int pitch() const { return static_cast<int>(rect_.Pitch); }
  const DesktopSize& size() const { return size_; }
  const DesktopVector& dpi() const { return dpi_; }

  int32_t AddRef() {
    return ++ref_count_;
  }

  int32_t Release() {
    int32_t ref_count;
    ref_count = --ref_count_;
    if (ref_count == 0) {
      delete this;
    }
    return ref_count;
  }

  const DesktopRegion& updated_region() {
    return updated_region_;
  }

  const DesktopRegion& copied_region() {
    return copied_region_;
  }

 private:
  // Texture should only be deleted by Release function.
  ~Texture() = default;

  // Initializes stage_ from a CPU inaccessible IDXGIResource. Returns false
  // if it fails to execute windows api.
  bool InitializeStage(ID3D11Texture2D* texture) {
    RTC_DCHECK(texture);
    D3D11_TEXTURE2D_DESC desc = {0};
    texture->GetDesc(&desc);
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    if (stage_) {
      // Make sure stage_ and surface_ are always pointing to a same object.
      // We need an ID3D11Texture2D instance for
      // ID3D11DeviceContext::CopySubresourceRegion, but an IDXGISurface for
      // IDXGISurface::Map.
      {
        ComPtr<IUnknown> left;
        ComPtr<IUnknown> right;
        bool left_result = SUCCEEDED(stage_.As(&left));
        bool right_result = SUCCEEDED(surface_.As(&right));
        RTC_DCHECK(left_result);
        RTC_DCHECK(right_result);
        RTC_DCHECK(left.Get() == right.Get());
      }

      // This buffer should be used already.
      _com_error error = _com_error(surface_->Unmap());
      if (error.Error() == S_OK) {
        D3D11_TEXTURE2D_DESC current_desc;
        stage_->GetDesc(&current_desc);
        if (memcmp(&desc, &current_desc, sizeof(D3D11_TEXTURE2D_DESC)) == 0) {
          return true;
        }
      } else {
        // Let's recreate stage_ and surface_ later.
        LOG(LS_ERROR) << "Failed to unmap surface, error "
                      << error.ErrorMessage() << ", code " << error.Error();
      }

      stage_.Reset();
      surface_.Reset();
    } else {
      RTC_DCHECK(!surface_);
    }

    HDC hdc = GetDC(nullptr);
    // Use old DPI value if failed.
    if (hdc != nullptr) {
      dpi_.set(GetDeviceCaps(hdc, LOGPIXELSX), GetDeviceCaps(hdc, LOGPIXELSY));
      ReleaseDC(nullptr, hdc);
    }

    _com_error error = _com_error(g_container->device->CreateTexture2D(
        &desc, nullptr, stage_.GetAddressOf()));
    if (error.Error() != S_OK || !stage_) {
      LOG(LS_ERROR) << "Failed to create a new ID3D11Texture2D as stage, "
                       "error "
                    << error.ErrorMessage() << ", code " << error.Error();
      return false;
    }

    error = _com_error(stage_.As(&surface_));
    if (error.Error() != S_OK || !surface_) {
      LOG(LS_ERROR) << "Failed to convert ID3D11Texture2D to IDXGISurface, "
                       "error "
                    << error.ErrorMessage() << ", code " << error.Error();
      return false;
    }

    size_.set(desc.Width, desc.Height);
    return true;
  }

  ComPtr<ID3D11Texture2D> stage_;
  ComPtr<IDXGISurface> surface_;
  DXGI_MAPPED_RECT rect_;
  DesktopSize size_;
  Atomic32 ref_count_;
  // The updated region from Windows API.
  DesktopRegion updated_region_;
  // Combination of updated regions from both current frame and previous frame.
  DesktopRegion copied_region_;
  // The DPI of current frame.
  DesktopVector dpi_;
};

// A DesktopFrame which does not own the data buffer, and also does not have
// shared memory. This uses in IT2ME scenario only.
class ScreenCapturerWinDirectx::DxgiDesktopFrame : public DesktopFrame {
 public:
  DxgiDesktopFrame(
      const rtc::scoped_refptr<ScreenCapturerWinDirectx::Texture>& texture)
      : DesktopFrame(texture.get()->size(),
                     texture.get()->pitch(),
                     texture.get()->bits(),
                     nullptr),
        texture_(texture) {
    set_dpi(texture->dpi());
  }

  virtual ~DxgiDesktopFrame() {}

 private:
  // Keep a reference to the Texture instance to make sure we can still access
  // its bytes array.
  rtc::scoped_refptr<ScreenCapturerWinDirectx::Texture> texture_;
};

bool ScreenCapturerWinDirectx::Initialize() {
  if (!g_container) {
    rtc::GlobalLockScope lock(&g_initialize_lock);
    if (!g_container) {
      g_container = new DxgiContainer();
      g_container->initialize_result = DoInitialize();
      if (g_container->initialize_result) {
        return true;
      }

      // Clean up if DirectX cannot work on the system.
      if (g_container->duplication) {
        g_container->duplication.Reset();
      }

      if (g_container->output1) {
        g_container->output1->Release();
        g_container->output1 = nullptr;
      }

      if (g_container->context) {
        g_container->context->Release();
        g_container->context = nullptr;
      }

      if (g_container->device) {
        g_container->device->Release();
        g_container->device = nullptr;
      }

      return false;
    }
  }

  return g_container->initialize_result;
}

bool ScreenCapturerWinDirectx::DoInitialize() {
  D3D_FEATURE_LEVEL feature_level;
  _com_error error = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED,
      nullptr, 0, D3D11_SDK_VERSION, &g_container->device, &feature_level,
      &g_container->context);
  if (error.Error() != S_OK || !g_container->device || !g_container->context) {
    LOG(LS_WARNING) << "D3D11CreateDeivce returns error "
                    << error.ErrorMessage() << " with code " << error.Error();
    return false;
  }

  if (feature_level < D3D_FEATURE_LEVEL_11_0) {
    LOG(LS_WARNING) << "D3D11CreateDevice returns an instance without DirectX "
                       "11 support, level "
                    << feature_level;
    return false;
  }

  ComPtr<IDXGIDevice> device;
  error = _com_error(g_container->device->QueryInterface(
      __uuidof(IDXGIDevice), reinterpret_cast<void**>(device.GetAddressOf())));
  if (error.Error() != S_OK || !device) {
    LOG(LS_WARNING) << "ID3D11Device is not an implementation of IDXGIDevice, "
                       "this usually means the system does not support DirectX "
                       "11";
    return false;
  }

  ComPtr<IDXGIAdapter> adapter;
  error = _com_error(device->GetAdapter(adapter.GetAddressOf()));
  if (error.Error() != S_OK || !adapter) {
    LOG(LS_WARNING) << "Failed to get an IDXGIAdapter implementation from "
                       "IDXGIDevice.";
    return false;
  }

  ComPtr<IDXGIOutput> output;
  for (int i = 0;; i++) {
    error = _com_error(adapter->EnumOutputs(i, output.GetAddressOf()));
    if (error.Error() == DXGI_ERROR_NOT_FOUND) {
      LOG(LS_WARNING) << "No output detected.";
      return false;
    }
    if (error.Error() == S_OK && output) {
      DXGI_OUTPUT_DESC desc;
      error = _com_error(output->GetDesc(&desc));
      if (error.Error() == S_OK) {
        if (desc.AttachedToDesktop) {
          // Current output instance is the device attached to desktop.
          break;
        }
      } else {
        LOG(LS_WARNING) << "Failed to get output description of device " << i
                        << ", ignore.";
      }
    }
  }

  RTC_DCHECK(output);
  error = _com_error(output.CopyTo(
      __uuidof(IDXGIOutput1), reinterpret_cast<void**>(&g_container->output1)));
  if (error.Error() != S_OK || !g_container->output1) {
    LOG(LS_WARNING) << "Failed to convert IDXGIOutput to IDXGIOutput1, this "
                       "usually means the system does not support DirectX 11";
    return false;
  }

  // When we are initializing the DXGI, retrying several times to avoid any
  // temporary issue, such as display mode changing, to block us from using
  // DXGI based capturer.
  for (int i = 0; i < kDuplicateOutputAttempts; i++) {
    if (DuplicateOutput()) {
      return true;
    }
    Sleep(kDuplicateOutputWaitMs);
  }
  return false;
}

bool ScreenCapturerWinDirectx::DuplicateOutput() {
  // We are updating the instance.
  rtc::CritScope lock(&g_container->duplication_lock);
  // Make sure nobody is using current instance.
  rtc::CritScope lock2(&g_container->acquire_lock);
  if (g_container->duplication) {
    return true;
  }

  _com_error error = g_container->output1->DuplicateOutput(
      static_cast<IUnknown*>(g_container->device),
      g_container->duplication.GetAddressOf());
  if (error.Error() != S_OK || !g_container->duplication) {
    g_container->duplication.Reset();
    LOG(LS_WARNING) << "Failed to duplicate output from IDXGIOutput1, error "
                    << error.ErrorMessage() << ", with code "
                    << error.Error();
    return false;
  }

  memset(&g_container->duplication_desc, 0, sizeof(DXGI_OUTDUPL_DESC));
  g_container->duplication->GetDesc(&g_container->duplication_desc);
  if (g_container->duplication_desc.ModeDesc.Format !=
      DXGI_FORMAT_B8G8R8A8_UNORM) {
    g_container->duplication.Reset();
    LOG(LS_ERROR) << "IDXGIDuplicateOutput does not use RGBA (8 bit) "
                     "format, which is required by downstream components, "
                     "format is "
                  << g_container->duplication_desc.ModeDesc.Format;
    return false;
  }

  return true;
}

bool ScreenCapturerWinDirectx::ForceDuplicateOutput() {
  // We are updating the instance.
  rtc::CritScope lock(&g_container->duplication_lock);
  // Make sure nobody is using current instance.
  rtc::CritScope lock2(&g_container->acquire_lock);

  if (g_container->duplication) {
    g_container->duplication->ReleaseFrame();
    g_container->duplication.Reset();
  }

  return DuplicateOutput();
}

ScreenCapturerWinDirectx::ScreenCapturerWinDirectx(
    const DesktopCaptureOptions& options)
    : callback_(nullptr), set_thread_execution_state_failed_(false) {
  RTC_DCHECK(g_container && g_container->initialize_result);

  // Texture instance won't change forever.
  while (!surfaces_.current_frame()) {
    surfaces_.ReplaceCurrentFrame(std::unique_ptr<rtc::scoped_refptr<Texture>>(
        new rtc::scoped_refptr<Texture>(new Texture())));
    surfaces_.MoveToNextFrame();
  }
}

ScreenCapturerWinDirectx::~ScreenCapturerWinDirectx() {}

void ScreenCapturerWinDirectx::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void ScreenCapturerWinDirectx::SetSharedMemoryFactory(
    std::unique_ptr<SharedMemoryFactory> shared_memory_factory) {
  shared_memory_factory_ = std::move(shared_memory_factory);
}

bool ScreenCapturerWinDirectx::HandleDetectUpdatedRegionError(
    const _com_error& error,
    const char* stage) {
  if (error.Error() != S_OK) {
    if (error.Error() == DXGI_ERROR_ACCESS_LOST) {
      ForceDuplicateOutput();
    } else {
      LOG(LS_ERROR) << "Failed to get " << stage << " rectangles, error "
                    << error.ErrorMessage() << ", code " << error.Error();
    }
    // Send entire desktop as we cannot get dirty or move rectangles.
    return false;
  }

  return true;
}

bool ScreenCapturerWinDirectx::DetectUpdatedRegion(
    const DXGI_OUTDUPL_FRAME_INFO& frame_info,
    DesktopRegion* updated_region) {
  RTC_DCHECK(g_container->duplication);
  RTC_DCHECK(updated_region);
  updated_region->Clear();
  if (frame_info.TotalMetadataBufferSize == 0) {
    // This should not happen, since frame_info.AccumulatedFrames > 0.
    LOG(LS_ERROR) << "frame_info.AccumulatedFrames > 0, "
                     "but TotalMetadataBufferSize == 0";
    return false;
  }

  if (g_container->metadata.capacity() < frame_info.TotalMetadataBufferSize) {
    g_container->metadata.clear();  // Avoid data copy
    g_container->metadata.reserve(frame_info.TotalMetadataBufferSize);
  }

  UINT buff_size = 0;
  DXGI_OUTDUPL_MOVE_RECT* move_rects =
      reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(g_container->metadata.data());
  size_t move_rects_count = 0;
  _com_error error = _com_error(g_container->duplication->GetFrameMoveRects(
      static_cast<UINT>(g_container->metadata.capacity()),
      move_rects, &buff_size));
  if (!HandleDetectUpdatedRegionError(error, "move")) {
    return false;
  }
  move_rects_count = buff_size / sizeof(DXGI_OUTDUPL_MOVE_RECT);

  RECT* dirty_rects =
      reinterpret_cast<RECT*>(g_container->metadata.data() + buff_size);
  size_t dirty_rects_count = 0;
  error = _com_error(g_container->duplication->GetFrameDirtyRects(
      static_cast<UINT>(g_container->metadata.capacity()) - buff_size,
      dirty_rects, &buff_size));
  if (!HandleDetectUpdatedRegionError(error, "dirty")) {
    return false;
  }
  dirty_rects_count = buff_size / sizeof(RECT);

  while (move_rects_count > 0) {
    updated_region->AddRect(DesktopRect::MakeXYWH(
        move_rects->SourcePoint.x, move_rects->SourcePoint.y,
        move_rects->DestinationRect.right - move_rects->DestinationRect.left,
        move_rects->DestinationRect.bottom - move_rects->DestinationRect.top));
    updated_region->AddRect(DesktopRect::MakeLTRB(
        move_rects->DestinationRect.left, move_rects->DestinationRect.top,
        move_rects->DestinationRect.right, move_rects->DestinationRect.bottom));
    move_rects++;
    move_rects_count--;
  }

  while (dirty_rects_count > 0) {
    updated_region->AddRect(DesktopRect::MakeLTRB(
        dirty_rects->left, dirty_rects->top,
        dirty_rects->right, dirty_rects->bottom));
    dirty_rects++;
    dirty_rects_count--;
  }

  return true;
}

std::unique_ptr<DesktopFrame> ScreenCapturerWinDirectx::ProcessFrame(
    const DXGI_OUTDUPL_FRAME_INFO& frame_info,
    IDXGIResource* resource) {
  RTC_DCHECK(resource);
  RTC_DCHECK(frame_info.AccumulatedFrames > 0);
  // We have something to update, so move to next surface.
  surfaces_.MoveToNextFrame();
  if (shared_memory_factory_) {
    // Make sure frames_ and surfaces_ are synchronized if we are using both.
    frames_.MoveToNextFrame();
  }
  RTC_DCHECK(surfaces_.current_frame());
  if (!surfaces_.current_frame()->get()->CopyFrom(frame_info, resource,
          surfaces_.previous_frame()->get()->updated_region())) {
    return std::unique_ptr<DesktopFrame>();
  }

  std::unique_ptr<DesktopFrame> result;
  if (shared_memory_factory_) {
    // When using shared memory, |frames_| is used to store a queue of
    // SharedMemoryDesktopFrame's.
    if (!frames_.current_frame() ||
        !frames_.current_frame()->size().equals(
            surfaces_.current_frame()->get()->size())) {
      // Current frame does not have a same size as last captured surface.
      std::unique_ptr<DesktopFrame> new_frame =
          SharedMemoryDesktopFrame::Create(
              surfaces_.current_frame()->get()->size(),
              shared_memory_factory_.get());
      if (!new_frame) {
        LOG(LS_ERROR) << "Failed to allocate a new SharedMemoryDesktopFrame";
        return std::unique_ptr<DesktopFrame>();
      }
      frames_.ReplaceCurrentFrame(
          SharedDesktopFrame::Wrap(std::move(new_frame)));
    }
    result = frames_.current_frame()->Share();

    std::unique_ptr<DesktopFrame> frame(
        new DxgiDesktopFrame(*surfaces_.current_frame()));
    // Copy data into SharedMemory.
    for (DesktopRegion::Iterator it(
             surfaces_.current_frame()->get()->copied_region());
         !it.IsAtEnd();
         it.Advance()) {
      result->CopyPixelsFrom(*frame, it.rect().top_left(), it.rect());
    }
    result->set_dpi(frame->dpi());
  } else {
    result.reset(new DxgiDesktopFrame(*surfaces_.current_frame()));
  }
  RTC_DCHECK(result);
  *result->mutable_updated_region() =
      surfaces_.current_frame()->get()->updated_region();
  return result;
}

void ScreenCapturerWinDirectx::Capture(const DesktopRegion& region) {
  RTC_DCHECK(callback_);

  if (!g_container->duplication && !DuplicateOutput()) {
    // Failed to initialize desktop duplication. This usually happens when
    // Windows is switching display mode. Retrying later usually resolves the
    // issue.
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  RTC_DCHECK(g_container->duplication);
  int64_t capture_start_time_nanos = rtc::TimeNanos();

  if (!SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED)) {
    if (!set_thread_execution_state_failed_) {
      set_thread_execution_state_failed_ = true;
      LOG(LS_WARNING) << "Failed to make system & display power assertion: "
                      << GetLastError();
    }
  }

  DXGI_OUTDUPL_FRAME_INFO frame_info;
  memset(&frame_info, 0, sizeof(DXGI_OUTDUPL_FRAME_INFO));
  ComPtr<IDXGIResource> resource;
  rtc::CritScope lock(&g_container->acquire_lock);
  _com_error error = g_container->duplication->AcquireNextFrame(
      kAcquireTimeoutMs, &frame_info, resource.GetAddressOf());

  if (error.Error() == DXGI_ERROR_WAIT_TIMEOUT) {
    // Nothing changed.
    EmitCurrentFrame();
    return;
  }

  if (error.Error() != S_OK) {
    LOG(LS_ERROR) << "Failed to capture frame, error " << error.ErrorMessage()
                  << ", code " << error.Error();
    if (ForceDuplicateOutput()) {
      EmitCurrentFrame();
    } else {
      callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    }
    return;
  }

  if (frame_info.AccumulatedFrames == 0) {
    g_container->duplication->ReleaseFrame();
    EmitCurrentFrame();
    return;
  }

  // Everything looks good so far, build next frame.
  std::unique_ptr<DesktopFrame> result =
      ProcessFrame(frame_info, resource.Get());
  // DetectUpdatedRegion may release last g_container->duplication. But
  // ForctDuplicateOutput function will always release last frame, so there is
  // no potential leak.
  if (g_container->duplication) {
    g_container->duplication->ReleaseFrame();
  }
  if (!result) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  result->set_capture_time_ms(
      (rtc::TimeNanos() - capture_start_time_nanos) /
      rtc::kNumNanosecsPerMillisec);
  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
}

bool ScreenCapturerWinDirectx::GetScreenList(ScreenList* screens) {
  return true;
}

bool ScreenCapturerWinDirectx::SelectScreen(ScreenId id) {
  // Only full desktop capture is supported.
  return id == kFullDesktopScreenId;
}

void ScreenCapturerWinDirectx::EmitCurrentFrame() {
  if (!surfaces_.current_frame()->get()->bits()) {
    // At the very begining, we have not captured any frames.
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  if (shared_memory_factory_) {
    // If shared_memory_factory_ is provided, last frame is stored in frames_
    // queue. If there is not an existing frame (at the very begining), we can
    // only return a nullptr.
    if (frames_.current_frame()) {
      std::unique_ptr<SharedDesktopFrame> frame =
          frames_.current_frame()->Share();
      frame->mutable_updated_region()->Clear();
      callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
    } else {
      callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    }
    return;
  }

  // If there is no shared_memory_factory_, last frame is stored in surfaces_
  // queue.
  std::unique_ptr<DesktopFrame> frame(
      new DxgiDesktopFrame(*surfaces_.current_frame()));
  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

}  // namespace webrtc
