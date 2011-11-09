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

#ifdef WIN32
#include <direct.h>
#define GET_CURRENT_DIR _getcwd
#else
#include <unistd.h>
#define GET_CURRENT_DIR getcwd
#endif

#include <sys/stat.h>  // To check for directory existence.
#ifndef S_ISDIR  // Not defined in stat.h on Windows.
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#include <cstdio>

namespace webrtc {
namespace test {

#ifdef WIN32
static const char* kPathDelimiter = "\\";
#else
static const char* kPathDelimiter = "/";
#endif
// The file we're looking for to identify the project root dir.
static const char* kProjectRootFileName = "DEPS";
static const char* kOutputDirName = "out";
const char* kCannotFindProjectRootDir = "ERROR_CANNOT_FIND_PROJECT_ROOT_DIR";

std::string GetProjectRootPath() {
  char path_buffer[FILENAME_MAX];
  if (!GET_CURRENT_DIR(path_buffer, sizeof(path_buffer))) {
    fprintf(stderr, "Cannot get current directory!\n");
    return kCannotFindProjectRootDir;
  }

  // Check for our file that verifies the root dir.
  std::string current_path(path_buffer);
  FILE* file = NULL;
  int path_delimiter_index = current_path.find_last_of(kPathDelimiter);
  while (path_delimiter_index > -1) {
    std::string root_filename = current_path + kPathDelimiter +
        kProjectRootFileName;
    file = fopen(root_filename.c_str(), "r");
    if (file != NULL) {
      return current_path + kPathDelimiter;
    }

    // Move up one directory in the directory tree.
    current_path = current_path.substr(0, path_delimiter_index);
    path_delimiter_index = current_path.find_last_of(kPathDelimiter);
  }

  // Reached the root directory.
  fprintf(stderr, "Cannot find project root directory!\n");
  return kCannotFindProjectRootDir;
}

std::string GetOutputDir() {
  std::string path = GetProjectRootPath();
  if (path == kCannotFindProjectRootDir) {
    return kCannotFindProjectRootDir;
  }
  path += kOutputDirName;
  struct stat path_info = {0};
  // Check if the path exists already:
  if (stat(path.c_str(), &path_info) == 0) {
    if (!S_ISDIR(path_info.st_mode)) {
      fprintf(stderr, "Path %s exists but is not a directory! Remove this file "
              "and re-run to create the output folder.\n", path.c_str());
      return kCannotFindProjectRootDir;
    }
  } else {
#ifdef WIN32
      _mkdir(path.c_str());
#else
      mkdir(path.c_str(),  S_IRWXU | S_IRWXG | S_IRWXO);
#endif
  }
  return path + kPathDelimiter;
}
}  // namespace test
}  // namespace webrtc
