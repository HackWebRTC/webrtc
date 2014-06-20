/*
 * libjingle
 * Copyright 2004--2013, Google Inc.
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
#ifndef TALK_MEDIA_WEBRTC_WEBRTCEXPORT_H_
#define TALK_MEDIA_WEBRTC_WEBRTCEXPORT_H_

#if !defined(GOOGLE_CHROME_BUILD) && !defined(CHROMIUM_BUILD)
#define LIBPEERCONNECTION_LIB 1
#endif

#ifndef NON_EXPORTED_BASE
#ifdef WIN32

// MSVC_SUPPRESS_WARNING disables warning |n| for the remainder of the line and
// for the next line of the source file.
#define MSVC_SUPPRESS_WARNING(n) __pragma(warning(suppress:n))

// Allows exporting a class that inherits from a non-exported base class.
// This uses suppress instead of push/pop because the delimiter after the
// declaration (either "," or "{") has to be placed before the pop macro.
//
// Example usage:
// class EXPORT_API Foo : NON_EXPORTED_BASE(public Bar) {
//
// MSVC Compiler warning C4275:
// non dll-interface class 'Bar' used as base for dll-interface class 'Foo'.
// Note that this is intended to be used only when no access to the base class'
// static data is done through derived classes or inline methods. For more info,
// see http://msdn.microsoft.com/en-us/library/3tdb471s(VS.80).aspx
#define NON_EXPORTED_BASE(code) MSVC_SUPPRESS_WARNING(4275) \
                                code

#else  // Not WIN32
#define NON_EXPORTED_BASE(code) code
#endif  // WIN32
#endif  // NON_EXPORTED_BASE

#if defined (LIBPEERCONNECTION_LIB)
  #define WRME_EXPORT
#else
  #if defined(WIN32)
    #if defined(LIBPEERCONNECTION_IMPLEMENTATION)
      #define WRME_EXPORT __declspec(dllexport)
    #else
      #define WRME_EXPORT __declspec(dllimport)
    #endif
  #else // defined(WIN32)
    #if defined(LIBPEERCONNECTION_IMPLEMENTATION)
      #define WRME_EXPORT __attribute__((visibility("default")))
    #else
      #define WRME_EXPORT
    #endif
  #endif
#endif  // LIBPEERCONNECTION_LIB

#endif  // TALK_MEDIA_WEBRTC_WEBRTCEXPORT_H_
