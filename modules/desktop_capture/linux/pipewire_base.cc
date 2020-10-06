/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/pipewire_base.h"

#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/raw-utils.h>
#include <spa/support/type-map.h>

#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include "absl/memory/memory.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/desktop_capture/linux/pipewire_stubs.h"

using modules_desktop_capture_linux::InitializeStubs;
using modules_desktop_capture_linux::kModulePipewire;
using modules_desktop_capture_linux::StubPathMap;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

const int kBytesPerPixel = 4;

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
const char kPipeWireLib[] = "libpipewire-0.2.so.1";
#endif

// static
void PipeWireBase::SyncDmaBuf(int fd, uint64_t start_or_end) {
  struct dma_buf_sync sync = {0};

  sync.flags = start_or_end | DMA_BUF_SYNC_READ;

  while (true) {
    int ret;
    ret = ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
    if (ret == -1 && errno == EINTR) {
      continue;
    } else if (ret == -1) {
      RTC_LOG(LS_ERROR) << "Failed to synchronize DMA buffer: "
                        << g_strerror(errno);
      break;
    } else {
      break;
    }
  }
}

// static
void PipeWireBase::OnStateChanged(void* data,
                                  pw_remote_state old_state,
                                  pw_remote_state state,
                                  const char* error_message) {
  PipeWireBase* that = static_cast<PipeWireBase*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_REMOTE_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire remote state error: " << error_message;
      break;
    case PW_REMOTE_STATE_CONNECTED:
      RTC_LOG(LS_INFO) << "PipeWire remote state: connected.";
      that->CreateReceivingStream();
      break;
    case PW_REMOTE_STATE_CONNECTING:
      RTC_LOG(LS_INFO) << "PipeWire remote state: connecting.";
      break;
    case PW_REMOTE_STATE_UNCONNECTED:
      RTC_LOG(LS_INFO) << "PipeWire remote state: unconnected.";
      break;
  }
}

// static
void PipeWireBase::OnStreamStateChanged(void* data,
                                        pw_stream_state old_state,
                                        pw_stream_state state,
                                        const char* error_message) {
  PipeWireBase* that = static_cast<PipeWireBase*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_STREAM_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire stream state error: " << error_message;
      break;
    case PW_STREAM_STATE_CONFIGURE:
      pw_stream_set_active(that->pw_stream_, true);
      break;
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
    case PW_STREAM_STATE_READY:
    case PW_STREAM_STATE_PAUSED:
    case PW_STREAM_STATE_STREAMING:
      break;
  }
}

// static
void PipeWireBase::OnStreamFormatChanged(void* data,
                                         const struct spa_pod* format) {
  PipeWireBase* that = static_cast<PipeWireBase*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire stream format changed.";

  if (!format) {
    pw_stream_finish_format(that->pw_stream_, /*res=*/0, /*params=*/nullptr,
                            /*n_params=*/0);
    return;
  }

  that->spa_video_format_ = new spa_video_info_raw();
  spa_format_video_raw_parse(format, that->spa_video_format_,
                             &that->pw_type_->format_video);

  auto width = that->spa_video_format_->size.width;
  auto height = that->spa_video_format_->size.height;
  auto stride = SPA_ROUND_UP_N(width * kBytesPerPixel, 4);
  auto size = height * stride;

  that->desktop_size_ = DesktopSize(width, height);

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.
  const struct spa_pod* params[3];
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate buffer requirements
      that->pw_core_type_->param.idBuffers,
      that->pw_core_type_->param_buffers.Buffers,
      // Size: specified as integer (i) and set to specified size
      ":", that->pw_core_type_->param_buffers.size, "i", size,
      // Stride: specified as integer (i) and set to specified stride
      ":", that->pw_core_type_->param_buffers.stride, "i", stride,
      // Buffers: specifies how many buffers we want to deal with, set as
      // integer (i) where preferred number is 8, then allowed number is defined
      // as range (r) from min and max values and it is undecided (u) to allow
      // negotiation
      ":", that->pw_core_type_->param_buffers.buffers, "iru", 8,
      SPA_POD_PROP_MIN_MAX(1, 32),
      // Align: memory alignment of the buffer, set as integer (i) to specified
      // value
      ":", that->pw_core_type_->param_buffers.align, "i", 16));
  params[1] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate supported metadata
      that->pw_core_type_->param.idMeta, that->pw_core_type_->param_meta.Meta,
      // Type: specified as id or enum (I)
      ":", that->pw_core_type_->param_meta.type, "I",
      that->pw_core_type_->meta.Header,
      // Size: size of the metadata, specified as integer (i)
      ":", that->pw_core_type_->param_meta.size, "i",
      sizeof(struct spa_meta_header)));
  params[2] = reinterpret_cast<spa_pod*>(
      spa_pod_builder_object(&builder, that->pw_core_type_->param.idMeta,
                             that->pw_core_type_->param_meta.Meta, ":",
                             that->pw_core_type_->param_meta.type, "I",
                             that->pw_core_type_->meta.VideoCrop, ":",
                             that->pw_core_type_->param_meta.size, "i",
                             sizeof(struct spa_meta_video_crop)));
  pw_stream_finish_format(that->pw_stream_, /*res=*/0, params, /*n_params=*/3);
}

