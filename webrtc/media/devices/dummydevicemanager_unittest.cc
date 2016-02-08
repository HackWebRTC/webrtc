/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/gunit.h"
#include "webrtc/media/devices/dummydevicemanager.h"

using cricket::Device;
using cricket::DummyDeviceManager;

// Test that we startup/shutdown properly.
TEST(DummyDeviceManagerTest, StartupShutdown) {
  DummyDeviceManager dm;
  EXPECT_TRUE(dm.Init());
  dm.Terminate();
}

// Test enumerating capabilities.
TEST(DummyDeviceManagerTest, GetCapabilities) {
  DummyDeviceManager dm;
  int capabilities = dm.GetCapabilities();
  EXPECT_EQ((cricket::AUDIO_SEND | cricket::AUDIO_RECV |
      cricket::VIDEO_SEND | cricket::VIDEO_RECV), capabilities);
}

// Test enumerating devices.
TEST(DummyDeviceManagerTest, GetDevices) {
  DummyDeviceManager dm;
  EXPECT_TRUE(dm.Init());
  std::vector<Device> audio_ins, audio_outs, video_ins;
  EXPECT_TRUE(dm.GetAudioInputDevices(&audio_ins));
  EXPECT_TRUE(dm.GetAudioOutputDevices(&audio_outs));
  EXPECT_TRUE(dm.GetVideoCaptureDevices(&video_ins));
}

// Test that we return correct ids for default and bogus devices.
TEST(DummyDeviceManagerTest, GetAudioDeviceIds) {
  DummyDeviceManager dm;
  Device device;
  EXPECT_TRUE(dm.Init());
  EXPECT_TRUE(dm.GetAudioInputDevice(
      cricket::DeviceManagerInterface::kDefaultDeviceName, &device));
  EXPECT_EQ("-1", device.id);
  EXPECT_TRUE(dm.GetAudioOutputDevice(
      cricket::DeviceManagerInterface::kDefaultDeviceName, &device));
  EXPECT_EQ("-1", device.id);
  EXPECT_FALSE(dm.GetAudioInputDevice("_NOT A REAL DEVICE_", &device));
  EXPECT_FALSE(dm.GetAudioOutputDevice("_NOT A REAL DEVICE_", &device));
}

// Test that we get the video capture device by name properly.
TEST(DummyDeviceManagerTest, GetVideoDeviceIds) {
  DummyDeviceManager dm;
  Device device;
  EXPECT_TRUE(dm.Init());
  EXPECT_FALSE(dm.GetVideoCaptureDevice("_NOT A REAL DEVICE_", &device));
  EXPECT_TRUE(dm.GetVideoCaptureDevice(
      cricket::DeviceManagerInterface::kDefaultDeviceName, &device));
}

TEST(DummyDeviceManagerTest, VerifyDevicesListsAreCleared) {
  const std::string imaginary("_NOT A REAL DEVICE_");
  DummyDeviceManager dm;
  std::vector<Device> audio_ins, audio_outs, video_ins;
  audio_ins.push_back(Device(imaginary, imaginary));
  audio_outs.push_back(Device(imaginary, imaginary));
  video_ins.push_back(Device(imaginary, imaginary));
  EXPECT_TRUE(dm.Init());
  EXPECT_TRUE(dm.GetAudioInputDevices(&audio_ins));
  EXPECT_TRUE(dm.GetAudioOutputDevices(&audio_outs));
  EXPECT_TRUE(dm.GetVideoCaptureDevices(&video_ins));
  for (size_t i = 0; i < audio_ins.size(); ++i) {
    EXPECT_NE(imaginary, audio_ins[i].name);
  }
  for (size_t i = 0; i < audio_outs.size(); ++i) {
    EXPECT_NE(imaginary, audio_outs[i].name);
  }
  for (size_t i = 0; i < video_ins.size(); ++i) {
    EXPECT_NE(imaginary, video_ins[i].name);
  }
}
