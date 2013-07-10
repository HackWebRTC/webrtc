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

#ifndef _URLENCODE_H_
#define _URLENCODE_H_ 

#include <string>

// Decode all encoded characters. Also decode + as space.
int UrlDecode(const char *source, char *dest);

// Decode all encoded characters.
int UrlDecodeWithoutEncodingSpaceAsPlus(const char *source, char *dest);

// Encode all characters except alphas, numbers, and -_.!~*'()
// Also encode space as +.
int UrlEncode(const char *source, char *dest, unsigned max);

// Encode all characters except alphas, numbers, and -_.!~*'()
int UrlEncodeWithoutEncodingSpaceAsPlus(const char *source, char *dest,
                                        unsigned max);

// Encode only unsafe chars, including \ "^&`<>[]{}
// Also encode space as %20, instead of +
int UrlEncodeOnlyUnsafeChars(const char *source, char *dest, unsigned max);

std::string UrlDecodeString(const std::string & encoded);
std::string UrlDecodeStringWithoutEncodingSpaceAsPlus(
    const std::string & encoded);
std::string UrlEncodeString(const std::string & decoded);
std::string UrlEncodeStringWithoutEncodingSpaceAsPlus(
    const std::string & decoded);
std::string UrlEncodeStringForOnlyUnsafeChars(const std::string & decoded);

#endif

