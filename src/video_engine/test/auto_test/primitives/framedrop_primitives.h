/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_SOURCE_FRAMEDROP_PRIMITIVES_H_
#define WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_SOURCE_FRAMEDROP_PRIMITIVES_H_

#include <list>
#include <map>

#include "video_engine/main/interface/vie_codec.h"
#include "video_engine/main/interface/vie_image_process.h"
#include "video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "video_engine/test/auto_test/helpers/vie_to_file_renderer.h"

class FrameDropDetector;
class TbInterfaces;

// Initializes the Video engine and its components, runs video playback using
// for KAutoTestSleepTimeMs milliseconds, then shuts down everything.
// The bit rate should be low enough to make the video encoder being forced to
// drop some frames, in order to test the frame drop detection that is performed
// by the FrameDropDetector class.
void TestFullStack(const TbInterfaces& interfaces,
                   int capture_id,
                   int video_channel,
                   int width,
                   int height,
                   int bit_rate_kbps,
                   FrameDropDetector* frame_drop_detector);

// A frame in a video file. The three different points in the stack when
// register the frame state are (in time order): sent, decoded, rendered.
class Frame {
 public:
  Frame(int number, unsigned int timestamp)
    : number_(number), frame_timestamp_(timestamp),
      sent_timestamp_in_us_(0), decoded_timestamp_in_us_(0),
      rendered_timestamp_in_us_(0) {}

  // Frame number, starting at 0.
  int number_;

  // Frame timestamp, that is used by Video Engine and RTP headers and set when
  // the frame is sent into the stack.
  unsigned int frame_timestamp_;

  // Timestamps for our measurements of when the frame is in different states.
  int64_t sent_timestamp_in_us_;
  int64_t decoded_timestamp_in_us_;
  int64_t rendered_timestamp_in_us_;
};

// Fixes the output file by copying the last successful frame into the place
// where the dropped frame would be, for all dropped frames (if any).
// This method will not be able to fix data for the first frame if that is
// dropped, since there'll be no previous frame to copy. This case should never
// happen because of encoder frame dropping at least.
// Parameters:
//    output_file              The output file to modify (pad with frame copies
//                             for all dropped frames)
//    total_number_of_frames   Number of frames in the reference file we want
//                             to match.
//    frame_length_in_bytes    Byte length of each frame.
//    dropped_frames           List of Frame objects. Must be sorted by frame
//                             number. If empty this method will do nothing.
void FixOutputFileForComparison(const std::string& output_file,
                                int total_number_of_frames,
                                int frame_length_in_bytes,
                                std::list<Frame*> dropped_frames);

// Handles statistics about dropped frames. Frames travel through the stack
// with different timestamps. The sent frames have one timestamp on the sending
// side while the decoded/rendered frames have another timestamp on the
// receiving side. However the difference between these timestamps is fixed,
// which we can use to identify the frames when they arrive, since the
// FrameDropDetector class gets data reported from both sides.
// The three different points in the stack when this class examines the frame
// states are (in time order): sent, decoded, rendered.
class FrameDropDetector {
 public:
  FrameDropDetector()
      : frame_timestamp_diff_(0) {}

  // Report a frame being sent; the first step of a frame transfer.
  // This timestamp becomes the frame timestamp in the Frame objects.
  void ReportSent(unsigned int timestamp);

  // Report a frame being rendered; happens right before it is received.
  // This timestamp differs from the one in ReportSent timestamp.
  void ReportDecoded(unsigned int timestamp);

  // Report a frame being rendered; the last step of a frame transfer.
  // This timestamp differs from the one in ReportSent timestamp, but is the
  // same as the ReportRendered timestamp.
  void ReportRendered(unsigned int timestamp);

  // The number of sent frames, i.e. the number of times the ReportSent has been
  // called successfully.
  int NumberSentFrames();

  // Calculates which frames have been registered as dropped at the decode step.
  const std::list<Frame*> GetFramesDroppedAtDecodeStep();

  // Calculates which frames have been registered as dropped at the render step.
  const std::list<Frame*> GetFramesDroppedAtRenderStep();

  // Prints a detailed report about all the different frame states and which
  // ones are detected as dropped, using ViETest::Log.
  void PrintReport();

 private:
  // Maps mapping frame timestamps to Frame objects.
  std::map<unsigned int, Frame*> sent_frames_;
  std::map<unsigned int, Frame*> decoded_frames_;
  std::map<unsigned int, Frame*> rendered_frames_;

  // A list with the frames sorted in their sent order:
  std::list<Frame*> sent_frames_list_;

  // The constant diff between the sent and rendered frames, since their
  // timestamps are converted.
  unsigned int frame_timestamp_diff_;
};

// Tracks which frames are received on the remote side and reports back to the
// FrameDropDetector class when they are rendered.
class FrameDropMonitoringRemoteFileRenderer : public ViEToFileRenderer {
 public:
  explicit FrameDropMonitoringRemoteFileRenderer(
      FrameDropDetector* frame_drop_detector)
      : frame_drop_detector_(frame_drop_detector) {}
  virtual ~FrameDropMonitoringRemoteFileRenderer() {}

  // Implementation of ExternalRenderer:
  int FrameSizeChange(unsigned int width, unsigned int height,
                      unsigned int number_of_streams);
  int DeliverFrame(unsigned char* buffer, int buffer_size,
                   unsigned int time_stamp);
 private:
  FrameDropDetector* frame_drop_detector_;
};

#endif  // WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_SOURCE_FRAMEDROP_PRIMITIVES_H_
