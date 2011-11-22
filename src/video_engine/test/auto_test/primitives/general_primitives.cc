/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "general_primitives.h"

#include "video_capture_factory.h"
#include "vie_autotest.h"
#include "vie_autotest_defines.h"
#include "vie_to_file_renderer.h"

void FindCaptureDeviceOnSystem(webrtc::ViECapture* capture,
                               unsigned char* device_name,
                               unsigned int device_name_length,
                               int* device_id,
                               int* number_of_errors,
                               webrtc::VideoCaptureModule** device_video) {

  bool capture_device_set = false;
  webrtc::VideoCaptureModule::DeviceInfo *dev_info =
      webrtc::VideoCaptureFactory::CreateDeviceInfo(0);

  const unsigned int kMaxUniqueIdLength = 256;
  WebRtc_UWord8 unique_id[kMaxUniqueIdLength];
  memset(unique_id, 0, kMaxUniqueIdLength);

  for (unsigned int i = 0; i < dev_info->NumberOfDevices(); i++) {
    int error = dev_info->GetDeviceName(i, device_name, device_name_length,
                                        unique_id, kMaxUniqueIdLength);
    *number_of_errors += ViETest::TestError(
        error == 0, "ERROR: %s at line %d", __FUNCTION__, __LINE__);
    *device_video =
        webrtc::VideoCaptureFactory::Create(4571, unique_id);

    *number_of_errors += ViETest::TestError(
        *device_video != NULL, "ERROR: %s at line %d", __FUNCTION__, __LINE__);
    (*device_video)->AddRef();

    error = capture->AllocateCaptureDevice(**device_video, *device_id);
    if (error == 0) {
      ViETest::Log("Using capture device: %s, captureId: %d.",
                   device_name, *device_id);
      capture_device_set = true;
      break;
    } else {
      (*device_video)->Release();
      (*device_video) = NULL;
    }
  }
  delete dev_info;
  *number_of_errors += ViETest::TestError(
      capture_device_set, "ERROR: %s at line %d - Could not set capture device",
      __FUNCTION__, __LINE__);
}

void RenderInWindow(webrtc::ViERender* video_render_interface,
                    int* numberOfErrors,
                    int frame_provider_id,
                    void* os_window,
                    float z_index) {
  int error = video_render_interface->AddRenderer(frame_provider_id, os_window,
                                                  z_index, 0.0, 0.0, 1.0, 1.0);
  *numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                        __FUNCTION__, __LINE__);

  error = video_render_interface->StartRender(frame_provider_id);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
}

void RenderToFile(webrtc::ViERender* renderer_interface,
                  int frame_provider_id,
                  ViEToFileRenderer *to_file_renderer) {
  int result = renderer_interface->AddRenderer(frame_provider_id,
                                               webrtc::kVideoI420,
                                               to_file_renderer);
  ViETest::TestError(result == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);
  result = renderer_interface->StartRender(frame_provider_id);
  ViETest::TestError(result == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);
}

void StopAndRemoveRenderers(webrtc::ViEBase* base_interface,
                            webrtc::ViERender* render_interface,
                            int* number_of_errors,
                            int channel_id,
                            int capture_id) {
  int error = render_interface->StopRender(channel_id);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = render_interface->RemoveRenderer(channel_id);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = render_interface->RemoveRenderer(capture_id);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
}

void ConfigureRtpRtcp(webrtc::ViERTP_RTCP* rtcp_interface,
                      int* number_of_errors,
                      int video_channel) {
  int error = rtcp_interface->SetRTCPStatus(
      video_channel, webrtc::kRtcpCompound_RFC4585);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = rtcp_interface->SetKeyFrameRequestMethod(
      video_channel, webrtc::kViEKeyFrameRequestPliRtcp);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = rtcp_interface->SetTMMBRStatus(video_channel, true);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
}

bool FindSpecificCodec(webrtc::VideoCodecType of_type,
                       webrtc::ViECodec* codec_interface,
                       webrtc::VideoCodec* result) {

  memset(result, 1, sizeof(webrtc::VideoCodec));

  for (int i = 0; i < codec_interface->NumberOfCodecs(); i++) {
    webrtc::VideoCodec codec;
    if (codec_interface->GetCodec(i, codec) != 0) {
      return false;
    }
    if (codec.codecType == of_type) {
      // Done
      *result = codec;
      return true;
    }
  }
  // Didn't find it
  return false;
}

