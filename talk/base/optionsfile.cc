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

#include "talk/base/optionsfile.h"

#include <ctype.h>

#include "talk/base/logging.h"
#include "talk/base/stream.h"
#include "talk/base/stringencode.h"

namespace talk_base {

OptionsFile::OptionsFile(const std::string &path) : path_(path) {
}

bool OptionsFile::Load() {
  options_.clear();
  // Open file.
  FileStream stream;
  int err;
  if (!stream.Open(path_, "r", &err)) {
    LOG_F(LS_WARNING) << "Could not open file, err=" << err;
    // We do not consider this an error because we expect there to be no file
    // until the user saves a setting.
    return true;
  }
  // Read in all its data.
  std::string line;
  StreamResult res;
  for (;;) {
    res = stream.ReadLine(&line);
    if (res != SR_SUCCESS) {
      break;
    }
    size_t equals_pos = line.find('=');
    if (equals_pos == std::string::npos) {
      // We do not consider this an error. Instead we ignore the line and
      // keep going.
      LOG_F(LS_WARNING) << "Ignoring malformed line in " << path_;
      continue;
    }
    std::string key(line, 0, equals_pos);
    std::string value(line, equals_pos + 1, line.length() - (equals_pos + 1));
    options_[key] = value;
  }
  if (res != SR_EOS) {
    LOG_F(LS_ERROR) << "Error when reading from file";
    return false;
  } else {
    return true;
  }
}

bool OptionsFile::Save() {
  // Open file.
  FileStream stream;
  int err;
  if (!stream.Open(path_, "w", &err)) {
    LOG_F(LS_ERROR) << "Could not open file, err=" << err;
    return false;
  }
  // Write out all the data.
  StreamResult res = SR_SUCCESS;
  size_t written;
  int error;
  for (OptionsMap::const_iterator i = options_.begin(); i != options_.end();
       ++i) {
    res = stream.WriteAll(i->first.c_str(), i->first.length(), &written,
        &error);
    if (res != SR_SUCCESS) {
      break;
    }
    res = stream.WriteAll("=", 1, &written, &error);
    if (res != SR_SUCCESS) {
      break;
    }
    res = stream.WriteAll(i->second.c_str(), i->second.length(), &written,
        &error);
    if (res != SR_SUCCESS) {
      break;
    }
    res = stream.WriteAll("\n", 1, &written, &error);
    if (res != SR_SUCCESS) {
      break;
    }
  }
  if (res != SR_SUCCESS) {
    LOG_F(LS_ERROR) << "Unable to write to file";
    return false;
  } else {
    return true;
  }
}

bool OptionsFile::IsLegalName(const std::string &name) {
  for (size_t pos = 0; pos < name.length(); ++pos) {
    if (name[pos] == '\n' || name[pos] == '\\' || name[pos] == '=') {
      // Illegal character.
      LOG(LS_WARNING) << "Ignoring operation for illegal option " << name;
      return false;
    }
  }
  return true;
}

bool OptionsFile::IsLegalValue(const std::string &value) {
  for (size_t pos = 0; pos < value.length(); ++pos) {
    if (value[pos] == '\n' || value[pos] == '\\') {
      // Illegal character.
      LOG(LS_WARNING) << "Ignoring operation for illegal value " << value;
      return false;
    }
  }
  return true;
}

bool OptionsFile::GetStringValue(const std::string& option,
                                 std::string *out_val) const {
  LOG(LS_VERBOSE) << "OptionsFile::GetStringValue "
                  << option;
  if (!IsLegalName(option)) {
    return false;
  }
  OptionsMap::const_iterator i = options_.find(option);
  if (i == options_.end()) {
    return false;
  }
  *out_val = i->second;
  return true;
}

bool OptionsFile::GetIntValue(const std::string& option,
                              int *out_val) const {
  LOG(LS_VERBOSE) << "OptionsFile::GetIntValue "
                  << option;
  if (!IsLegalName(option)) {
    return false;
  }
  OptionsMap::const_iterator i = options_.find(option);
  if (i == options_.end()) {
    return false;
  }
  return FromString(i->second, out_val);
}

bool OptionsFile::SetStringValue(const std::string& option,
                                 const std::string& value) {
  LOG(LS_VERBOSE) << "OptionsFile::SetStringValue "
                  << option << ":" << value;
  if (!IsLegalName(option) || !IsLegalValue(value)) {
    return false;
  }
  options_[option] = value;
  return true;
}

bool OptionsFile::SetIntValue(const std::string& option,
                              int value) {
  LOG(LS_VERBOSE) << "OptionsFile::SetIntValue "
                  << option << ":" << value;
  if (!IsLegalName(option)) {
    return false;
  }
  return ToString(value, &options_[option]);
}

bool OptionsFile::RemoveValue(const std::string& option) {
  LOG(LS_VERBOSE) << "OptionsFile::RemoveValue " << option;
  if (!IsLegalName(option)) {
    return false;
  }
  options_.erase(option);
  return true;
}

}  // namespace talk_base
