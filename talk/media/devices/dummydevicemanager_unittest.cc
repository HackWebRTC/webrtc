/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include "talk/base/gunit.h"
#include "talk/media/devices/dummydevicemanager.h"

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
