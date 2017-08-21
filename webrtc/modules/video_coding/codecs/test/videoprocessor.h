/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/video/video_frame.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/test/packet_manipulator.h"
#include "webrtc/modules/video_coding/codecs/test/stats.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"
#include "webrtc/modules/video_coding/utility/vp8_header_parser.h"
#include "webrtc/modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "webrtc/rtc_base/buffer.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/constructormagic.h"
#include "webrtc/rtc_base/sequenced_task_checker.h"
#include "webrtc/rtc_base/task_queue.h"
#include "webrtc/test/testsupport/frame_reader.h"
#include "webrtc/test/testsupport/frame_writer.h"

namespace webrtc {

class VideoBitrateAllocator;

namespace test {

// Defines which frame types shall be excluded from packet loss and when.
enum ExcludeFrameTypes {
  // Will exclude the first keyframe in the video sequence from packet loss.
  // Following keyframes will be targeted for packet loss.
  kExcludeOnlyFirstKeyFrame,
  // Exclude all keyframes from packet loss, no matter where in the video
  // sequence they occur.
  kExcludeAllKeyFrames
};

// Returns a string representation of the enum value.
const char* ExcludeFrameTypesToStr(ExcludeFrameTypes e);

// Test configuration for a test run.
struct TestConfig {
  // Name of the test. This is purely metadata and does not affect the test.
  std::string name;

  // More detailed description of the test. This is purely metadata and does
  // not affect the test.
  std::string description;

  // Number of this test. Useful if multiple runs of the same test with
  // different configurations shall be managed.
  int test_number = 0;

  // Plain name of YUV file to process without file extension.
  std::string filename;

  // File to process. This must be a video file in the YUV format.
  std::string input_filename;

  // File to write to during processing for the test. Will be a video file
  // in the YUV format.
  std::string output_filename;

  // Path to the directory where encoded files will be put
  // (absolute or relative to the executable).
  std::string output_dir = "out";

  // Configurations related to networking.
  NetworkingConfig networking_config;

  // Decides how the packet loss simulations shall exclude certain frames
  // from packet loss.
  ExcludeFrameTypes exclude_frame_types = kExcludeOnlyFirstKeyFrame;

  // The length of a single frame of the input video file. Calculated out of the
  // width and height according to the video format specification (i.e. YUV).
  size_t frame_length_in_bytes = 0;

  // Force the encoder and decoder to use a single core for processing.
  // Using a single core is necessary to get a deterministic behavior for the
  // encoded frames - using multiple cores will produce different encoded frames
  // since multiple cores are competing to consume the byte budget for each
  // frame in parallel.
  // If set to false, the maximum number of available cores will be used.
  bool use_single_core = false;

  // If > 0: forces the encoder to create a keyframe every Nth frame.
  // Note that the encoder may create a keyframe in other locations in addition
  // to this setting. Forcing key frames may also affect encoder planning
  // optimizations in a negative way, since it will suddenly be forced to
  // produce an expensive key frame.
  int keyframe_interval = 0;

  // The codec settings to use for the test (target bitrate, video size,
  // framerate and so on). This struct should be filled in using the
  // VideoCodingModule::Codec() method.
  webrtc::VideoCodec codec_settings;

  // If printing of information to stdout shall be performed during processing.
  bool verbose = true;

  // If HW or SW codec should be used.
  bool hw_codec = false;

