/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include "fileutils.h"
#include "gtest/gtest.h"

#ifdef WIN32
#include <direct.h>
#define GET_CURRENT_DIR _getcwd
static const char* kPathDelimiter = "\\";
#else
#include <unistd.h>
#define GET_CURRENT_DIR getcwd
static const char* kPathDelimiter = "/";
#endif

namespace webrtc {
namespace test {

// Test fixture to restore the working directory between each test, since some
// of them change it with chdir during execution (not restored by the
// gtest framework).
class FileUtilsTest: public testing::Test {
 protected:
  FileUtilsTest() {
    original_working_dir_ = GetWorkingDir();
  }
  virtual ~FileUtilsTest() {}
  void SetUp() {
    chdir(original_working_dir_.c_str());
  }
  void TearDown() {}
 private:
  std::string original_working_dir_;
  static std::string GetWorkingDir() {
    char path_buffer[FILENAME_MAX];
    EXPECT_TRUE(GET_CURRENT_DIR(path_buffer, sizeof(path_buffer)))
      << "Cannot get current working directory!";
    return std::string(path_buffer);
  }
};

// Tests that the project root path is returned for the default working
// directory that is automatically set when the test executable is launched.
// The test is not fully testing the implementation, since we cannot be sure
// of where the executable was launched from.
// The test will fail if the top level directory is not named "trunk".
TEST_F(FileUtilsTest, GetProjectRootPathFromUnchangedWorkingDir) {
  std::string path = GetProjectRootPath();
  std::string expected_end = "trunk";
  expected_end = kPathDelimiter + expected_end + kPathDelimiter;
  ASSERT_EQ(path.length() - expected_end.length(), path.find(expected_end));
}

// Similar to the above test, but for the output dir
TEST_F(FileUtilsTest, GetOutputDirFromUnchangedWorkingDir) {
  std::string path = GetOutputDir();
  std::string expected_end = "out";
  expected_end = kPathDelimiter + expected_end + kPathDelimiter;
  ASSERT_EQ(path.length() - expected_end.length(), path.find(expected_end));
}

// Tests setting the current working directory to a directory three levels
// deeper from the current one. Then testing that the project path returned
// is still the same, when the function under test is called again.
TEST_F(FileUtilsTest, GetProjectRootPathFromDeeperWorkingDir) {
  std::string path = GetProjectRootPath();
  std::string original_working_dir = path;  // This is the correct project root
  // Change to a subdirectory path (the full path doesn't have to exist).
  path += "foo/bar/baz";
  chdir(path.c_str());
  ASSERT_EQ(original_working_dir, GetProjectRootPath());
}

// Similar to the above test, but for the output dir
TEST_F(FileUtilsTest, GetOutputDirFromDeeperWorkingDir) {
  std::string path = GetOutputDir();
  std::string original_working_dir = path;
  path += "foo/bar/baz";
  chdir(path.c_str());
  ASSERT_EQ(original_working_dir, GetOutputDir());
}

// Tests with current working directory set to a directory higher up in the
// directory tree than the project root dir. This case shall return a specified
// error string as a directory (which will be an invalid path).
TEST_F(FileUtilsTest, GetProjectRootPathFromRootWorkingDir) {
  // Change current working dir to the root of the current file system
  // (this will always be "above" our project root dir).
  chdir(kPathDelimiter);
  ASSERT_EQ(kCannotFindProjectRootDir, GetProjectRootPath());
}

// Similar to the above test, but for the output dir
TEST_F(FileUtilsTest, GetOutputDirFromRootWorkingDir) {
  chdir(kPathDelimiter);
  ASSERT_EQ(kCannotFindProjectRootDir, GetOutputDir());
}

}  // namespace test
}  // namespace webrtc
