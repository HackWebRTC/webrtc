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

#ifndef TALK_MEDIA_BASE_RTPDATAENGINE_H_
#define TALK_MEDIA_BASE_RTPDATAENGINE_H_

#include <string>
#include <vector>

#include "talk/base/timing.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/mediaengine.h"

namespace cricket {

struct DataCodec;

class RtpDataEngine : public DataEngineInterface {
 public:
  RtpDataEngine();

  virtual DataMediaChannel* CreateChannel(DataChannelType data_channel_type);

  virtual const std::vector<DataCodec>& data_codecs() {
    return data_codecs_;
  }

  // Mostly for testing with a fake clock.  Ownership is passed in.
  void SetTiming(talk_base::Timing* timing) {
    timing_.reset(timing);
  }

 private:
  std::vector<DataCodec> data_codecs_;
  talk_base::scoped_ptr<talk_base::Timing> timing_;
};

// Keep track of sequence number and timestamp of an RTP stream.  The
// sequence number starts with a "random" value and increments.  The
// timestamp starts with a "random" value and increases monotonically
// according to the clockrate.
class RtpClock {
 public:
  RtpClock(int clockrate, uint16 first_seq_num, uint32 timestamp_offset)
      : clockrate_(clockrate),
        last_seq_num_(first_seq_num),
        timestamp_offset_(timestamp_offset) {
  }

  // Given the current time (in number of seconds which must be
  // monotonically increasing), Return the next sequence number and
  // timestamp.
  void Tick(double now, int* seq_num, uint32* timestamp);

 private:
  int clockrate_;
  uint16 last_seq_num_;
  uint32 timestamp_offset_;
};

class RtpDataMediaChannel : public DataMediaChannel {
 public:
  // Timing* Used for the RtpClock
  explicit RtpDataMediaChannel(talk_base::Timing* timing);
  // Sets Timing == NULL, so you'll need to call set_timer() before
  // using it.  This is needed by FakeMediaEngine.
  RtpDataMediaChannel();
  virtual ~RtpDataMediaChannel();

  void set_timing(talk_base::Timing* timing) {
    timing_ = timing;
  }

  virtual bool SetSendBandwidth(bool autobw, int bps);
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) { return true; }
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) { return true; }
  virtual bool SetSendCodecs(const std::vector<DataCodec>& codecs);
  virtual bool SetRecvCodecs(const std::vector<DataCodec>& codecs);
  virtual bool AddSendStream(const StreamParams& sp);
  virtual bool RemoveSendStream(uint32 ssrc);
  virtual bool AddRecvStream(const StreamParams& sp);
  virtual bool RemoveRecvStream(uint32 ssrc);
  virtual bool SetSend(bool send) {
    sending_ = send;
    return true;
  }
  virtual bool SetReceive(bool receive) {
    receiving_ = receive;
    return true;
  }
  virtual void OnPacketReceived(talk_base::Buffer* packet,
                                const talk_base::PacketTime& packet_time);
  virtual void OnRtcpReceived(talk_base::Buffer* packet,
                              const talk_base::PacketTime& packet_time) {}
  virtual void OnReadyToSend(bool ready) {}
  virtual bool SendData(
    const SendDataParams& params,
    const talk_base::Buffer& payload,
    SendDataResult* result);

 private:
  void Construct(talk_base::Timing* timing);

  bool sending_;
  bool receiving_;
  talk_base::Timing* timing_;
  std::vector<DataCodec> send_codecs_;
  std::vector<DataCodec> recv_codecs_;
  std::vector<StreamParams> send_streams_;
  std::vector<StreamParams> recv_streams_;
  std::map<uint32, RtpClock*> rtp_clock_by_send_ssrc_;
  talk_base::scoped_ptr<talk_base::RateLimiter> send_limiter_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_RTPDATAENGINE_H_
