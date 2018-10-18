/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/delta_encoding.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/bitbuffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace {

// TODO(eladalon): Only build the decoder in tools and unit tests.

size_t BitsToBytes(size_t bits) {
  return (bits / 8) + (bits % 8 > 0 ? 1 : 0);
}

// TODO(eladalon): Replace by something more efficient.
uint64_t BitWidth(uint64_t input) {
  uint64_t width = 0;
  do {  // input == 0 -> width == 1
    width += 1;
    input >>= 1;
  } while (input != 0);
  return width;
}

// Return the maximum integer of a given bit width.
// Examples:
// MaxValueOfBitWidth(1) = 0x01
// MaxValueOfBitWidth(6) = 0x3f
// MaxValueOfBitWidth(8) = 0xff
// MaxValueOfBitWidth(32) = 0xffffffff
uint64_t MaxValueOfBitWidth(uint64_t bit_width) {
  RTC_DCHECK_GE(bit_width, 1);
  RTC_DCHECK_LE(bit_width, 64);
  return (bit_width == 64) ? std::numeric_limits<uint64_t>::max()
                           : ((static_cast<uint64_t>(1) << bit_width) - 1);
}

// Computes the delta between |previous| and |current|, under the assumption
// that wrap-around occurs after width |bit_width| is exceeded.
uint64_t ComputeDelta(uint64_t previous, uint64_t current, uint64_t width) {
  RTC_DCHECK(width == 64 || current < (static_cast<uint64_t>(1) << width));
  RTC_DCHECK(width == 64 || previous < (static_cast<uint64_t>(1) << width));

  if (current >= previous) {
    // Simply "walk" forward.
    return current - previous;
  } else {  // previous > current
    // "Walk" until the max value, one more step to 0, then to |current|.
    return (MaxValueOfBitWidth(width) - previous) + 1 + current;
  }
}

// Determines the encoding type (e.g. fixed-size encoding).
// Given an encoding type, may also distinguish between some variants of it
// (e.g. which fields of the fixed-size encoding are explicitly mentioned by
// the header, and which are implicitly assumed to hold certain default values).
enum class EncodingType {
  kFixedSizeUnsignedDeltasNoEarlyWrapNoOpt = 0,
  kFixedSizeSignedDeltasEarlyWrapAndOptSupported = 1,
  kReserved1 = 2,
  kReserved2 = 3,
  kNumberOfEncodingTypes  // Keep last
};

// The width of each field in the encoding header. Note that this is the
// width in case the field exists; not all fields occur in all encoding types.
constexpr size_t kBitsInHeaderForEncodingType = 2;
constexpr size_t kBitsInHeaderForDeltaWidthBits = 6;
constexpr size_t kBitsInHeaderForSignedDeltas = 1;
constexpr size_t kBitsInHeaderForValuesOptional = 1;
constexpr size_t kBitsInHeaderForOriginalWidthBits = 6;

static_assert(static_cast<size_t>(EncodingType::kNumberOfEncodingTypes) <=
                  1 << kBitsInHeaderForEncodingType,
              "Not all encoding types fit.");

// Default values for when the encoding header does not specify explicitly.
constexpr bool kDefaultSignedDeltas = false;
constexpr bool kDefaultValuesOptional = false;
constexpr uint64_t kDefaultOriginalWidthBits = 64;

// Wrap BitBufferWriter and extend its functionality by (1) keeping track of
// the number of bits written and (2) owning its buffer.
class BitWriter final {
 public:
  explicit BitWriter(size_t byte_count)
      : buffer_(byte_count, '\0'),
        bit_writer_(reinterpret_cast<uint8_t*>(&buffer_[0]), buffer_.size()),
        written_bits_(0),
        valid_(true) {
    RTC_DCHECK_GT(byte_count, 0);
  }

  void WriteBits(uint64_t val, size_t bit_count) {
    RTC_DCHECK(valid_);
    const bool success = bit_writer_.WriteBits(val, bit_count);
    RTC_DCHECK(success);
    written_bits_ += bit_count;
  }

  // Returns everything that was written so far.
  // Nothing more may be written after this is called.
  std::string GetString() {
    RTC_DCHECK(valid_);
    valid_ = false;

    buffer_.resize(BitsToBytes(written_bits_));
    written_bits_ = 0;

    std::string result;
    std::swap(buffer_, result);
    return result;
  }

 private:
  std::string buffer_;
  rtc::BitBufferWriter bit_writer_;
  // Note: Counting bits instead of bytes wraps around earlier than it has to,
  // which means the maximum length is lower than it could be. We don't expect
  // to go anywhere near the limit, though, so this is good enough.
  size_t written_bits_;
  bool valid_;

  RTC_DISALLOW_COPY_AND_ASSIGN(BitWriter);
};

// Parameters for fixed-size delta-encoding/decoding.
// These are tailored for the sequence which will be encoded (e.g. widths).
struct FixedLengthEncodingParameters final {
  FixedLengthEncodingParameters(uint64_t delta_width_bits,
                                bool signed_deltas,
                                bool values_optional,
                                uint64_t original_width_bits)
      : delta_width_bits(delta_width_bits),
        signed_deltas(signed_deltas),
        values_optional(values_optional),
        original_width_bits(original_width_bits) {}

  // Number of bits necessary to hold the widest(*) of the deltas between the
  // values in the sequence.
  // (*) - Widest might not be the largest, if signed deltas are used.
  uint64_t delta_width_bits;

  // Whether deltas are signed.
  // TODO(eladalon): Add support for signed deltas.
  bool signed_deltas;

  // Whether the values of the sequence are optional. That is, it may be
  // that some of them do not have a value (not even a sentinel value indicating
  // invalidity).
  // TODO(eladalon): Add support for optional elements.
  bool values_optional;

  // Number of bits necessary to hold the largest value in the sequence.
  uint64_t original_width_bits;
};

// Performs delta-encoding of a single (non-empty) sequence of values, using
// an encoding where all deltas are encoded using the same number of bits.
// (With the exception of optional elements; those are encoded as a bit vector
// with one bit per element, plus a fixed number of bits for every element that
// has a value.)
class FixedLengthDeltaEncoder final {
 public:
  // See webrtc::EncodeDeltas() for general details.
  // This function return a bit pattern that would allow the decoder to
  // determine whether it was produced by FixedLengthDeltaEncoder, and can
  // therefore be decoded by FixedLengthDeltaDecoder, or whether it was produced
  // by a different encoder.
  static std::string EncodeDeltas(uint64_t base,
                                  const std::vector<uint64_t>& values);

 private:
  // FixedLengthDeltaEncoder objects are to be created by EncodeDeltas() and
  // released by it before it returns. They're mostly a convenient way to
  // avoid having to pass a lot of state between different functions.
  // Therefore, it was deemed acceptable to let them have a reference to
  // |values|, whose lifetime must exceed the lifetime of |this|.
  FixedLengthDeltaEncoder(const FixedLengthEncodingParameters& params,
                          uint64_t base,
                          const std::vector<uint64_t>& values);

  // Perform delta-encoding using the parameters given to the ctor on the
  // sequence of values given to the ctor.
  std::string Encode();

  // Exact lengths.
  size_t OutputLengthBytes() const;
  size_t HeaderLengthBits() const;
  size_t EncodedDeltasLengthBits() const;

  // Encode the compression parameters into the stream.
  void EncodeHeader();

  // Encode a given delta into the stream.
  void EncodeDelta(uint64_t previous, uint64_t current);

  // The parameters according to which encoding will be done (width of
  // fields, whether signed deltas should be used, etc.)
  const FixedLengthEncodingParameters params_;

  // The encoding scheme assumes that at least one value is transmitted OOB,
  // so that the first value can be encoded as a delta from that OOB value,
  // which is |base_|.
  const uint64_t base_;

  // The values to be encoded.
  // Note: This is a non-owning reference. See comment above ctor for details.
  const std::vector<uint64_t>& values_;

  // Buffer into which encoded values will be written.
  // This is created dynmically as a way to enforce that the rest of the
  // ctor has finished running when this is constructed, so that the lower
  // bound on the buffer size would be guaranteed correct.
  std::unique_ptr<BitWriter> writer_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FixedLengthDeltaEncoder);
};

