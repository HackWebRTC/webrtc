/*
 * libjingle
 * Copyright 2011, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/app/webrtc_dev/test/filevideocapturemodule.h"

#ifdef WEBRTC_RELATIVE_PATH
#include "system_wrappers/interface/ref_count.h"
#else
#include "third_party/webrtc/files/include/ref_count.h"
#endif

FileVideoCaptureModule::~FileVideoCaptureModule() {
  camera_thread_->Stop();
  if (i420_file_ != NULL) fclose(i420_file_);
  // The memory associated with video_capture_ is owned by impl_.
}

webrtc::VideoCaptureModule*
FileVideoCaptureModule::CreateFileVideoCaptureModule(const char* file_name) {
  webrtc::RefCountImpl<FileVideoCaptureModule>* capture_module =
      new webrtc::RefCountImpl<FileVideoCaptureModule>();
  if (!capture_module->Init(file_name)) {
    capture_module->Release();
    return NULL;
  }
  return capture_module;
}

// TODO(henrike): deal with the rounding error.
bool FileVideoCaptureModule::SetFrameRate(int fps) {
  fps_ = fps;
  time_per_frame_ms_ = 1000 / fps;
  return true;
}

void FileVideoCaptureModule::SetSize(int width, int height) {
  width_ = width;
  height_ = height;
  image_.reset(new uint8[GetI420FrameLength()]);
}

FileVideoCaptureModule::FileVideoCaptureModule()
    : impl_(),
      i420_file_(NULL),
      camera_thread_(new talk_base::Thread()),
      video_capture_(NULL),
      started_(false),
      sent_frames_(0),
      next_frame_time_(0),
      time_per_frame_ms_(0),
      fps_(0),
      width_(0),
      height_(0),
      image_() {}

bool FileVideoCaptureModule::Init(const char* file_name) {
  impl_ = webrtc::VideoCaptureFactory::Create(0,  // id
                                              video_capture_);
  if (impl_.get() == NULL) {
    return false;
  }
  if (video_capture_ == NULL) {
    return false;
  }
  if (!SetFrameRate(kStartFrameRate)) {
    return false;
  }
  SetSize(kStartWidth, kStartHeight);
  i420_file_ = fopen(file_name, "rb");
  if (i420_file_ == NULL) {
    // Not generally unexpected but for this class it is.
    ASSERT(false);
    return false;
  }
  if (!camera_thread_->Start()) {
    return false;
  }
  // Only one post, no need to add any data to post.
  camera_thread_->Post(this);
  return true;
}

// TODO(henrike): handle time wrapparound.
void FileVideoCaptureModule::GenerateNewFrame() {
  if (!started_) {
    next_frame_time_ = talk_base::Time();
    started_ = true;
  }
  // Read from file.
  int read = fread(image_.get(), sizeof(uint8), GetI420FrameLength(),
                   i420_file_);
  // Loop file if end is reached.
  if (read != GetI420FrameLength()) {
    fseek(i420_file_, 0, SEEK_SET);
    read = fread(image_.get(), sizeof(uint8), GetI420FrameLength(),
                 i420_file_);
    if (read != GetI420FrameLength()) {
      ASSERT(false);
      return;
    }
  }

  webrtc::VideoCaptureCapability capability;
  capability.width = width_;
  capability.height = height_;
  capability.maxFPS = 0;
  capability.expectedCaptureDelay = 0;
  capability.rawType =webrtc:: kVideoI420;
  capability.codecType = webrtc::kVideoCodecUnknown;
  capability.interlaced = false;
  video_capture_->IncomingFrame(image_.get(), GetI420FrameLength(),
                                  capability, GetTimestamp());
  ++sent_frames_;
  next_frame_time_ += time_per_frame_ms_;
  const uint32 current_time = talk_base::Time();
  const uint32 wait_time = (next_frame_time_ > current_time) ?
      next_frame_time_ - current_time : 0;
  camera_thread_->PostDelayed(wait_time, this);
}

int FileVideoCaptureModule::GetI420FrameLength() {
  return width_ * height_ * 3 >> 1;
}

// TODO(henrike): use this function instead of/in addition to reading from a
// file.
void FileVideoCaptureModule::SetFrame(uint8* image) {
  // Set Y plane.
  memset(image, 128, width_ * height_);
  // Set U plane.
  int write_position = width_ * height_;
  memset(&image[write_position], 64, width_ * height_ / 4);
  // Set V plane.
  write_position += width_ * height_ / 4;
  memset(&image[write_position], 32, width_ * height_ / 4);
}

// TODO(henrike): handle timestamp wrapparound.
uint32 FileVideoCaptureModule::GetTimestamp() {
  return kStartTimeStamp + sent_frames_ * time_per_frame_ms_;
}
