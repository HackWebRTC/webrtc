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
#define PATH_DELIMITER "\\"
#else
#include <unistd.h>
#define GET_CURRENT_DIR getcwd
#define PATH_DELIMITER "/"
#endif

#include <cstdio>

namespace webrtc {
namespace test {

const std::string GetProjectRootPath() {
  char path_buffer[FILENAME_MAX];
  if (!GET_CURRENT_DIR(path_buffer, sizeof(path_buffer))) {
    fprintf(stderr, "Cannot get current directory!\n");
    return kCannotFindProjectRootDir;
  }

  // Check for our file that verifies the root dir.
  std::string current_path(path_buffer);
  FILE* file = NULL;
  int path_delimiter_index = current_path.find_last_of(PATH_DELIMITER);
  while (path_delimiter_index > -1) {
    std::string root_filename = current_path + PATH_DELIMITER +
        kProjectRootFileName;
    file = fopen(root_filename.c_str(), "r");
    if (file != NULL) {
      return current_path + PATH_DELIMITER;
    }

    // Move up one directory in the directory tree.
    current_path = current_path.substr(0, path_delimiter_index);
    path_delimiter_index = current_path.find_last_of(PATH_DELIMITER);
  }

  // Reached the root directory.
  fprintf(stderr, "Cannot find project root directory!\n");
  return kCannotFindProjectRootDir;
}

}  // namespace webrtc
}  // namespace test