// TODO(eladalon): Reduce the number of passes.
std::string FixedLengthDeltaEncoder::EncodeDeltas(
    uint64_t base,
    const std::vector<uint64_t>& values) {
  RTC_DCHECK(!values.empty());

  bool non_decreasing = base <= values[0];
  uint64_t max_value_including_base = std::max(base, values[0]);
  for (size_t i = 1; i < values.size(); ++i) {
    non_decreasing &= (values[i - 1] <= values[i]);
    max_value_including_base = std::max(max_value_including_base, values[i]);
  }

  // If the sequence is non-decreasing, it may be assumed to have width = 64;
  // there's no reason to encode the actual max width in the encoding header.
  const uint64_t original_width_bits =
      non_decreasing ? 64 : BitWidth(max_value_including_base);

  uint64_t max_unsigned_delta =
      ComputeDelta(base, values[0], original_width_bits);
  for (size_t i = 1; i < values.size(); ++i) {
    const uint64_t unsigned_delta =
        ComputeDelta(values[i - 1], values[i], original_width_bits);
    max_unsigned_delta = std::max(unsigned_delta, max_unsigned_delta);
  }

  // We indicate the special case of all values being equal to the base with
  // the empty string.
  if (max_unsigned_delta == 0) {
    RTC_DCHECK(std::all_of(values.cbegin(), values.cend(),
                           [base](uint64_t val) { return val == base; }));
    return std::string();
  }

  const uint64_t delta_width_bits_unsigned = BitWidth(max_unsigned_delta);
  // This will prevent the use of signed deltas, by always assuming
  // they will not provide value over unsigned.
  // TODO(eladalon): Add support for signed deltas.
  const uint64_t delta_width_bits_signed = 64;

  // Note: Preference for unsigned if the two have the same width (efficiency).
  const bool signed_deltas =
      delta_width_bits_signed < delta_width_bits_unsigned;
  const uint64_t delta_width_bits =
      signed_deltas ? delta_width_bits_signed : delta_width_bits_unsigned;

  const bool values_optional = false;

  FixedLengthEncodingParameters params(delta_width_bits, signed_deltas,
                                       values_optional, original_width_bits);
  FixedLengthDeltaEncoder encoder(params, base, values);
  return encoder.Encode();
}