// static
void PipeWireBase::OnStreamProcess(void* data) {
  PipeWireBase* that = static_cast<PipeWireBase*>(data);
  RTC_DCHECK(that);

  struct pw_buffer* next_buffer;
  struct pw_buffer* buffer = nullptr;

  next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);
  while (next_buffer) {
    buffer = next_buffer;
    next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);

    if (next_buffer) {
      pw_stream_queue_buffer(that->pw_stream_, buffer);
    }
  }

  if (!buffer) {
    return;
  }

  that->HandleBuffer(buffer);

  pw_stream_queue_buffer(that->pw_stream_, buffer);
}

PipeWireBase::PipeWireBase(int32_t fd) {
#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  StubPathMap paths;

  // Check if the PipeWire library is available.
  paths[kModulePipewire].push_back(kPipeWireLib);
  if (!InitializeStubs(paths)) {
    RTC_LOG(LS_ERROR) << "Failed to load the PipeWire library and symbols.";
    pipewire_init_failed_ = true;
    return;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

  pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

  pw_loop_ = pw_loop_new(/*properties=*/nullptr);
  pw_main_loop_ = pw_thread_loop_new(pw_loop_, "pipewire-main-loop");

  pw_thread_loop_lock(pw_main_loop_);

  pw_core_ = pw_core_new(pw_loop_, /*properties=*/nullptr);
  pw_core_type_ = pw_core_get_type(pw_core_);
  pw_remote_ = pw_remote_new(pw_core_, nullptr, /*user_data_size=*/0);

  InitPipeWireTypes();

  // Initialize event handlers, remote end and stream-related.
  pw_remote_events_.version = PW_VERSION_REMOTE_EVENTS;
  pw_remote_events_.state_changed = &OnStateChanged;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.format_changed = &OnStreamFormatChanged;
  pw_stream_events_.process = &OnStreamProcess;

  pw_remote_add_listener(pw_remote_, &spa_remote_listener_, &pw_remote_events_,
                         this);
  pw_remote_connect_fd(pw_remote_, fd);

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    pipewire_init_failed_ = true;
  }

  pw_thread_loop_unlock(pw_main_loop_);

  RTC_LOG(LS_INFO) << "PipeWire remote opened.";
}

PipeWireBase::~PipeWireBase() {
  if (pw_main_loop_) {
    pw_thread_loop_stop(pw_main_loop_);
  }

  if (pw_type_) {
    delete pw_type_;
  }

  if (spa_video_format_) {
    delete spa_video_format_;
  }

  if (pw_stream_) {
    pw_stream_destroy(pw_stream_);
  }

  if (pw_remote_) {
    pw_remote_destroy(pw_remote_);
  }

  if (pw_core_) {
    pw_core_destroy(pw_core_);
  }

  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
  }

  if (pw_loop_) {
    pw_loop_destroy(pw_loop_);
  }
}

uint8_t* PipeWireBase::Frame() const {
  if (!current_frame_) {
    return nullptr;
  }

  return current_frame_.get();
}

DesktopSize PipeWireBase::FrameSize() const {
  return video_crop_size_.value_or(desktop_size_);
}

