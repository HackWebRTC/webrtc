/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INTERFACE_TEXTURE_VIDEO_FRAME_H
#define COMMON_VIDEO_INTERFACE_TEXTURE_VIDEO_FRAME_H

// TextureVideoFrame class
//
// Storing and handling of video frames backed by textures.

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/common_video/interface/native_handle.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class TextureVideoFrame : public I420VideoFrame {
 public:
  TextureVideoFrame(NativeHandle* handle,
                    int width,
                    int height,
                    uint32_t timestamp,
                    int64_t render_time_ms);
  virtual ~TextureVideoFrame();

  // I420VideoFrame implementation
  virtual int CreateEmptyFrame(int width,
                               int height,
                               int stride_y,
                               int stride_u,
                               int stride_v) override;
  virtual int CreateFrame(int size_y,
                          const uint8_t* buffer_y,
                          int size_u,
                          const uint8_t* buffer_u,
                          int size_v,
                          const uint8_t* buffer_v,
                          int width,
                          int height,
                          int stride_y,
                          int stride_u,
                          int stride_v) override;
  virtual int CreateFrame(int size_y,
                          const uint8_t* buffer_y,
                          int size_u,
                          const uint8_t* buffer_u,
                          int size_v,
                          const uint8_t* buffer_v,
                          int width,
                          int height,
                          int stride_y,
                          int stride_u,
                          int stride_v,
                          webrtc::VideoRotation rotation) override;
  virtual int CopyFrame(const I420VideoFrame& videoFrame) override;
  virtual I420VideoFrame* CloneFrame() const override;
  virtual void SwapFrame(I420VideoFrame* videoFrame) override;
  virtual uint8_t* buffer(PlaneType type) override;
  virtual const uint8_t* buffer(PlaneType type) const override;
  virtual int allocated_size(PlaneType type) const override;
  virtual int stride(PlaneType type) const override;
  virtual bool IsZeroSize() const override;
  virtual void* native_handle() const override;

 protected:
  virtual int CheckDimensions(
      int width, int height, int stride_y, int stride_u, int stride_v) override;

 private:
  // An opaque handle that stores the underlying video frame.
  scoped_refptr<NativeHandle> handle_;
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_INTERFACE_TEXTURE_VIDEO_FRAME_H
