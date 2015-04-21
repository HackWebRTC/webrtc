/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/bitbuffer.h"

#include <limits>

#include "webrtc/base/checks.h"

namespace {

// Returns the lowest (right-most) |bit_count| bits in |byte|.
uint8 LowestBits(uint8 byte, size_t bit_count) {
  DCHECK_LE(bit_count, 8u);
  uint8 mask_shift = 8 - static_cast<uint8>(bit_count);
  return byte & (0xFF >> mask_shift);
}

// Returns the highest (left-most) |bit_count| bits in |byte|, shifted to the
// lowest bits (to the right).
uint8 HighestBits(uint8 byte, size_t bit_count) {
  DCHECK_LE(bit_count, 8u);
  uint8 shift = 8 - static_cast<uint8>(bit_count);
  uint8 mask = 0xFF << shift;
  return (byte & mask) >> shift;
}

}  // namespace

namespace rtc {

BitBuffer::BitBuffer(const uint8* bytes, size_t byte_count)
    : bytes_(bytes), byte_count_(byte_count), byte_offset_(), bit_offset_() {
  DCHECK(static_cast<uint64>(byte_count_) <=
         std::numeric_limits<uint32>::max());
}

uint64 BitBuffer::RemainingBitCount() const {
  return (static_cast<uint64>(byte_count_) - byte_offset_) * 8 - bit_offset_;
}

bool BitBuffer::ReadUInt8(uint8* val) {
  uint32 bit_val;
  if (!ReadBits(&bit_val, sizeof(uint8) * 8)) {
    return false;
  }
  DCHECK(bit_val <= std::numeric_limits<uint8>::max());
  *val = static_cast<uint8>(bit_val);
  return true;
}

bool BitBuffer::ReadUInt16(uint16* val) {
  uint32 bit_val;
  if (!ReadBits(&bit_val, sizeof(uint16) * 8)) {
    return false;
  }
  DCHECK(bit_val <= std::numeric_limits<uint16>::max());
  *val = static_cast<uint16>(bit_val);
  return true;
}

bool BitBuffer::ReadUInt32(uint32* val) {
  return ReadBits(val, sizeof(uint32) * 8);
}

bool BitBuffer::PeekBits(uint32* val, size_t bit_count) {
  if (!val || bit_count > RemainingBitCount() || bit_count > 32) {
    return false;
  }
  const uint8* bytes = bytes_ + byte_offset_;
  size_t remaining_bits_in_current_byte = 8 - bit_offset_;
  uint32 bits = LowestBits(*bytes++, remaining_bits_in_current_byte);
  // If we're reading fewer bits than what's left in the current byte, just
  // return the portion of this byte that we need.
  if (bit_count < remaining_bits_in_current_byte) {
    *val = HighestBits(bits, bit_offset_ + bit_count);
    return true;
  }
  // Otherwise, subtract what we've read from the bit count and read as many
  // full bytes as we can into bits.
  bit_count -= remaining_bits_in_current_byte;
  while (bit_count >= 8) {
    bits = (bits << 8) | *bytes++;
    bit_count -= 8;
  }
  // Whatever we have left is smaller than a byte, so grab just the bits we need
  // and shift them into the lowest bits.
  if (bit_count > 0) {
    bits <<= bit_count;
    bits |= HighestBits(*bytes, bit_count);
  }
  *val = bits;
  return true;
}

bool BitBuffer::ReadBits(uint32* val, size_t bit_count) {
  return PeekBits(val, bit_count) && ConsumeBits(bit_count);
}

bool BitBuffer::ConsumeBytes(size_t byte_count) {
  return ConsumeBits(byte_count * 8);
}

bool BitBuffer::ConsumeBits(size_t bit_count) {
  if (bit_count > RemainingBitCount()) {
    return false;
  }

  byte_offset_ += (bit_offset_ + bit_count) / 8;
  bit_offset_ = (bit_offset_ + bit_count) % 8;
  return true;
}

bool BitBuffer::ReadExponentialGolomb(uint32* val) {
  if (!val) {
    return false;
  }
  // Store off the current byte/bit offset, in case we want to restore them due
  // to a failed parse.
  size_t original_byte_offset = byte_offset_;
  size_t original_bit_offset = bit_offset_;

  // Count the number of leading 0 bits by peeking/consuming them one at a time.
  size_t zero_bit_count = 0;
  uint32 peeked_bit;
  while (PeekBits(&peeked_bit, 1) && peeked_bit == 0) {
    zero_bit_count++;
    ConsumeBits(1);
  }

  // We should either be at the end of the stream, or the next bit should be 1.
  DCHECK(!PeekBits(&peeked_bit, 1) || peeked_bit == 1);

  // The bit count of the value is the number of zeros + 1. Make sure that many
  // bits fits in a uint32 and that we have enough bits left for it, and then
  // read the value.
  size_t value_bit_count = zero_bit_count + 1;
  if (value_bit_count > 32 || !ReadBits(val, value_bit_count)) {
    byte_offset_ = original_byte_offset;
    bit_offset_ = original_bit_offset;
    return false;
  }
  *val -= 1;
  return true;
}

}  // namespace rtc
