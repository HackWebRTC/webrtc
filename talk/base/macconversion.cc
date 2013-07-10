/*
 * libjingle
 * Copyright 2004--2009, Google Inc.
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

#ifdef OSX

#include <CoreFoundation/CoreFoundation.h>

#include "talk/base/logging.h"
#include "talk/base/macconversion.h"

bool p_convertHostCFStringRefToCPPString(
  const CFStringRef cfstr, std::string& cppstr) {
  bool result = false;

  // First this must be non-null,
  if (NULL != cfstr) {
    // it must actually *be* a CFString, and not something just masquerading
    // as one,
    if (CFGetTypeID(cfstr) == CFStringGetTypeID()) {
      // and we must be able to get the characters out of it.
      // (The cfstr owns this buffer; it came from somewhere else,
      // so someone else gets to take care of getting rid of the cfstr,
      // and then this buffer will go away automatically.)
      unsigned length = CFStringGetLength(cfstr);
      char* buf = new char[1 + length];
      if (CFStringGetCString(cfstr, buf, 1 + length, kCFStringEncodingASCII)) {
        if (strlen(buf) == length) {
          cppstr.assign(buf);
          result = true;
        }
      }
      delete [] buf;
    }
  }

  return result;
}

bool p_convertCFNumberToInt(CFNumberRef cfn, int* i) {
  bool converted = false;

  // It must not be null.
  if (NULL != cfn) {
    // It must actually *be* a CFNumber and not something just masquerading
    // as one.
    if (CFGetTypeID(cfn) == CFNumberGetTypeID()) {
      CFNumberType ntype = CFNumberGetType(cfn);
      switch (ntype) {
        case kCFNumberSInt8Type:
          SInt8 sint8;
          converted = CFNumberGetValue(cfn, ntype, static_cast<void*>(&sint8));
          if (converted) *i = static_cast<int>(sint8);
          break;
        case kCFNumberSInt16Type:
          SInt16 sint16;
          converted = CFNumberGetValue(cfn, ntype, static_cast<void*>(&sint16));
          if (converted) *i = static_cast<int>(sint16);
          break;
        case kCFNumberSInt32Type:
          SInt32 sint32;
          converted = CFNumberGetValue(cfn, ntype, static_cast<void*>(&sint32));
          if (converted) *i = static_cast<int>(sint32);
          break;
        case kCFNumberSInt64Type:
          SInt64 sint64;
          converted = CFNumberGetValue(cfn, ntype, static_cast<void*>(&sint64));
          if (converted) *i = static_cast<int>(sint64);
          break;
        case kCFNumberFloat32Type:
          Float32 float32;
          converted = CFNumberGetValue(cfn, ntype,
                                       static_cast<void*>(&float32));
          if (converted) *i = static_cast<int>(float32);
          break;
        case kCFNumberFloat64Type:
          Float64 float64;
          converted = CFNumberGetValue(cfn, ntype,
                                       static_cast<void*>(&float64));
          if (converted) *i = static_cast<int>(float64);
          break;
        case kCFNumberCharType:
          char charvalue;
          converted = CFNumberGetValue(cfn, ntype,
                                       static_cast<void*>(&charvalue));
          if (converted) *i = static_cast<int>(charvalue);
          break;
        case kCFNumberShortType:
          short shortvalue;
          converted = CFNumberGetValue(cfn, ntype,
                                       static_cast<void*>(&shortvalue));
          if (converted) *i = static_cast<int>(shortvalue);
          break;
        case kCFNumberIntType:
          int intvalue;
          converted = CFNumberGetValue(cfn, ntype,
                                       static_cast<void*>(&intvalue));
          if (converted) *i = static_cast<int>(intvalue);
          break;
        case kCFNumberLongType:
          long longvalue;
          converted = CFNumberGetValue(cfn, ntype,
                     static_cast<void*>(&longvalue));
          if (converted) *i = static_cast<int>(longvalue);
          break;
        case kCFNumberLongLongType:
          long long llvalue;
          converted = CFNumberGetValue(cfn, ntype,
                                       static_cast<void*>(&llvalue));
          if (converted) *i = static_cast<int>(llvalue);
          break;
        case kCFNumberFloatType:
          float floatvalue;
          converted = CFNumberGetValue(cfn, ntype,
                                       static_cast<void*>(&floatvalue));
          if (converted) *i = static_cast<int>(floatvalue);
          break;
        case kCFNumberDoubleType:
          double doublevalue;
          converted = CFNumberGetValue(cfn, ntype,
                                       static_cast<void*>(&doublevalue));
          if (converted) *i = static_cast<int>(doublevalue);
          break;
        case kCFNumberCFIndexType:
          CFIndex cfindex;
          converted = CFNumberGetValue(cfn, ntype,
                                       static_cast<void*>(&cfindex));
          if (converted) *i = static_cast<int>(cfindex);
          break;
        default:
          LOG(LS_ERROR) << "got unknown type.";
          break;
      }
    }
  }

  return converted;
}

bool p_isCFNumberTrue(CFNumberRef cfn) {
  // We assume it's false until proven otherwise.
  bool result = false;
  int asInt;
  bool converted = p_convertCFNumberToInt(cfn, &asInt);

  if (converted && (0 != asInt)) {
    result = true;
  }

  return result;
}

#endif  // OSX
