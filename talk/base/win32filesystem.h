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

#ifndef _TALK_BASE_WIN32FILESYSTEM_H__
#define _TALK_BASE_WIN32FILESYSTEM_H__

#include "fileutils.h"

namespace talk_base {

class Win32Filesystem : public FilesystemInterface {
 public:
  // Opens a file. Returns an open StreamInterface if function succeeds. Otherwise,
  // returns NULL.
  virtual FileStream *OpenFile(const Pathname &filename, 
                               const std::string &mode);

  // Atomically creates an empty file accessible only to the current user if one
  // does not already exist at the given path, otherwise fails.
  virtual bool CreatePrivateFile(const Pathname &filename);

  // This will attempt to delete the path located at filename.
  // If the path points to a folder, it will fail with VERIFY
  virtual bool DeleteFile(const Pathname &filename);

  // This will attempt to delete an empty folder. If the path does not point to
  // a folder, it fails with VERIFY. If the folder is not empty, it fails normally
  virtual bool DeleteEmptyFolder(const Pathname &folder);

  // Creates a directory. This will call itself recursively to create /foo/bar even if
  // /foo does not exist.
  // Returns TRUE if function succeeds
  virtual bool CreateFolder(const Pathname &pathname);
  
  // This moves a file from old_path to new_path. If the new path is on a 
  // different volume than the old, it will attempt to copy and then delete
  // the folder
  // Returns true if the file is successfully moved
  virtual bool MoveFile(const Pathname &old_path, const Pathname &new_path);
  
  // Moves a folder from old_path to new_path. If the new path is on a different
  // volume from the old, it will attempt to Copy and then Delete the folder
  // Returns true if the folder is successfully moved
  virtual bool MoveFolder(const Pathname &old_path, const Pathname &new_path);
  
  // This copies a file from old_path to _new_path
  // Returns true if function succeeds
  virtual bool CopyFile(const Pathname &old_path, const Pathname &new_path);

  // Returns true if a pathname is a directory
  virtual bool IsFolder(const Pathname& pathname);
  
  // Returns true if a file exists at path
  virtual bool IsFile(const Pathname &path);

  // Returns true if pathname refers to no filesystem object, every parent
  // directory either exists, or is also absent.
  virtual bool IsAbsent(const Pathname& pathname);

  // Returns true if pathname represents a temporary location on the system.
  virtual bool IsTemporaryPath(const Pathname& pathname);

  // All of the following functions set pathname and return true if successful.
  // Returned paths always include a trailing backslash.
  // If create is true, the path will be recursively created.
  // If append is non-NULL, it will be appended (and possibly created).

  virtual std::string TempFilename(const Pathname &dir, const std::string &prefix);

  virtual bool GetFileSize(const Pathname& path, size_t* size);
  virtual bool GetFileTime(const Pathname& path, FileTimeType which,
                           time_t* time);
 
  // A folder appropriate for storing temporary files (Contents are
  // automatically deleted when the program exists)
  virtual bool GetTemporaryFolder(Pathname &path, bool create,
                                 const std::string *append);

  // Returns the path to the running application.
  virtual bool GetAppPathname(Pathname* path);

  virtual bool GetAppDataFolder(Pathname* path, bool per_user);

  // Get a temporary folder that is unique to the current user and application.
  virtual bool GetAppTempFolder(Pathname* path);

  virtual bool GetDiskFreeSpace(const Pathname& path, int64 *freebytes);

  virtual Pathname GetCurrentDirectory();
};

}  // namespace talk_base

#endif  // _WIN32FILESYSTEM_H__
