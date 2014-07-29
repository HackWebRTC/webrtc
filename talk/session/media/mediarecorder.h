/*
 * libjingle
 * Copyright 2010 Google Inc.
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

#ifndef TALK_SESSION_MEDIA_MEDIARECORDER_H_
#define TALK_SESSION_MEDIA_MEDIARECORDER_H_

#include <map>
#include <string>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/sigslot.h"
#include "talk/session/media/channel.h"
#include "talk/session/media/mediasink.h"

namespace rtc {
class Pathname;
class FileStream;
}

namespace cricket {

class BaseChannel;
class VideoChannel;
class VoiceChannel;
class RtpDumpWriter;

// RtpDumpSink implements MediaSinkInterface by dumping the RTP/RTCP packets to
// a file.
class RtpDumpSink : public MediaSinkInterface, public sigslot::has_slots<> {
 public:
  // Takes ownership of stream.
  explicit RtpDumpSink(rtc::StreamInterface* stream);
  virtual ~RtpDumpSink();

  virtual void SetMaxSize(size_t size);
  virtual bool Enable(bool enable);
  virtual bool IsEnabled() const { return recording_; }
  virtual void OnPacket(const void* data, size_t size, bool rtcp);
  virtual void set_packet_filter(int filter);
  int packet_filter() const { return packet_filter_; }
  void Flush();

 private:
  size_t max_size_;
  bool recording_;
  int packet_filter_;
  rtc::scoped_ptr<rtc::StreamInterface> stream_;
  rtc::scoped_ptr<RtpDumpWriter> writer_;
  rtc::CriticalSection critical_section_;

  DISALLOW_COPY_AND_ASSIGN(RtpDumpSink);
};

class MediaRecorder {
 public:
  MediaRecorder();
  virtual ~MediaRecorder();

  bool AddChannel(VoiceChannel* channel,
                  rtc::StreamInterface* send_stream,
                  rtc::StreamInterface* recv_stream,
                  int filter);
  bool AddChannel(VideoChannel* channel,
                  rtc::StreamInterface* send_stream,
                  rtc::StreamInterface* recv_stream,
                  int filter);
  void RemoveChannel(BaseChannel* channel, SinkType type);
  bool EnableChannel(BaseChannel* channel, bool enable_send, bool enable_recv,
                     SinkType type);
  void FlushSinks();

 private:
  struct SinkPair {
    bool video_channel;
    int filter;
    rtc::scoped_ptr<RtpDumpSink> send_sink;
    rtc::scoped_ptr<RtpDumpSink> recv_sink;
  };

  bool InternalAddChannel(BaseChannel* channel,
                          bool video_channel,
                          rtc::StreamInterface* send_stream,
                          rtc::StreamInterface* recv_stream,
                          int filter);

  std::map<BaseChannel*, SinkPair*> sinks_;
  rtc::CriticalSection critical_section_;

  DISALLOW_COPY_AND_ASSIGN(MediaRecorder);
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_MEDIARECORDER_H_
