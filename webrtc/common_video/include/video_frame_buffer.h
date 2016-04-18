/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_VIDEO_INCLUDE_VIDEO_FRAME_BUFFER_H_
#define WEBRTC_COMMON_VIDEO_INCLUDE_VIDEO_FRAME_BUFFER_H_

#include <stdint.h>

#include <memory>

#include "webrtc/base/callback.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/system_wrappers/include/aligned_malloc.h"

namespace webrtc {

enum PlaneType {
  kYPlane = 0,
  kUPlane = 1,
  kVPlane = 2,
  kNumOfPlanes = 3,
};

// Interface of a simple frame buffer containing pixel data. This interface does
// not contain any frame metadata such as rotation, timestamp, pixel_width, etc.
class VideoFrameBuffer : public rtc::RefCountInterface {
 public:
  // Returns true if the caller is exclusive owner, and allowed to
  // call MutableData.

  // TODO(nisse): Delete default implementation when subclasses in
  // Chrome are updated.
  virtual bool IsMutable() { return false; }

  // Underlying refcount access, used to implement IsMutable.
  // TODO(nisse): Demote to protected, as soon as Chrome is changed to
  // use IsMutable.
  virtual bool HasOneRef() const = 0;

  // The resolution of the frame in pixels. For formats where some planes are
  // subsampled, this is the highest-resolution plane.
  virtual int width() const = 0;
  virtual int height() const = 0;

  // TODO(nisse): For the transition, we use default implementations
  // of the stride and data methods where the new methods calls the
  // old method, and the old method calls the new methods. Subclasses
  // must override either the new methods or the old method, to break
  // infinite recursion. And similarly for the strides. When
  // applications, in particular Chrome, are updated, delete the old
  // method and delete the default implementation of the new methods.

  // Returns pointer to the pixel data for a given plane. The memory is owned by
  // the VideoFrameBuffer object and must not be freed by the caller.
  virtual const uint8_t* DataY() const;
  virtual const uint8_t* DataU() const;
  virtual const uint8_t* DataV() const;
  // Deprecated method.
  // TODO(nisse): Delete after all users are updated.
  virtual const uint8_t* data(PlaneType type) const;

  // Non-const data access is allowed only if HasOneRef() is true.
  virtual uint8_t* MutableDataY();
  virtual uint8_t* MutableDataU();
  virtual uint8_t* MutableDataV();
  // Deprecated method. TODO(nisse): Delete after all users are updated.
  virtual uint8_t* MutableData(PlaneType type);

  // Returns the number of bytes between successive rows for a given plane.
  virtual int StrideY() const;
  virtual int StrideU() const;
  virtual int StrideV() const;
  // Deprecated method. TODO(nisse): Delete after all users are updated.
  virtual int stride(PlaneType type) const;

  // Return the handle of the underlying video frame. This is used when the
  // frame is backed by a texture.
  virtual void* native_handle() const = 0;

  // Returns a new memory-backed frame buffer converted from this buffer's
  // native handle.
  virtual rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() = 0;

 protected:
  virtual ~VideoFrameBuffer();
};

// Plain I420 buffer in standard memory.
class I420Buffer : public VideoFrameBuffer {
 public:
  I420Buffer(int width, int height);
  I420Buffer(int width, int height, int stride_y, int stride_u, int stride_v);
  void InitializeData();

  int width() const override;
  int height() const override;
  const uint8_t* DataY() const override;
  const uint8_t* DataU() const override;
  const uint8_t* DataV() const override;
  // Non-const data access is only allowed if IsMutable() is true, to protect
  // against unexpected overwrites.
  bool IsMutable() override;
  uint8_t* MutableDataY() override;
  uint8_t* MutableDataU() override;
  uint8_t* MutableDataV() override;
  int StrideY() const override;
  int StrideU() const override;
  int StrideV() const override;

  void* native_handle() const override;
  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override;

  // Create a new buffer and copy the pixel data.
  static rtc::scoped_refptr<I420Buffer> Copy(
      const rtc::scoped_refptr<VideoFrameBuffer>& buffer);

 protected:
  ~I420Buffer() override;

 private:
  const int width_;
  const int height_;
  const int stride_y_;
  const int stride_u_;
  const int stride_v_;
  const std::unique_ptr<uint8_t, AlignedFreeDeleter> data_;
};

// Base class for native-handle buffer is a wrapper around a |native_handle|.
// This is used for convenience as most native-handle implementations can share
// many VideoFrame implementations, but need to implement a few others (such
// as their own destructors or conversion methods back to software I420).
class NativeHandleBuffer : public VideoFrameBuffer {
 public:
  NativeHandleBuffer(void* native_handle, int width, int height);

  int width() const override;
  int height() const override;
  const uint8_t* DataY() const override;
  const uint8_t* DataU() const override;
  const uint8_t* DataV() const override;
  int StrideY() const override;
  int StrideU() const override;
  int StrideV() const override;

  void* native_handle() const override;
  bool IsMutable() override;

 protected:
  void* native_handle_;
  const int width_;
  const int height_;
};

class WrappedI420Buffer : public webrtc::VideoFrameBuffer {
 public:
  WrappedI420Buffer(int width,
                    int height,
                    const uint8_t* y_plane,
                    int y_stride,
                    const uint8_t* u_plane,
                    int u_stride,
                    const uint8_t* v_plane,
                    int v_stride,
                    const rtc::Callback0<void>& no_longer_used);
  int width() const override;
  int height() const override;

  bool IsMutable() override;

  const uint8_t* DataY() const override;
  const uint8_t* DataU() const override;
  const uint8_t* DataV() const override;
  int StrideY() const override;
  int StrideU() const override;
  int StrideV() const override;

  void* native_handle() const override;

  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override;

 private:
  friend class rtc::RefCountedObject<WrappedI420Buffer>;
  ~WrappedI420Buffer() override;

  const int width_;
  const int height_;
  const uint8_t* const y_plane_;
  const uint8_t* const u_plane_;
  const uint8_t* const v_plane_;
  const int y_stride_;
  const int u_stride_;
  const int v_stride_;
  rtc::Callback0<void> no_longer_used_cb_;
};

// Helper function to crop |buffer| without making a deep copy. May only be used
// for non-native frames.
rtc::scoped_refptr<VideoFrameBuffer> ShallowCenterCrop(
    const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
    int cropped_width,
    int cropped_height);

}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_INCLUDE_VIDEO_FRAME_BUFFER_H_
