/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/system/file_wrapper.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <string.h>
#endif

#include <utility>

namespace webrtc {
namespace {
FILE* FileOpen(const char* file_name_utf8, bool read_only) {
#if defined(_WIN32)
  int len = MultiByteToWideChar(CP_UTF8, 0, file_name_utf8, -1, nullptr, 0);
  std::wstring wstr(len, 0);
  MultiByteToWideChar(CP_UTF8, 0, file_name_utf8, -1, &wstr[0], len);
  FILE* file = _wfopen(wstr.c_str(), read_only ? L"rb" : L"wb");
#else
  FILE* file = fopen(file_name_utf8, read_only ? "rb" : "wb");
#endif
  return file;
}
}  // namespace

// static
FileWrapper FileWrapper::OpenReadOnly(const char* file_name_utf8) {
  return FileWrapper(FileOpen(file_name_utf8, true));
}

// static
FileWrapper FileWrapper::OpenWriteOnly(const char* file_name_utf8) {
  return FileWrapper(FileOpen(file_name_utf8, false));
}

FileWrapper::FileWrapper(FileWrapper&& other) {
  operator=(std::move(other));
}

FileWrapper& FileWrapper::operator=(FileWrapper&& other) {
  Close();
  file_ = other.file_;
  other.file_ = nullptr;
  return *this;
}

bool FileWrapper::Rewind() {
  RTC_DCHECK(file_);
  return fseek(file_, 0, SEEK_SET) == 0;
}

bool FileWrapper::Flush() {
  RTC_DCHECK(file_);
  return fflush(file_) == 0;
}

size_t FileWrapper::Read(void* buf, size_t length) {
  RTC_DCHECK(file_);
  return fread(buf, 1, length, file_);
}

bool FileWrapper::Write(const void* buf, size_t length) {
  RTC_DCHECK(file_);
  return fwrite(buf, 1, length, file_) == length;
}

bool FileWrapper::Close() {
  if (file_ == nullptr)
    return true;

  bool success = fclose(file_) == 0;
  file_ = nullptr;
  return success;
}

}  // namespace webrtc
