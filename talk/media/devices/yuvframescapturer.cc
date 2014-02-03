#include "talk/media/devices/yuvframescapturer.h"

#include "talk/base/bytebuffer.h"
#include "talk/base/criticalsection.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"

#include "webrtc/system_wrappers/interface/clock.h"

namespace cricket {
///////////////////////////////////////////////////////////////////////
// Definition of private class YuvFramesThread that periodically generates
// frames.
///////////////////////////////////////////////////////////////////////
class YuvFramesCapturer::YuvFramesThread
    : public talk_base::Thread, public talk_base::MessageHandler {
 public:
  explicit YuvFramesThread(YuvFramesCapturer* capturer)
      : capturer_(capturer),
        finished_(false) {
  }

  virtual ~YuvFramesThread() {
    Stop();
  }

  // Override virtual method of parent Thread. Context: Worker Thread.
  virtual void Run() {
    // Read the first frame and start the message pump. The pump runs until
    // Stop() is called externally or Quit() is called by OnMessage().
    int waiting_time_ms = 0;
    if (capturer_) {
      capturer_->ReadFrame(true);
      PostDelayed(waiting_time_ms, this);
      Thread::Run();
    }

    talk_base::CritScope cs(&crit_);
    finished_ = true;
  }

  // Override virtual method of parent MessageHandler. Context: Worker Thread.
  virtual void OnMessage(talk_base::Message* /*pmsg*/) {
    int waiting_time_ms = 0;
    if (capturer_) {
      capturer_->ReadFrame(false);
      PostDelayed(waiting_time_ms, this);
    } else {
      Quit();
    }
  }

  // Check if Run() is finished.
  bool Finished() const {
    talk_base::CritScope cs(&crit_);
    return finished_;
  }

 private:
  YuvFramesCapturer* capturer_;
  mutable talk_base::CriticalSection crit_;
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(YuvFramesThread);
};

/////////////////////////////////////////////////////////////////////
// Implementation of class YuvFramesCapturer.
/////////////////////////////////////////////////////////////////////

const char* YuvFramesCapturer::kYuvFrameDeviceName = "YuvFramesGenerator";

// TODO(shaowei): allow width_ and height_ to be configurable.
YuvFramesCapturer::YuvFramesCapturer()
    : frames_generator_thread(NULL),
      width_(640),
      height_(480),
      frame_index_(0),
      barcode_interval_(1) {
}

YuvFramesCapturer::~YuvFramesCapturer() {
  Stop();
  delete[] static_cast<char*>(captured_frame_.data);
}

void YuvFramesCapturer::Init() {
  int size = width_ * height_;
  int qsize = size / 4;
  frame_generator_ = new YuvFrameGenerator(width_, height_, true);
  frame_data_size_ = size + 2 * qsize;
  captured_frame_.data = new char[frame_data_size_];
  captured_frame_.fourcc = FOURCC_IYUV;
  captured_frame_.pixel_height = 1;
  captured_frame_.pixel_width = 1;
  captured_frame_.width = width_;
  captured_frame_.height = height_;
  captured_frame_.data_size = frame_data_size_;

  // Enumerate the supported formats. We have only one supported format.
  VideoFormat format(width_, height_, VideoFormat::kMinimumInterval,
                     FOURCC_IYUV);
  std::vector<VideoFormat> supported;
  supported.push_back(format);
  SetSupportedFormats(supported);
}

CaptureState YuvFramesCapturer::Start(const VideoFormat& capture_format) {
  if (IsRunning()) {
    LOG(LS_ERROR) << "Yuv Frame Generator is already running";
    return CS_FAILED;
  }
  SetCaptureFormat(&capture_format);

  barcode_reference_timestamp_millis_ =
      static_cast<int64>(talk_base::Time()) * 1000;
  // Create a thread to generate frames.
  frames_generator_thread = new YuvFramesThread(this);
  bool ret = frames_generator_thread->Start();
  if (ret) {
    LOG(LS_INFO) << "Yuv Frame Generator started";
    return CS_RUNNING;
  } else {
    LOG(LS_ERROR) << "Yuv Frame Generator failed to start";
    return CS_FAILED;
  }
}

bool YuvFramesCapturer::IsRunning() {
  return frames_generator_thread && !frames_generator_thread->Finished();
}

void YuvFramesCapturer::Stop() {
  if (frames_generator_thread) {
    frames_generator_thread->Stop();
    frames_generator_thread = NULL;
    LOG(LS_INFO) << "Yuv Frame Generator stopped";
  }
  SetCaptureFormat(NULL);
}

bool YuvFramesCapturer::GetPreferredFourccs(std::vector<uint32>* fourccs) {
  if (!fourccs) {
    return false;
  }
  fourccs->push_back(GetSupportedFormats()->at(0).fourcc);
  return true;
}

// Executed in the context of YuvFramesThread.
void YuvFramesCapturer::ReadFrame(bool first_frame) {
  // 1. Signal the previously read frame to downstream.
  if (!first_frame) {
    SignalFrameCaptured(this, &captured_frame_);
  }
  uint8* buffer = new uint8[frame_data_size_];
  frame_generator_->GenerateNextFrame(buffer, GetBarcodeValue());
  frame_index_++;
  memmove(captured_frame_.data, buffer, frame_data_size_);
  delete[] buffer;
}


int32 YuvFramesCapturer::GetBarcodeValue() {
  if (barcode_reference_timestamp_millis_ == -1 ||
       frame_index_ % barcode_interval_ != 0) {
     return -1;
  }
  int64 now_millis = static_cast<int64>(talk_base::Time()) * 1000;
  return static_cast<int32>(now_millis - barcode_reference_timestamp_millis_);
}

}  // namespace cricket
