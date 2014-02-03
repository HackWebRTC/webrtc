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

#ifndef TALK_BASE_LINUX_H_
#define TALK_BASE_LINUX_H_

#if defined(LINUX) || defined(ANDROID)
#include <string>
#include <map>
#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/base/stream.h"

namespace talk_base {

//////////////////////////////////////////////////////////////////////////////
// ConfigParser parses a FileStream of an ".ini."-type format into a map.
//////////////////////////////////////////////////////////////////////////////

// Sample Usage:
//   ConfigParser parser;
//   ConfigParser::MapVector key_val_pairs;
//   if (parser.Open(inifile) && parser.Parse(&key_val_pairs)) {
//     for (int section_num=0; i < key_val_pairs.size(); ++section_num) {
//       std::string val1 = key_val_pairs[section_num][key1];
//       std::string val2 = key_val_pairs[section_num][key2];
//       // Do something with valn;
//     }
//   }

class ConfigParser {
 public:
  typedef std::map<std::string, std::string> SimpleMap;
  typedef std::vector<SimpleMap> MapVector;

  ConfigParser();
  virtual ~ConfigParser();

  virtual bool Open(const std::string& filename);
  virtual void Attach(StreamInterface* stream);
  virtual bool Parse(MapVector* key_val_pairs);
  virtual bool ParseSection(SimpleMap* key_val_pair);
  virtual bool ParseLine(std::string* key, std::string* value);

 private:
  scoped_ptr<StreamInterface> instream_;
};

//////////////////////////////////////////////////////////////////////////////
// ProcCpuInfo reads CPU info from the /proc subsystem on any *NIX platform.
//////////////////////////////////////////////////////////////////////////////

// Sample Usage:
//   ProcCpuInfo proc_info;
//   int no_of_cpu;
//   if (proc_info.LoadFromSystem()) {
//      std::string out_str;
//      proc_info.GetNumCpus(&no_of_cpu);
//      proc_info.GetCpuStringValue(0, "vendor_id", &out_str);
//      }
//   }

class ProcCpuInfo {
 public:
  ProcCpuInfo();
  virtual ~ProcCpuInfo();

  // Reads the proc subsystem's cpu info into memory. If this fails, this
  // returns false; if it succeeds, it returns true.
  virtual bool LoadFromSystem();

  // Obtains the number of logical CPU threads and places the value num.
  virtual bool GetNumCpus(int* num);

  // Obtains the number of physical CPU cores and places the value num.
  virtual bool GetNumPhysicalCpus(int* num);

  // Obtains the CPU family id.
  virtual bool GetCpuFamily(int* id);

  // Obtains the number of sections in /proc/cpuinfo, which may be greater
  // than the number of CPUs (e.g. on ARM)
  virtual bool GetSectionCount(size_t* count);

  // Looks for the CPU proc item with the given name for the given section
  // number and places the string value in result.
  virtual bool GetSectionStringValue(size_t section_num, const std::string& key,
                                     std::string* result);

  // Looks for the CPU proc item with the given name for the given section
  // number and places the int value in result.
  virtual bool GetSectionIntValue(size_t section_num, const std::string& key,
                                  int* result);

 private:
  ConfigParser::MapVector sections_;
};

#if !defined(GOOGLE_CHROME_BUILD) && !defined(CHROMIUM_BUILD)
// Builds a string containing the info from lsb_release on a single line.
std::string ReadLinuxLsbRelease();
#endif

// Returns the output of "uname".
std::string ReadLinuxUname();

// Returns the content (int) of
// /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq
// Returns -1 on error.
int ReadCpuMaxFreq();

}  // namespace talk_base

#endif  // defined(LINUX) || defined(ANDROID)
#endif  // TALK_BASE_LINUX_H_
