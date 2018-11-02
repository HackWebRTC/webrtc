/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/unixfilesystem.h"

#include <sys/stat.h>
#include <unistd.h>
#include <string>

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <CoreServices/CoreServices.h>
#include <IOKit/IOCFBundle.h>
#include <sys/statvfs.h>

#include "rtc_base/macutils.h"
#endif  // WEBRTC_MAC && !defined(WEBRTC_IOS)

#if defined(WEBRTC_POSIX) && !defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
#include <sys/types.h>
#if defined(WEBRTC_ANDROID)
#include <sys/statfs.h>
#elif !defined(__native_client__)
#include <sys/statvfs.h>
#endif  //  !defined(__native_client__)
#include <stdio.h>
#endif  // WEBRTC_POSIX && !WEBRTC_MAC || WEBRTC_IOS

#if defined(__native_client__) && !defined(__GLIBC__)
#include <sys/syslimits.h>
#endif

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace rtc {

UnixFilesystem::UnixFilesystem() {}

UnixFilesystem::~UnixFilesystem() {}

bool UnixFilesystem::DeleteFile(const std::string& filename) {
  RTC_LOG(LS_INFO) << "Deleting file:" << filename;

  if (!IsFile(filename)) {
    RTC_DCHECK(IsFile(filename));
    return false;
  }
  return ::unlink(filename.c_str()) == 0;
}

bool UnixFilesystem::MoveFile(const std::string& old_path,
                              const std::string& new_path) {
  if (!IsFile(old_path)) {
    RTC_DCHECK(IsFile(old_path));
    return false;
  }
  RTC_LOG(LS_VERBOSE) << "Moving " << old_path << " to " << new_path;
  if (rename(old_path.c_str(), new_path.c_str()) != 0) {
    return false;
  }
  return true;
}

bool UnixFilesystem::IsFile(const std::string& pathname) {
  struct stat st;
  int res = ::stat(pathname.c_str(), &st);
  // Treat symlinks, named pipes, etc. all as files.
  return res == 0 && !S_ISDIR(st.st_mode);
}

bool UnixFilesystem::GetFileSize(const std::string& pathname, size_t* size) {
  struct stat st;
  if (::stat(pathname.c_str(), &st) != 0)
    return false;
  *size = st.st_size;
  return true;
}

}  // namespace rtc

#if defined(__native_client__)
extern "C" int __attribute__((weak))
link(const char* oldpath, const char* newpath) {
  errno = EACCES;
  return -1;
}
#endif
