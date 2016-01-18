/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_H_

#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace rtcp {

static const int kCommonFbFmtLength = 12;

class RawPacket;

// Class for building RTCP packets.
//
//  Example:
//  ReportBlock report_block;
//  report_block.To(234)
//  report_block.FractionLost(10);
//
//  ReceiverReport rr;
//  rr.From(123);
//  rr.WithReportBlock(&report_block)
//
//  Fir fir;
//  fir.From(123);
//  fir.To(234)
//  fir.WithCommandSeqNum(123);
//
//  size_t length = 0;                     // Builds an intra frame request
//  uint8_t packet[kPacketSize];           // with sequence number 123.
//  fir.Build(packet, &length, kPacketSize);
//
//  RawPacket packet = fir.Build();        // Returns a RawPacket holding
//                                         // the built rtcp packet.
//
//  rr.Append(&fir)                        // Builds a compound RTCP packet with
//  RawPacket packet = rr.Build();         // a receiver report, report block
//                                         // and fir message.

class RtcpPacket {
 public:
  virtual ~RtcpPacket() {}

  void Append(RtcpPacket* packet);

  // Callback used to signal that an RTCP packet is ready. Note that this may
  // not contain all data in this RtcpPacket; if a packet cannot fit in
  // max_length bytes, it will be fragmented and multiple calls to this
  // callback will be made.
  class PacketReadyCallback {
   public:
    PacketReadyCallback() {}
    virtual ~PacketReadyCallback() {}

    virtual void OnPacketReady(uint8_t* data, size_t length) = 0;
  };

  // Convenience method mostly used for test. Max length of IP_PACKET_SIZE is
  // used, will cause assertion error if fragmentation occurs.
  rtc::scoped_ptr<RawPacket> Build() const;

  // Returns true if all calls to Create succeeded. A buffer of size
  // IP_PACKET_SIZE will be allocated and reused between calls to callback.
  bool Build(PacketReadyCallback* callback) const;

  // Returns true if all calls to Create succeeded. Provided buffer reference
  // will be used for all calls to callback.
  bool BuildExternalBuffer(uint8_t* buffer,
                           size_t max_length,
                           PacketReadyCallback* callback) const;

  // Size of this packet in bytes (including headers, excluding nested packets).
  virtual size_t BlockLength() const = 0;

 protected:
  RtcpPacket() {}

  virtual bool Create(uint8_t* packet,
                      size_t* index,
                      size_t max_length,
                      PacketReadyCallback* callback) const = 0;

  static void CreateHeader(uint8_t count_or_format,
                           uint8_t packet_type,
                           size_t block_length,  // Size in 32bit words - 1.
                           uint8_t* buffer,
                           size_t* pos);

  bool OnBufferFull(uint8_t* packet,
                    size_t* index,
                    RtcpPacket::PacketReadyCallback* callback) const;

  size_t HeaderLength() const;

  static const size_t kHeaderLength = 4;
  std::vector<RtcpPacket*> appended_packets_;

 private:
  bool CreateAndAddAppended(uint8_t* packet,
                            size_t* index,
                            size_t max_length,
                            PacketReadyCallback* callback) const;
};

// Class holding a RTCP packet.
//
// Takes a built rtcp packet.
//  RawPacket raw_packet(buffer, length);
//
// To access the raw packet:
//  raw_packet.Buffer();         - pointer to the raw packet
//  raw_packet.BufferLength();   - the length of the raw packet

class RawPacket {
 public:
  explicit RawPacket(size_t buffer_length);
  RawPacket(const uint8_t* packet, size_t packet_length);

  const uint8_t* Buffer() const;
  uint8_t* MutableBuffer();
  size_t BufferLength() const;
  size_t Length() const;
  void SetLength(size_t length);

 private:
  const size_t buffer_length_;
  size_t length_;
  rtc::scoped_ptr<uint8_t[]> buffer_;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_H_