void PipeWireBase::InitPipeWireTypes() {
  spa_type_map* map = pw_core_type_->map;
  pw_type_ = new PipeWireType();

  spa_type_media_type_map(map, &pw_type_->media_type);
  spa_type_media_subtype_map(map, &pw_type_->media_subtype);
  spa_type_format_video_map(map, &pw_type_->format_video);
  spa_type_video_format_map(map, &pw_type_->video_format);
}

void PipeWireBase::CreateReceivingStream() {
  spa_rectangle pwMinScreenBounds = spa_rectangle{1, 1};
  spa_rectangle pwMaxScreenBounds = spa_rectangle{INT32_MAX, INT32_MAX};

  pw_properties* reuseProps =
      pw_properties_new_string("pipewire.client.reuse=1");
  pw_stream_ = pw_stream_new(pw_remote_, "webrtc-consume-stream", reuseProps);

  uint8_t buffer[1024] = {};
  const spa_pod* params[1];
  spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate formats
      pw_core_type_->param.idEnumFormat, pw_core_type_->spa_format, "I",
      pw_type_->media_type.video, "I", pw_type_->media_subtype.raw,
      // Video format: specified as id or enum (I), preferred format is BGRx,
      // then allowed formats are enumerated (e) and the format is undecided (u)
      // to allow negotiation
      ":", pw_type_->format_video.format, "Ieu", pw_type_->video_format.BGRx,
      SPA_POD_PROP_ENUM(
          4, pw_type_->video_format.RGBx, pw_type_->video_format.BGRx,
          pw_type_->video_format.RGBA, pw_type_->video_format.BGRA),
      // Video size: specified as rectangle (R), preferred size is specified as
      // first parameter, then allowed size is defined as range (r) from min and
      // max values and the format is undecided (u) to allow negotiation
      ":", pw_type_->format_video.size, "Rru", &pwMinScreenBounds,
      SPA_POD_PROP_MIN_MAX(&pwMinScreenBounds, &pwMaxScreenBounds)));

  pw_stream_add_listener(pw_stream_, &spa_stream_listener_, &pw_stream_events_,
                         this);
  pw_stream_flags flags = static_cast<pw_stream_flags>(
      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE);
  if (pw_stream_connect(pw_stream_, PW_DIRECTION_INPUT, /*port_path=*/nullptr,
                        flags, params,
                        /*n_params=*/1) != 0) {
    RTC_LOG(LS_ERROR) << "Could not connect receiving stream.";
    pipewire_init_failed_ = true;
    return;
  }
}