FixedLengthDeltaEncoder::FixedLengthDeltaEncoder(
    const FixedLengthEncodingParameters& params,
    uint64_t base,
    const std::vector<uint64_t>& values)
    : params_(params), base_(base), values_(values) {
  RTC_DCHECK_GE(params_.delta_width_bits, 1);
  RTC_DCHECK_LE(params_.delta_width_bits, 64);
  RTC_DCHECK_GE(params_.original_width_bits, 1);
  RTC_DCHECK_LE(params_.original_width_bits, 64);
  RTC_DCHECK_LE(params_.delta_width_bits, params_.original_width_bits);
  RTC_DCHECK(!values_.empty());
  writer_ = absl::make_unique<BitWriter>(OutputLengthBytes());
}

std::string FixedLengthDeltaEncoder::Encode() {
  EncodeHeader();

  uint64_t previous = base_;
  for (uint64_t value : values_) {
    EncodeDelta(previous, value);
    previous = value;
  }

  return writer_->GetString();
}

size_t FixedLengthDeltaEncoder::OutputLengthBytes() const {
  const size_t length_bits = HeaderLengthBits() + EncodedDeltasLengthBits();
  return BitsToBytes(length_bits);
}

size_t FixedLengthDeltaEncoder::HeaderLengthBits() const {
  if (params_.signed_deltas == kDefaultSignedDeltas &&
      params_.values_optional == kDefaultValuesOptional &&
      params_.original_width_bits == kDefaultOriginalWidthBits) {
    return kBitsInHeaderForEncodingType + kBitsInHeaderForDeltaWidthBits;
  } else {
    return kBitsInHeaderForEncodingType + kBitsInHeaderForDeltaWidthBits +
           kBitsInHeaderForSignedDeltas + kBitsInHeaderForValuesOptional +
           kBitsInHeaderForOriginalWidthBits;
  }
}

size_t FixedLengthDeltaEncoder::EncodedDeltasLengthBits() const {
  // TODO(eladalon): When optional values are supported, iterate over the
  // deltas to check the exact cost of each.
  RTC_DCHECK(!params_.values_optional);
  return values_.size() * params_.delta_width_bits;
}

