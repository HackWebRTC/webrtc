/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>
#include <string>

#include "framedrop_primitives.h"

#include "general_primitives.h"
#include "system_wrappers/interface/tick_util.h"
#include "tb_interfaces.h"
#include "testsupport/fileutils.h"
#include "testsupport/frame_reader.h"
#include "testsupport/frame_writer.h"
#include "video_capture_factory.h"
#include "vie_autotest.h"
#include "vie_autotest_defines.h"
#include "vie_to_file_renderer.h"

// Tracks which frames are sent on the local side and reports them to the
// FrameDropDetector class.
class SendTimestampEffectFilter: public webrtc::ViEEffectFilter {
 public:
  explicit SendTimestampEffectFilter(FrameDropDetector* frame_drop_detector)
      : frame_drop_detector_(frame_drop_detector) {}
  virtual ~SendTimestampEffectFilter() {}
  virtual int Transform(int size, unsigned char* frameBuffer,
                        unsigned int timeStamp90KHz, unsigned int width,
                        unsigned int height) {
    frame_drop_detector_->ReportSent(timeStamp90KHz);
    return 0;
  }
 private:
  FrameDropDetector* frame_drop_detector_;
};

// Tracks when frames are decoded on the remote side (received from the
// jitter buffer) and reports them to the FrameDropDetector class.
class DecodeTimestampEffectFilter: public webrtc::ViEEffectFilter {
 public:
  explicit DecodeTimestampEffectFilter(FrameDropDetector* frame_drop_detector)
      : frame_drop_detector_(frame_drop_detector) {}
  virtual ~DecodeTimestampEffectFilter() {}
  virtual int Transform(int size, unsigned char* frameBuffer,
                        unsigned int timeStamp90KHz, unsigned int width,
                        unsigned int height) {
    frame_drop_detector_->ReportDecoded(timeStamp90KHz);
    return 0;
  }
 private:
  FrameDropDetector* frame_drop_detector_;
};

void TestFullStack(const TbInterfaces& interfaces,
                   int capture_id,
                   int video_channel,
                   int width,
                   int height,
                   int bit_rate_kbps,
                   FrameDropDetector* frame_drop_detector) {
  webrtc::VideoEngine *video_engine_interface = interfaces.video_engine;
  webrtc::ViEBase *base_interface = interfaces.base;
  webrtc::ViECapture *capture_interface = interfaces.capture;
  webrtc::ViERender *render_interface = interfaces.render;
  webrtc::ViECodec *codec_interface = interfaces.codec;
  webrtc::ViENetwork *network_interface = interfaces.network;

  // ***************************************************************
  // Engine ready. Begin testing class
  // ***************************************************************
  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof (webrtc::VideoCodec));

  // Set up all receive codecs. This basically setup the codec interface
  // to be able to recognize all receive codecs based on payload type.
  for (int idx = 0; idx < codec_interface->NumberOfCodecs(); idx++) {
    EXPECT_EQ(0, codec_interface->GetCodec(idx, video_codec));
    SetSuitableResolution(&video_codec, width, height);

    EXPECT_EQ(0, codec_interface->SetReceiveCodec(video_channel, video_codec));
  }
  const char *ip_address = "127.0.0.1";
  const unsigned short rtp_port = 6000;
  EXPECT_EQ(0, network_interface->SetLocalReceiver(video_channel, rtp_port));
  EXPECT_EQ(0, base_interface->StartReceive(video_channel));
  EXPECT_EQ(0, network_interface->SetSendDestination(video_channel, ip_address,
                                                     rtp_port));
  // Setup only the VP8 codec, which is what we'll use.
  webrtc::VideoCodec codec;
  EXPECT_TRUE(FindSpecificCodec(webrtc::kVideoCodecVP8, codec_interface,
                                &codec));
  codec.startBitrate = bit_rate_kbps;
  codec.maxBitrate = bit_rate_kbps;
  codec.width = width;
  codec.height = height;
  EXPECT_EQ(0, codec_interface->SetSendCodec(video_channel, codec));

  webrtc::ViEImageProcess *image_process =
      webrtc::ViEImageProcess::GetInterface(video_engine_interface);
  EXPECT_TRUE(image_process);

  // Setup the effect filters
  DecodeTimestampEffectFilter decode_filter(frame_drop_detector);
  EXPECT_EQ(0, image_process->RegisterRenderEffectFilter(video_channel,
                                                         decode_filter));
  SendTimestampEffectFilter send_filter(frame_drop_detector);
  EXPECT_EQ(0, image_process->RegisterSendEffectFilter(video_channel,
                                                       send_filter));
  // Send video.
  EXPECT_EQ(0, base_interface->StartSend(video_channel));
  AutoTestSleep(KAutoTestSleepTimeMs);

  // Cleanup.
  EXPECT_EQ(0, image_process->DeregisterRenderEffectFilter(video_channel));
  EXPECT_EQ(0, image_process->DeregisterSendEffectFilter(video_channel));
  image_process->Release();
  ViETest::Log("Done!");

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************
  EXPECT_EQ(0, base_interface->StopSend(video_channel));
  EXPECT_EQ(0, base_interface->StopReceive(video_channel));
  EXPECT_EQ(0, render_interface->StopRender(capture_id));
  EXPECT_EQ(0, render_interface->StopRender(video_channel));
  EXPECT_EQ(0, render_interface->RemoveRenderer(capture_id));
  EXPECT_EQ(0, render_interface->RemoveRenderer(video_channel));
  EXPECT_EQ(0, capture_interface->DisconnectCaptureDevice(video_channel));
  EXPECT_EQ(0, base_interface->DeleteChannel(video_channel));
}

