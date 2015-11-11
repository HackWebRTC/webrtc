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
#include <algorithm>

#include "talk/media/base/executablehelpers.h"
#include "talk/media/base/rtpdump.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videoframe.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/testutils.h"

namespace cricket {

/////////////////////////////////////////////////////////////////////////
// Implementation of RawRtpPacket
/////////////////////////////////////////////////////////////////////////
void RawRtpPacket::WriteToByteBuffer(uint32_t in_ssrc,
                                     rtc::ByteBuffer* buf) const {
  if (!buf) return;

  buf->WriteUInt8(ver_to_cc);
  buf->WriteUInt8(m_to_pt);
  buf->WriteUInt16(sequence_number);
  buf->WriteUInt32(timestamp);
  buf->WriteUInt32(in_ssrc);
  buf->WriteBytes(payload, sizeof(payload));
}

bool RawRtpPacket::ReadFromByteBuffer(rtc::ByteBuffer* buf) {
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

bool RawRtpPacket::SameExceptSeqNumTimestampSsrc(const RawRtpPacket& packet,
                                                 uint16_t seq,
                                                 uint32_t ts,
                                                 uint32_t ssc) const {
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
void RawRtcpPacket::WriteToByteBuffer(rtc::ByteBuffer *buf) const {
  if (!buf) return;

  buf->WriteUInt8(ver_to_count);
  buf->WriteUInt8(type);
  buf->WriteUInt16(length);
  buf->WriteBytes(payload, sizeof(payload));
}

bool RawRtcpPacket::ReadFromByteBuffer(rtc::ByteBuffer* buf) {
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
  return std::min(arraysize(kTestRawRtpPackets),
                  arraysize(kTestRawRtcpPackets));
}

bool RtpTestUtility::WriteTestPackets(size_t count,
                                      bool rtcp,
                                      uint32_t rtp_ssrc,
                                      RtpDumpWriter* writer) {
  if (!writer || count > GetTestPacketCount()) return false;

  bool result = true;
  uint32_t elapsed_time_ms = 0;
  for (size_t i = 0; i < count && result; ++i) {
    rtc::ByteBuffer buf;
    if (rtcp) {
      kTestRawRtcpPackets[i].WriteToByteBuffer(&buf);
    } else {
      kTestRawRtpPackets[i].WriteToByteBuffer(rtp_ssrc, &buf);
    }

    RtpDumpPacket dump_packet(buf.Data(), buf.Length(), elapsed_time_ms, rtcp);
    elapsed_time_ms += kElapsedTimeInterval;
    result &= (rtc::SR_SUCCESS == writer->WritePacket(dump_packet));
  }
  return result;
}

bool RtpTestUtility::VerifyTestPacketsFromStream(size_t count,
                                                 rtc::StreamInterface* stream,
                                                 uint32_t ssrc) {
  if (!stream) return false;

  uint32_t prev_elapsed_time = 0;
  bool result = true;
  stream->Rewind();
  RtpDumpLoopReader reader(stream);
  for (size_t i = 0; i < count && result; ++i) {
    // Which loop and which index in the loop are we reading now.
    size_t loop = i / GetTestPacketCount();
    size_t index = i % GetTestPacketCount();

    RtpDumpPacket packet;
    result &= (rtc::SR_SUCCESS == reader.ReadPacket(&packet));
    // Check the elapsed time of the dump packet.
    result &= (packet.elapsed_time >= prev_elapsed_time);
    prev_elapsed_time = packet.elapsed_time;

    // Check the RTP or RTCP packet.
    rtc::ByteBuffer buf(reinterpret_cast<const char*>(&packet.data[0]),
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
          static_cast<uint16_t>(kTestRawRtpPackets[index].sequence_number +
                                loop * GetTestPacketCount()),
          static_cast<uint32_t>(kTestRawRtpPackets[index].timestamp +
                                loop * kRtpTimestampIncrease),
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

  rtc::ByteBuffer buf;
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
#ifdef ENABLE_WEBRTC
  rtc::Pathname path = rtc::GetExecutablePath();
  EXPECT_FALSE(path.empty());
  path.AppendPathname("../../talk/");
#else
  rtc::Pathname path = testing::GetTalkDirectory();
  EXPECT_FALSE(path.empty());  // must be run from inside "talk"
#endif
  path.AppendFolder("media/testdata/");
  path.SetFilename(filename);
  return path.pathname();
}

// Loads the image with the specified prefix and size into |out|.
bool LoadPlanarYuvTestImage(const std::string& prefix,
                            int width,
                            int height,
                            uint8_t* out) {
  std::stringstream ss;
  ss << prefix << "." << width << "x" << height << "_P420.yuv";

  rtc::scoped_ptr<rtc::FileStream> stream(
      rtc::Filesystem::OpenFile(rtc::Pathname(
          GetTestFilePath(ss.str())), "rb"));
  if (!stream) {
    return false;
  }

  rtc::StreamResult res =
      stream->ReadAll(out, I420_SIZE(width, height), NULL, NULL);
  return (res == rtc::SR_SUCCESS);
}

// Dumps the YUV image out to a file, for visual inspection.
// PYUV tool can be used to view dump files.
void DumpPlanarYuvTestImage(const std::string& prefix,
                            const uint8_t* img,
                            int w,
                            int h) {
  rtc::FileStream fs;
  char filename[256];
  rtc::sprintfn(filename, sizeof(filename), "%s.%dx%d_P420.yuv",
                      prefix.c_str(), w, h);
  fs.Open(filename, "wb", NULL);
  fs.Write(img, I420_SIZE(w, h), NULL, NULL);
}

// Dumps the ARGB image out to a file, for visual inspection.
// ffplay tool can be used to view dump files.
void DumpPlanarArgbTestImage(const std::string& prefix,
                             const uint8_t* img,
                             int w,
                             int h) {
  rtc::FileStream fs;
  char filename[256];
  rtc::sprintfn(filename, sizeof(filename), "%s.%dx%d_ARGB.raw",
                      prefix.c_str(), w, h);
  fs.Open(filename, "wb", NULL);
  fs.Write(img, ARGB_SIZE(w, h), NULL, NULL);
}

bool VideoFrameEqual(const VideoFrame* frame0, const VideoFrame* frame1) {
  const uint8_t* y0 = frame0->GetYPlane();
  const uint8_t* u0 = frame0->GetUPlane();
  const uint8_t* v0 = frame0->GetVPlane();
  const uint8_t* y1 = frame1->GetYPlane();
  const uint8_t* u1 = frame1->GetUPlane();
  const uint8_t* v1 = frame1->GetVPlane();

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

cricket::StreamParams CreateSimStreamParams(
    const std::string& cname,
    const std::vector<uint32_t>& ssrcs) {
  cricket::StreamParams sp;
  cricket::SsrcGroup sg(cricket::kSimSsrcGroupSemantics, ssrcs);
  sp.ssrcs = ssrcs;
  sp.ssrc_groups.push_back(sg);
  sp.cname = cname;
  return sp;
}

// There should be an rtx_ssrc per ssrc.
cricket::StreamParams CreateSimWithRtxStreamParams(
    const std::string& cname,
    const std::vector<uint32_t>& ssrcs,
    const std::vector<uint32_t>& rtx_ssrcs) {
  cricket::StreamParams sp = CreateSimStreamParams(cname, ssrcs);
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    sp.ssrcs.push_back(rtx_ssrcs[i]);
    std::vector<uint32_t> fid_ssrcs;
    fid_ssrcs.push_back(ssrcs[i]);
    fid_ssrcs.push_back(rtx_ssrcs[i]);
    cricket::SsrcGroup fid_group(cricket::kFidSsrcGroupSemantics, fid_ssrcs);
    sp.ssrc_groups.push_back(fid_group);
  }
  return sp;
}

}  // namespace cricket
