/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_BITBUFFER_H_
#define WEBRTC_BASE_BITBUFFER_H_

#include "webrtc/base/common.h"

namespace rtc {

// A class, similar to ByteBuffer, that can parse bit-sized data out of a set of
// bytes. Has a similar API to the read-only parts of ByteBuffer, plus methods
// for reading bit-sized data and processing exponential golomb encoded data.
// Sizes/counts specify bits/bytes, for clarity.
// Byte order is assumed big-endian/network.
class BitBuffer {
 public:
  BitBuffer(const uint8* bytes, size_t byte_count);

  // The remaining bits in the byte buffer.
  uint64 RemainingBitCount() const;

  // Reads byte-sized values from the buffer. Returns false if there isn't
  // enough data left for the specified type.
  bool ReadUInt8(uint8* val);
  bool ReadUInt16(uint16* val);
  bool ReadUInt32(uint32* val);

  // Reads bit-sized values from the buffer. Returns false if there isn't enough
  // data left for the specified type.
  bool ReadBits(uint32* val, size_t bit_count);

  // Peeks bit-sized values from the buffer. Returns false if there isn't enough
  // data left for the specified type. Doesn't move the current read offset.
  bool PeekBits(uint32* val, size_t bit_count);

  // Reads the exponential golomb encoded value at the current bit offset.
  // Exponential golomb values are encoded as:
  // 1) x = source val + 1
  // 2) In binary, write [countbits(x) - 1] 0s, then x
  // To decode, we count the number of leading 0 bits, read that many + 1 bits,
  // and increment the result by 1.
  // Returns false if there isn't enough data left for the specified type, or if
  // the value wouldn't fit in a uint32.
  bool ReadExponentialGolomb(uint32* val);

  // Moves current position |byte_count| bytes forward. Returns false if
  // there aren't enough bytes left in the buffer.
  bool ConsumeBytes(size_t byte_count);
  // Moves current position |bit_count| bits forward. Returns false if
  // there aren't enough bits left in the buffer.
  bool ConsumeBits(size_t bit_count);

 private:
  const uint8* const bytes_;
  // The total size of |bytes_|.
  size_t byte_count_;
  // The current offset, in bytes, from the start of |bytes_|.
  size_t byte_offset_;
  // The current offset, in bits, into the current byte.
  size_t bit_offset_;

  DISALLOW_COPY_AND_ASSIGN(BitBuffer);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_BITBUFFER_H_