void FixOutputFileForComparison(const std::string& output_file,
                                int total_number_of_frames,
                                int frame_length_in_bytes,
                                std::list<Frame*> dropped_frames) {
  if (dropped_frames.size() == 0) {
    // No need to modify if no frames are dropped, since the file is already
    // frame-per-frame in sync in that case.
    return;
  }
  webrtc::test::FrameReaderImpl frame_reader(output_file,
                                             frame_length_in_bytes);
  const std::string temp_file = output_file + ".fixed";
  webrtc::test::FrameWriterImpl frame_writer(temp_file, frame_length_in_bytes);
  frame_reader.Init();
  frame_writer.Init();

  // Assume the dropped_frames list is sorted by frame number.
  int next_dropped_frame = dropped_frames.front()->number_;
  dropped_frames.pop_front();
  ASSERT_NE(0, next_dropped_frame) << "It should not be possible to drop the "
      "first frame. Both because we don't have anything useful to fill that "
      "gap with and it is impossible to detect it without any previous "
      "timestamps to compare with.";

  WebRtc_UWord8* last_read_frame_data =
      new WebRtc_UWord8[frame_length_in_bytes];

  // Write the first frame now since it will always be the same.
  EXPECT_TRUE(frame_reader.ReadFrame(last_read_frame_data));
  EXPECT_TRUE(frame_writer.WriteFrame(last_read_frame_data));

  // Process the file and write frame duplicates for all dropped frames.
  for (int i = 1; i < total_number_of_frames; ++i) {
    if (i == next_dropped_frame) {
      // Write the previous frame to the output file:
      EXPECT_TRUE(frame_writer.WriteFrame(last_read_frame_data));
      if (!dropped_frames.empty()) {
        next_dropped_frame = dropped_frames.front()->number_;
        dropped_frames.pop_front();
      }
    } else {
      // Read a new frame and write it to the output file.
      EXPECT_TRUE(frame_reader.ReadFrame(last_read_frame_data));
      EXPECT_TRUE(frame_writer.WriteFrame(last_read_frame_data));
    }
  }
  delete[] last_read_frame_data;
  frame_reader.Close();
  frame_writer.Close();
  ASSERT_EQ(0, std::remove(output_file.c_str()));
  ASSERT_EQ(0, std::rename(temp_file.c_str(), output_file.c_str()));
}

void FrameDropDetector::ReportSent(unsigned int timestamp) {
  int number = sent_frames_list_.size();
  Frame* frame = new Frame(number, timestamp);
  frame->sent_timestamp_in_us_ = webrtc::TickTime::MicrosecondTimestamp();
  sent_frames_list_.push_back(frame);
  sent_frames_[timestamp] = frame;
}

void FrameDropDetector::ReportDecoded(unsigned int timestamp) {
  // When the first sent frame arrives we calculate the fixed difference
  // between the timestamps of the sent frames and the decoded/rendered frames.
  // This diff is then used to identify the frames from the sent_frames_ map.
  if (frame_timestamp_diff_ == 0) {
    Frame* first_sent_frame = sent_frames_list_.front();
    frame_timestamp_diff_ = timestamp - first_sent_frame->frame_timestamp_;
  }
  // Calculate the sent timestamp required to identify the frame:
  unsigned int sent_timestamp = timestamp - frame_timestamp_diff_;

  // Find the right Frame object in the map of sent frames:
  Frame* frame = sent_frames_[sent_timestamp];
  frame->decoded_timestamp_in_us_ = webrtc::TickTime::MicrosecondTimestamp();
  decoded_frames_[sent_timestamp] = frame;
}

