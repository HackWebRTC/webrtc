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

#include <cassert>

#ifdef WIN32
// TODO(grunell): Remove io.h includes when Chromium has started
// to use AEC in each source. http://crbug.com/264611.
#include <io.h>
#include "talk/base/win32.h"
#endif

#include "talk/base/pathutils.h"
#include "talk/base/fileutils.h"
#include "talk/base/stringutils.h"
#include "talk/base/stream.h"

#ifdef WIN32
#include "talk/base/win32filesystem.h"
#else
#include "talk/base/unixfilesystem.h"
#endif

#ifndef WIN32
#define MAX_PATH 260
#endif

namespace talk_base {

//////////////////////////
// Directory Iterator   //
//////////////////////////

// A DirectoryIterator is created with a given directory. It originally points
// to the first file in the directory, and can be advanecd with Next(). This
// allows you to get information about each file.

  // Constructor
DirectoryIterator::DirectoryIterator()
#ifdef _WIN32
    : handle_(INVALID_HANDLE_VALUE) {
#else
    : dir_(NULL), dirent_(NULL) {
#endif
}

  // Destructor
DirectoryIterator::~DirectoryIterator() {
#ifdef WIN32
  if (handle_ != INVALID_HANDLE_VALUE)
    ::FindClose(handle_);
#else
  if (dir_)
    closedir(dir_);
#endif
}

  // Starts traversing a directory.
  // dir is the directory to traverse
  // returns true if the directory exists and is valid
bool DirectoryIterator::Iterate(const Pathname &dir) {
  directory_ = dir.pathname();
#ifdef WIN32
  if (handle_ != INVALID_HANDLE_VALUE)
    ::FindClose(handle_);
  std::string d = dir.pathname() + '*';
  handle_ = ::FindFirstFile(ToUtf16(d).c_str(), &data_);
  if (handle_ == INVALID_HANDLE_VALUE)
    return false;
#else
  if (dir_ != NULL)
    closedir(dir_);
  dir_ = ::opendir(directory_.c_str());
  if (dir_ == NULL)
    return false;
  dirent_ = readdir(dir_);
  if (dirent_ == NULL)
    return false;

  if (::stat(std::string(directory_ + Name()).c_str(), &stat_) != 0)
    return false;
#endif
  return true;
}

  // Advances to the next file
  // returns true if there were more files in the directory.
bool DirectoryIterator::Next() {
#ifdef WIN32
  return ::FindNextFile(handle_, &data_) == TRUE;
#else
  dirent_ = ::readdir(dir_);
  if (dirent_ == NULL)
    return false;

  return ::stat(std::string(directory_ + Name()).c_str(), &stat_) == 0;
#endif
}

  // returns true if the file currently pointed to is a directory
bool DirectoryIterator::IsDirectory() const {
#ifdef WIN32
  return (data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FALSE;
#else
  return S_ISDIR(stat_.st_mode);
#endif
}

  // returns the name of the file currently pointed to
std::string DirectoryIterator::Name() const {
#ifdef WIN32
  return ToUtf8(data_.cFileName);
#else
  assert(dirent_ != NULL);
  return dirent_->d_name;
#endif
}

  // returns the size of the file currently pointed to
size_t DirectoryIterator::FileSize() const {
#ifndef WIN32
  return stat_.st_size;
#else
  return data_.nFileSizeLow;
#endif
}

  // returns the last modified time of this file
time_t DirectoryIterator::FileModifyTime() const {
#ifdef WIN32
  time_t val;
  FileTimeToUnixTime(data_.ftLastWriteTime, &val);
  return val;
#else
  return stat_.st_mtime;
#endif
}

FilesystemInterface* Filesystem::default_filesystem_ = NULL;

FilesystemInterface *Filesystem::EnsureDefaultFilesystem() {
  if (!default_filesystem_) {
#ifdef WIN32
    default_filesystem_ = new Win32Filesystem();
#else
    default_filesystem_ = new UnixFilesystem();
#endif
  }
  return default_filesystem_;
}

bool FilesystemInterface::CopyFolder(const Pathname &old_path,
                                     const Pathname &new_path) {
  bool success = true;
  VERIFY(IsFolder(old_path));
  Pathname new_dir;
  new_dir.SetFolder(new_path.pathname());
  Pathname old_dir;
  old_dir.SetFolder(old_path.pathname());
  if (!CreateFolder(new_dir))
    return false;
  DirectoryIterator *di = IterateDirectory();
  if (!di)
    return false;
  if (di->Iterate(old_dir.pathname())) {
    do {
      if (di->Name() == "." || di->Name() == "..")
        continue;
      Pathname source;
      Pathname dest;
      source.SetFolder(old_dir.pathname());
      dest.SetFolder(new_path.pathname());
      source.SetFilename(di->Name());
      dest.SetFilename(di->Name());
      if (!CopyFileOrFolder(source, dest))
        success = false;
    } while (di->Next());
  }
  delete di;
  return success;
}

bool FilesystemInterface::DeleteFolderContents(const Pathname &folder) {
  bool success = true;
  VERIFY(IsFolder(folder));
  DirectoryIterator *di = IterateDirectory();
  if (!di)
    return false;
  if (di->Iterate(folder)) {
    do {
      if (di->Name() == "." || di->Name() == "..")
        continue;
      Pathname subdir;
      subdir.SetFolder(folder.pathname());
      if (di->IsDirectory()) {
        subdir.AppendFolder(di->Name());
        if (!DeleteFolderAndContents(subdir)) {
          success = false;
        }
      } else {
        subdir.SetFilename(di->Name());
        if (!DeleteFile(subdir)) {
          success = false;
        }
      }
    } while (di->Next());
  }
  delete di;
  return success;
}

bool FilesystemInterface::CleanAppTempFolder() {
  Pathname path;
  if (!GetAppTempFolder(&path))
    return false;
  if (IsAbsent(path))
    return true;
  if (!IsTemporaryPath(path)) {
    ASSERT(false);
    return false;
  }
  return DeleteFolderContents(path);
}

Pathname Filesystem::GetCurrentDirectory() {
  return EnsureDefaultFilesystem()->GetCurrentDirectory();
}

bool CreateUniqueFile(Pathname& path, bool create_empty) {
  LOG(LS_INFO) << "Path " << path.pathname() << std::endl;
  // If no folder is supplied, use the temporary folder
  if (path.folder().empty()) {
    Pathname temporary_path;
    if (!Filesystem::GetTemporaryFolder(temporary_path, true, NULL)) {
      printf("Get temp failed\n");
      return false;
    }
    path.SetFolder(temporary_path.pathname());
  }

  // If no filename is supplied, use a temporary name
  if (path.filename().empty()) {
    std::string folder(path.folder());
    std::string filename = Filesystem::TempFilename(folder, "gt");
    path.SetPathname(filename);
    if (!create_empty) {
      Filesystem::DeleteFile(path.pathname());
    }
    return true;
  }

  // Otherwise, create a unique name based on the given filename
  // foo.txt -> foo-N.txt
  const std::string basename = path.basename();
  const size_t MAX_VERSION = 100;
  size_t version = 0;
  while (version < MAX_VERSION) {
    std::string pathname = path.pathname();

    if (!Filesystem::IsFile(pathname)) {
      if (create_empty) {
        FileStream* fs = Filesystem::OpenFile(pathname, "w");
        delete fs;
      }
      return true;
    }
    version += 1;
    char version_base[MAX_PATH];
    sprintfn(version_base, ARRAY_SIZE(version_base), "%s-%u",
             basename.c_str(), version);
    path.SetBasename(version_base);
  }
  return true;
}

// Taken from Chromium's base/platform_file_*.cc.
// TODO(grunell): Remove when Chromium has started to use AEC in each source.
// http://crbug.com/264611.
FILE* FdopenPlatformFileForWriting(PlatformFile file) {
#if defined(WIN32)
  if (file == kInvalidPlatformFileValue)
    return NULL;
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(file), 0);
  if (fd < 0)
    return NULL;
  return _fdopen(fd, "w");
#else
  return fdopen(file, "w");
#endif
}

bool ClosePlatformFile(PlatformFile file) {
#if defined(WIN32)
  return CloseHandle(file) != 0;
#else
  return close(file);
#endif
}

}  // namespace talk_base
