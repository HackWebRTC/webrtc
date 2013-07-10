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

#include "talk/media/base/testutils.h"

#include <math.h>

#include "talk/base/bytebuffer.h"
#include "talk/base/fileutils.h"
#include "talk/base/gunit.h"
#include "talk/base/pathutils.h"
#include "talk/base/stream.h"
#include "talk/base/stringutils.h"
#include "talk/media/base/rtpdump.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videoframe.h"

namespace cricket {

/////////////////////////////////////////////////////////////////////////
// Implementation of RawRtpPacket
/////////////////////////////////////////////////////////////////////////
void RawRtpPacket::WriteToByteBuffer(
    uint32 in_ssrc, talk_base::ByteBuffer *buf) const {
  if (!buf) return;

  buf->WriteUInt8(ver_to_cc);
  buf->WriteUInt8(m_to_pt);
  buf->WriteUInt16(sequence_number);
  buf->WriteUInt32(timestamp);
  buf->WriteUInt32(in_ssrc);
  buf->WriteBytes(payload, sizeof(payload));
}

bool RawRtpPacket::ReadFromByteBuffer(talk_base::ByteBuffer* buf) {
  if (!buf) return false;

  bool ret = true;
  ret &= buf->ReadUInt8(&ver_to_cc);
  ret &= buf->ReadUInt8(&m_to_pt);
  ret &= buf->ReadUInt16(&sequence_number);
  ret &= buf->ReadUInt32(&timestamp);
  ret &= buf->ReadUInt32(&ssrc);
  ret &= buf->ReadBytes(payload, sizeof(payload));
  return ret;
}

bool RawRtpPacket::SameExceptSeqNumTimestampSsrc(
    const RawRtpPacket& packet, uint16 seq, uint32 ts, uint32 ssc) const {
  return sequence_number == seq &&
      timestamp == ts &&
      ver_to_cc == packet.ver_to_cc &&
      m_to_pt == packet.m_to_pt &&
      ssrc == ssc &&
      0 == memcmp(payload, packet.payload, sizeof(payload));
}

/////////////////////////////////////////////////////////////////////////
// Implementation of RawRtcpPacket
/////////////////////////////////////////////////////////////////////////
void RawRtcpPacket::WriteToByteBuffer(talk_base::ByteBuffer *buf) const {
  if (!buf) return;

  buf->WriteUInt8(ver_to_count);
  buf->WriteUInt8(type);
  buf->WriteUInt16(length);
  buf->WriteBytes(payload, sizeof(payload));
}

bool RawRtcpPacket::ReadFromByteBuffer(talk_base::ByteBuffer* buf) {
  if (!buf) return false;

  bool ret = true;
  ret &= buf->ReadUInt8(&ver_to_count);
  ret &= buf->ReadUInt8(&type);
  ret &= buf->ReadUInt16(&length);
  ret &= buf->ReadBytes(payload, sizeof(payload));
  return ret;
}

bool RawRtcpPacket::EqualsTo(const RawRtcpPacket& packet) const {
  return ver_to_count == packet.ver_to_count &&
      type == packet.type &&
      length == packet.length &&
      0 == memcmp(payload, packet.payload, sizeof(payload));
}

/////////////////////////////////////////////////////////////////////////
// Implementation of class RtpTestUtility
/////////////////////////////////////////////////////////////////////////
const RawRtpPacket RtpTestUtility::kTestRawRtpPackets[] = {
    {0x80, 0, 0, 0,  RtpTestUtility::kDefaultSsrc, "RTP frame 0"},
    {0x80, 0, 1, 30, RtpTestUtility::kDefaultSsrc, "RTP frame 1"},
    {0x80, 0, 2, 30, RtpTestUtility::kDefaultSsrc, "RTP frame 1"},
    {0x80, 0, 3, 60, RtpTestUtility::kDefaultSsrc, "RTP frame 2"}
};
const RawRtcpPacket RtpTestUtility::kTestRawRtcpPackets[] = {
    // The Version is 2, the Length is 2, and the payload has 8 bytes.
    {0x80, 0, 2, "RTCP0000"},
    {0x80, 0, 2, "RTCP0001"},
    {0x80, 0, 2, "RTCP0002"},
    {0x80, 0, 2, "RTCP0003"},
};

size_t RtpTestUtility::GetTestPacketCount() {
  return talk_base::_min(
      ARRAY_SIZE(kTestRawRtpPackets),
      ARRAY_SIZE(kTestRawRtcpPackets));
}

bool RtpTestUtility::WriteTestPackets(
    size_t count, bool rtcp, uint32 rtp_ssrc, RtpDumpWriter* writer) {
  if (!writer || count > GetTestPacketCount()) return false;

  bool result = true;
  uint32 elapsed_time_ms = 0;
  for (size_t i = 0; i < count && result; ++i) {
    talk_base::ByteBuffer buf;
    if (rtcp) {
      kTestRawRtcpPackets[i].WriteToByteBuffer(&buf);
    } else {
      kTestRawRtpPackets[i].WriteToByteBuffer(rtp_ssrc, &buf);
    }

    RtpDumpPacket dump_packet(buf.Data(), buf.Length(), elapsed_time_ms, rtcp);
    elapsed_time_ms += kElapsedTimeInterval;
    result &= (talk_base::SR_SUCCESS == writer->WritePacket(dump_packet));
  }
  return result;
}

bool RtpTestUtility::VerifyTestPacketsFromStream(
    size_t count, talk_base::StreamInterface* stream, uint32 ssrc) {
  if (!stream) return false;

  uint32 prev_elapsed_time = 0;
  bool result = true;
  stream->Rewind();
  RtpDumpLoopReader reader(stream);
  for (size_t i = 0; i < count && result; ++i) {
    // Which loop and which index in the loop are we reading now.
    size_t loop = i / GetTestPacketCount();
    size_t index = i % GetTestPacketCount();

    RtpDumpPacket packet;
    result &= (talk_base::SR_SUCCESS == reader.ReadPacket(&packet));
    // Check the elapsed time of the dump packet.
    result &= (packet.elapsed_time >= prev_elapsed_time);
    prev_elapsed_time = packet.elapsed_time;

    // Check the RTP or RTCP packet.
    talk_base::ByteBuffer buf(reinterpret_cast<const char*>(&packet.data[0]),
                              packet.data.size());
    if (packet.is_rtcp()) {
      // RTCP packet.
      RawRtcpPacket rtcp_packet;
      result &= rtcp_packet.ReadFromByteBuffer(&buf);
      result &= rtcp_packet.EqualsTo(kTestRawRtcpPackets[index]);
    } else {
      // RTP packet.
      RawRtpPacket rtp_packet;
      result &= rtp_packet.ReadFromByteBuffer(&buf);
      result &= rtp_packet.SameExceptSeqNumTimestampSsrc(
          kTestRawRtpPackets[index],
          kTestRawRtpPackets[index].sequence_number +
              loop * GetTestPacketCount(),
          kTestRawRtpPackets[index].timestamp + loop * kRtpTimestampIncrease,
          ssrc);
    }
  }

  stream->Rewind();
  return result;
}

bool RtpTestUtility::VerifyPacket(const RtpDumpPacket* dump,
                                  const RawRtpPacket* raw,
                                  bool header_only) {
  if (!dump || !raw) return false;

  talk_base::ByteBuffer buf;
  raw->WriteToByteBuffer(RtpTestUtility::kDefaultSsrc, &buf);

  if (header_only) {
    size_t header_len = 0;
    dump->GetRtpHeaderLen(&header_len);
    return header_len == dump->data.size() &&
        buf.Length() > dump->data.size() &&
        0 == memcmp(buf.Data(), &dump->data[0], dump->data.size());
  } else {
    return buf.Length() == dump->data.size() &&
        0 == memcmp(buf.Data(), &dump->data[0], dump->data.size());
  }
}

// Implementation of VideoCaptureListener.
VideoCapturerListener::VideoCapturerListener(VideoCapturer* capturer)
    : last_capture_state_(CS_STARTING),
      frame_count_(0),
      frame_fourcc_(0),
      frame_width_(0),
      frame_height_(0),
      frame_size_(0),
      resolution_changed_(false) {
  capturer->SignalStateChange.connect(this,
      &VideoCapturerListener::OnStateChange);
  capturer->SignalFrameCaptured.connect(this,
      &VideoCapturerListener::OnFrameCaptured);
}

void VideoCapturerListener::OnStateChange(VideoCapturer* capturer,
                                          CaptureState result) {
  last_capture_state_ = result;
}

void VideoCapturerListener::OnFrameCaptured(VideoCapturer* capturer,
                                            const CapturedFrame* frame) {
  ++frame_count_;
  if (1 == frame_count_) {
    frame_fourcc_ = frame->fourcc;
    frame_width_ = frame->width;
    frame_height_ = frame->height;
    frame_size_ = frame->data_size;
  } else if (frame_width_ != frame->width || frame_height_ != frame->height) {
    resolution_changed_ = true;
  }
}

// Returns the absolute path to a file in the testdata/ directory.
std::string GetTestFilePath(const std::string& filename) {
  // Locate test data directory.
  talk_base::Pathname path = GetTalkDirectory();
  EXPECT_FALSE(path.empty());  // must be run from inside "talk"
  path.AppendFolder("media");
  path.AppendFolder("testdata");
  path.SetFilename(filename);
  return path.pathname();
}

// Loads the image with the specified prefix and size into |out|.
bool LoadPlanarYuvTestImage(const std::string& prefix,
                            int width, int height, uint8* out) {
  std::stringstream ss;
  ss << prefix << "." << width << "x" << height << "_P420.yuv";

  talk_base::scoped_ptr<talk_base::FileStream> stream(
      talk_base::Filesystem::OpenFile(talk_base::Pathname(
          GetTestFilePath(ss.str())), "rb"));
  if (!stream) {
    return false;
  }

  talk_base::StreamResult res =
      stream->ReadAll(out, I420_SIZE(width, height), NULL, NULL);
  return (res == talk_base::SR_SUCCESS);
}

// Dumps the YUV image out to a file, for visual inspection.
// PYUV tool can be used to view dump files.
void DumpPlanarYuvTestImage(const std::string& prefix, const uint8* img,
                            int w, int h) {
  talk_base::FileStream fs;
  char filename[256];
  talk_base::sprintfn(filename, sizeof(filename), "%s.%dx%d_P420.yuv",
                      prefix.c_str(), w, h);
  fs.Open(filename, "wb", NULL);
  fs.Write(img, I420_SIZE(w, h), NULL, NULL);
}

// Dumps the ARGB image out to a file, for visual inspection.
// ffplay tool can be used to view dump files.
void DumpPlanarArgbTestImage(const std::string& prefix, const uint8* img,
                             int w, int h) {
  talk_base::FileStream fs;
  char filename[256];
  talk_base::sprintfn(filename, sizeof(filename), "%s.%dx%d_ARGB.raw",
                      prefix.c_str(), w, h);
  fs.Open(filename, "wb", NULL);
  fs.Write(img, ARGB_SIZE(w, h), NULL, NULL);
}

bool VideoFrameEqual(const VideoFrame* frame0, const VideoFrame* frame1) {
  const uint8* y0 = frame0->GetYPlane();
  const uint8* u0 = frame0->GetUPlane();
  const uint8* v0 = frame0->GetVPlane();
  const uint8* y1 = frame1->GetYPlane();
  const uint8* u1 = frame1->GetUPlane();
  const uint8* v1 = frame1->GetVPlane();

  for (size_t i = 0; i < frame0->GetHeight(); ++i) {
    if (0 != memcmp(y0, y1, frame0->GetWidth())) {
      return false;
    }
    y0 += frame0->GetYPitch();
    y1 += frame1->GetYPitch();
  }

  for (size_t i = 0; i < frame0->GetChromaHeight(); ++i) {
    if (0 != memcmp(u0, u1, frame0->GetChromaWidth())) {
      return false;
    }
    if (0 != memcmp(v0, v1, frame0->GetChromaWidth())) {
      return false;
    }
    u0 += frame0->GetUPitch();
    v0 += frame0->GetVPitch();
    u1 += frame1->GetUPitch();
    v1 += frame1->GetVPitch();
  }

  return true;
}

}  // namespace cricket
