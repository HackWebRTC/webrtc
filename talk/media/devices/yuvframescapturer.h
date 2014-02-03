#ifndef TALK_MEDIA_DEVICES_YUVFRAMESCAPTURER_H_
#define TALK_MEDIA_DEVICES_YUVFRAMESCAPTURER_H_

#include <string>
#include <vector>

#include "talk/base/stream.h"
#include "talk/base/stringutils.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/yuvframegenerator.h"


namespace talk_base {
class FileStream;
}

namespace cricket {


// Simulated video capturer that periodically reads frames from a file.
class YuvFramesCapturer : public VideoCapturer {
 public:
  YuvFramesCapturer();
  YuvFramesCapturer(int width, int height);
  virtual ~YuvFramesCapturer();

  static const char* kYuvFrameDeviceName;
  static Device CreateYuvFramesCapturerDevice() {
    std::stringstream id;
    id << kYuvFrameDeviceName;
    return Device(id.str(), id.str());
  }
  static bool IsYuvFramesCapturerDevice(const Device& device) {
    return talk_base::starts_with(device.id.c_str(), kYuvFrameDeviceName);
  }

  void Init();
  // Override virtual methods of parent class VideoCapturer.
  virtual CaptureState Start(const VideoFormat& capture_format);
  virtual void Stop();
  virtual bool IsRunning();
  virtual bool IsScreencast() const { return false; }

 protected:
  // Override virtual methods of parent class VideoCapturer.
  virtual bool GetPreferredFourccs(std::vector<uint32>* fourccs);

  // Read a frame and determine how long to wait for the next frame.
  void ReadFrame(bool first_frame);

 private:
  class YuvFramesThread;  // Forward declaration, defined in .cc.

  YuvFrameGenerator* frame_generator_;
  CapturedFrame captured_frame_;
  YuvFramesThread* frames_generator_thread;
  int width_;
  int height_;
  uint32 frame_data_size_;
  uint32 frame_index_;

  int64 barcode_reference_timestamp_millis_;
  int32 barcode_interval_;
  int32 GetBarcodeValue();

  DISALLOW_COPY_AND_ASSIGN(YuvFramesCapturer);
};

}  // namespace cricket

#endif  // TALK_MEDIA_DEVICES_YUVFRAMESCAPTURER_H_
