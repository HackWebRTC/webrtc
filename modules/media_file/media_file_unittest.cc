/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/media_file/media_file.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

class MediaFileTest : public testing::Test {
 protected:
  void SetUp() {
    // Use number 0 as the the identifier and pass to CreateMediaFile.
    media_file_ = webrtc::MediaFile::CreateMediaFile(0);
    ASSERT_TRUE(media_file_ != NULL);
  }
  void TearDown() {
    webrtc::MediaFile::DestroyMediaFile(media_file_);
    media_file_ = NULL;
  }
  webrtc::MediaFile* media_file_;
};

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_StartPlayingAudioFileWithoutError \
  DISABLED_StartPlayingAudioFileWithoutError
#else
#define MAYBE_StartPlayingAudioFileWithoutError \
  StartPlayingAudioFileWithoutError
#endif
TEST_F(MediaFileTest, MAYBE_StartPlayingAudioFileWithoutError) {
  // TODO(leozwang): Use hard coded filename here, we want to
  // loop through all audio files in future
  const std::string audio_file =
      webrtc::test::ResourcePath("voice_engine/audio_tiny48", "wav");
  ASSERT_EQ(0, media_file_->StartPlayingAudioFile(
      audio_file.c_str(),
      0,
      false,
      webrtc::kFileFormatWavFile));

  ASSERT_EQ(true, media_file_->IsPlaying());

  webrtc::SleepMs(1);

  ASSERT_EQ(0, media_file_->StopPlaying());
}
