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

#include "talk/base/urlencode.h"

#include "talk/base/common.h"
#include "talk/base/stringutils.h"

static int HexPairValue(const char * code) {
  int value = 0;
  const char * pch = code;
  for (;;) {
    int digit = *pch++;
    if (digit >= '0' && digit <= '9') {
      value += digit - '0';
    }
    else if (digit >= 'A' && digit <= 'F') {
      value += digit - 'A' + 10;
    }
    else if (digit >= 'a' && digit <= 'f') {
      value += digit - 'a' + 10;
    }
    else {
      return -1;
    }
    if (pch == code + 2)
      return value;
    value <<= 4;
  }
}

int InternalUrlDecode(const char *source, char *dest,
                      bool encode_space_as_plus) {
  char * start = dest;

  while (*source) {
    switch (*source) {
    case '+':
      if (encode_space_as_plus) {
        *(dest++) = ' ';
      } else {
        *dest++ = *source;
      }
      break;
    case '%':
      if (source[1] && source[2]) {
        int value = HexPairValue(source + 1);
        if (value >= 0) {
          *(dest++) = value;
          source += 2;
        }
        else {
          *dest++ = '?';
        }
      }
      else {
        *dest++ = '?';
      }
      break;
    default:
      *dest++ = *source;
    }
    source++;
  }

  *dest = 0;
  return static_cast<int>(dest - start);
}

int UrlDecode(const char *source, char *dest) {
  return InternalUrlDecode(source, dest, true);
}

int UrlDecodeWithoutEncodingSpaceAsPlus(const char *source, char *dest) {
  return InternalUrlDecode(source, dest, false);
}

bool IsValidUrlChar(char ch, bool unsafe_only) {
  if (unsafe_only) {
    return !(ch <= ' ' || strchr("\\\"^&`<>[]{}", ch));
  } else {
    return isalnum(ch) || strchr("-_.!~*'()", ch);
  }
}

int InternalUrlEncode(const char *source, char *dest, unsigned int max,
                      bool encode_space_as_plus, bool unsafe_only) {
  static const char *digits = "0123456789ABCDEF";
  if (max == 0) {
    return 0;
  }

  char *start = dest;
  while (static_cast<unsigned>(dest - start) < max && *source) {
    unsigned char ch = static_cast<unsigned char>(*source);
    if (*source == ' ' && encode_space_as_plus && !unsafe_only) {
      *dest++ = '+';
    } else if (IsValidUrlChar(ch, unsafe_only)) {
      *dest++ = *source;
    } else {
      if (static_cast<unsigned>(dest - start) + 4 > max) {
        break;
      }
      *dest++ = '%';
      *dest++ = digits[(ch >> 4) & 0x0F];
      *dest++ = digits[       ch & 0x0F];
    }
    source++;
  }
  ASSERT(static_cast<unsigned int>(dest - start) < max);
  *dest = 0;

  return static_cast<int>(dest - start);
}

int UrlEncode(const char *source, char *dest, unsigned max) {
  return InternalUrlEncode(source, dest, max, true, false);
}

int UrlEncodeWithoutEncodingSpaceAsPlus(const char *source, char *dest,
                                        unsigned max) {
  return InternalUrlEncode(source, dest, max, false, false);
}

int UrlEncodeOnlyUnsafeChars(const char *source, char *dest, unsigned max) {
  return InternalUrlEncode(source, dest, max, false, true);
}

std::string
InternalUrlDecodeString(const std::string & encoded,
                        bool encode_space_as_plus) {
  size_t needed_length = encoded.length() + 1;
  char* buf = STACK_ARRAY(char, needed_length);
  InternalUrlDecode(encoded.c_str(), buf, encode_space_as_plus);
  return buf;
}

std::string
UrlDecodeString(const std::string & encoded) {
  return InternalUrlDecodeString(encoded, true);
}

std::string
UrlDecodeStringWithoutEncodingSpaceAsPlus(const std::string & encoded) {
  return InternalUrlDecodeString(encoded, false);
}

std::string
InternalUrlEncodeString(const std::string & decoded,
                        bool encode_space_as_plus,
                        bool unsafe_only) {
  int needed_length = static_cast<int>(decoded.length()) * 3 + 1;
  char* buf = STACK_ARRAY(char, needed_length);
  InternalUrlEncode(decoded.c_str(), buf, needed_length,
                    encode_space_as_plus, unsafe_only);
  return buf;
}

std::string
UrlEncodeString(const std::string & decoded) {
  return InternalUrlEncodeString(decoded, true, false);
}

std::string
UrlEncodeStringWithoutEncodingSpaceAsPlus(const std::string & decoded) {
  return InternalUrlEncodeString(decoded, false, false);
}

std::string
UrlEncodeStringForOnlyUnsafeChars(const std::string & decoded) {
  return InternalUrlEncodeString(decoded, false, true);
}
