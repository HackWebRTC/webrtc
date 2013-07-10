/*
 * libjingle
 * Copyright 2004--2006, Google Inc.
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

#ifndef TALK_BASE_UNIXFILESYSTEM_H_
#define TALK_BASE_UNIXFILESYSTEM_H_

#include <sys/types.h>

#include "talk/base/fileutils.h"

namespace talk_base {

class UnixFilesystem : public FilesystemInterface {
 public:

#if defined(ANDROID) || defined(IOS)
// Android does not have a native code API to fetch the app data or temp
// folders. That needs to be passed into this class from Java. Similarly, iOS
// only supports an Objective-C API for fetching the folder locations, so that
// needs to be passed in here from Objective-C.

  static void SetAppDataFolder(const std::string& folder);
  static void SetAppTempFolder(const std::string& folder);
#endif

  // Opens a file. Returns an open StreamInterface if function succeeds.
  // Otherwise, returns NULL.
  virtual FileStream *OpenFile(const Pathname &filename,
                               const std::string &mode);

  // Atomically creates an empty file accessible only to the current user if one
  // does not already exist at the given path, otherwise fails.
  virtual bool CreatePrivateFile(const Pathname &filename);

  // This will attempt to delete the file located at filename.
  // It will fail with VERIY if you pass it a non-existant file, or a directory.
  virtual bool DeleteFile(const Pathname &filename);

  // This will attempt to delete the folder located at 'folder'
  // It ASSERTs and returns false if you pass it a non-existant folder or a
  // plain file.
  virtual bool DeleteEmptyFolder(const Pathname &folder);

  // Creates a directory. This will call itself recursively to create /foo/bar
  // even if /foo does not exist. All created directories are created with the
  // given mode.
  // Returns TRUE if function succeeds
  virtual bool CreateFolder(const Pathname &pathname, mode_t mode);

  // As above, with mode = 0755.
  virtual bool CreateFolder(const Pathname &pathname);

  // This moves a file from old_path to new_path, where "file" can be a plain
  // file or directory, which will be moved recursively.
  // Returns true if function succeeds.
  virtual bool MoveFile(const Pathname &old_path, const Pathname &new_path);
  virtual bool MoveFolder(const Pathname &old_path, const Pathname &new_path);

  // This copies a file from old_path to _new_path where "file" can be a plain
  // file or directory, which will be copied recursively.
  // Returns true if function succeeds
  virtual bool CopyFile(const Pathname &old_path, const Pathname &new_path);

  // Returns true if a pathname is a directory
  virtual bool IsFolder(const Pathname& pathname);

  // Returns true if pathname represents a temporary location on the system.
  virtual bool IsTemporaryPath(const Pathname& pathname);

  // Returns true of pathname represents an existing file
  virtual bool IsFile(const Pathname& pathname);

  // Returns true if pathname refers to no filesystem object, every parent
  // directory either exists, or is also absent.
  virtual bool IsAbsent(const Pathname& pathname);

  virtual std::string TempFilename(const Pathname &dir,
                                   const std::string &prefix);

  // A folder appropriate for storing temporary files (Contents are
  // automatically deleted when the program exists)
  virtual bool GetTemporaryFolder(Pathname &path, bool create,
                                 const std::string *append);

  virtual bool GetFileSize(const Pathname& path, size_t* size);
  virtual bool GetFileTime(const Pathname& path, FileTimeType which,
                           time_t* time);

  // Returns the path to the running application.
  virtual bool GetAppPathname(Pathname* path);

  virtual bool GetAppDataFolder(Pathname* path, bool per_user);

  // Get a temporary folder that is unique to the current user and application.
  virtual bool GetAppTempFolder(Pathname* path);

  virtual bool GetDiskFreeSpace(const Pathname& path, int64 *freebytes);

  // Returns the absolute path of the current directory.
  virtual Pathname GetCurrentDirectory();

 private:
#if defined(ANDROID) || defined(IOS)
  static char* provided_app_data_folder_;
  static char* provided_app_temp_folder_;
#else
  static char* app_temp_path_;
#endif

  static char* CopyString(const std::string& str);
};

}  // namespace talk_base

#endif  // TALK_BASE_UNIXFILESYSTEM_H_
