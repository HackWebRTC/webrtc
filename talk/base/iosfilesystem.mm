/*
 * libjingle
 * Copyright 2014 Google Inc.
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

// This file only exists because various iOS system APIs are only
// available from Objective-C.  See unixfilesystem.cc for the only use
// (enforced by a lack of a header file).

#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSProcessInfo.h>
#include <string.h>

#include "talk/base/common.h"
#include "talk/base/pathutils.h"

// Return a new[]'d |char*| copy of the UTF8 representation of |s|.
// Caller owns the returned memory and must use delete[] on it.
static char* copyString(NSString* s) {
  const char* utf8 = [s UTF8String];
  size_t len = strlen(utf8) + 1;
  char* copy = new char[len];
  // This uses a new[] + strcpy (instead of strdup) because the
  // receiver expects to be able to delete[] the returned pointer
  // (instead of free()ing it).
  strcpy(copy, utf8);
  return copy;
}

// Return a (leaked) copy of a directory name suitable for application data.
char* IOSDataDirectory() {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSUserDomainMask, YES);
  ASSERT([paths count] == 1);
  return copyString([paths objectAtIndex:0]);
}

// Return a (leaked) copy of a directory name suitable for use as a $TEMP.
char* IOSTempDirectory() {
  return copyString(NSTemporaryDirectory());
}

// Return the binary's path.
void IOSAppName(talk_base::Pathname* path) {
  NSProcessInfo *pInfo = [NSProcessInfo processInfo];
  NSString* argv0 = [[pInfo arguments] objectAtIndex:0];
  path->SetPathname([argv0 UTF8String]);
}