void PipeWireBase::HandleBuffer(pw_buffer* buffer) {
  struct spa_meta_video_crop* video_crop;
  spa_buffer* spaBuffer = buffer->buffer;
  uint8_t* map = nullptr;
  uint8_t* src = nullptr;
  uint8_t* dst = nullptr;

  if (spaBuffer->datas[0].chunk->size == 0) {
    map = nullptr;
    src = nullptr;
  } else if (spaBuffer->datas[0].type == pw_core_type_->data.MemFd) {
    map = static_cast<uint8_t*>(mmap(
        nullptr, spaBuffer->datas[0].maxsize + spaBuffer->datas[0].mapoffset,
        PROT_READ, MAP_PRIVATE, spaBuffer->datas[0].fd, 0));

    if (map == MAP_FAILED) {
      RTC_LOG(LS_ERROR) << "Failed to mmap the memory: "
                        << std::strerror(errno);
      return;
    }

    src = SPA_MEMBER(map, spaBuffer->datas[0].mapoffset, uint8_t);
  } else if (spaBuffer->datas[0].type == pw_core_type_->data.DmaBuf) {
    int fd;
    fd = spaBuffer->datas[0].fd;

    map = static_cast<uint8_t*>(mmap(
        nullptr, spaBuffer->datas[0].maxsize + spaBuffer->datas[0].mapoffset,
        PROT_READ, MAP_PRIVATE, fd, 0));

    if (map == MAP_FAILED) {
      RTC_LOG(LS_ERROR) << "Failed to mmap the memory: "
                        << std::strerror(errno);
      return;
    }

    SyncDmaBuf(fd, DMA_BUF_SYNC_START);

    src = SPA_MEMBER(map, spaBuffer->datas[0].mapoffset, uint8_t);
  } else if (spaBuffer->datas[0].type == pw_core_type_->data.MemPtr) {
    map = nullptr;
    src = static_cast<uint8_t*>(spaBuffer->datas[0].data);
  } else {
    return;
  }

  if (!src) {
    return;
  }

  DesktopSize prev_crop_size = video_crop_size_.value_or(DesktopSize(0, 0));

  if ((video_crop = static_cast<struct spa_meta_video_crop*>(
           spa_buffer_find_meta(spaBuffer, pw_core_type_->meta.VideoCrop)))) {
    RTC_DCHECK(video_crop->width <= desktop_size_.width() &&
               video_crop->height <= desktop_size_.height());
    if ((video_crop->width != desktop_size_.width() ||
         video_crop->height != desktop_size_.height()) &&
        video_crop->width && video_crop->height) {
      video_crop_size_ = DesktopSize(video_crop->width, video_crop->height);
    } else {
      video_crop_size_.reset();
    }
  } else {
    video_crop_size_.reset();
  }

  size_t frame_size;
  if (video_crop_size_) {
    frame_size =
        video_crop_size_->width() * video_crop_size_->height() * kBytesPerPixel;
  } else {
    frame_size =
        desktop_size_.width() * desktop_size_.height() * kBytesPerPixel;
  }

  if (!current_frame_ ||
      (video_crop_size_ && !video_crop_size_->equals(prev_crop_size)) ||
      (!video_crop_size_ && !prev_crop_size.is_empty())) {
    current_frame_ = std::make_unique<uint8_t[]>(frame_size);
  }
  RTC_DCHECK(current_frame_ != nullptr);

  const int32_t dstStride = video_crop_size_
                                ? video_crop_size_->width() * kBytesPerPixel
                                : desktop_size_.width() * kBytesPerPixel;
  const int32_t srcStride = spaBuffer->datas[0].chunk->stride;

  if (srcStride != (desktop_size_.width() * kBytesPerPixel)) {
    RTC_LOG(LS_ERROR) << "Got buffer with stride different from screen stride: "
                      << srcStride
                      << " != " << (desktop_size_.width() * kBytesPerPixel);
    pipewire_init_failed_ = true;
    return;
  }

  dst = current_frame_.get();

  // Adjust source content based on crop video position
  if (video_crop_size_ &&
      (video_crop->y + video_crop_size_->height() <= desktop_size_.height())) {
    for (int i = 0; i < video_crop->y; ++i) {
      src += srcStride;
    }
  }

  const int xOffset =
      video_crop_size_ && (video_crop->x + video_crop_size_->width() <=
                           desktop_size_.width())
          ? video_crop->x * kBytesPerPixel
          : 0;
  const int height = video_crop_size_.value_or(desktop_size_).height();
  for (int i = 0; i < height; ++i) {
    // Adjust source content based on crop video position if needed
    src += xOffset;
    std::memcpy(dst, src, dstStride);
    // If both sides decided to go with the RGBx format we need to convert it to
    // BGRx to match color format expected by WebRTC.
    if (spa_video_format_->format == pw_type_->video_format.RGBx ||
        spa_video_format_->format == pw_type_->video_format.RGBA) {
      ConvertRGBToBGR(dst, dstStride);
    }
    src += srcStride - xOffset;
    dst += dstStride;
  }

  if (map) {
    if (spaBuffer->datas[0].type == pw_core_type_->data.DmaBuf) {
      SyncDmaBuf(spaBuffer->datas[0].fd, DMA_BUF_SYNC_END);
    }
    munmap(map, spaBuffer->datas[0].maxsize + spaBuffer->datas[0].mapoffset);
  }
}

void PipeWireBase::ConvertRGBToBGR(uint8_t* frame, uint32_t size) {
  // Change color format for KDE KWin which uses RGBx and not BGRx
  for (uint32_t i = 0; i < size; i += 4) {
    uint8_t tempR = frame[i];
    uint8_t tempB = frame[i + 2];
    frame[i] = tempB;
    frame[i + 2] = tempR;
  }
}

}  // namespace webrtc
