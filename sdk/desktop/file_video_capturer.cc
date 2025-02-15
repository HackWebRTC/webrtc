#include "sdk/desktop/file_video_capturer.h"

#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/logging.h"

namespace AvConf {

FileVideoCapturer* FileVideoCapturer::Create(int width,
                                             int height,
                                             const char* path) {
  std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory =
      webrtc::CreateDefaultTaskQueueFactory();
  return new FileVideoCapturer(new webrtc::FileCapturer(
      path, "", width, height, task_queue_factory.get()));
}

FileVideoCapturer::FileVideoCapturer(webrtc::FileCapturer* capturer)
    : capturer_(capturer), loop_(true) {}

FileVideoCapturer::~FileVideoCapturer() {}

void FileVideoCapturer::Start() {
  capturer_->RegisterVideoFrameCallback(this);
  capturer_->Start(loop_);
}

void FileVideoCapturer::Stop() {
  capturer_->DeRegisterVideoFrameCallback();
  capturer_->Stop();
}

void FileVideoCapturer::OnFrame(const webrtc::VideoFrame& frame) {
  AvcfVideoCapturer::OnFrame(frame);
}

}  // namespace AvConf
