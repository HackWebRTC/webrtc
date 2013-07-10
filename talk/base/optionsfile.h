/*
 * libjingle
 * Copyright 2008, Google Inc.
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

#ifndef TALK_BASE_OPTIONSFILE_H_
#define TALK_BASE_OPTIONSFILE_H_

#include <map>
#include <string>

namespace talk_base {

// Implements storage of simple options in a text file on disk. This is
// cross-platform, but it is intended mostly for Linux where there is no
// first-class options storage system.
class OptionsFile {
 public:
  OptionsFile(const std::string &path);

  // Loads the file from disk, overwriting the in-memory values.
  bool Load();
  // Saves the contents in memory, overwriting the on-disk values.
  bool Save();

  bool GetStringValue(const std::string& option, std::string* out_val) const;
  bool GetIntValue(const std::string& option, int* out_val) const;
  bool SetStringValue(const std::string& option, const std::string& val);
  bool SetIntValue(const std::string& option, int val);
  bool RemoveValue(const std::string& option);

 private:
  typedef std::map<std::string, std::string> OptionsMap;

  static bool IsLegalName(const std::string &name);
  static bool IsLegalValue(const std::string &value);

  std::string path_;
  OptionsMap options_;
};

}  // namespace talk_base

#endif  // TALK_BASE_OPTIONSFILE_H_
