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

#include "talk/session/media/mediarecorder.h"

#include <limits.h>

#include <string>

#include "talk/base/fileutils.h"
#include "talk/base/logging.h"
#include "talk/base/pathutils.h"
#include "talk/media/base/rtpdump.h"


namespace cricket {

///////////////////////////////////////////////////////////////////////////
// Implementation of RtpDumpSink.
///////////////////////////////////////////////////////////////////////////
RtpDumpSink::RtpDumpSink(talk_base::StreamInterface* stream)
    : max_size_(INT_MAX),
      recording_(false),
      packet_filter_(PF_NONE) {
  stream_.reset(stream);
}

RtpDumpSink::~RtpDumpSink() {}

void RtpDumpSink::SetMaxSize(size_t size) {
  talk_base::CritScope cs(&critical_section_);
  max_size_ = size;
}

bool RtpDumpSink::Enable(bool enable) {
  talk_base::CritScope cs(&critical_section_);

  recording_ = enable;

  // Create a file and the RTP writer if we have not done yet.
  if (recording_ && !writer_) {
    if (!stream_) {
      return false;
    }
    writer_.reset(new RtpDumpWriter(stream_.get()));
    writer_->set_packet_filter(packet_filter_);
  } else if (!recording_ && stream_) {
    stream_->Flush();
  }
  return true;
}

void RtpDumpSink::OnPacket(const void* data, size_t size, bool rtcp) {
  talk_base::CritScope cs(&critical_section_);

  if (recording_ && writer_) {
    size_t current_size;
    if (writer_->GetDumpSize(&current_size) &&
        current_size + RtpDumpPacket::kHeaderLength + size <= max_size_) {
      if (!rtcp) {
        writer_->WriteRtpPacket(data, size);
      } else {
        // TODO(whyuan): Enable recording RTCP.
      }
    }
  }
}

void RtpDumpSink::set_packet_filter(int filter) {
  talk_base::CritScope cs(&critical_section_);
  packet_filter_ = filter;
  if (writer_) {
    writer_->set_packet_filter(packet_filter_);
  }
}

void RtpDumpSink::Flush() {
  talk_base::CritScope cs(&critical_section_);
  if (stream_) {
    stream_->Flush();
  }
}

///////////////////////////////////////////////////////////////////////////
// Implementation of MediaRecorder.
///////////////////////////////////////////////////////////////////////////
MediaRecorder::MediaRecorder() {}

MediaRecorder::~MediaRecorder() {
  talk_base::CritScope cs(&critical_section_);
  std::map<BaseChannel*, SinkPair*>::iterator itr;
  for (itr = sinks_.begin(); itr != sinks_.end(); ++itr) {
    delete itr->second;
  }
}

bool MediaRecorder::AddChannel(VoiceChannel* channel,
                               talk_base::StreamInterface* send_stream,
                               talk_base::StreamInterface* recv_stream,
                               int filter) {
  return InternalAddChannel(channel, false, send_stream, recv_stream,
                            filter);
}
bool MediaRecorder::AddChannel(VideoChannel* channel,
                               talk_base::StreamInterface* send_stream,
                               talk_base::StreamInterface* recv_stream,
                               int filter) {
  return InternalAddChannel(channel, true, send_stream, recv_stream,
                            filter);
}

bool MediaRecorder::InternalAddChannel(BaseChannel* channel,
                                       bool video_channel,
                                       talk_base::StreamInterface* send_stream,
                                       talk_base::StreamInterface* recv_stream,
                                       int filter) {
  if (!channel) {
    return false;
  }

  talk_base::CritScope cs(&critical_section_);
  if (sinks_.end() != sinks_.find(channel)) {
    return false;  // The channel was added already.
  }

  SinkPair* sink_pair = new SinkPair;
  sink_pair->video_channel = video_channel;
  sink_pair->filter = filter;
  sink_pair->send_sink.reset(new RtpDumpSink(send_stream));
  sink_pair->send_sink->set_packet_filter(filter);
  sink_pair->recv_sink.reset(new RtpDumpSink(recv_stream));
  sink_pair->recv_sink->set_packet_filter(filter);
  sinks_[channel] = sink_pair;

  return true;
}

void MediaRecorder::RemoveChannel(BaseChannel* channel,
                                  SinkType type) {
  talk_base::CritScope cs(&critical_section_);
  std::map<BaseChannel*, SinkPair*>::iterator itr = sinks_.find(channel);
  if (sinks_.end() != itr) {
    channel->UnregisterSendSink(itr->second->send_sink.get(), type);
    channel->UnregisterRecvSink(itr->second->recv_sink.get(), type);
    delete itr->second;
    sinks_.erase(itr);
  }
}

bool MediaRecorder::EnableChannel(
    BaseChannel* channel, bool enable_send, bool enable_recv,
    SinkType type) {
  talk_base::CritScope cs(&critical_section_);
  std::map<BaseChannel*, SinkPair*>::iterator itr = sinks_.find(channel);
  if (sinks_.end() == itr) {
    return false;
  }

  SinkPair* sink_pair = itr->second;
  RtpDumpSink* sink = sink_pair->send_sink.get();
  sink->Enable(enable_send);
  if (enable_send) {
    channel->RegisterSendSink(sink, &RtpDumpSink::OnPacket, type);
  } else {
    channel->UnregisterSendSink(sink, type);
  }

  sink = sink_pair->recv_sink.get();
  sink->Enable(enable_recv);
  if (enable_recv) {
    channel->RegisterRecvSink(sink, &RtpDumpSink::OnPacket, type);
  } else {
    channel->UnregisterRecvSink(sink, type);
  }

  if (sink_pair->video_channel &&
      (sink_pair->filter & PF_RTPPACKET) == PF_RTPPACKET) {
    // Request a full intra frame.
    VideoChannel* video_channel = static_cast<VideoChannel*>(channel);
    if (enable_send) {
      video_channel->SendIntraFrame();
    }
    if (enable_recv) {
      video_channel->RequestIntraFrame();
    }
  }

  return true;
}

void MediaRecorder::FlushSinks() {
  talk_base::CritScope cs(&critical_section_);
  std::map<BaseChannel*, SinkPair*>::iterator itr;
  for (itr = sinks_.begin(); itr != sinks_.end(); ++itr) {
    itr->second->send_sink->Flush();
    itr->second->recv_sink->Flush();
  }
}

}  // namespace cricket
