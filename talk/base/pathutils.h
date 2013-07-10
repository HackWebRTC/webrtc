/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_BASE_PATHUTILS_H__
#define TALK_BASE_PATHUTILS_H__

#include <string>
// Temporary, until deprecated helpers are removed.
#include "talk/base/fileutils.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// Pathname - parsing of pathnames into components, and vice versa.
//
// To establish consistent terminology, a filename never contains a folder
// component.  A folder never contains a filename.  A pathname may include
// a folder and/or filename component.  Here are some examples:
//
//   pathname()      /home/john/example.txt
//   folder()        /home/john/
//   filename()                 example.txt
//   parent_folder() /home/
//   folder_name()         john/
//   basename()                 example
//   extension()                       .txt
//
// Basename may begin, end, and/or include periods, but no folder delimiters.
// If extension exists, it consists of a period followed by zero or more
// non-period/non-delimiter characters, and basename is non-empty.
///////////////////////////////////////////////////////////////////////////////

class Pathname {
public:
  // Folder delimiters are slash and backslash
  static bool IsFolderDelimiter(char ch);
  static char DefaultFolderDelimiter();

  Pathname();
  Pathname(const std::string& pathname);
  Pathname(const std::string& folder, const std::string& filename);

  // Set's the default folder delimiter for this Pathname
  char folder_delimiter() const { return folder_delimiter_; }
  void SetFolderDelimiter(char delimiter);

  // Normalize changes all folder delimiters to folder_delimiter()
  void Normalize();

  // Reset to the empty pathname
  void clear();

  // Returns true if the pathname is empty.  Note: this->pathname().empty()
  // is always false.
  bool empty() const;

  std::string url() const;

  // Returns the folder and filename components.  If the pathname is empty,
  // returns a string representing the current directory (as a relative path,
  // i.e., ".").
  std::string pathname() const;
  void SetPathname(const std::string& pathname);
  void SetPathname(const std::string& folder, const std::string& filename);

  // Append pathname to the current folder (if any).  Any existing filename
  // will be discarded.
  void AppendPathname(const std::string& pathname);

  std::string folder() const;
  std::string folder_name() const;
  std::string parent_folder() const;
  // SetFolder and AppendFolder will append a folder delimiter, if needed.
  void SetFolder(const std::string& folder);
  void AppendFolder(const std::string& folder);

  std::string basename() const;
  bool SetBasename(const std::string& basename);

  std::string extension() const;
  // SetExtension will prefix a period, if needed.
  bool SetExtension(const std::string& extension);

  std::string filename() const;
  bool SetFilename(const std::string& filename);

#ifdef WIN32
  bool GetDrive(char *drive, uint32 bytes) const;
  static bool GetDrive(char *drive, uint32 bytes,const std::string& pathname);
#endif

private:
  std::string folder_, basename_, extension_;
  char folder_delimiter_;
};

///////////////////////////////////////////////////////////////////////////////
// Global Helpers (deprecated)
///////////////////////////////////////////////////////////////////////////////

inline void SetOrganizationName(const std::string& organization) {
  Filesystem::SetOrganizationName(organization);
}
inline void SetApplicationName(const std::string& application) {
  Filesystem::SetApplicationName(application);
}
inline void GetOrganizationName(std::string* organization) {
  Filesystem::GetOrganizationName(organization);
}
inline void GetApplicationName(std::string* application) {
  Filesystem::GetApplicationName(application);
}
inline bool CreateFolder(const Pathname& path) {
  return Filesystem::CreateFolder(path);
}
inline bool FinishPath(Pathname& path, bool create, const std::string& append) {
  if (!append.empty())
    path.AppendFolder(append);
  return !create || CreateFolder(path);
}
// Note: this method uses the convention of <temp>/<appname> for the temporary
// folder.  Filesystem uses <temp>/<exename>.  We will be migrating exclusively
// to <temp>/<orgname>/<appname> eventually.  Since these are temp folders,
// it's probably ok to orphan them during the transition.
inline bool GetTemporaryFolder(Pathname& path, bool create,
                               const std::string& append) {
  std::string application_name;
  Filesystem::GetApplicationName(&application_name);
  ASSERT(!application_name.empty());
  return Filesystem::GetTemporaryFolder(path, create, &application_name)
         && FinishPath(path, create, append);
}
inline bool GetAppDataFolder(Pathname& path, bool create,
                             const std::string& append) {
  ASSERT(!create); // TODO: Support create flag on Filesystem::GetAppDataFolder.
  return Filesystem::GetAppDataFolder(&path, true)
         && FinishPath(path, create, append);
}
inline bool CleanupTemporaryFolder() {
  Pathname path;
  if (!GetTemporaryFolder(path, false, ""))
    return false;
  if (Filesystem::IsAbsent(path))
    return true;
  if (!Filesystem::IsTemporaryPath(path)) {
    ASSERT(false);
    return false;
  }
  return Filesystem::DeleteFolderContents(path);
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base

#endif // TALK_BASE_PATHUTILS_H__
