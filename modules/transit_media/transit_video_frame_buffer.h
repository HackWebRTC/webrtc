#pragma once

#include <memory>

#include "api/video/video_frame_buffer.h"

namespace webrtc {

class TransitVideoFrameBuffer : public VideoFrameBuffer {
 public:
  TransitVideoFrameBuffer(int width, int height, int size)
      : width_(width), height_(height), size_(size), data_(new uint8_t[size]) {}
  ~TransitVideoFrameBuffer() {}

  Type type() const override { return Type::kNative; }

  int width() const override { return width_; }

  int height() const override { return height_; }

  rtc::scoped_refptr<I420BufferInterface> ToI420() override { return nullptr; }

  int size() { return size_; }

  const uint8_t* data() const { return data_.get(); }

  uint8_t* mutable_data() { return data_.get(); }

 private:
  int width_;
  int height_;
  int size_;
  const std::unique_ptr<uint8_t> data_;
};

}  // namespace webrtc
