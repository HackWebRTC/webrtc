/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

namespace {
static const int kHeaderSize = 32;
static const int kFrameHeaderSize = 12;
static uint8_t dummy_payload[4] = {0, 1, 2, 3};
static const int kMaxFileRetries = 5;
}  // namespace

class IvfFileWriterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const int64_t start_id =
        reinterpret_cast<int64_t>(this) ^ rtc::TimeMicros();
    int64_t id = start_id;
    do {
      std::ostringstream oss;
      oss << test::OutputPath() << "ivf_test_file_" << id++ << ".ivf";
      file_name_ = oss.str();
    } while (id < start_id + 100 && FileExists(false));
    ASSERT_LT(id, start_id + 100);
  }

  bool WriteDummyTestFrames(int width,
                            int height,
                            int num_frames,
                            bool use_capture_tims_ms) {
    EncodedImage frame;
    frame._buffer = dummy_payload;
    frame._encodedWidth = width;
    frame._encodedHeight = height;
    for (int i = 1; i <= num_frames; ++i) {
      frame._length = i % sizeof(dummy_payload);
      if (use_capture_tims_ms) {
        frame.capture_time_ms_ = i;
      } else {
        frame._timeStamp = i;
      }
      if (!file_writer_->WriteFrame(frame))
        return false;
    }
    return true;
  }

  void VerifyIvfHeader(FileWrapper* file,
                       const uint8_t fourcc[4],
                       int width,
                       int height,
                       uint32_t num_frames,
                       bool use_capture_tims_ms) {
    ASSERT_TRUE(file->is_open());
    uint8_t data[kHeaderSize];
    ASSERT_EQ(kHeaderSize, file->Read(data, kHeaderSize));

    uint8_t dkif[4] = {'D', 'K', 'I', 'F'};
    EXPECT_EQ(0, memcmp(dkif, data, 4));
    EXPECT_EQ(0u, ByteReader<uint16_t>::ReadLittleEndian(&data[4]));
    EXPECT_EQ(32u, ByteReader<uint16_t>::ReadLittleEndian(&data[6]));
    EXPECT_EQ(0, memcmp(fourcc, &data[8], 4));
    EXPECT_EQ(width, ByteReader<uint16_t>::ReadLittleEndian(&data[12]));
    EXPECT_EQ(height, ByteReader<uint16_t>::ReadLittleEndian(&data[14]));
    EXPECT_EQ(use_capture_tims_ms ? 1000u : 90000u,
              ByteReader<uint32_t>::ReadLittleEndian(&data[16]));
    EXPECT_EQ(1u, ByteReader<uint32_t>::ReadLittleEndian(&data[20]));
    EXPECT_EQ(num_frames, ByteReader<uint32_t>::ReadLittleEndian(&data[24]));
    EXPECT_EQ(0u, ByteReader<uint32_t>::ReadLittleEndian(&data[28]));
  }

  void VerifyDummyTestFrames(FileWrapper* file, uint32_t num_frames) {
    const int kMaxFrameSize = 4;
    for (uint32_t i = 1; i <= num_frames; ++i) {
      uint8_t frame_header[kFrameHeaderSize];
      ASSERT_EQ(kFrameHeaderSize, file->Read(frame_header, kFrameHeaderSize));
      uint32_t frame_length =
          ByteReader<uint32_t>::ReadLittleEndian(&frame_header[0]);
      EXPECT_EQ(i % 4, frame_length);
      uint64_t timestamp =
          ByteReader<uint64_t>::ReadLittleEndian(&frame_header[4]);
      EXPECT_EQ(i, timestamp);

      uint8_t data[kMaxFrameSize] = {};
      ASSERT_EQ(frame_length,
                static_cast<uint32_t>(file->Read(data, frame_length)));
      EXPECT_EQ(0, memcmp(data, dummy_payload, frame_length));
    }
  }

  void RunBasicFileStructureTest(VideoCodecType codec_type,
                                 const uint8_t fourcc[4],
                                 bool use_capture_tims_ms) {
    file_writer_ = IvfFileWriter::Open(file_name_, codec_type);
    ASSERT_TRUE(file_writer_.get());
    const int kWidth = 320;
    const int kHeight = 240;
    const int kNumFrames = 257;
    EXPECT_TRUE(
        WriteDummyTestFrames(kWidth, kHeight, kNumFrames, use_capture_tims_ms));
    EXPECT_TRUE(file_writer_->Close());

    std::unique_ptr<FileWrapper> out_file(FileWrapper::Create());
    ASSERT_TRUE(out_file->OpenFile(file_name_.c_str(), true));
    VerifyIvfHeader(out_file.get(), fourcc, kWidth, kHeight, kNumFrames,
                    use_capture_tims_ms);
    VerifyDummyTestFrames(out_file.get(), kNumFrames);

    out_file->CloseFile();

    bool file_removed = false;
    for (int i = 0; i < kMaxFileRetries; ++i) {
      file_removed = remove(file_name_.c_str()) == 0;
      if (file_removed)
        break;

      // Couldn't remove file for some reason, wait a sec and try again.
      rtc::Thread::SleepMs(1000);
    }
    EXPECT_TRUE(file_removed);
  }

  // Check whether file exists or not, and if it does not meet expectation,
  // wait a bit and check again, up to kMaxFileRetries times. This is an ugly
  // hack to avoid flakiness on certain operating systems where antivirus
  // software may unexpectedly lock files and keep them from disappearing or
  // being reused.
  bool FileExists(bool expected) {
    bool file_exists = expected;
    std::unique_ptr<FileWrapper> file_wrapper;
    int iterations = 0;
    do {
      if (file_wrapper.get() != nullptr)
        rtc::Thread::SleepMs(1000);
      file_wrapper.reset(FileWrapper::Create());
      file_exists = file_wrapper->OpenFile(file_name_.c_str(), true);
      file_wrapper->CloseFile();
    } while (file_exists != expected && ++iterations < kMaxFileRetries);
    return file_exists;
  }

  std::string file_name_;
  std::unique_ptr<IvfFileWriter> file_writer_;
};