void FixedLengthDeltaEncoder::EncodeHeader() {
  RTC_DCHECK(writer_);

  const EncodingType encoding_type =
      (params_.original_width_bits == kDefaultOriginalWidthBits &&
       params_.signed_deltas == kDefaultSignedDeltas &&
       params_.values_optional == kDefaultValuesOptional)
          ? EncodingType::kFixedSizeUnsignedDeltasNoEarlyWrapNoOpt
          : EncodingType::kFixedSizeSignedDeltasEarlyWrapAndOptSupported;

  writer_->WriteBits(static_cast<uint64_t>(encoding_type),
                     kBitsInHeaderForEncodingType);

  // Note: Since it's meaningless for a field to be of width 0, when it comes
  // to fields that relate widths, we encode  width 1 as 0, width 2 as 1,

  writer_->WriteBits(params_.delta_width_bits - 1,
                     kBitsInHeaderForDeltaWidthBits);

  if (encoding_type == EncodingType::kFixedSizeUnsignedDeltasNoEarlyWrapNoOpt) {
    return;
  }

  writer_->WriteBits(static_cast<uint64_t>(params_.signed_deltas),
                     kBitsInHeaderForSignedDeltas);
  writer_->WriteBits(static_cast<uint64_t>(params_.values_optional),
                     kBitsInHeaderForValuesOptional);
  writer_->WriteBits(params_.original_width_bits - 1,
                     kBitsInHeaderForOriginalWidthBits);
}

void FixedLengthDeltaEncoder::EncodeDelta(uint64_t previous, uint64_t current) {
  RTC_DCHECK(writer_);
  writer_->WriteBits(
      ComputeDelta(previous, current, params_.original_width_bits),
      params_.delta_width_bits);
}

// Perform decoding of a a delta-encoded stream, extracting the original
// sequence of values.
class FixedLengthDeltaDecoder final {
 public:
  // Checks whether FixedLengthDeltaDecoder is a suitable decoder for this
  // bitstream. Note that this does NOT imply that stream is valid, and will
  // be decoded successfully. It DOES imply that all other decoder classes
  // will fail to decode this input, though.
  static bool IsSuitableDecoderFor(const std::string& input);

  // Assuming that |input| is the result of fixed-size delta-encoding
  // that took place with the same value to |base| and over |num_of_deltas|
  // original values, this will return the sequence of original values.
  // If an error occurs (can happen if |input| is corrupt), an empty
  // vector will be returned.
  static std::vector<uint64_t> DecodeDeltas(const std::string& input,
                                            uint64_t base,
                                            size_t num_of_deltas);

 private:
  // Reads the encoding header in |input| and returns a FixedLengthDeltaDecoder
  // with the corresponding configuration, that can be used to decode the
  // values in |input|.
  // If the encoding header is corrupt (contains an illegal configuration),
  // nullptr will be returned.
  // When a valid FixedLengthDeltaDecoder is returned, this does not mean that
  // the entire stream is free of error. Rather, only the encoding header is
  // examined and guaranteed.
  static std::unique_ptr<FixedLengthDeltaDecoder>
  Create(const std::string& input, uint64_t base, size_t num_of_deltas);

  // FixedLengthDeltaDecoder objects are to be created by DecodeDeltas() and
  // released by it before it returns. They're mostly a convenient way to
  // avoid having to pass a lot of state between different functions.
  // Therefore, it was deemed acceptable that |reader| does not own the buffer
  // it reads, meaning the lifetime of |this| must not exceed the lifetime
  // of |reader|'s underlying buffer.
  FixedLengthDeltaDecoder(std::unique_ptr<rtc::BitBuffer> reader,
                          const FixedLengthEncodingParameters& params,
                          uint64_t base,
                          size_t num_of_deltas);

  // Perform the decoding using the parameters given to the ctor.
  std::vector<uint64_t> Decode();

  // Attempt to parse a delta from the input reader.
  // Returns true/false for success/failure.
  // Writes the delta into |delta| if successful.
  bool ParseDelta(uint64_t* delta);

  // Add |delta| to |base| to produce the next value in a sequence.
  // The delta is applied as signed/unsigned depending on the parameters
  // given to the ctor. Wrap-around is taken into account according to the
  // values' width, as specified by the aforementioned encoding parameters.
  uint64_t ApplyDelta(uint64_t base, uint64_t delta) const;

  // Reader of the input stream to be decoded. Does not own that buffer.
  // See comment above ctor for details.
  const std::unique_ptr<rtc::BitBuffer> reader_;

  // The parameters according to which encoding will be done (width of
  // fields, whether signed deltas should be used, etc.)
  const FixedLengthEncodingParameters params_;

  // The encoding scheme assumes that at least one value is transmitted OOB,
  // so that the first value can be encoded as a delta from that OOB value,
  // which is |base_|.
  const uint64_t base_;

  // The number of values to be known to be decoded.
  const size_t num_of_deltas_;

