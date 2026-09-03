/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/resumption_cache.h"

#include <sys/stat.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

class TempDirectory {
 public:
  TempDirectory() {
    std::array<char, 64> pattern = {};
    const std::string value = "/tmp/roq-resumption-cache.XXXXXX";
    std::copy(value.begin(), value.end(), pattern.begin());
    path_ = mkdtemp(pattern.data());
  }
  ~TempDirectory() { std::filesystem::remove_all(path_); }
  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

ResumptionCacheKey Key(uint8_t context_byte = 7) {
  ResumptionCacheKey key = {
      .endpoint = "example.test:443",
      .server_name = "example.test",
      .alpn = "roq-14",
  };
  key.context.fill(context_byte);
  return key;
}

std::filesystem::path OnlyEntry(const std::string& directory) {
  const auto begin = std::filesystem::directory_iterator(directory);
  EXPECT_NE(begin, std::filesystem::directory_iterator());
  const std::filesystem::path path = begin->path();
  auto next = begin;
  ++next;
  EXPECT_EQ(next, std::filesystem::directory_iterator());
  return path;
}

TEST(ResumptionCacheTest, PersistsOpaqueStateWithPrivatePermissions) {
  TempDirectory directory;
  ResumptionCache cache(directory.path(), TimeDelta::Seconds(24 * 60 * 60));
  const ResumptionState expected = {
      .created_at = Timestamp::Seconds(100),
      .session = {1, 2, 3},
      .address_token = {4, 5},
  };
  std::string error;

  ASSERT_TRUE(cache.Save(Key(), expected, &error)) << error;
  const std::optional<ResumptionState> loaded =
      cache.Load(Key(), Timestamp::Seconds(101), &error);

  ASSERT_TRUE(loaded.has_value()) << error;
  EXPECT_EQ(loaded->session, expected.session);
  EXPECT_EQ(loaded->address_token, expected.address_token);
  struct stat attributes = {};
  ASSERT_EQ(stat(OnlyEntry(directory.path()).c_str(), &attributes), 0);
  EXPECT_EQ(attributes.st_mode & 0777, 0600);
}

TEST(ResumptionCacheTest, RejectsDifferentContext) {
  TempDirectory directory;
  ResumptionCache cache(directory.path(), TimeDelta::Seconds(24 * 60 * 60));
  std::string error;
  ASSERT_TRUE(cache.Save(
      Key(1), {.created_at = Timestamp::Seconds(100), .session = {1}}, &error));

  EXPECT_FALSE(cache.Load(Key(2), Timestamp::Seconds(101), &error).has_value());
}

TEST(ResumptionCacheTest, DeletesExpiredAndCorruptEntries) {
  TempDirectory directory;
  ResumptionCache cache(directory.path(), TimeDelta::Seconds(24 * 60 * 60));
  std::string error;
  ASSERT_TRUE(cache.Save(
      Key(), {.created_at = Timestamp::Seconds(100), .session = {1}}, &error));
  EXPECT_FALSE(
      cache
          .Load(Key(),
                Timestamp::Seconds(100) + TimeDelta::Seconds(25 * 60 * 60),
                &error)
          .has_value());
  EXPECT_EQ(std::filesystem::directory_iterator(directory.path()),
            std::filesystem::directory_iterator());

  ASSERT_TRUE(cache.Save(
      Key(), {.created_at = Timestamp::Seconds(200), .session = {1}}, &error));
  const std::string path = OnlyEntry(directory.path());
  FILE* file = fopen(path.c_str(), "wb");
  ASSERT_NE(file, nullptr);
  ASSERT_EQ(fwrite("bad", 1, 3, file), 3u);
  ASSERT_EQ(fclose(file), 0);
  EXPECT_FALSE(cache.Load(Key(), Timestamp::Seconds(201), &error).has_value());
  EXPECT_FALSE(std::filesystem::exists(path));
}

}  // namespace
}  // namespace webrtc::roq