TEST_F(IvfFileWriterTest, RemovesUnusedFile) {
  file_writer_ = IvfFileWriter::Open(file_name_, kVideoCodecVP8);
  ASSERT_TRUE(file_writer_.get() != nullptr);
  EXPECT_TRUE(FileExists(true));
  EXPECT_TRUE(file_writer_->Close());
  EXPECT_FALSE(FileExists(false));
  EXPECT_FALSE(file_writer_->Close());  // Can't close twice.
}

TEST_F(IvfFileWriterTest, WritesBasicVP8FileNtpTimestamp) {
  const uint8_t fourcc[4] = {'V', 'P', '8', '0'};
  RunBasicFileStructureTest(kVideoCodecVP8, fourcc, false);
}

TEST_F(IvfFileWriterTest, WritesBasicVP8FileMsTimestamp) {
  const uint8_t fourcc[4] = {'V', 'P', '8', '0'};
  RunBasicFileStructureTest(kVideoCodecVP8, fourcc, true);
}

TEST_F(IvfFileWriterTest, WritesBasicVP9FileNtpTimestamp) {
  const uint8_t fourcc[4] = {'V', 'P', '9', '0'};
  RunBasicFileStructureTest(kVideoCodecVP9, fourcc, false);
}

TEST_F(IvfFileWriterTest, WritesBasicVP9FileMsTimestamp) {
  const uint8_t fourcc[4] = {'V', 'P', '9', '0'};
  RunBasicFileStructureTest(kVideoCodecVP9, fourcc, true);
}

TEST_F(IvfFileWriterTest, WritesBasicH264FileNtpTimestamp) {
  const uint8_t fourcc[4] = {'H', '2', '6', '4'};
  RunBasicFileStructureTest(kVideoCodecH264, fourcc, false);
}

TEST_F(IvfFileWriterTest, WritesBasicH264FileMsTimestamp) {
  const uint8_t fourcc[4] = {'H', '2', '6', '4'};
  RunBasicFileStructureTest(kVideoCodecH264, fourcc, true);
}

}  // namespace webrtc
