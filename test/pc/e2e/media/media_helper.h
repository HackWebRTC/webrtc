/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_MEDIA_MEDIA_HELPER_H_
#define TEST_PC_E2E_MEDIA_MEDIA_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "api/test/frame_generator_interface.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"
#include "test/pc/e2e/media/test_video_capturer_video_track_source.h"
#include "test/pc/e2e/test_peer.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class MediaHelper {
 public:
  MediaHelper(VideoQualityAnalyzerInjectionHelper*
                  video_quality_analyzer_injection_helper,
              TaskQueueFactory* task_queue_factory)
      : clock_(Clock::GetRealTimeClock()),
        task_queue_factory_(task_queue_factory),
        video_quality_analyzer_injection_helper_(
            video_quality_analyzer_injection_helper) {}
  ~MediaHelper();

  void MaybeAddAudio(TestPeer* peer);

  std::vector<rtc::scoped_refptr<TestVideoCapturerVideoTrackSource>>
  MaybeAddVideo(TestPeer* peer);

  // Creates a video file writer if |file_name| is not empty. Created writer
  // will be owned by MediaHelper and will be closed on MediaHelper destruction.
  // If |file_name| is empty will return nullptr.
  test::VideoFrameWriter* MaybeCreateVideoWriter(
      absl::optional<std::string> file_name,
      const PeerConnectionE2EQualityTestFixture::VideoConfig& config);

 private:
  std::unique_ptr<test::TestVideoCapturer> CreateVideoCapturer(
      const PeerConnectionE2EQualityTestFixture::VideoConfig& video_config,
      std::unique_ptr<test::FrameGeneratorInterface> generator,
      std::unique_ptr<test::TestVideoCapturer::FramePreprocessor>
          frame_preprocessor);
  std::unique_ptr<test::FrameGeneratorInterface>
  CreateScreenShareFrameGenerator(
      const PeerConnectionE2EQualityTestFixture::VideoConfig& video_config);

  Clock* const clock_;
  TaskQueueFactory* const task_queue_factory_;
  VideoQualityAnalyzerInjectionHelper* video_quality_analyzer_injection_helper_;
  std::vector<std::unique_ptr<test::VideoFrameWriter>> video_writers_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_MEDIA_MEDIA_HELPER_H_
