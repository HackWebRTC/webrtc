/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "fileutils.h"
#include "gtest/gtest.h"

#ifdef WIN32
#define PATH_DELIMITER "\\"
#else
#define PATH_DELIMITER "/"
#endif

namespace webrtc {
namespace test {

// Tests that the project root path is returnd for the default working directory
// that is automatically set when the test executable is launched.
// The test is not fully testing the implementation, since we cannot be sure
// of where the executable was launched from.
// The test will fail if the top level directory is not named "trunk".
TEST(FileUtilsTest, GetProjectRootPathFromUnchangedWorkingDir) {
  std::string path = GetProjectRootPath();
  std::string expected_end = "trunk";
  expected_end = PATH_DELIMITER + expected_end + PATH_DELIMITER;
  ASSERT_EQ(path.length() - expected_end.length(), path.find(expected_end));
}

// Tests setting the current working directory to a directory three levels
// deeper from the current one. Then testing that the project path returned
// is still the same, when the function under test is called again.
TEST(FileUtilsTest, GetProjectRootPathFromDeeperWorkingDir) {
  std::string path = GetProjectRootPath();
  std::string original_working_dir = path;  // This is the correct project root

  // Change to a subdirectory path (the full path doesn't have to exist).
  path += "foo/bar/baz";
  chdir(path.c_str());

  ASSERT_EQ(original_working_dir, GetProjectRootPath());
}

// Tests with current working directory set to a directory higher up in the
// directory tree than the project root dir. This case shall return a specified
// error string as a directory (which will be an invalid path).
TEST(FileUtilsTest, GetProjectRootPathFromRootWorkingDir) {
  // Change current working dir to the root of the current file system
  // (this will always be "above" our project root dir).
  chdir(PATH_DELIMITER);
  ASSERT_EQ(kCannotFindProjectRootDir, GetProjectRootPath());
}

}  // namespace test
}  // namespace webrtc
