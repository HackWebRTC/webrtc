/*
 * libjingle
 * Copyright 2014 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_MEDIA_BASE_EXECUTABLEHELPERS_H_
#define TALK_MEDIA_BASE_EXECUTABLEHELPERS_H_

#ifdef OSX
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
#elif defined(OSX) || defined(LINUX)
  char exe_path_buffer[kMaxExePathSize];
#ifdef OSX
  uint32_t copied_length = kMaxExePathSize - 1;
  if (_NSGetExecutablePath(exe_path_buffer, &copied_length) == -1) {
    LOG(LS_ERROR) << "Buffer too small";
    return rtc::Pathname();
  }
#elif defined LINUX
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
#endif  // LINUX
  rtc::Pathname path(exe_path_buffer);
#else  // Android || IOS
  rtc::Pathname path;
#endif  // OSX || LINUX
  return path;
}

}  // namespace rtc

#endif  // TALK_MEDIA_BASE_EXECUTABLEHELPERS_H_

