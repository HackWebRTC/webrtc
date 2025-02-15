#pragma once

#include <memory>
#include <queue>
#include <string>

#include "api/task_queue/task_queue_factory.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "rtc_base/task_queue.h"

struct AVFormatContext;
struct AVStream;

namespace webrtc {

class FileCapturer {
 public:
  FileCapturer(const std::string& path,
               const std::string& dump_path,
               int width,
               int height,
               TaskQueueFactory* task_queue_factory);
  ~FileCapturer();

  void RegisterVideoFrameCallback(
      rtc::VideoSinkInterface<VideoFrame>* callback) {
    video_callback_ = callback;
  }

  void DeRegisterVideoFrameCallback() { video_callback_ = nullptr; }

  int Start(bool loop);

  void Stop();

 private:
  rtc::TaskQueue queue_;

  std::string path_;
  std::string dump_path_;
  int width_;
  int height_;
  volatile bool running_;

  rtc::VideoSinkInterface<VideoFrame>* video_callback_;
};

}  // namespace webrtc
