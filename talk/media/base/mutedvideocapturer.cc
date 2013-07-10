/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/media/base/mutedvideocapturer.h"
#include "talk/media/base/videoframe.h"

#if defined(HAVE_WEBRTC_VIDEO)
#include "talk/media/webrtc/webrtcvideoframe.h"
#endif  // HAVE_WEBRTC_VIDEO


namespace cricket {

const char MutedVideoCapturer::kCapturerId[] = "muted_camera";

class MutedFramesGenerator : public talk_base::MessageHandler {
 public:
  explicit MutedFramesGenerator(const VideoFormat& format);
  virtual ~MutedFramesGenerator();

  // Called every |interval| ms. From |format|.interval given in the
  // constructor.
  sigslot::signal1<VideoFrame*> SignalFrame;

 protected:
  virtual void OnMessage(talk_base::Message* message);

 private:
  talk_base::Thread capture_thread_;
  talk_base::scoped_ptr<VideoFrame> muted_frame_;
  const VideoFormat format_;
  const int interval_;
  uint32 create_time_;
};

MutedFramesGenerator::MutedFramesGenerator(const VideoFormat& format)
    : format_(format),
      interval_(static_cast<int>(format.interval /
                                 talk_base::kNumNanosecsPerMillisec)),
      create_time_(talk_base::Time()) {
  capture_thread_.Start();
  capture_thread_.PostDelayed(interval_, this);
}

MutedFramesGenerator::~MutedFramesGenerator() { capture_thread_.Clear(this); }

void MutedFramesGenerator::OnMessage(talk_base::Message* message) {
  // Queue a new frame as soon as possible to minimize drift.
  capture_thread_.PostDelayed(interval_, this);
  if (!muted_frame_) {
#if defined(HAVE_WEBRTC_VIDEO)
#define VIDEO_FRAME_NAME WebRtcVideoFrame
#endif
#if defined(VIDEO_FRAME_NAME)
    muted_frame_.reset(new VIDEO_FRAME_NAME());
#else
    return;
#endif
  }
  uint32 current_timestamp = talk_base::Time();
  // Delta between create time and current time will be correct even if there is
  // a wraparound since they are unsigned integers.
  uint32 elapsed_time = current_timestamp - create_time_;
  if (!muted_frame_->InitToBlack(format_.width, format_.height, 1, 1,
                                 elapsed_time, current_timestamp)) {
    LOG(LS_ERROR) << "Failed to create a black frame.";
  }
  SignalFrame(muted_frame_.get());
}

MutedVideoCapturer::MutedVideoCapturer() { SetId(kCapturerId); }

MutedVideoCapturer::~MutedVideoCapturer() { Stop(); }

bool MutedVideoCapturer::GetBestCaptureFormat(const VideoFormat& desired,
                                              VideoFormat* best_format) {
  *best_format = desired;
  return true;
}

CaptureState MutedVideoCapturer::Start(const VideoFormat& capture_format) {
  if (frame_generator_.get()) {
    return CS_RUNNING;
  }
  frame_generator_.reset(new MutedFramesGenerator(capture_format));
  frame_generator_->SignalFrame
      .connect(this, &MutedVideoCapturer::OnMutedFrame);
  SetCaptureFormat(&capture_format);
  return CS_RUNNING;
}

void MutedVideoCapturer::Stop() {
  frame_generator_.reset();
  SetCaptureFormat(NULL);
}

bool MutedVideoCapturer::IsRunning() { return frame_generator_.get() != NULL; }

bool MutedVideoCapturer::GetPreferredFourccs(std::vector<uint32>* fourccs) {
  fourccs->clear();
  fourccs->push_back(cricket::FOURCC_I420);
  return true;
}

void MutedVideoCapturer::OnMutedFrame(VideoFrame* muted_frame) {
  SignalVideoFrame(this, muted_frame);
}

}  // namespace cricket
