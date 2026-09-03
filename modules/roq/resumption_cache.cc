/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/resumption_cache.h"

#include <fcntl.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "rtc_base/crypto_random.h"

namespace webrtc::roq {
namespace {

constexpr std::array<uint8_t, 8> kMagic = {'R', 'O', 'Q', 'C',
                                           'A', 'C', 'H', 'E'};
constexpr uint32_t kVersion = 1;
constexpr size_t kMaximumBlobSize = 1024 * 1024;

void AppendUint32(uint32_t value, std::vector<uint8_t>* output) {
  for (int shift = 24; shift >= 0; shift -= 8)
    output->push_back(static_cast<uint8_t>(value >> shift));
}

void AppendUint64(uint64_t value, std::vector<uint8_t>* output) {
  for (int shift = 56; shift >= 0; shift -= 8)
    output->push_back(static_cast<uint8_t>(value >> shift));
}

bool ReadUint32(std::span<const uint8_t>* input, uint32_t* value) {
  if (input->size() < 4)
    return false;
  *value = 0;
  for (size_t index = 0; index < 4; ++index)
    *value = (*value << 8) | (*input)[index];
  *input = input->subspan(4);
  return true;
}

bool ReadUint64(std::span<const uint8_t>* input, uint64_t* value) {
  if (input->size() < 8)
    return false;
  *value = 0;
  for (size_t index = 0; index < 8; ++index)
    *value = (*value << 8) | (*input)[index];
  *input = input->subspan(8);
  return true;
}

void AppendBytes(std::span<const uint8_t> value, std::vector<uint8_t>* output) {
  AppendUint32(static_cast<uint32_t>(value.size()), output);
  output->insert(output->end(), value.begin(), value.end());
}

void AppendString(const std::string& value, std::vector<uint8_t>* output) {
  AppendBytes(std::span<const uint8_t>(
                  reinterpret_cast<const uint8_t*>(value.data()), value.size()),
              output);
}

bool ReadBytes(std::span<const uint8_t>* input, std::vector<uint8_t>* value) {
  uint32_t length = 0;
  if (!ReadUint32(input, &length) || length > input->size())
    return false;
  value->assign(input->begin(), input->begin() + length);
  *input = input->subspan(length);
  return true;
}

bool ReadString(std::span<const uint8_t>* input, std::string* value) {
  std::vector<uint8_t> bytes;
  if (!ReadBytes(input, &bytes))
    return false;
  value->assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  return true;
}

std::vector<uint8_t> Serialize(const ResumptionCacheKey& key,
                               const ResumptionState& state) {
  std::vector<uint8_t> output(kMagic.begin(), kMagic.end());
  AppendUint32(kVersion, &output);
  AppendUint64(static_cast<uint64_t>(state.created_at.us()), &output);
  AppendString(key.endpoint, &output);
  AppendString(key.server_name, &output);
  AppendString(key.alpn, &output);
  AppendBytes(key.context, &output);
  AppendBytes(state.session, &output);
  AppendBytes(state.address_token, &output);
  return output;
}

bool Deserialize(std::span<const uint8_t> input,
                 const ResumptionCacheKey& key,
                 ResumptionState* state) {
  if (input.size() < kMagic.size() ||
      !std::equal(kMagic.begin(), kMagic.end(), input.begin())) {
    return false;
  }
  input = input.subspan(kMagic.size());
  uint32_t version = 0;
  uint64_t created_at_us = 0;
  std::string endpoint;
  std::string server_name;
  std::string alpn;
  std::vector<uint8_t> context;
  if (!ReadUint32(&input, &version) || version != kVersion ||
      !ReadUint64(&input, &created_at_us) || !ReadString(&input, &endpoint) ||
      !ReadString(&input, &server_name) || !ReadString(&input, &alpn) ||
      !ReadBytes(&input, &context) || !ReadBytes(&input, &state->session) ||
      !ReadBytes(&input, &state->address_token) || !input.empty() ||
      endpoint != key.endpoint || server_name != key.server_name ||
      alpn != key.alpn ||
      !std::equal(context.begin(), context.end(), key.context.begin(),
                  key.context.end())) {
    return false;
  }
  state->created_at = Timestamp::Micros(static_cast<int64_t>(created_at_us));
  return true;
}

bool WriteAll(int file, std::span<const uint8_t> bytes) {
  while (!bytes.empty()) {
    const ssize_t written = write(file, bytes.data(), bytes.size());
    if (written <= 0)
      return false;
    bytes = bytes.subspan(static_cast<size_t>(written));
  }
  return true;
}

}  // namespace

ResumptionCache::ResumptionCache(std::string directory, TimeDelta maximum_age)
    : directory_(std::move(directory)), maximum_age_(maximum_age) {}

std::string ResumptionCache::EntryPath(const ResumptionCacheKey& key) const {
  std::string material =
      key.endpoint + "\n" + key.server_name + "\n" + key.alpn + "\n";
  material.append(reinterpret_cast<const char*>(key.context.data()),
                  key.context.size());
  std::array<uint8_t, SHA256_DIGEST_LENGTH> digest = {};
  SHA256(reinterpret_cast<const uint8_t*>(material.data()), material.size(),
         digest.data());
  std::ostringstream name;
  name << std::hex << std::setfill('0');
  for (uint8_t byte : digest)
    name << std::setw(2) << static_cast<unsigned>(byte);
  return (std::filesystem::path(directory_) / (name.str() + ".roqcache"))
      .string();
}

bool ResumptionCache::Save(const ResumptionCacheKey& key,
                           const ResumptionState& state,
                           std::string* error) const {
  std::error_code filesystem_error;
  std::filesystem::create_directories(directory_, filesystem_error);
  if (filesystem_error) {
    *error = "failed to create resumption cache directory";
    return false;
  }
  std::ostringstream suffix;
  suffix << std::hex << CreateRandomId64();
  const std::string path = EntryPath(key);
  const std::string temporary = path + "." + suffix.str() + ".tmp";
  const std::vector<uint8_t> bytes = Serialize(key, state);
  const int file = open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
  if (file < 0) {
    *error = "failed to atomically write resumption cache";
    return false;
  }
  const bool write_succeeded = WriteAll(file, bytes);
  const bool sync_succeeded = write_succeeded && fsync(file) == 0;
  const bool close_succeeded = close(file) == 0;
  if (!write_succeeded || !sync_succeeded || !close_succeeded ||
      chmod(temporary.c_str(), 0600) != 0 ||
      rename(temporary.c_str(), path.c_str()) != 0) {
    unlink(temporary.c_str());
    *error = "failed to atomically write resumption cache";
    return false;
  }
  return true;
}

std::optional<ResumptionState> ResumptionCache::Load(
    const ResumptionCacheKey& key,
    Timestamp now,
    std::string* error,
    ResumptionCacheLoadStatus* status) const {
  if (status)
    *status = ResumptionCacheLoadStatus::kMiss;
  const std::string path = EntryPath(key);
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return std::nullopt;
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
  ResumptionState state;
  const bool valid =
      bytes.size() <= kMaximumBlobSize && Deserialize(bytes, key, &state);
  const bool expired = valid && (state.created_at > now ||
                                 now - state.created_at > maximum_age_);
  if (!valid || expired) {
    input.close();
    unlink(path.c_str());
    if (status)
      *status = expired ? ResumptionCacheLoadStatus::kExpired
                        : ResumptionCacheLoadStatus::kInvalid;
    *error = expired ? "discarded expired resumption cache entry"
                     : "discarded invalid resumption cache entry";
    return std::nullopt;
  }
  if (status)
    *status = ResumptionCacheLoadStatus::kLoaded;
  return state;
}

}  // namespace webrtc::roq
