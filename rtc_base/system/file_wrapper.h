/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_FILE_WRAPPER_H_
#define RTC_BASE_SYSTEM_FILE_WRAPPER_H_

#include <stddef.h>
#include <stdio.h>

#include "rtc_base/critical_section.h"

// Implementation that can read (exclusive) or write from/to a file.

namespace webrtc {

class FileWrapper final {
 public:
  // Opens a file, in read or write mode. Use the is_open() method on the
  // returned object to check if the open operation was successful. The file is
  // closed by the destructor.
  static FileWrapper OpenReadOnly(const char* file_name_utf8);
  static FileWrapper OpenWriteOnly(const char* file_name_utf8);

  FileWrapper() = default;

  // Takes over ownership of |file|, closing it on destruction.
  explicit FileWrapper(FILE* file) : file_(file) {}
  ~FileWrapper() { Close(); }

  // Copying is not supported.
  FileWrapper(const FileWrapper&) = delete;
  FileWrapper& operator=(const FileWrapper&) = delete;

  // Support for move semantics.
  FileWrapper(FileWrapper&&);
  FileWrapper& operator=(FileWrapper&&);

  // Returns true if a file has been opened. If the file is not open, no methods
  // but is_open and Close may be called.
  bool is_open() const { return file_ != nullptr; }

  // Closes the file, and implies Flush. Returns true on success, false if
  // writing buffered data fails. On failure, the file is nevertheless closed.
  // Calling Close on an already closed file does nothing and returns success.
  bool Close();

  // Write any buffered data to the underlying file. Returns true on success,
  // false on write error. Note: Flushing when closing, is not required.
  // TODO(nisse): Delete this method.
  bool Flush();

  // Seeks to the beginning of file. Returns true on success, false on failure,
  // e.g., if the underlying file isn't seekable.
  // TODO(nisse): Delete this method.
  bool Rewind();

  // Returns number of bytes read. Short count indicates EOF or error.
  size_t Read(void* buf, size_t length);

  // Returns true if all data was successfully written (or buffered), or false
  // if there was an error. Writing buffered data can fail later, and is
  // reported with return value from Flush or Close.
  bool Write(const void* buf, size_t length);

 private:
  FILE* file_ = nullptr;
};

}  // namespace webrtc

#endif  // RTC_BASE_SYSTEM_FILE_WRAPPER_H_
