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

#ifndef TALK_MEDIA_BASE_RTPDUMP_H_
#define TALK_MEDIA_BASE_RTPDUMP_H_

#include <cstring>
#include <string>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/bytebuffer.h"
#include "talk/base/stream.h"

namespace cricket {

// We use the RTP dump file format compatible to the format used by rtptools
// (http://www.cs.columbia.edu/irt/software/rtptools/) and Wireshark
// (http://wiki.wireshark.org/rtpdump). In particular, the file starts with the
// first line "#!rtpplay1.0 address/port\n", followed by a 16 byte file header.
// For each packet, the file contains a 8 byte dump packet header, followed by
// the actual RTP or RTCP packet.

enum RtpDumpPacketFilter {
  PF_NONE = 0x0,
  PF_RTPHEADER = 0x1,
  PF_RTPPACKET = 0x3,  // includes header
  // PF_RTCPHEADER = 0x4,  // TODO(juberti)
  PF_RTCPPACKET = 0xC,  // includes header
  PF_ALL = 0xF
};

struct RtpDumpFileHeader {
  RtpDumpFileHeader(uint32 start_ms, uint32 s, uint16 p);
  void WriteToByteBuffer(talk_base::ByteBuffer* buf);

  static const char kFirstLine[];
  static const size_t kHeaderLength = 16;
  uint32 start_sec;   // start of recording, the seconds part.
  uint32 start_usec;  // start of recording, the microseconds part.
  uint32 source;      // network source (multicast address).
  uint16 port;        // UDP port.
  uint16 padding;     // 2 bytes padding.
};

struct RtpDumpPacket {
  RtpDumpPacket() {}

  RtpDumpPacket(const void* d, size_t s, uint32 elapsed, bool rtcp)
      : elapsed_time(elapsed),
        original_data_len((rtcp) ? 0 : s) {
    data.resize(s);
    memcpy(&data[0], d, s);
  }

  // In the rtpdump file format, RTCP packets have their data len set to zero,
  // since RTCP has an internal length field.
  bool is_rtcp() const { return original_data_len == 0; }
  bool IsValidRtpPacket() const;
  bool IsValidRtcpPacket() const;
  // Get the payload type, sequence number, timestampe, and SSRC of the RTP
  // packet. Return true and set the output parameter if successful.
  bool GetRtpPayloadType(int* pt) const;
  bool GetRtpSeqNum(int* seq_num) const;
  bool GetRtpTimestamp(uint32* ts) const;
  bool GetRtpSsrc(uint32* ssrc) const;
  bool GetRtpHeaderLen(size_t* len) const;
  // Get the type of the RTCP packet. Return true and set the output parameter
  // if successful.
  bool GetRtcpType(int* type) const;

  static const size_t kHeaderLength = 8;
  uint32 elapsed_time;       // Milliseconds since the start of recording.
  std::vector<uint8> data;   // The actual RTP or RTCP packet.
  size_t original_data_len;  // The original length of the packet; may be
                             // greater than data.size() if only part of the
                             // packet was recorded.
};

class RtpDumpReader {
 public:
  explicit RtpDumpReader(talk_base::StreamInterface* stream)
      : stream_(stream),
        file_header_read_(false),
        first_line_and_file_header_len_(0),
        start_time_ms_(0),
        ssrc_override_(0) {
  }
  virtual ~RtpDumpReader() {}

  // Use the specified ssrc, rather than the ssrc from dump, for RTP packets.
  void SetSsrc(uint32 ssrc);
  virtual talk_base::StreamResult ReadPacket(RtpDumpPacket* packet);

 protected:
  talk_base::StreamResult ReadFileHeader();
  bool RewindToFirstDumpPacket() {
    return stream_->SetPosition(first_line_and_file_header_len_);
  }

 private:
  // Check if its matches "#!rtpplay1.0 address/port\n".
  bool CheckFirstLine(const std::string& first_line);

  talk_base::StreamInterface* stream_;
  bool file_header_read_;
  size_t first_line_and_file_header_len_;
  uint32 start_time_ms_;
  uint32 ssrc_override_;

