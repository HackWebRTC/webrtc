/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_PIPEWIRE_BASE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_PIPEWIRE_BASE_H_

#define typeof __typeof__
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <memory>

#include "modules/desktop_capture/desktop_geometry.h"

#include "absl/types/optional.h"
#include "api/ref_counted_base.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class PipeWireType {
 public:
  spa_type_media_type media_type;
  spa_type_media_subtype media_subtype;
  spa_type_format_video format_video;
  spa_type_video_format video_format;
};

class PipeWireBase : public rtc::RefCountedBase {
 public:
  explicit PipeWireBase(int32_t fd);
  ~PipeWireBase();

  uint8_t* Frame() const;
  DesktopSize FrameSize() const;

 private:
  pw_core* pw_core_ = nullptr;
  pw_type* pw_core_type_ = nullptr;
  pw_stream* pw_stream_ = nullptr;
  pw_remote* pw_remote_ = nullptr;
  pw_loop* pw_loop_ = nullptr;
  pw_thread_loop* pw_main_loop_ = nullptr;
  PipeWireType* pw_type_ = nullptr;

  spa_hook spa_stream_listener_ = {};
  spa_hook spa_remote_listener_ = {};

  pw_stream_events pw_stream_events_ = {};
  pw_remote_events pw_remote_events_ = {};

  spa_video_info_raw* spa_video_format_ = nullptr;

  int32_t pw_fd_ = -1;

  absl::optional<DesktopSize> video_crop_size_;
  DesktopSize desktop_size_ = {};

  bool pipewire_init_failed_ = false;

  std::unique_ptr<uint8_t[]> current_frame_;

  void InitPipeWireTypes();

  void CreateReceivingStream();
  void HandleBuffer(pw_buffer* buffer);

  void ConvertRGBToBGR(uint8_t* frame, uint32_t size);

  static void SyncDmaBuf(int fd, uint64_t start_or_end);

  static void OnStateChanged(void* data,
                             pw_remote_state old_state,
                             pw_remote_state state,
                             const char* error);
  static void OnStreamStateChanged(void* data,
                                   pw_stream_state old_state,
                                   pw_stream_state state,
                                   const char* error_message);

  static void OnStreamFormatChanged(void* data, const struct spa_pod* format);
  static void OnStreamProcess(void* data);
  static void OnNewBuffer(void* data, uint32_t id);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_PIPEWIRE_BASE_H_