  // In batch mode, the VideoProcessor is fed all the frames for processing
  // before any metrics are calculated. This is useful for pipelining HW codecs,
  // for which some calculated metrics otherwise would be incorrect. The
  // downside with batch mode is that mid-test rate allocation is not supported.
  bool batch_mode = false;
};

// Handles encoding/decoding of video using the VideoEncoder/VideoDecoder
// interfaces. This is done in a sequential manner in order to be able to
// measure times properly.
// The class processes a frame at the time for the configured input file.
// It maintains state of where in the source input file the processing is at.
//
// Regarding packet loss: Note that keyframes are excluded (first or all
// depending on the ExcludeFrameTypes setting). This is because if key frames
// would be altered, all the following delta frames would be pretty much
// worthless. VP8 has an error-resilience feature that makes it able to handle
// packet loss in key non-first keyframes, which is why only the first is
// excluded by default.
// Packet loss in such important frames is handled on a higher level in the
// Video Engine, where signaling would request a retransmit of the lost packets,
// since they're so important.
//
// Note this class is not thread safe in any way and is meant for simple testing
// purposes.
class VideoProcessor {
 public:
  VideoProcessor(webrtc::VideoEncoder* encoder,
                 webrtc::VideoDecoder* decoder,
                 FrameReader* analysis_frame_reader,
                 FrameWriter* analysis_frame_writer,
                 PacketManipulator* packet_manipulator,
                 const TestConfig& config,
                 Stats* stats,
                 IvfFileWriter* encoded_frame_writer,
                 FrameWriter* decoded_frame_writer);
  ~VideoProcessor();

  // Sets up callbacks and initializes the encoder and decoder.
  void Init();

  // Tears down callbacks and releases the encoder and decoder.
  void Release();

  // Processes a single frame. The frames must be processed in order, and the
  // VideoProcessor must be initialized first.
  void ProcessFrame(int frame_number);

  // Updates the encoder with target rates. Must be called at least once.
  void SetRates(int bitrate_kbps, int framerate_fps);


  // TODO(brandtr): Get rid of these functions by moving the corresponding QP
  // fields to the Stats object.
  int GetQpFromEncoder(int frame_number) const;
  int GetQpFromBitstream(int frame_number) const;


  // Return the number of dropped frames.
  int NumberDroppedFrames();

  // Return the number of spatial resizes.
  int NumberSpatialResizes();

 private:
  // Container that holds per-frame information that needs to be stored between
  // calls to Encode and Decode, as well as the corresponding callbacks. It is
  // not directly used for statistics -- for that, test::FrameStatistic is used.
  // TODO(brandtr): Get rid of this struct and use the Stats class instead.
  struct FrameInfo {
    int64_t encode_start_ns = 0;
    int64_t decode_start_ns = 0;
    int qp_encoder = 0;
    int qp_bitstream = 0;
    int decoded_width = 0;
    int decoded_height = 0;
    size_t manipulated_length = 0;
  };

  class VideoProcessorEncodeCompleteCallback
      : public webrtc::EncodedImageCallback {
   public:
    explicit VideoProcessorEncodeCompleteCallback(
        VideoProcessor* video_processor)
        : video_processor_(video_processor),
          task_queue_(rtc::TaskQueue::Current()) {}

    Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info,
        const webrtc::RTPFragmentationHeader* fragmentation) override {
      RTC_CHECK(codec_specific_info);

      if (task_queue_ && !task_queue_->IsCurrent()) {
        task_queue_->PostTask(std::unique_ptr<rtc::QueuedTask>(
            new EncodeCallbackTask(video_processor_, encoded_image,
                                   codec_specific_info, fragmentation)));
        return Result(Result::OK, 0);
      }

      video_processor_->FrameEncoded(codec_specific_info->codecType,
                                     encoded_image, fragmentation);
      return Result(Result::OK, 0);
    }

   private:
    class EncodeCallbackTask : public rtc::QueuedTask {
     public:
      EncodeCallbackTask(VideoProcessor* video_processor,
                         const webrtc::EncodedImage& encoded_image,
                         const webrtc::CodecSpecificInfo* codec_specific_info,
                         const webrtc::RTPFragmentationHeader* fragmentation)
          : video_processor_(video_processor),
            buffer_(encoded_image._buffer, encoded_image._length),
            encoded_image_(encoded_image),
            codec_specific_info_(*codec_specific_info) {
        encoded_image_._buffer = buffer_.data();
        RTC_CHECK(fragmentation);
        fragmentation_.CopyFrom(*fragmentation);
      }

      bool Run() override {
        video_processor_->FrameEncoded(codec_specific_info_.codecType,
                                       encoded_image_, &fragmentation_);
        return true;
      }

     private:
      VideoProcessor* const video_processor_;
      rtc::Buffer buffer_;
      webrtc::EncodedImage encoded_image_;
      const webrtc::CodecSpecificInfo codec_specific_info_;
      webrtc::RTPFragmentationHeader fragmentation_;
    };