void FrameDropDetector::ReportRendered(unsigned int timestamp) {
  // Calculate the sent timestamp required to identify the frame:
  unsigned int sent_timestamp = timestamp - frame_timestamp_diff_;

  // Find this frame in the map of sent frames:
  Frame* frame = sent_frames_[sent_timestamp];
  frame->rendered_timestamp_in_us_ = webrtc::TickTime::MicrosecondTimestamp();
  rendered_frames_[sent_timestamp] = frame;
}

int FrameDropDetector::NumberSentFrames() {
  return static_cast<int>(sent_frames_.size());
}

void FrameDropDetector::PrintReport() {
  ViETest::Log("Frame Drop Detector report:");
  ViETest::Log("Sent     frames: %ld", sent_frames_.size());
  ViETest::Log("Decoded  frames: %ld", decoded_frames_.size());
  ViETest::Log("Rendered frames: %ld", rendered_frames_.size());

  // Display all frames and stats for them:
  long last_sent = 0;
  long last_decoded = 0;
  long last_rendered = 0;
  ViETest::Log("Sent frames summary:");
  ViETest::Log("Deltas are in microseconds and only cover existing frames.");
  ViETest::Log("Frame no  SentDelta  DecodedDelta  RenderedDelta  DecodedDrop? "
      "RenderedDrop?");
  for (std::list<Frame*>::iterator it = sent_frames_list_.begin();
       it != sent_frames_list_.end(); it++) {
    bool dropped_decode = (decoded_frames_.find((*it)->frame_timestamp_) ==
        decoded_frames_.end());
    bool dropped_render = (rendered_frames_.find((*it)->frame_timestamp_) ==
        rendered_frames_.end());
    int sent_delta = static_cast<int>((*it)->sent_timestamp_in_us_ - last_sent);
    int decoded_delta = dropped_decode ? 0 :
        static_cast<int>((*it)->decoded_timestamp_in_us_ - last_decoded);
    int rendered_delta = dropped_render ? 0 :
        static_cast<int>((*it)->rendered_timestamp_in_us_ - last_rendered);

    // Set values to 0 for the first frame:
    if ((*it)->number_ == 0) {
      sent_delta = 0;
      decoded_delta = 0;
      rendered_delta = 0;
    }
    ViETest::Log("%8d %10d    %10d     %10d    %s     %s", (*it)->number_,
           sent_delta, decoded_delta, rendered_delta,
           dropped_decode ? "DROPPED" : "      ",
           dropped_render ? "DROPPED" : "      ");
    last_sent = (*it)->sent_timestamp_in_us_;
    if (!dropped_render) {
      last_decoded = (*it)->decoded_timestamp_in_us_;
      last_rendered = (*it)->rendered_timestamp_in_us_;
    }
  }
  // Find and print the dropped frames. Work at a copy of the sent_frames_ map.
  std::list<Frame*> decode_dropped_frames = GetFramesDroppedAtDecodeStep();
  ViETest::Log("Number of dropped frames at the decode step: %d",
               static_cast<int>(decode_dropped_frames.size()));
  std::list<Frame*> render_dropped_frames = GetFramesDroppedAtRenderStep();
  ViETest::Log("Number of dropped frames at the render step: %d",
               static_cast<int>(render_dropped_frames.size()));
}

const std::list<Frame*> FrameDropDetector::GetFramesDroppedAtDecodeStep() {
  std::list<Frame*> dropped_frames;
  std::map<unsigned int, Frame*>::iterator it;
  for (it = sent_frames_.begin(); it != sent_frames_.end(); it++) {
    if (decoded_frames_.find(it->first) == decoded_frames_.end()) {
      dropped_frames.push_back(it->second);
    }
  }
  return dropped_frames;
}

const std::list<Frame*> FrameDropDetector::GetFramesDroppedAtRenderStep() {
  std::list<Frame*> dropped_frames;
  std::map<unsigned int, Frame*>::iterator it;
  for (it = sent_frames_.begin(); it != sent_frames_.end(); it++) {
    if (rendered_frames_.find(it->first) == rendered_frames_.end()) {
      dropped_frames.push_back(it->second);
    }
  }
  return dropped_frames;
}

int FrameDropMonitoringRemoteFileRenderer::DeliverFrame(
    unsigned char *buffer, int buffer_size, unsigned int time_stamp) {
  // Register that this frame has been rendered:
  frame_drop_detector_->ReportRendered(time_stamp);
  return ViEToFileRenderer::DeliverFrame(buffer, buffer_size, time_stamp);
}

int FrameDropMonitoringRemoteFileRenderer::FrameSizeChange(
    unsigned int width, unsigned int height, unsigned int number_of_streams) {
  return ViEToFileRenderer::FrameSizeChange(width, height, number_of_streams);
}
