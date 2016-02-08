/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_EXECUTABLEHELPERS_H_
#define WEBRTC_MEDIA_BASE_EXECUTABLEHELPERS_H_

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <mach-o/dyld.h>
#endif

#include <string>

#include "webrtc/base/logging.h"
#include "webrtc/base/pathutils.h"

namespace rtc {

// Returns the path to the running executable or an empty path.
// TODO(thorcarpenter): Consolidate with FluteClient::get_executable_dir.
inline Pathname GetExecutablePath() {
  const int32_t kMaxExePathSize = 255;
#ifdef WIN32
  TCHAR exe_path_buffer[kMaxExePathSize];
  DWORD copied_length = GetModuleFileName(NULL,  // NULL = Current process
                                          exe_path_buffer, kMaxExePathSize);
  if (0 == copied_length) {
    LOG(LS_ERROR) << "Copied length is zero";
    return rtc::Pathname();
  }
  if (kMaxExePathSize == copied_length) {
    LOG(LS_ERROR) << "Buffer too small";
    return rtc::Pathname();
  }
#ifdef UNICODE
  std::wstring wdir(exe_path_buffer);
  std::string dir_tmp(wdir.begin(), wdir.end());
  rtc::Pathname path(dir_tmp);
#else  // UNICODE
  rtc::Pathname path(exe_path_buffer);
#endif  // UNICODE
#elif (defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)) || defined(WEBRTC_LINUX)
  char exe_path_buffer[kMaxExePathSize];
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  uint32_t copied_length = kMaxExePathSize - 1;
  if (_NSGetExecutablePath(exe_path_buffer, &copied_length) == -1) {
    LOG(LS_ERROR) << "Buffer too small";
    return rtc::Pathname();
  }
#elif defined WEBRTC_LINUX
  int32_t copied_length = kMaxExePathSize - 1;
  const char* kProcExeFmt = "/proc/%d/exe";
  char proc_exe_link[40];
  snprintf(proc_exe_link, sizeof(proc_exe_link), kProcExeFmt, getpid());
  copied_length = readlink(proc_exe_link, exe_path_buffer, copied_length);
  if (copied_length == -1) {
    LOG_ERR(LS_ERROR) << "Error reading link " << proc_exe_link;
    return rtc::Pathname();
  }
  if (copied_length == kMaxExePathSize - 1) {
    LOG(LS_ERROR) << "Probably truncated result when reading link "
                  << proc_exe_link;
    return rtc::Pathname();
  }
  exe_path_buffer[copied_length] = '\0';
#endif  // WEBRTC_LINUX
  rtc::Pathname path(exe_path_buffer);
#else  // Android || iOS
  rtc::Pathname path;
#endif  // Mac || Linux
  return path;
}

}  // namespace rtc

#endif  // WEBRTC_MEDIA_BASE_EXECUTABLEHELPERS_H_

