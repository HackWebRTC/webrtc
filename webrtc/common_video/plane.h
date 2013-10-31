/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_PLANE_H
#define COMMON_VIDEO_PLANE_H

#include "webrtc/system_wrappers/interface/aligned_malloc.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Helper class for I420VideoFrame: Store plane data and perform basic plane
// operations.
class Plane {
 public:
  Plane();
  ~Plane();
  // CreateEmptyPlane - set allocated size, actual plane size and stride:
  // If current size is smaller than current size, then a buffer of sufficient
  // size will be allocated.
  // Return value: 0 on success, -1 on error.
  int CreateEmptyPlane(size_t allocated_size, size_t stride, size_t plane_size);

  // Copy the entire plane data.
  // Return value: 0 on success, -1 on error.
  int Copy(const Plane& plane);

  // Copy buffer: If current size is smaller
  // than current size, then a buffer of sufficient size will be allocated.
  // Return value: 0 on success, -1 on error.
  int Copy(size_t size, size_t stride, const uint8_t* buffer);

  // Make this plane refer to a memory buffer. Plane will not own buffer.
  void Alias(size_t size, size_t stride, uint8_t* buffer);

  // Swap plane data.
  void Swap(Plane& plane);

  // Get allocated size.
  size_t allocated_size() const { return allocated_size_; }

  // Set actual size.
  void ResetSize() {plane_size_ = 0;}

  // Return true is plane size is zero, false if not.
  bool IsZeroSize() const {return plane_size_ == 0;}

  // Get stride value.
  size_t stride() const { return stride_; }

  // Return data pointer.
  const uint8_t* buffer() const { return pointer_; }
  // Overloading with non-const.
  uint8_t* buffer() { return pointer_; }

 private:
  // Reallocate when needed: If current allocated size is less than new_size,
  // buffer will be updated. In any case, old data becomes undefined.
  // Return value: 0 on success, -1 on error.
  int Reallocate(size_t new_size);

  uint8_t* pointer_;
  Allocator<uint8_t>::scoped_ptr_aligned allocation_;
  size_t allocated_size_;
  size_t plane_size_;
  size_t stride_;
};  // Plane

}  // namespace webrtc

#endif  // COMMON_VIDEO_PLANE_H
