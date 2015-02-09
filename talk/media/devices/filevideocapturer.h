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

//
// This file contains two classes, VideoRecorder and FileVideoCapturer.
// VideoRecorder records the captured frames into a file. The file stores a
// sequence of captured frames; each frame has a header defined in struct
// CapturedFrame, followed by the frame data.
//
// FileVideoCapturer, a subclass of VideoCapturer, is a simulated video capturer
// that periodically reads images from a previously recorded file.

#ifndef TALK_MEDIA_DEVICES_FILEVIDEOCAPTURER_H_
#define TALK_MEDIA_DEVICES_FILEVIDEOCAPTURER_H_

#include <string>
#include <vector>

#include "talk/media/base/videocapturer.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/stringutils.h"

namespace rtc {
class FileStream;
}

namespace cricket {

// Utility class to record the frames captured by a video capturer into a file.
class VideoRecorder {
 public:
  VideoRecorder() {}
  ~VideoRecorder() { Stop(); }

  // Start the recorder by opening the specified file. Return true if the file
  // is opened successfully. write_header should normally be true; false means
  // write raw frame pixel data to file without any headers.
  bool Start(const std::string& filename, bool write_header);
  // Stop the recorder by closing the file.
  void Stop();
  // Record a video frame to the file. Return true if the frame is written to
  // the file successfully. This method needs to be called after Start() and
  // before Stop().
  bool RecordFrame(const CapturedFrame& frame);

 private:
  rtc::FileStream video_file_;
  bool write_header_;

  DISALLOW_COPY_AND_ASSIGN(VideoRecorder);
};

// Simulated video capturer that periodically reads frames from a file.
class FileVideoCapturer : public VideoCapturer {
 public:
  static const int kForever = -1;

  FileVideoCapturer();
  virtual ~FileVideoCapturer();

  // Determines if the given device is actually a video file, to be captured
  // with a FileVideoCapturer.
  static bool IsFileVideoCapturerDevice(const Device& device) {
    return rtc::starts_with(device.id.c_str(), kVideoFileDevicePrefix);
  }

  // Creates a fake device for the given filename.
  static Device CreateFileVideoCapturerDevice(const std::string& filename) {
    std::stringstream id;
    id << kVideoFileDevicePrefix << filename;
    return Device(filename, id.str());
  }

  // Set how many times to repeat reading the file. Repeat forever if the
  // parameter is kForever; no repeat if the parameter is 0 or
  // less than -1.
  void set_repeat(int repeat) { repeat_ = repeat; }

  // If ignore_framerate is true, file is read as quickly as possible. If
  // false, read rate is controlled by the timestamps in the video file
  // (thus simulating camera capture). Default value set to false.
  void set_ignore_framerate(bool ignore_framerate) {
    ignore_framerate_ = ignore_framerate;
  }

  // Initializes the capturer with the given file.
  bool Init(const std::string& filename);

  // Initializes the capturer with the given device. This should only be used
  // if IsFileVideoCapturerDevice returned true for the given device.
  bool Init(const Device& device);

  // Override virtual methods of parent class VideoCapturer.
  virtual CaptureState Start(const VideoFormat& capture_format);
  virtual void Stop();
  virtual bool IsRunning();
  virtual bool IsScreencast() const { return false; }

 protected:
  // Override virtual methods of parent class VideoCapturer.
  virtual bool GetPreferredFourccs(std::vector<uint32>* fourccs);

  // Read the frame header from the file stream, video_file_.
  rtc::StreamResult ReadFrameHeader(CapturedFrame* frame);

  // Read a frame and determine how long to wait for the next frame. If the
  // frame is read successfully, Set the output parameter, wait_time_ms and
  // return true. Otherwise, do not change wait_time_ms and return false.
  bool ReadFrame(bool first_frame, int* wait_time_ms);

  // Return the CapturedFrame - useful for extracting contents after reading
  // a frame. Should be used only while still reading a file (i.e. only while
  // the CapturedFrame object still exists).
  const CapturedFrame* frame() const {
    return &captured_frame_;
  }

 private:
  class FileReadThread;  // Forward declaration, defined in .cc.

  static const char* kVideoFileDevicePrefix;
  rtc::FileStream video_file_;
  CapturedFrame captured_frame_;
  // The number of bytes allocated buffer for captured_frame_.data.
  uint32 frame_buffer_size_;
  FileReadThread* file_read_thread_;
  int repeat_;  // How many times to repeat the file.
  int64 start_time_ns_;  // Time when the file video capturer starts.
  int64 last_frame_timestamp_ns_;  // Timestamp of last read frame.
  bool ignore_framerate_;

  DISALLOW_COPY_AND_ASSIGN(FileVideoCapturer);
};

}  // namespace cricket

#endif  // TALK_MEDIA_DEVICES_FILEVIDEOCAPTURER_H_
