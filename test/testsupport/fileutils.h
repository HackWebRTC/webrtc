/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// File utilities for testing purposes.
// The GetProjectRootPath() method is a convenient way of getting an absolute
// path to the project source tree root directory. Using this, it is easy to
// refer to test resource files in a portable way.
//
// Notice that even if Windows platforms use backslash as path delimiter, it is
// also supported to use slash, so there's no need for #ifdef checks in test
// code for setting up the paths to the resource files.
//
// Example use:
// Assume we have the following code being used in a test source file:
// const std::string kInputFile = webrtc::testing::GetProjectRootPath() +
//     "test/data/voice_engine/audio_long16.wav";
// // Use the kInputFile for the tests...
//
// Then here's some example outputs for different platforms:
// Linux:
// * Source tree located in /home/user/webrtc/trunk
// * Test project located in /home/user/webrtc/trunk/src/testproject
// * Test binary compiled as:
//   /home/user/webrtc/trunk/out/Debug/testproject_unittests
// Then GetProjectRootPath() will return /home/user/webrtc/trunk/ no matter if
// the test binary is executed from standing in either of:
// /home/user/webrtc/trunk
// or
// /home/user/webrtc/trunk/out/Debug
// (or any other directory below the trunk for that matter).
//
// Windows:
// * Source tree located in C:\Users\user\webrtc\trunk
// * Test project located in C:\Users\user\webrtc\trunk\src\testproject
// * Test binary compiled as:
//   C:\Users\user\webrtc\trunk\src\testproject\Debug\testproject_unittests.exe
// Then GetProjectRootPath() will return C:\Users\user\webrtc\trunk\ when the
// test binary is executed from inside Visual Studio.
// It will also return the same path if the test is executed from a command
// prompt standing in C:\Users\user\webrtc\trunk\src\testproject\Debug
//
// Mac:
// * Source tree located in /Users/user/webrtc/trunk
// * Test project located in /Users/user/webrtc/trunk/src/testproject
// * Test binary compiled as:
//   /Users/user/webrtc/trunk/xcodebuild/Debug/testproject_unittests
// Then GetProjectRootPath() will return /Users/user/webrtc/trunk/ no matter if
// the test binary is executed from standing in either of:
// /Users/user/webrtc/trunk
// or
// /Users/user/webrtc/trunk/out/Debug
// (or any other directory below the trunk for that matter).

#ifndef TEST_TESTSUPPORT_FILEUTILS_H_
#define TEST_TESTSUPPORT_FILEUTILS_H_

#include <string>

namespace webrtc {
namespace test {

// The file we're looking for to identify the project root dir.
const std::string kProjectRootFileName = "DEPS";

// This is the "directory" returned if the GetProjectPath() function fails
// to find the project root.
const std::string kCannotFindProjectRootDir =
    "ERROR_CANNOT_FIND_PROJECT_ROOT_DIR";

// Finds the root dir of the project, to be able to set correct paths to
// resource files used by tests.
// The implementation is simple: it just looks for the file defined by
// kProjectRootFileName, starting in the current directory (the working
// directory) and then steps upward until it is found (or it is at the root of
// the file system).
// If the current working directory is above the project root dir, it will not
// be found.
//
// If symbolic links occur in the path they will be resolved and the actual
// directory will be returned.
//
// Returns the absolute path to the project root dir (usually the trunk dir)
// WITH a trailing path delimiter.
// If the project root is not found, the string specified by
// kCannotFindProjectRootDir is returned.
const std::string GetProjectRootPath();

}  // namespace webrtc
}  // namespace test

#endif  // TEST_TESTSUPPORT_FILEUTILS_H_