  // Bit mask corresponding to |params_.original_width_bits|. That is, the bits
  // necessary for encoding all of the values in the encoded sequence are on.
  // Used as an optimization.
  const uint64_t value_mask_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FixedLengthDeltaDecoder);
};

bool FixedLengthDeltaDecoder::IsSuitableDecoderFor(const std::string& input) {
  if (input.length() < kBitsInHeaderForEncodingType) {
    return false;
  }

  rtc::BitBuffer reader(reinterpret_cast<const uint8_t*>(&input[0]),
                        kBitsInHeaderForEncodingType);

  uint32_t encoding_type_bits;
  const bool result =
      reader.ReadBits(&encoding_type_bits, kBitsInHeaderForEncodingType);
  RTC_DCHECK(result);

  const auto encoding_type = static_cast<EncodingType>(encoding_type_bits);
  return encoding_type ==
             EncodingType::kFixedSizeUnsignedDeltasNoEarlyWrapNoOpt ||
         encoding_type ==
             EncodingType::kFixedSizeSignedDeltasEarlyWrapAndOptSupported;
}

std::vector<uint64_t> FixedLengthDeltaDecoder::DecodeDeltas(
    const std::string& input,
    uint64_t base,
    size_t num_of_deltas) {
  auto decoder = FixedLengthDeltaDecoder::Create(input, base, num_of_deltas);
  if (!decoder) {
    return std::vector<uint64_t>();
  }

  return decoder->Decode();
}

std::unique_ptr<FixedLengthDeltaDecoder> FixedLengthDeltaDecoder::Create(
    const std::string& input,
    uint64_t base,
    size_t num_of_deltas) {
  if (input.length() < kBitsInHeaderForEncodingType) {
    return nullptr;
  }

  auto reader = absl::make_unique<rtc::BitBuffer>(
      reinterpret_cast<const uint8_t*>(&input[0]), input.length());

  // Encoding type
  uint32_t encoding_type_bits;
  const bool result =
      reader->ReadBits(&encoding_type_bits, kBitsInHeaderForEncodingType);
  RTC_DCHECK(result);
  const EncodingType encoding = static_cast<EncodingType>(encoding_type_bits);
  if (encoding != EncodingType::kFixedSizeUnsignedDeltasNoEarlyWrapNoOpt &&
      encoding !=
          EncodingType::kFixedSizeSignedDeltasEarlyWrapAndOptSupported) {
    RTC_LOG(LS_WARNING) << "Unrecognized encoding type.";
    return nullptr;
  }

  uint32_t read_buffer;

  // delta_width_bits
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForDeltaWidthBits)) {
    return nullptr;
  }
  RTC_DCHECK_LE(read_buffer, 64 - 1);  // See encoding for -1's rationale.
  const uint64_t delta_width_bits =
      read_buffer + 1;  // See encoding for +1's rationale.

  // signed_deltas, values_optional, original_width_bits
  bool signed_deltas;
  bool values_optional;
  uint64_t original_width_bits;
  if (encoding == EncodingType::kFixedSizeUnsignedDeltasNoEarlyWrapNoOpt) {
    signed_deltas = kDefaultSignedDeltas;
    values_optional = kDefaultValuesOptional;
    original_width_bits = kDefaultOriginalWidthBits;
  } else {
    // signed_deltas
    if (!reader->ReadBits(&read_buffer, kBitsInHeaderForSignedDeltas)) {
      return nullptr;
    }
    signed_deltas = rtc::dchecked_cast<bool>(read_buffer);
    if (signed_deltas) {
      RTC_LOG(LS_WARNING) << "Not implemented.";
      return nullptr;
    }

    // values_optional
    if (!reader->ReadBits(&read_buffer, kBitsInHeaderForValuesOptional)) {
      return nullptr;
    }
    RTC_DCHECK_LE(read_buffer, 1);
    values_optional = rtc::dchecked_cast<bool>(read_buffer);
    if (values_optional) {
      RTC_LOG(LS_WARNING) << "Not implemented.";
      return nullptr;
    }

    // original_width_bits
    if (!reader->ReadBits(&read_buffer, kBitsInHeaderForOriginalWidthBits)) {
      return nullptr;
    }
    RTC_DCHECK_LE(read_buffer, 64 - 1);     // See encoding for -1's rationale.
    original_width_bits = read_buffer + 1;  // See encoding for +1's rationale.
  }

  FixedLengthEncodingParameters params(delta_width_bits, signed_deltas,
                                       values_optional, original_width_bits);
  return absl::WrapUnique(new FixedLengthDeltaDecoder(std::move(reader), params,
                                                      base, num_of_deltas));
}

