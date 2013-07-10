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

#include "talk/media/base/rtpdump.h"

#include <ctype.h>

#include <string>

#include "talk/base/byteorder.h"
#include "talk/base/logging.h"
#include "talk/base/timeutils.h"
#include "talk/media/base/rtputils.h"

namespace {
static const int kRtpSsrcOffset = 8;
const int  kWarnSlowWritesDelayMs = 50;
}  // namespace

namespace cricket {

const char RtpDumpFileHeader::kFirstLine[] = "#!rtpplay1.0 0.0.0.0/0\n";

RtpDumpFileHeader::RtpDumpFileHeader(uint32 start_ms, uint32 s, uint16 p)
    : start_sec(start_ms / 1000),
      start_usec(start_ms % 1000 * 1000),
      source(s),
      port(p),
      padding(0) {
}

void RtpDumpFileHeader::WriteToByteBuffer(talk_base::ByteBuffer* buf) {
  buf->WriteUInt32(start_sec);
  buf->WriteUInt32(start_usec);
  buf->WriteUInt32(source);
  buf->WriteUInt16(port);
  buf->WriteUInt16(padding);
}

static const uint32 kDefaultTimeIncrease = 30;

bool RtpDumpPacket::IsValidRtpPacket() const {
  return original_data_len >= data.size() &&
      data.size() >= kMinRtpPacketLen;
}

bool RtpDumpPacket::IsValidRtcpPacket() const {
  return original_data_len == 0 &&
      data.size() >= kMinRtcpPacketLen;
}

bool RtpDumpPacket::GetRtpPayloadType(int* pt) const {
  return IsValidRtpPacket() &&
      cricket::GetRtpPayloadType(&data[0], data.size(), pt);
}

bool RtpDumpPacket::GetRtpSeqNum(int* seq_num) const {
  return IsValidRtpPacket() &&
      cricket::GetRtpSeqNum(&data[0], data.size(), seq_num);
}

bool RtpDumpPacket::GetRtpTimestamp(uint32* ts) const {
  return IsValidRtpPacket() &&
      cricket::GetRtpTimestamp(&data[0], data.size(), ts);
}

bool RtpDumpPacket::GetRtpSsrc(uint32* ssrc) const {
  return IsValidRtpPacket() &&
      cricket::GetRtpSsrc(&data[0], data.size(), ssrc);
}

bool RtpDumpPacket::GetRtpHeaderLen(size_t* len) const {
  return IsValidRtpPacket() &&
      cricket::GetRtpHeaderLen(&data[0], data.size(), len);
}

bool RtpDumpPacket::GetRtcpType(int* type) const {
  return IsValidRtcpPacket() &&
      cricket::GetRtcpType(&data[0], data.size(), type);
}

///////////////////////////////////////////////////////////////////////////
// Implementation of RtpDumpReader.
///////////////////////////////////////////////////////////////////////////

void RtpDumpReader::SetSsrc(uint32 ssrc) {
  ssrc_override_ = ssrc;
}

talk_base::StreamResult RtpDumpReader::ReadPacket(RtpDumpPacket* packet) {
  if (!packet) return talk_base::SR_ERROR;

  talk_base::StreamResult res = talk_base::SR_SUCCESS;
  // Read the file header if it has not been read yet.
  if (!file_header_read_) {
    res = ReadFileHeader();
    if (res != talk_base::SR_SUCCESS) {
      return res;
    }
    file_header_read_ = true;
  }

  // Read the RTP dump packet header.
  char header[RtpDumpPacket::kHeaderLength];
  res = stream_->ReadAll(header, sizeof(header), NULL, NULL);
  if (res != talk_base::SR_SUCCESS) {
    return res;
  }
  talk_base::ByteBuffer buf(header, sizeof(header));
  uint16 dump_packet_len;
  uint16 data_len;
  // Read the full length of the rtpdump packet, including the rtpdump header.
  buf.ReadUInt16(&dump_packet_len);
  packet->data.resize(dump_packet_len - sizeof(header));
  // Read the size of the original packet, which may be larger than the size in
  // the rtpdump file, in the event that only part of the packet (perhaps just
  // the header) was recorded. Note that this field is set to zero for RTCP
  // packets, which have their own internal length field.
  buf.ReadUInt16(&data_len);
  packet->original_data_len = data_len;
  // Read the elapsed time for this packet (different than RTP timestamp).
  buf.ReadUInt32(&packet->elapsed_time);

  // Read the actual RTP or RTCP packet.
  res = stream_->ReadAll(&packet->data[0], packet->data.size(), NULL, NULL);

  // If the packet is RTP and we have specified a ssrc, replace the RTP ssrc
  // with the specified ssrc.
  if (res == talk_base::SR_SUCCESS &&
      packet->IsValidRtpPacket() &&
      ssrc_override_ != 0) {
    talk_base::SetBE32(&packet->data[kRtpSsrcOffset], ssrc_override_);
  }

  return res;
}

talk_base::StreamResult RtpDumpReader::ReadFileHeader() {
  // Read the first line.
  std::string first_line;
  talk_base::StreamResult res = stream_->ReadLine(&first_line);
  if (res != talk_base::SR_SUCCESS) {
    return res;
  }
  if (!CheckFirstLine(first_line)) {
    return talk_base::SR_ERROR;
  }

  // Read the 16 byte file header.
  char header[RtpDumpFileHeader::kHeaderLength];
  res = stream_->ReadAll(header, sizeof(header), NULL, NULL);
  if (res == talk_base::SR_SUCCESS) {
    talk_base::ByteBuffer buf(header, sizeof(header));
    uint32 start_sec;
    uint32 start_usec;
    buf.ReadUInt32(&start_sec);
    buf.ReadUInt32(&start_usec);
    start_time_ms_ = start_sec * 1000 + start_usec / 1000;
    // Increase the length by 1 since first_line does not contain the ending \n.
    first_line_and_file_header_len_ = first_line.size() + 1 + sizeof(header);
  }
  return res;
}

bool RtpDumpReader::CheckFirstLine(const std::string& first_line) {
  // The first line is like "#!rtpplay1.0 address/port"
  bool matched = (0 == first_line.find("#!rtpplay1.0 "));

  // The address could be IP or hostname. We do not check it here. Instead, we
  // check the port at the end.
  size_t pos = first_line.find('/');
  matched &= (pos != std::string::npos && pos < first_line.size() - 1);
  for (++pos; pos < first_line.size() && matched; ++pos) {
    matched &= (0 != isdigit(first_line[pos]));
  }

  return matched;
}

///////////////////////////////////////////////////////////////////////////
// Implementation of RtpDumpLoopReader.
///////////////////////////////////////////////////////////////////////////
RtpDumpLoopReader::RtpDumpLoopReader(talk_base::StreamInterface* stream)
    : RtpDumpReader(stream),
      loop_count_(0),
      elapsed_time_increases_(0),
      rtp_seq_num_increase_(0),
      rtp_timestamp_increase_(0),
      packet_count_(0),
      frame_count_(0),
      first_elapsed_time_(0),
      first_rtp_seq_num_(0),
      first_rtp_timestamp_(0),
      prev_elapsed_time_(0),
      prev_rtp_seq_num_(0),
      prev_rtp_timestamp_(0) {
}

talk_base::StreamResult RtpDumpLoopReader::ReadPacket(RtpDumpPacket* packet) {
  if (!packet) return talk_base::SR_ERROR;

  talk_base::StreamResult res = RtpDumpReader::ReadPacket(packet);
  if (talk_base::SR_SUCCESS == res) {
    if (0 == loop_count_) {
      // During the first loop, we update the statistics of the input stream.
      UpdateStreamStatistics(*packet);
    }
  } else if (talk_base::SR_EOS == res) {
    if (0 == loop_count_) {
      // At the end of the first loop, calculate elapsed_time_increases_,
      // rtp_seq_num_increase_, and rtp_timestamp_increase_, which will be
      // used during the second and later loops.
      CalculateIncreases();
    }

    // Rewind the input stream to the first dump packet and read again.
    ++loop_count_;
    if (RewindToFirstDumpPacket()) {
      res = RtpDumpReader::ReadPacket(packet);
    }
  }

  if (talk_base::SR_SUCCESS == res && loop_count_ > 0) {
    // During the second and later loops, we update the elapsed time of the dump
    // packet. If the dumped packet is a RTP packet, we also update its RTP
    // sequence number and timestamp.
    UpdateDumpPacket(packet);
  }

  return res;
}

void RtpDumpLoopReader::UpdateStreamStatistics(const RtpDumpPacket& packet) {
  // Get the RTP sequence number and timestamp of the dump packet.
  int rtp_seq_num = 0;
  packet.GetRtpSeqNum(&rtp_seq_num);
  uint32 rtp_timestamp = 0;
  packet.GetRtpTimestamp(&rtp_timestamp);

  // Set the timestamps and sequence number for the first dump packet.
  if (0 == packet_count_++) {
    first_elapsed_time_ = packet.elapsed_time;
    first_rtp_seq_num_ = rtp_seq_num;
    first_rtp_timestamp_ = rtp_timestamp;
    // The first packet belongs to a new payload frame.
    ++frame_count_;
  } else if (rtp_timestamp != prev_rtp_timestamp_) {
    // The current and previous packets belong to different payload frames.
    ++frame_count_;
  }

  prev_elapsed_time_ = packet.elapsed_time;
  prev_rtp_timestamp_ = rtp_timestamp;
  prev_rtp_seq_num_ = rtp_seq_num;
}

void RtpDumpLoopReader::CalculateIncreases() {
  // At this time, prev_elapsed_time_, prev_rtp_seq_num_, and
  // prev_rtp_timestamp_ are values of the last dump packet in the input stream.
  rtp_seq_num_increase_ = prev_rtp_seq_num_ - first_rtp_seq_num_ + 1;
  // If we have only one packet or frame, we use the default timestamp
  // increase. Otherwise, we use the difference between the first and the last
  // packets or frames.
  elapsed_time_increases_ = packet_count_ <= 1 ? kDefaultTimeIncrease :
      (prev_elapsed_time_ - first_elapsed_time_) * packet_count_ /
      (packet_count_ - 1);
  rtp_timestamp_increase_ = frame_count_ <= 1 ? kDefaultTimeIncrease :
      (prev_rtp_timestamp_ - first_rtp_timestamp_) * frame_count_ /
      (frame_count_ - 1);
}

void RtpDumpLoopReader::UpdateDumpPacket(RtpDumpPacket* packet) {
  // Increase the elapsed time of the dump packet.
  packet->elapsed_time += loop_count_ * elapsed_time_increases_;

  if (packet->IsValidRtpPacket()) {
    // Get the old RTP sequence number and timestamp.
    int sequence = 0;
    packet->GetRtpSeqNum(&sequence);
    uint32 timestamp = 0;
    packet->GetRtpTimestamp(&timestamp);
    // Increase the RTP sequence number and timestamp.
    sequence += loop_count_ * rtp_seq_num_increase_;
    timestamp += loop_count_ * rtp_timestamp_increase_;
    // Write the updated sequence number and timestamp back to the RTP packet.
    talk_base::ByteBuffer buffer;
    buffer.WriteUInt16(sequence);
    buffer.WriteUInt32(timestamp);
    memcpy(&packet->data[2], buffer.Data(), buffer.Length());
  }
}

///////////////////////////////////////////////////////////////////////////
// Implementation of RtpDumpWriter.
///////////////////////////////////////////////////////////////////////////

RtpDumpWriter::RtpDumpWriter(talk_base::StreamInterface* stream)
    : stream_(stream),
      packet_filter_(PF_ALL),
      file_header_written_(false),
      start_time_ms_(talk_base::Time()),
      warn_slow_writes_delay_(kWarnSlowWritesDelayMs) {
}

void RtpDumpWriter::set_packet_filter(int filter) {
  packet_filter_ = filter;
  LOG(LS_INFO) << "RtpDumpWriter set_packet_filter to " << packet_filter_;
}

uint32 RtpDumpWriter::GetElapsedTime() const {
  return talk_base::TimeSince(start_time_ms_);
}

talk_base::StreamResult RtpDumpWriter::WriteFileHeader() {
  talk_base::StreamResult res = WriteToStream(
      RtpDumpFileHeader::kFirstLine,
      strlen(RtpDumpFileHeader::kFirstLine));
  if (res != talk_base::SR_SUCCESS) {
    return res;
  }

  talk_base::ByteBuffer buf;
  RtpDumpFileHeader file_header(talk_base::Time(), 0, 0);
  file_header.WriteToByteBuffer(&buf);
  return WriteToStream(buf.Data(), buf.Length());
}

talk_base::StreamResult RtpDumpWriter::WritePacket(
    const void* data, size_t data_len, uint32 elapsed, bool rtcp) {
  if (!stream_ || !data || 0 == data_len) return talk_base::SR_ERROR;

  talk_base::StreamResult res = talk_base::SR_SUCCESS;
  // Write the file header if it has not been written yet.
  if (!file_header_written_) {
    res = WriteFileHeader();
    if (res != talk_base::SR_SUCCESS) {
      return res;
    }
    file_header_written_ = true;
  }

  // Figure out what to write.
  size_t write_len = FilterPacket(data, data_len, rtcp);
  if (write_len == 0) {
    return talk_base::SR_SUCCESS;
  }

  // Write the dump packet header.
  talk_base::ByteBuffer buf;
  buf.WriteUInt16(static_cast<uint16>(
                      RtpDumpPacket::kHeaderLength + write_len));
  buf.WriteUInt16(static_cast<uint16>(rtcp ? 0 : data_len));
  buf.WriteUInt32(elapsed);
  res = WriteToStream(buf.Data(), buf.Length());
  if (res != talk_base::SR_SUCCESS) {
    return res;
  }

  // Write the header or full packet as indicated by write_len.
  return WriteToStream(data, write_len);
}

size_t RtpDumpWriter::FilterPacket(const void* data, size_t data_len,
                                   bool rtcp) {
  size_t filtered_len = 0;
  if (!rtcp) {
    if ((packet_filter_ & PF_RTPPACKET) == PF_RTPPACKET) {
      // RTP header + payload
      filtered_len = data_len;
    } else if ((packet_filter_ & PF_RTPHEADER) == PF_RTPHEADER) {
      // RTP header only
      size_t header_len;
      if (GetRtpHeaderLen(data, data_len, &header_len)) {
        filtered_len = header_len;
      }
    }
  } else {
    if ((packet_filter_ & PF_RTCPPACKET) == PF_RTCPPACKET) {
      // RTCP header + payload
      filtered_len = data_len;
    }
  }

  return filtered_len;
}

talk_base::StreamResult RtpDumpWriter::WriteToStream(
    const void* data, size_t data_len) {
  uint32 before = talk_base::Time();
  talk_base::StreamResult result =
      stream_->WriteAll(data, data_len, NULL, NULL);
  uint32 delay = talk_base::TimeSince(before);
  if (delay >= warn_slow_writes_delay_) {
    LOG(LS_WARNING) << "Slow RtpDump: took " << delay << "ms to write "
                    << data_len << " bytes.";
  }
  return result;
}

}  // namespace cricket
