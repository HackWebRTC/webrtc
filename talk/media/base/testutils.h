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

#ifndef TALK_MEDIA_BASE_TESTUTILS_H_
#define TALK_MEDIA_BASE_TESTUTILS_H_

#include <string>
#include <vector>

#if !defined(DISABLE_YUV)
#include "libyuv/compare.h"
#endif
#include "talk/base/basictypes.h"
#include "talk/base/sigslot.h"
#include "talk/base/window.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"

namespace talk_base {
class ByteBuffer;
class StreamInterface;
}

namespace cricket {

// Returns size of 420 image with rounding on chroma for odd sizes.
#define I420_SIZE(w, h) (w * h + (((w + 1) / 2) * ((h + 1) / 2)) * 2)
// Returns size of ARGB image.
#define ARGB_SIZE(w, h) (w * h * 4)

template <class T> inline std::vector<T> MakeVector(const T a[], size_t s) {
  return std::vector<T>(a, a + s);
}
#define MAKE_VECTOR(a) cricket::MakeVector(a, ARRAY_SIZE(a))

struct RtpDumpPacket;
class RtpDumpWriter;
class VideoFrame;

struct RawRtpPacket {
  void WriteToByteBuffer(uint32 in_ssrc, talk_base::ByteBuffer* buf) const;
  bool ReadFromByteBuffer(talk_base::ByteBuffer* buf);
  // Check if this packet is the same as the specified packet except the
  // sequence number and timestamp, which should be the same as the specified
  // parameters.
  bool SameExceptSeqNumTimestampSsrc(
      const RawRtpPacket& packet, uint16 seq, uint32 ts, uint32 ssc) const;
  int size() const { return 28; }

  uint8 ver_to_cc;
  uint8 m_to_pt;
  uint16 sequence_number;
  uint32 timestamp;
  uint32 ssrc;
  char payload[16];
};

struct RawRtcpPacket {
  void WriteToByteBuffer(talk_base::ByteBuffer* buf) const;
  bool ReadFromByteBuffer(talk_base::ByteBuffer* buf);
  bool EqualsTo(const RawRtcpPacket& packet) const;

  uint8 ver_to_count;
  uint8 type;
  uint16 length;
  char payload[16];
};

class RtpTestUtility {
 public:
  static size_t GetTestPacketCount();

  // Write the first count number of kTestRawRtcpPackets or kTestRawRtpPackets,
  // depending on the flag rtcp. If it is RTP, use the specified SSRC. Return
  // true if successful.
  static bool WriteTestPackets(
      size_t count, bool rtcp, uint32 rtp_ssrc, RtpDumpWriter* writer);

  // Loop read the first count number of packets from the specified stream.
  // Verify the elapsed time of the dump packets increase monotonically. If the
  // stream is a RTP stream, verify the RTP sequence number, timestamp, and
  // payload. If the stream is a RTCP stream, verify the RTCP header and
  // payload.
  static bool VerifyTestPacketsFromStream(
      size_t count, talk_base::StreamInterface* stream, uint32 ssrc);

  // Verify the dump packet is the same as the raw RTP packet.
  static bool VerifyPacket(const RtpDumpPacket* dump,
                           const RawRtpPacket* raw,
                           bool header_only);

  static const uint32 kDefaultSsrc = 1;
  static const uint32 kRtpTimestampIncrease = 90;
  static const uint32 kDefaultTimeIncrease = 30;
  static const uint32 kElapsedTimeInterval = 10;
  static const RawRtpPacket kTestRawRtpPackets[];
  static const RawRtcpPacket kTestRawRtcpPackets[];

 private:
  RtpTestUtility() {}
};

// Test helper for testing VideoCapturer implementations.
class VideoCapturerListener : public sigslot::has_slots<> {
 public:
  explicit VideoCapturerListener(VideoCapturer* cap);