FixedLengthDeltaDecoder::FixedLengthDeltaDecoder(
    std::unique_ptr<rtc::BitBuffer> reader,
    const FixedLengthEncodingParameters& params,
    uint64_t base,
    size_t num_of_deltas)
    : reader_(std::move(reader)),
      params_(params),
      base_(base),
      num_of_deltas_(num_of_deltas),
      value_mask_(MaxValueOfBitWidth(params_.original_width_bits)) {
  RTC_DCHECK(reader_);
  // TODO(eladalon): Support signed deltas.
  RTC_DCHECK(!params.signed_deltas) << "Not implemented.";
  // TODO(eladalon): Support optional values.
  RTC_DCHECK(!params.values_optional) << "Not implemented.";
}

std::vector<uint64_t> FixedLengthDeltaDecoder::Decode() {
  std::vector<uint64_t> values(num_of_deltas_);

  uint64_t previous = base_;
  for (size_t i = 0; i < num_of_deltas_; ++i) {
    uint64_t delta;
    if (!ParseDelta(&delta)) {
      return std::vector<uint64_t>();
    }
    values[i] = ApplyDelta(previous, delta);
    previous = values[i];
  }

  return values;
}

bool FixedLengthDeltaDecoder::ParseDelta(uint64_t* delta) {
  RTC_DCHECK(reader_);
  RTC_DCHECK(!params_.signed_deltas) << "Not implemented.";    // Reminder.
  RTC_DCHECK(!params_.values_optional) << "Not implemented.";  // Reminder.

  // BitBuffer and BitBufferWriter read/write higher bits before lower bits.

  const size_t lower_bit_count =
      std::min<uint64_t>(params_.delta_width_bits, 32u);
  const size_t higher_bit_count =
      (params_.delta_width_bits <= 32u) ? 0 : params_.delta_width_bits - 32u;

  uint32_t lower_bits;
  uint32_t higher_bits;

  if (higher_bit_count > 0) {
    if (!reader_->ReadBits(&higher_bits, higher_bit_count)) {
      RTC_LOG(LS_WARNING) << "Failed to read higher half of delta.";
      return false;
    }
  } else {
    higher_bits = 0;
  }

  if (!reader_->ReadBits(&lower_bits, lower_bit_count)) {
    RTC_LOG(LS_WARNING) << "Failed to read lower half of delta.";
    return false;
  }

  const uint64_t lower_bits_64 = static_cast<uint64_t>(lower_bits);
  const uint64_t higher_bits_64 = static_cast<uint64_t>(higher_bits);

  *delta = (higher_bits_64 << 32) | lower_bits_64;
  return true;
}

uint64_t FixedLengthDeltaDecoder::ApplyDelta(uint64_t base,
                                             uint64_t delta) const {
  RTC_DCHECK(!params_.signed_deltas) << "Not implemented.";    // Reminder.
  RTC_DCHECK(!params_.values_optional) << "Not implemented.";  // Reminder.
  RTC_DCHECK_LE(base, MaxValueOfBitWidth(params_.original_width_bits));
  RTC_DCHECK_LE(delta, MaxValueOfBitWidth(params_.delta_width_bits));
  RTC_DCHECK_LE(params_.delta_width_bits,
                params_.original_width_bits);  // Reminder.
  return (base + delta) & value_mask_;
}

}  // namespace

std::string EncodeDeltas(uint64_t base, const std::vector<uint64_t>& values) {
  // TODO(eladalon): Support additional encodings.
  return FixedLengthDeltaEncoder::EncodeDeltas(base, values);
}

std::vector<uint64_t> DecodeDeltas(const std::string& input,
                                   uint64_t base,
                                   size_t num_of_deltas) {
  RTC_DCHECK_GT(num_of_deltas, 0);  // Allows empty vector to indicate error.

  // The empty string is a special case indicating that all values were equal
  // to the base.
  if (input.empty()) {
    std::vector<uint64_t> result(num_of_deltas);
    std::fill(result.begin(), result.end(), base);
    return result;
  }

  if (FixedLengthDeltaDecoder::IsSuitableDecoderFor(input)) {
    return FixedLengthDeltaDecoder::DecodeDeltas(input, base, num_of_deltas);
  }

  RTC_LOG(LS_WARNING) << "Could not decode delta-encoded stream.";
  return std::vector<uint64_t>();
}

}  // namespace webrtc
