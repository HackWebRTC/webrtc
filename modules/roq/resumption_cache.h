/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_RESUMPTION_CACHE_H_
#define MODULES_ROQ_RESUMPTION_CACHE_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc::roq {

struct ResumptionCacheKey {
  std::string endpoint;
  std::string server_name;
  std::string alpn;
  std::array<uint8_t, 32> context = {};
};

struct ResumptionState {
  Timestamp created_at = Timestamp::MinusInfinity();
  std::vector<uint8_t> session;
  std::vector<uint8_t> address_token;
};

enum class ResumptionCacheLoadStatus { kMiss, kLoaded, kExpired, kInvalid };

class ResumptionCache {
 public:
  ResumptionCache(std::string directory, TimeDelta maximum_age);

  bool Save(const ResumptionCacheKey& key,
            const ResumptionState& state,
            std::string* error) const;
  std::optional<ResumptionState> Load(
      const ResumptionCacheKey& key,
      Timestamp now,
      std::string* error,
      ResumptionCacheLoadStatus* status = nullptr) const;

 private:
  std::string EntryPath(const ResumptionCacheKey& key) const;

  const std::string directory_;
  const TimeDelta maximum_age_;
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_RESUMPTION_CACHE_H_
