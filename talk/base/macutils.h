/*
 * libjingle
 * Copyright 2007 Google Inc.
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

#ifndef TALK_BASE_MACUTILS_H__
#define TALK_BASE_MACUTILS_H__

#include <CoreFoundation/CoreFoundation.h>
#ifdef OSX
#include <Carbon/Carbon.h>
#endif
#include <string>

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////

// Note that some of these functions work for both iOS and Mac OS X.  The ones
// that are specific to Mac are #ifdef'ed as such.

bool ToUtf8(const CFStringRef str16, std::string* str8);
bool ToUtf16(const std::string& str8, CFStringRef* str16);

#ifdef OSX
void DecodeFourChar(UInt32 fc, std::string* out);

enum MacOSVersionName {
  kMacOSUnknown,       // ???
  kMacOSOlder,         // 10.2-
  kMacOSPanther,       // 10.3
  kMacOSTiger,         // 10.4
  kMacOSLeopard,       // 10.5
  kMacOSSnowLeopard,   // 10.6
  kMacOSLion,          // 10.7
  kMacOSMountainLion,  // 10.8
  kMacOSMavericks,     // 10.9
  kMacOSNewer,         // 10.10+
};

bool GetOSVersion(int* major, int* minor, int* bugfix);
MacOSVersionName GetOSVersionName();
bool GetQuickTimeVersion(std::string* version);

// Runs the given apple script. Only supports scripts that does not
// require user interaction.
bool RunAppleScript(const std::string& script);
#endif

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base

#endif  // TALK_BASE_MACUTILS_H__