    VideoProcessor* const video_processor_;
    rtc::TaskQueue* const task_queue_;
  };

  class VideoProcessorDecodeCompleteCallback
      : public webrtc::DecodedImageCallback {
   public:
    explicit VideoProcessorDecodeCompleteCallback(
        VideoProcessor* video_processor)
        : video_processor_(video_processor),
          task_queue_(rtc::TaskQueue::Current()) {}

    int32_t Decoded(webrtc::VideoFrame& image) override {
      if (task_queue_ && !task_queue_->IsCurrent()) {
        task_queue_->PostTask(
            [this, image]() { video_processor_->FrameDecoded(image); });
        return 0;
      }

      video_processor_->FrameDecoded(image);
      return 0;
    }

    int32_t Decoded(webrtc::VideoFrame& image,
                    int64_t decode_time_ms) override {
      return Decoded(image);
    }

    void Decoded(webrtc::VideoFrame& image,
                 rtc::Optional<int32_t> decode_time_ms,
                 rtc::Optional<uint8_t> qp) override {
      Decoded(image);
    }

   private:
    VideoProcessor* const video_processor_;
    rtc::TaskQueue* const task_queue_;
  };

  // Invoked by the callback adapter when a frame has completed encoding.
  void FrameEncoded(webrtc::VideoCodecType codec,
                    const webrtc::EncodedImage& encodedImage,
                    const webrtc::RTPFragmentationHeader* fragmentation);

  // Invoked by the callback adapter when a frame has completed decoding.
  void FrameDecoded(const webrtc::VideoFrame& image);

  // Use the frame number as the basis for timestamp to identify frames. Let the
  // first timestamp be non-zero, to not make the IvfFileWriter believe that we
  // want to use capture timestamps in the IVF files.
  uint32_t FrameNumberToTimestamp(int frame_number) const;
  int TimestampToFrameNumber(uint32_t timestamp) const;

  bool initialized_ GUARDED_BY(sequence_checker_);

  TestConfig config_ GUARDED_BY(sequence_checker_);

  webrtc::VideoEncoder* const encoder_;
  webrtc::VideoDecoder* const decoder_;
  const std::unique_ptr<VideoBitrateAllocator> bitrate_allocator_;

  // Adapters for the codec callbacks.
  VideoProcessorEncodeCompleteCallback encode_callback_;
  VideoProcessorDecodeCompleteCallback decode_callback_;

  // Fake network.
  PacketManipulator* const packet_manipulator_;

  // These (mandatory) file manipulators are used for, e.g., objective PSNR and
  // SSIM calculations at the end of a test run.
  FrameReader* const analysis_frame_reader_;
  FrameWriter* const analysis_frame_writer_;

  // These (optional) file writers are used to persistently store the encoded
  // and decoded bitstreams. The purpose is to give the experimenter an option
  // to subjectively evaluate the quality of the processing. Each frame writer
  // is enabled by being non-null.
  IvfFileWriter* const encoded_frame_writer_;
  FrameWriter* const decoded_frame_writer_;

  // Frame metadata for all frames that have been added through a call to
  // ProcessFrames(). We need to store this metadata over the course of the
  // test run, to support pipelining HW codecs.
  std::vector<FrameInfo> frame_infos_ GUARDED_BY(sequence_checker_);
  int last_encoded_frame_num_ GUARDED_BY(sequence_checker_);
  int last_decoded_frame_num_ GUARDED_BY(sequence_checker_);

  // Keep track of if we have excluded the first key frame from packet loss.
  bool first_key_frame_has_been_excluded_ GUARDED_BY(sequence_checker_);

  // Keep track of the last successfully decoded frame, since we write that
  // frame to disk when decoding fails.
  rtc::Buffer last_decoded_frame_buffer_ GUARDED_BY(sequence_checker_);

  // Statistics.
  Stats* stats_;
  int num_dropped_frames_ GUARDED_BY(sequence_checker_);
  int num_spatial_resizes_ GUARDED_BY(sequence_checker_);

  rtc::SequencedTaskChecker sequence_checker_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoProcessor);
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_
