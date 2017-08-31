/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_RTC_BASE_UNIXFILESYSTEM_H_
#define WEBRTC_RTC_BASE_UNIXFILESYSTEM_H_

#include <sys/types.h>

#include "webrtc/rtc_base/fileutils.h"

namespace rtc {

class UnixFilesystem : public FilesystemInterface {
 public:
  UnixFilesystem();
  ~UnixFilesystem() override;

  // This will attempt to delete the file located at filename.
  // It will fail with VERIY if you pass it a non-existant file, or a directory.
  bool DeleteFile(const Pathname& filename) override;

  // Creates a directory. This will call itself recursively to create /foo/bar
  // even if /foo does not exist. All created directories are created with the
  // given mode.
  // Returns TRUE if function succeeds
  virtual bool CreateFolder(const Pathname &pathname, mode_t mode);

  // As above, with mode = 0755.
  bool CreateFolder(const Pathname& pathname) override;

  // This moves a file from old_path to new_path, where "file" can be a plain
  // file or directory, which will be moved recursively.
  // Returns true if function succeeds.
  bool MoveFile(const Pathname& old_path, const Pathname& new_path) override;

  // Returns true if a pathname is a directory
  bool IsFolder(const Pathname& pathname) override;

  // Returns true of pathname represents an existing file
  bool IsFile(const Pathname& pathname) override;

  std::string TempFilename(const Pathname& dir,
                           const std::string& prefix) override;

  bool GetFileSize(const Pathname& path, size_t* size) override;

 private:
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_MAC)
  static char* provided_app_data_folder_;
  static char* provided_app_temp_folder_;
#else
  static char* app_temp_path_;
#endif

  static char* CopyString(const std::string& str);
};

}  // namespace rtc

#endif  // WEBRTC_RTC_BASE_UNIXFILESYSTEM_H_
