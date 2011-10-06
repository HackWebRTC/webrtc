/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "file_handler.h"
#include "gtest/gtest.h"
#include "unittest_utils.h"

namespace webrtc {
namespace test {

const std::string kInputFilename = "temp_inputfile.tmp";
const std::string kOutputFilename = "temp_outputfile.tmp";
const std::string kInputFileContents = "baz";
const int kFrameLength = 1e5;  // 100 kB

// Boilerplate code for proper unit tests for FileHandler.
class FileHandlerTest: public testing::Test {
 protected:
  FileHandler* file_handler_;

  FileHandlerTest() {
    // To avoid warnings when using ASSERT_DEATH
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  }

  virtual ~FileHandlerTest() {
  }

  void SetUp() {
    // Cleanup any existing files:
    std::remove(kInputFilename.c_str());
    std::remove(kOutputFilename.c_str());

    // Create a dummy input file:
    FILE* dummy = fopen(kInputFilename.c_str(), "wb");
    fprintf(dummy, "%s", kInputFileContents.c_str());
    fclose(dummy);

    file_handler_ = new FileHandlerImpl(kInputFilename, kOutputFilename,
                                          kFrameLength);
    ASSERT_TRUE(file_handler_->Init());
  }

  void TearDown() {
    delete file_handler_;
    // Cleanup the temporary file:
    std::remove(kInputFilename.c_str());
    std::remove(kOutputFilename.c_str());
  }
};

TEST_F(FileHandlerTest, InitSuccess) {
  FileHandlerImpl file_handler(kInputFilename, kOutputFilename, kFrameLength);
  ASSERT_TRUE(file_handler.Init());
  ASSERT_EQ(kFrameLength, file_handler.GetFrameLength());
  ASSERT_EQ(0, file_handler.GetNumberOfFrames());
}

TEST_F(FileHandlerTest, ReadFrame) {
  WebRtc_UWord8 buffer[3];
  bool result = file_handler_->ReadFrame(buffer);
  ASSERT_FALSE(result);  // no more files to read
  ASSERT_EQ(kInputFileContents[0], buffer[0]);
  ASSERT_EQ(kInputFileContents[1], buffer[1]);
  ASSERT_EQ(kInputFileContents[2], buffer[2]);
}

TEST_F(FileHandlerTest, ReadFrameUninitialized) {
  WebRtc_UWord8 buffer[3];
  FileHandlerImpl file_handler(kInputFilename, kOutputFilename, kFrameLength);
  ASSERT_FALSE(file_handler.ReadFrame(buffer));
}

TEST_F(FileHandlerTest, ReadFrameNullArgument) {
  ASSERT_DEATH(file_handler_->ReadFrame(NULL), "");
}

TEST_F(FileHandlerTest, WriteFrame) {
  WebRtc_UWord8 buffer[kFrameLength];
  memset(buffer, 9, kFrameLength);  // Write lots of 9s to the buffer
  bool result = file_handler_->WriteFrame(buffer);
  ASSERT_TRUE(result);  // success
  // Close the file and verify the size:
  file_handler_->Close();
  ASSERT_EQ(kFrameLength,
            static_cast<int>(file_handler_->GetFileSize(kOutputFilename)));
}

TEST_F(FileHandlerTest, WriteFrameUninitialized) {
  WebRtc_UWord8 buffer[3];
  FileHandlerImpl file_handler(kInputFilename, kOutputFilename, kFrameLength);
  ASSERT_FALSE(file_handler.WriteFrame(buffer));
}

TEST_F(FileHandlerTest, WriteFrameNullArgument) {
  ASSERT_DEATH(file_handler_->WriteFrame(NULL), "");
}

TEST_F(FileHandlerTest, GetFileSizeExistingFile) {
  ASSERT_EQ(kInputFileContents.length(),
            file_handler_->GetFileSize(kInputFilename));
}

TEST_F(FileHandlerTest, GetFileSizeNonExistingFile) {
  ASSERT_EQ(0u, file_handler_->GetFileSize("non-existing-file.tmp"));
}

}  // namespace test
}  // namespace webrtc