  CaptureState last_capture_state() const { return last_capture_state_; }
  int frame_count() const { return frame_count_; }
  uint32 frame_fourcc() const { return frame_fourcc_; }
  int frame_width() const { return frame_width_; }
  int frame_height() const { return frame_height_; }
  uint32 frame_size() const { return frame_size_; }
  bool resolution_changed() const { return resolution_changed_; }

  void OnStateChange(VideoCapturer* capturer, CaptureState state);
  void OnFrameCaptured(VideoCapturer* capturer, const CapturedFrame* frame);

 private:
  CaptureState last_capture_state_;
  int frame_count_;
  uint32 frame_fourcc_;
  int frame_width_;
  int frame_height_;
  uint32 frame_size_;
  bool resolution_changed_;
};

class ScreencastEventCatcher : public sigslot::has_slots<> {
 public:
  ScreencastEventCatcher() : ssrc_(0), ev_(talk_base::WE_RESIZE) { }
  uint32 ssrc() const { return ssrc_; }
  talk_base::WindowEvent event() const { return ev_; }
  void OnEvent(uint32 ssrc, talk_base::WindowEvent ev) {
    ssrc_ = ssrc;
    ev_ = ev;
  }
 private:
  uint32 ssrc_;
  talk_base::WindowEvent ev_;
};

class VideoMediaErrorCatcher : public sigslot::has_slots<> {
 public:
  VideoMediaErrorCatcher() : ssrc_(0), error_(VideoMediaChannel::ERROR_NONE) { }
  uint32 ssrc() const { return ssrc_; }
  VideoMediaChannel::Error error() const { return error_; }
  void OnError(uint32 ssrc, VideoMediaChannel::Error error) {
    ssrc_ = ssrc;
    error_ = error;
  }
 private:
  uint32 ssrc_;
  VideoMediaChannel::Error error_;
};

// Returns the absolute path to a file in the testdata/ directory.
std::string GetTestFilePath(const std::string& filename);

// PSNR formula: psnr = 10 * log10 (Peak Signal^2 / mse)
// sse is set to a small number for identical frames or sse == 0
static inline double ComputePSNR(double sse, double count) {
#if !defined(DISABLE_YUV)
  return libyuv::SumSquareErrorToPsnr(static_cast<uint64>(sse),
                                      static_cast<uint64>(count));
#else
  if (sse <= 0.)
    sse = 65025.0 * count / pow(10., 128./10.);  // produces max PSNR of 128
  return 10.0 * log10(65025.0 * count / sse);
#endif
}

static inline double ComputeSumSquareError(const uint8 *org, const uint8 *rec,
                                           int size) {
#if !defined(DISABLE_YUV)
  return static_cast<double>(libyuv::ComputeSumSquareError(org, rec, size));
#else
  double sse = 0.;
  for (int j = 0; j < size; ++j) {
    const int diff = static_cast<int>(org[j]) - static_cast<int>(rec[j]);
    sse += static_cast<double>(diff * diff);
  }
  return sse;
#endif
}

// Loads the image with the specified prefix and size into |out|.
bool LoadPlanarYuvTestImage(const std::string& prefix,
                            int width, int height, uint8* out);

// Dumps the YUV image out to a file, for visual inspection.
// PYUV tool can be used to view dump files.
void DumpPlanarYuvTestImage(const std::string& prefix, const uint8* img,
                            int w, int h);

// Dumps the ARGB image out to a file, for visual inspection.
// ffplay tool can be used to view dump files.
void DumpPlanarArgbTestImage(const std::string& prefix, const uint8* img,
                             int w, int h);

// Compare two I420 frames.
bool VideoFrameEqual(const VideoFrame* frame0, const VideoFrame* frame1);

// Checks whether |codecs| contains |codec|; checks using Codec::Matches().
template <class C>
bool ContainsMatchingCodec(const std::vector<C>& codecs, const C& codec) {
  typename std::vector<C>::const_iterator it;
  for (it = codecs.begin(); it != codecs.end(); ++it) {
    if (it->Matches(codec)) {
      return true;
    }
  }
  return false;
}
}  // namespace cricket

#endif  // TALK_MEDIA_BASE_TESTUTILS_H_
