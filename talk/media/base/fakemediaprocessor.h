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

#ifndef TALK_MEDIA_BASE_FAKEMEDIAPROCESSOR_H_
#define TALK_MEDIA_BASE_FAKEMEDIAPROCESSOR_H_

#include "talk/media/base/videoprocessor.h"
#include "talk/media/base/voiceprocessor.h"

namespace cricket {

class AudioFrame;

class FakeMediaProcessor : public VoiceProcessor, public VideoProcessor {
 public:
  FakeMediaProcessor()
      : voice_frame_count_(0),
        video_frame_count_(0),
        drop_frames_(false),
        dropped_frame_count_(0) {
  }
  virtual ~FakeMediaProcessor() {}

  virtual void OnFrame(uint32 ssrc,
                       MediaProcessorDirection direction,
                       AudioFrame* frame) {
    ++voice_frame_count_;
  }
  virtual void OnFrame(uint32 ssrc, VideoFrame* frame_ptr, bool* drop_frame) {
    ++video_frame_count_;
    if (drop_frames_) {
      *drop_frame = true;
      ++dropped_frame_count_;
    }
  }
  virtual void OnVoiceMute(uint32 ssrc, bool muted) {}
  virtual void OnVideoMute(uint32 ssrc, bool muted) {}

  int voice_frame_count() const { return voice_frame_count_; }
  int video_frame_count() const { return video_frame_count_; }

  void set_drop_frames(bool b) { drop_frames_ = b; }
  int dropped_frame_count() const { return dropped_frame_count_; }

 private:
  // TODO(janahan): make is a map so that we can multiple ssrcs
  int voice_frame_count_;
  int video_frame_count_;
  bool drop_frames_;
  int dropped_frame_count_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_FAKEMEDIAPROCESSOR_H_
