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

// This class implements the VideoCaptureModule interface. Instead of capturing
// frames from a camera it captures frames from a file.

#ifndef TALK_APP_WEBRTC_TEST_FILEVIDEOCAPTUREMODULE_H_
#define TALK_APP_WEBRTC_TEST_FILEVIDEOCAPTUREMODULE_H_

#include <stdio.h>
#include "talk/base/common.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/base/time.h"

#ifdef WEBRTC_RELATIVE_PATH
#include "common_types.h"
#include "modules/video_capture/main/interface/video_capture.h"
#include "modules/video_capture/main/interface/video_capture_defines.h"
#include "modules/video_capture/main/interface/video_capture_factory.h"
#include "system_wrappers/interface/ref_count.h"
#include "system_wrappers/interface/scoped_refptr.h"
#else
#include "third_party/webrtc/files/include/common_types.h"
#include "third_party/webrtc/files/include/video_capture.h"
#include "third_party/webrtc/files/include/video_capture_defines.h"
#include "third_party/webrtc/files/include/video_capture_factory.h"
#include "third_party/webrtc/files/include/ref_count.h"
#include "third_party/webrtc/files/include/scoped_refptr.h"
#endif

// TODO(henrike): replace playing file with playing a buffer.
class FileVideoCaptureModule
    : public webrtc::VideoCaptureModule,
      public talk_base::MessageHandler {
 public:
  virtual ~FileVideoCaptureModule();
  static VideoCaptureModule* CreateFileVideoCaptureModule(
      const char* file_name);

  bool SetFrameRate(int fps);
  void SetSize(int width, int height);

  virtual int32_t Version(char* version,
                          uint32_t& remaining_buffer_in_bytes,
                          uint32_t& position) const {
    return impl_->Version(version, remaining_buffer_in_bytes,
                          position);
  }

  virtual int32_t ChangeUniqueId(const int32_t id) {
    return impl_->ChangeUniqueId(id);
  }

  virtual int32_t TimeUntilNextProcess() {
    return impl_->TimeUntilNextProcess();
  }

  virtual int32_t Process() {
    return impl_->Process();
  }

  virtual WebRtc_Word32 RegisterCaptureDataCallback(
      webrtc::VideoCaptureDataCallback& dataCallback) {
    return impl_->RegisterCaptureDataCallback(dataCallback);
  }

  virtual WebRtc_Word32 DeRegisterCaptureDataCallback() {
    return impl_->DeRegisterCaptureDataCallback();
  }

  virtual WebRtc_Word32 RegisterCaptureCallback(
      webrtc::VideoCaptureFeedBack& callBack) {
    return impl_->RegisterCaptureCallback(callBack);
  }

  virtual WebRtc_Word32 DeRegisterCaptureCallback() {
    return impl_->DeRegisterCaptureCallback();
  }

  virtual WebRtc_Word32 StartCapture(
      const webrtc::VideoCaptureCapability& capability) {
    return impl_->StartCapture(capability);
  }

  virtual WebRtc_Word32 StopCapture() {
    return impl_->StopCapture();
  }

  virtual WebRtc_Word32 StartSendImage(const webrtc::VideoFrame& videoFrame,
                                       WebRtc_Word32 frameRate = 1) {
    return impl_->StartSendImage(videoFrame, frameRate = 1);
  }

  virtual WebRtc_Word32 StopSendImage() {
    return impl_->StopSendImage();
  }

  virtual const WebRtc_UWord8* CurrentDeviceName() const {
    return impl_->CurrentDeviceName();
  }

  virtual bool CaptureStarted() {
    return impl_->CaptureStarted();
  }

  virtual WebRtc_Word32 CaptureSettings(
      webrtc::VideoCaptureCapability& settings) {
    return impl_->CaptureSettings(settings);
  }

  virtual WebRtc_Word32 SetCaptureDelay(WebRtc_Word32 delayMS) {
    return impl_->SetCaptureDelay(delayMS);
  }

  virtual WebRtc_Word32 CaptureDelay() {
    return impl_->CaptureDelay();
  }

  virtual WebRtc_Word32 SetCaptureRotation(
      webrtc::VideoCaptureRotation rotation) {
    return impl_->SetCaptureRotation(rotation);
  }

  virtual VideoCaptureEncodeInterface* GetEncodeInterface(
      const webrtc::VideoCodec& codec) {
    return impl_->GetEncodeInterface(codec);
  }

  virtual WebRtc_Word32 EnableFrameRateCallback(const bool enable) {
    return impl_->EnableFrameRateCallback(enable);
  }
  virtual WebRtc_Word32 EnableNoPictureAlarm(const bool enable) {
    return impl_->EnableNoPictureAlarm(enable);
  }

  // Inherited from MesageHandler.
  virtual void OnMessage(talk_base::Message* msg) {
    GenerateNewFrame();
  }

 protected:
  FileVideoCaptureModule();

 private:
  bool Init(const char* file_name);
  void GenerateNewFrame();
  int GetI420FrameLength();
  // Generate an arbitrary frame. (Will be used when file reading is replaced
  // with reading a buffer).
  void SetFrame(uint8* image);
  uint32 GetTimestamp();

  // Module interface implementation.
  webrtc::scoped_refptr<VideoCaptureModule> impl_;

  // File playing implementation.
  static const int kStartFrameRate = 30;
  // CIF
  static const int kStartWidth = 352;
  static const int kStartHeight = 288;
  static const uint32 kStartTimeStamp = 2000;

  FILE* i420_file_;
  talk_base::scoped_ptr<talk_base::Thread> camera_thread_;
  webrtc::VideoCaptureExternal* video_capture_;

  bool started_;
  int sent_frames_;
  uint32 next_frame_time_;
  uint32 time_per_frame_ms_;

  int fps_;
  int width_;
  int height_;
  talk_base::scoped_array<uint8> image_;
};

#endif  // TALK_APP_WEBRTC_TEST_FILEVIDEOCAPTUREMODULE_H_
