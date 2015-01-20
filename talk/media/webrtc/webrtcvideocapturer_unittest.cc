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

#include <stdio.h>
#include <vector>
#include "talk/media/base/testutils.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/webrtc/fakewebrtcvcmfactory.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"

using cricket::VideoFormat;

static const std::string kTestDeviceName = "JuberTech FakeCam Q123";
static const std::string kTestDeviceId = "foo://bar/baz";
const VideoFormat kDefaultVideoFormat =
    VideoFormat(640, 400, VideoFormat::FpsToInterval(30), cricket::FOURCC_ANY);

class WebRtcVideoCapturerTest : public testing::Test {
 public:
  WebRtcVideoCapturerTest()
      : factory_(new FakeWebRtcVcmFactory),
        capturer_(new cricket::WebRtcVideoCapturer(factory_)),
        listener_(capturer_.get()) {
    factory_->device_info.AddDevice(kTestDeviceName, kTestDeviceId);
    // add a VGA/I420 capability
    webrtc::VideoCaptureCapability vga;
    vga.width = 640;
    vga.height = 480;
    vga.maxFPS = 30;
    vga.rawType = webrtc::kVideoI420;
    factory_->device_info.AddCapability(kTestDeviceId, vga);
  }

 protected:
  FakeWebRtcVcmFactory* factory_;  // owned by capturer_
  rtc::scoped_ptr<cricket::WebRtcVideoCapturer> capturer_;
  cricket::VideoCapturerListener listener_;
};

TEST_F(WebRtcVideoCapturerTest, TestNotOpened) {
  EXPECT_EQ("", capturer_->GetId());
  EXPECT_TRUE(capturer_->GetSupportedFormats()->empty());
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);
  EXPECT_FALSE(capturer_->IsRunning());
}

TEST_F(WebRtcVideoCapturerTest, TestBadInit) {
  EXPECT_FALSE(capturer_->Init(cricket::Device("bad-name", "bad-id")));
  EXPECT_FALSE(capturer_->IsRunning());
}

TEST_F(WebRtcVideoCapturerTest, TestInit) {
  EXPECT_TRUE(capturer_->Init(cricket::Device(kTestDeviceName, kTestDeviceId)));
  EXPECT_EQ(kTestDeviceId, capturer_->GetId());
  EXPECT_TRUE(NULL != capturer_->GetSupportedFormats());
  ASSERT_EQ(1U, capturer_->GetSupportedFormats()->size());
  EXPECT_EQ(640, (*capturer_->GetSupportedFormats())[0].width);
  EXPECT_EQ(480, (*capturer_->GetSupportedFormats())[0].height);
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);  // not started yet
  EXPECT_FALSE(capturer_->IsRunning());
}

TEST_F(WebRtcVideoCapturerTest, TestInitVcm) {
  EXPECT_TRUE(capturer_->Init(factory_->Create(0,
      reinterpret_cast<const char*>(kTestDeviceId.c_str()))));
}

TEST_F(WebRtcVideoCapturerTest, TestCapture) {
  EXPECT_TRUE(capturer_->Init(cricket::Device(kTestDeviceName, kTestDeviceId)));
  cricket::VideoFormat format(
      capturer_->GetSupportedFormats()->at(0));
  EXPECT_EQ(cricket::CS_STARTING, capturer_->Start(format));
  EXPECT_TRUE(capturer_->IsRunning());
  ASSERT_TRUE(capturer_->GetCaptureFormat() != NULL);
  EXPECT_EQ(format, *capturer_->GetCaptureFormat());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, listener_.last_capture_state(), 1000);
  EXPECT_TRUE(factory_->modules[0]->SendFrame(640, 480));
  EXPECT_TRUE_WAIT(listener_.frame_count() > 0, 5000);
  EXPECT_EQ(capturer_->GetCaptureFormat()->fourcc, listener_.frame_fourcc());
  EXPECT_EQ(640, listener_.frame_width());
  EXPECT_EQ(480, listener_.frame_height());
  EXPECT_EQ(cricket::CS_FAILED, capturer_->Start(format));
  capturer_->Stop();
  EXPECT_FALSE(capturer_->IsRunning());
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);
}

TEST_F(WebRtcVideoCapturerTest, TestCaptureVcm) {
  EXPECT_TRUE(capturer_->Init(factory_->Create(0,
      reinterpret_cast<const char*>(kTestDeviceId.c_str()))));
  EXPECT_TRUE(capturer_->GetSupportedFormats()->empty());
  VideoFormat format;
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(kDefaultVideoFormat, &format));
  EXPECT_EQ(kDefaultVideoFormat.width, format.width);
  EXPECT_EQ(kDefaultVideoFormat.height, format.height);
  EXPECT_EQ(kDefaultVideoFormat.interval, format.interval);
  EXPECT_EQ(cricket::FOURCC_I420, format.fourcc);
  EXPECT_EQ(cricket::CS_STARTING, capturer_->Start(format));
  EXPECT_TRUE(capturer_->IsRunning());
  ASSERT_TRUE(capturer_->GetCaptureFormat() != NULL);
  EXPECT_EQ(format, *capturer_->GetCaptureFormat());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, listener_.last_capture_state(), 1000);
  EXPECT_TRUE(factory_->modules[0]->SendFrame(640, 480));
  EXPECT_TRUE_WAIT(listener_.frame_count() > 0, 5000);
  EXPECT_EQ(capturer_->GetCaptureFormat()->fourcc, listener_.frame_fourcc());
  EXPECT_EQ(640, listener_.frame_width());
  EXPECT_EQ(480, listener_.frame_height());
  EXPECT_EQ(cricket::CS_FAILED, capturer_->Start(format));
  capturer_->Stop();
  EXPECT_FALSE(capturer_->IsRunning());
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);
}

TEST_F(WebRtcVideoCapturerTest, TestCaptureWithoutInit) {
  cricket::VideoFormat format;
  EXPECT_EQ(cricket::CS_NO_DEVICE, capturer_->Start(format));
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);
  EXPECT_FALSE(capturer_->IsRunning());
}