  DISALLOW_COPY_AND_ASSIGN(RtpDumpReader);
};

// RtpDumpLoopReader reads RTP dump packets from the input stream and rewinds
// the stream when it ends. RtpDumpLoopReader maintains the elapsed time, the
// RTP sequence number and the RTP timestamp properly. RtpDumpLoopReader can
// handle both RTP dump and RTCP dump. We assume that the dump does not mix
// RTP packets and RTCP packets.
class RtpDumpLoopReader : public RtpDumpReader {
 public:
  explicit RtpDumpLoopReader(talk_base::StreamInterface* stream);
  virtual talk_base::StreamResult ReadPacket(RtpDumpPacket* packet);

 private:
  // During the first loop, update the statistics, including packet count, frame
  // count, timestamps, and sequence number, of the input stream.
  void UpdateStreamStatistics(const RtpDumpPacket& packet);

  // At the end of first loop, calculate elapsed_time_increases_,
  // rtp_seq_num_increase_, and rtp_timestamp_increase_.
  void CalculateIncreases();

  // During the second and later loops, update the elapsed time of the dump
  // packet. If the dumped packet is a RTP packet, update its RTP sequence
  // number and timestamp as well.
  void UpdateDumpPacket(RtpDumpPacket* packet);

  int loop_count_;
  // How much to increase the elapsed time, RTP sequence number, RTP timestampe
  // for each loop. They are calcualted with the variables below during the
  // first loop.
  uint32 elapsed_time_increases_;
  int rtp_seq_num_increase_;
  uint32 rtp_timestamp_increase_;
  // How many RTP packets and how many payload frames in the input stream. RTP
  // packets belong to the same frame have the same RTP timestamp, different
  // dump timestamp, and different RTP sequence number.
  uint32 packet_count_;
  uint32 frame_count_;
  // The elapsed time, RTP sequence number, and RTP timestamp of the first and
  // the previous dump packets in the input stream.
  uint32 first_elapsed_time_;
  int first_rtp_seq_num_;
  uint32 first_rtp_timestamp_;
  uint32 prev_elapsed_time_;
  int prev_rtp_seq_num_;
  uint32 prev_rtp_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(RtpDumpLoopReader);
};

class RtpDumpWriter {
 public:
  explicit RtpDumpWriter(talk_base::StreamInterface* stream);

  // Filter to control what packets we actually record.
  void set_packet_filter(int filter);
  // Write a RTP or RTCP packet. The parameters data points to the packet and
  // data_len is its length.
  talk_base::StreamResult WriteRtpPacket(const void* data, size_t data_len) {
    return WritePacket(data, data_len, GetElapsedTime(), false);
  }
  talk_base::StreamResult WriteRtcpPacket(const void* data, size_t data_len) {
    return WritePacket(data, data_len, GetElapsedTime(), true);
  }
  talk_base::StreamResult WritePacket(const RtpDumpPacket& packet) {
    return WritePacket(&packet.data[0], packet.data.size(), packet.elapsed_time,
                       packet.is_rtcp());
  }
  uint32 GetElapsedTime() const;

  bool GetDumpSize(size_t* size) {
    // Note that we use GetPosition(), rather than GetSize(), to avoid flush the
    // stream per write.
    return stream_ && size && stream_->GetPosition(size);
  }

 protected:
  talk_base::StreamResult WriteFileHeader();

 private:
  talk_base::StreamResult WritePacket(const void* data, size_t data_len,
                                      uint32 elapsed, bool rtcp);
  size_t FilterPacket(const void* data, size_t data_len, bool rtcp);
  talk_base::StreamResult WriteToStream(const void* data, size_t data_len);

  talk_base::StreamInterface* stream_;
  int packet_filter_;
  bool file_header_written_;
  uint32 start_time_ms_;  // Time when the record starts.
  // If writing to the stream takes longer than this many ms, log a warning.
  uint32 warn_slow_writes_delay_;
  DISALLOW_COPY_AND_ASSIGN(RtpDumpWriter);
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_RTPDUMP_H_
