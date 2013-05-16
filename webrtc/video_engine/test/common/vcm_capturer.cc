/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/common/vcm_capturer.h"

#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "webrtc/video_engine/new_include/video_send_stream.h"

namespace webrtc {
namespace test {

VcmCapturer::VcmCapturer(webrtc::newapi::VideoSendStreamInput* input)
    : VideoCapturer(input), started_(false), vcm(NULL), last_timestamp(0) {}

bool VcmCapturer::Init(size_t width, size_t height, size_t target_fps) {
  VideoCaptureModule::DeviceInfo* device_info =
      VideoCaptureFactory::CreateDeviceInfo(42);  // Any ID (42) will do.

  char device_name[256];
  char unique_name[256];
  if (device_info->GetDeviceName(0, device_name, sizeof(device_name),
                                 unique_name, sizeof(unique_name)) !=
      0) {
    Destroy();
    return false;
  }

  vcm = webrtc::VideoCaptureFactory::Create(0, unique_name);
  vcm->RegisterCaptureDataCallback(*this);

  device_info->GetCapability(vcm->CurrentDeviceName(), 0, capability_);
  delete device_info;

  capability_.width = static_cast<int32_t>(width);
  capability_.height = static_cast<int32_t>(height);
  capability_.maxFPS = static_cast<int32_t>(target_fps);
  capability_.rawType = kVideoI420;

  if (vcm->StartCapture(capability_) != 0) {
    Destroy();
    return false;
  }

  assert(vcm->CaptureStarted());

  return true;
}

VcmCapturer* VcmCapturer::Create(newapi::VideoSendStreamInput* input,
                                 size_t width, size_t height,
                                 size_t target_fps) {
  VcmCapturer* vcm_capturer = new VcmCapturer(input);
  if (!vcm_capturer->Init(width, height, target_fps)) {
    // TODO(pbos): Log a warning that this failed.
    delete vcm_capturer;
    return NULL;
  }
  return vcm_capturer;
}


void VcmCapturer::Start() { started_ = true; }

void VcmCapturer::Stop() { started_ = false; }

void VcmCapturer::Destroy() {
  if (vcm == NULL) {
    return;
  }

  vcm->StopCapture();
  vcm->DeRegisterCaptureDataCallback();
  vcm->Release();

  // TODO(pbos): How do I destroy the VideoCaptureModule? This still leaves
  //             non-freed memory.
  vcm = NULL;
}

VcmCapturer::~VcmCapturer() { Destroy(); }

void VcmCapturer::OnIncomingCapturedFrame(const int32_t id,
                                          I420VideoFrame& frame) {
  if (last_timestamp == 0 || frame.timestamp() < last_timestamp) {
    last_timestamp = frame.timestamp();
  }

  if (started_) {
    input_->PutFrame(frame, frame.timestamp() - last_timestamp);
  }
  last_timestamp = frame.timestamp();
}

void VcmCapturer::OnIncomingCapturedEncodedFrame(const int32_t id,
                                                 VideoFrame& frame,
                                                 VideoCodecType codec_type) {}

void VcmCapturer::OnCaptureDelayChanged(const int32_t id, const int32_t delay) {
}
}  // test
}  // webrtc
