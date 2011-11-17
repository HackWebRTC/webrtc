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

#include <cassert>

namespace webrtc {
namespace test {

FileHandlerImpl::FileHandlerImpl(std::string input_filename,
                                 std::string output_filename,
                                 int frame_length_in_bytes)
    : input_filename_(input_filename),
      output_filename_(output_filename),
      frame_length_in_bytes_(frame_length_in_bytes),
      input_file_(NULL),
      output_file_(NULL) {
}

FileHandlerImpl::~FileHandlerImpl() {
  Close();
}

bool FileHandlerImpl::Init() {
  if (frame_length_in_bytes_ <= 0) {
    fprintf(stderr, "Frame length must be >0, was %d\n",
            frame_length_in_bytes_);
    return false;
  }

  input_file_ = fopen(input_filename_.c_str(), "rb");
  if (input_file_ == NULL) {
    fprintf(stderr, "Couldn't open input file for reading: %s\n",
            input_filename_.c_str());
    return false;
  }
  output_file_ = fopen(output_filename_.c_str(), "wb");
  if (output_file_ == NULL) {
    fprintf(stderr, "Couldn't open output file for writing: %s\n",
            output_filename_.c_str());
    return false;
  }
  // Calculate total number of frames:
  WebRtc_UWord64 source_file_size = GetFileSize(input_filename_);
  if (source_file_size <= 0u) {
    fprintf(stderr, "Found empty file: %s\n", input_filename_.c_str());
        return false;
  }
  number_of_frames_ = source_file_size / frame_length_in_bytes_;
  return true;
}

void FileHandlerImpl::Close() {
  if (input_file_ != NULL) {
    fclose(input_file_);
    input_file_ = NULL;
  }
  if (output_file_ != NULL) {
    fclose(output_file_);
    output_file_ = NULL;
  }
}

bool FileHandlerImpl::ReadFrame(WebRtc_UWord8* source_buffer) {
  assert(source_buffer);
  if (input_file_ == NULL) {
    fprintf(stderr, "FileHandler is not initialized (input file is NULL)\n");
    return false;
  }
  size_t nbr_read = fread(source_buffer, 1, frame_length_in_bytes_,
                          input_file_);
  if (nbr_read != static_cast<unsigned int>(frame_length_in_bytes_) &&
      ferror(input_file_)) {
    fprintf(stderr, "Error reading from input file: %s\n",
            input_filename_.c_str());
    return false;
  }
  if (feof(input_file_) != 0) {
    return false;  // no more frames to process
  }
  return true;
}

WebRtc_UWord64 FileHandlerImpl::GetFileSize(std::string filename) {
  FILE* f = fopen(filename.c_str(), "rb");
  WebRtc_UWord64 size = 0;
  if (f != NULL) {
    if (fseek(f, 0, SEEK_END) == 0) {
      size = ftell(f);
    }
    fclose(f);
  }
  return size;
}

bool FileHandlerImpl::WriteFrame(WebRtc_UWord8* frame_buffer) {
  assert(frame_buffer);
  if (output_file_ == NULL) {
    fprintf(stderr, "FileHandler is not initialized (output file is NULL)\n");
    return false;
  }
  int bytes_written = fwrite(frame_buffer, 1, frame_length_in_bytes_,
                             output_file_);
  if (bytes_written != frame_length_in_bytes_) {
    fprintf(stderr, "Failed to write %d bytes to file %s\n",
            frame_length_in_bytes_, output_filename_.c_str());
    return false;
  }
  return true;
}

}  // namespace test
}  // namespace webrtc
