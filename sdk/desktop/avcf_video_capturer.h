#pragma once

#include <stddef.h>

#include <memory>

#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"

namespace AvConf {

class AvcfVideoCapturer : public rtc::VideoSourceInterface<webrtc::VideoFrame> {
 public:
  AvcfVideoCapturer();
  ~AvcfVideoCapturer() override;

  virtual void Start() = 0;

  virtual void Stop() = 0;

  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

  void OnOutputFormatRequest(int width, int height, int fps);

 protected:
  void OnFrame(const webrtc::VideoFrame& frame);
  rtc::VideoSinkWants GetSinkWants();

 private:
  void UpdateVideoAdapter();

  rtc::VideoBroadcaster broadcaster_;
  cricket::VideoAdapter video_adapter_;
};
}  // namespace AvConf
