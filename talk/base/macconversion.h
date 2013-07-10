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

#ifndef TALK_BASE_MACCONVERSION_H_
#define TALK_BASE_MACCONVERSION_H_

#ifdef OSX

#include <CoreFoundation/CoreFoundation.h>

#include <string>

// given a CFStringRef, attempt to convert it to a C++ string.
// returns true if it succeeds, false otherwise.
// We can safely assume, given our context, that the string is
// going to be in ASCII, because it will either be an IP address,
// or a domain name, which is guaranteed to be ASCII-representable.
bool p_convertHostCFStringRefToCPPString(const CFStringRef cfstr,
                                         std::string& cppstr);

// Convert the CFNumber to an integer, putting the integer in the location
// given, and returhing true, if the conversion succeeds.
// If given a NULL or a non-CFNumber, returns false.
// This is pretty aggresive about trying to convert to int.
bool p_convertCFNumberToInt(CFNumberRef cfn, int* i);

// given a CFNumberRef, determine if it represents a true value.
bool p_isCFNumberTrue(CFNumberRef cfn);

#endif  // OSX

#endif  // TALK_BASE_MACCONVERSION_H_
