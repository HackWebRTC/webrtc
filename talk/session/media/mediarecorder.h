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

#include "talk/base/criticalsection.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/session/media/channel.h"
#include "talk/session/media/mediasink.h"

namespace talk_base {
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
  explicit RtpDumpSink(talk_base::StreamInterface* stream);
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
  talk_base::scoped_ptr<talk_base::StreamInterface> stream_;
  talk_base::scoped_ptr<RtpDumpWriter> writer_;
  talk_base::CriticalSection critical_section_;

  DISALLOW_COPY_AND_ASSIGN(RtpDumpSink);
};

class MediaRecorder {
 public:
  MediaRecorder();
  virtual ~MediaRecorder();

  bool AddChannel(VoiceChannel* channel,
                  talk_base::StreamInterface* send_stream,
                  talk_base::StreamInterface* recv_stream,
                  int filter);
  bool AddChannel(VideoChannel* channel,
                  talk_base::StreamInterface* send_stream,
                  talk_base::StreamInterface* recv_stream,
                  int filter);
  void RemoveChannel(BaseChannel* channel, SinkType type);
  bool EnableChannel(BaseChannel* channel, bool enable_send, bool enable_recv,
                     SinkType type);
  void FlushSinks();

 private:
  struct SinkPair {
    bool video_channel;
    int filter;
    talk_base::scoped_ptr<RtpDumpSink> send_sink;
    talk_base::scoped_ptr<RtpDumpSink> recv_sink;
  };

  bool InternalAddChannel(BaseChannel* channel,
                          bool video_channel,
                          talk_base::StreamInterface* send_stream,
                          talk_base::StreamInterface* recv_stream,
                          int filter);

  std::map<BaseChannel*, SinkPair*> sinks_;
  talk_base::CriticalSection critical_section_;

  DISALLOW_COPY_AND_ASSIGN(MediaRecorder);
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_MEDIARECORDER_H_
