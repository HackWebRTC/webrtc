/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef SRC_MODULES_VIDEO_CODING_CODECS_TEST_FILE_HANDLER_H_
#define SRC_MODULES_VIDEO_CODING_CODECS_TEST_FILE_HANDLER_H_

#include <cstdio>
#include <string>

#include "typedefs.h"

namespace webrtc {
namespace test {

// Handles reading and writing video files for the test framework's needs.
class FileHandler {
 public:
  virtual ~FileHandler() {}

  // Initializes the file handler, i.e. opens the input and output files etc.
  // This must be called before reading or writing frames has started.
  // Returns false if an error has occurred, in addition to printing to stderr.
  virtual bool Init() = 0;

  // Reads a frame into the supplied buffer, which must contain enough space
  // for the frame size.
  // Returns true if there are more frames to read, false if we've already
  // read the last frame (in the previous call).
  virtual bool ReadFrame(WebRtc_UWord8* source_buffer) = 0;

  // Writes a frame of the configured frame length to the output file.
  // Returns true if the write was successful, false otherwise.
  virtual bool WriteFrame(WebRtc_UWord8* frame_buffer) = 0;

  // Closes the input and output files. Essentially makes this class impossible
  // to use anymore.
  virtual void Close() = 0;

  // File size of the supplied file in bytes. Will return 0 if the file is
  // empty or if the file does not exist/is readable.
  virtual WebRtc_UWord64 GetFileSize(std::string filename) = 0;
  // Frame length in bytes of a single frame image.
  virtual int GetFrameLength() = 0;
  // Total number of frames in the input video source.
  virtual int GetNumberOfFrames() = 0;
};

class FileHandlerImpl : public FileHandler {
 public:
  // Creates a file handler. The input file is assumed to exist and be readable
  // and the output file must be writable.
  FileHandlerImpl(std::string input_filename,
                  std::string output_filename,
                  int frame_length_in_bytes);
  virtual ~FileHandlerImpl();
  bool Init();
  bool ReadFrame(WebRtc_UWord8* source_buffer);
  bool WriteFrame(WebRtc_UWord8* frame_buffer);
  void Close();
  WebRtc_UWord64 GetFileSize(std::string filename);
  int GetFrameLength() { return frame_length_in_bytes_; }
  int GetNumberOfFrames() { return number_of_frames_; }

 private:
  std::string input_filename_;
  std::string output_filename_;
  int frame_length_in_bytes_;
  int number_of_frames_;
  FILE* input_file_;
  FILE* output_file_;
};

}  // namespace test
}  // namespace webrtc

#endif  // SRC_MODULES_VIDEO_CODING_CODECS_TEST_FILE_HANDLER_H_
