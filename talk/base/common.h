/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_BASE_COMMON_H_  // NOLINT
#define TALK_BASE_COMMON_H_

#include "talk/base/basictypes.h"
#include "talk/base/constructormagic.h"

#if defined(_MSC_VER)
// warning C4355: 'this' : used in base member initializer list
#pragma warning(disable:4355)
#endif

//////////////////////////////////////////////////////////////////////
// General Utilities
//////////////////////////////////////////////////////////////////////

// Note: UNUSED is also defined in basictypes.h
#ifndef UNUSED
#define UNUSED(x) Unused(static_cast<const void*>(&x))
#define UNUSED2(x, y) Unused(static_cast<const void*>(&x)); \
    Unused(static_cast<const void*>(&y))
#define UNUSED3(x, y, z) Unused(static_cast<const void*>(&x)); \
    Unused(static_cast<const void*>(&y)); \
    Unused(static_cast<const void*>(&z))
#define UNUSED4(x, y, z, a) Unused(static_cast<const void*>(&x)); \
    Unused(static_cast<const void*>(&y)); \
    Unused(static_cast<const void*>(&z)); \
    Unused(static_cast<const void*>(&a))
#define UNUSED5(x, y, z, a, b) Unused(static_cast<const void*>(&x)); \
    Unused(static_cast<const void*>(&y)); \
    Unused(static_cast<const void*>(&z)); \
    Unused(static_cast<const void*>(&a)); \
    Unused(static_cast<const void*>(&b))
inline void Unused(const void*) {}
#endif  // UNUSED

#ifndef WIN32

#ifndef strnicmp
#define strnicmp(x, y, n) strncasecmp(x, y, n)
#endif

#ifndef stricmp
#define stricmp(x, y) strcasecmp(x, y)
#endif

// TODO(fbarchard): Remove this. std::max should be used everywhere in the code.
// NOMINMAX must be defined where we include <windows.h>.
#define stdmax(x, y) std::max(x, y)
#else
#define stdmax(x, y) talk_base::_max(x, y)
#endif

#define ARRAY_SIZE(x) (static_cast<int>(sizeof(x) / sizeof(x[0])))

/////////////////////////////////////////////////////////////////////////////
// Assertions
/////////////////////////////////////////////////////////////////////////////

#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG _DEBUG
#endif  // !defined(ENABLE_DEBUG)

// Even for release builds, allow for the override of LogAssert. Though no
// macro is provided, this can still be used for explicit runtime asserts
// and allow applications to override the assert behavior.

namespace talk_base {


// If a debugger is attached, triggers a debugger breakpoint. If a debugger is
// not attached, forces program termination.
void Break();

// LogAssert writes information about an assertion to the log. It's called by
// Assert (and from the ASSERT macro in debug mode) before any other action
// is taken (e.g. breaking the debugger, abort()ing, etc.).
void LogAssert(const char* function, const char* file, int line,
               const char* expression);

typedef void (*AssertLogger)(const char* function,
                             const char* file,
                             int line,
                             const char* expression);

// Sets a custom assert logger to be used instead of the default LogAssert
// behavior. To clear the custom assert logger, pass NULL for |logger| and the
// default behavior will be restored. Only one custom assert logger can be set
// at a time, so this should generally be set during application startup and
// only by one component.
void SetCustomAssertLogger(AssertLogger logger);

}  // namespace talk_base

#if ENABLE_DEBUG

namespace talk_base {

inline bool Assert(bool result, const char* function, const char* file,
                   int line, const char* expression) {
  if (!result) {
    LogAssert(function, file, line, expression);
    Break();
    return false;
  }
  return true;
}

}  // namespace talk_base

#if defined(_MSC_VER) && _MSC_VER < 1300
#define __FUNCTION__ ""
#endif

#ifndef ASSERT
#define ASSERT(x) \
    (void)talk_base::Assert((x), __FUNCTION__, __FILE__, __LINE__, #x)
#endif

#ifndef VERIFY
#define VERIFY(x) talk_base::Assert((x), __FUNCTION__, __FILE__, __LINE__, #x)
#endif

#else  // !ENABLE_DEBUG

namespace talk_base {

inline bool ImplicitCastToBool(bool result) { return result; }

}  // namespace talk_base

#ifndef ASSERT
#define ASSERT(x) (void)0
#endif

#ifndef VERIFY
#define VERIFY(x) talk_base::ImplicitCastToBool(x)
#endif

#endif  // !ENABLE_DEBUG

#define COMPILE_TIME_ASSERT(expr)       char CTA_UNIQUE_NAME[expr]
#define CTA_UNIQUE_NAME                 CTA_MAKE_NAME(__LINE__)
#define CTA_MAKE_NAME(line)             CTA_MAKE_NAME2(line)
#define CTA_MAKE_NAME2(line)            constraint_ ## line

// Forces compiler to inline, even against its better judgement. Use wisely.
#if defined(__GNUC__)
#define FORCE_INLINE __attribute__((always_inline))
#elif defined(WIN32)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE
#endif

// Borrowed from Chromium's base/compiler_specific.h.
// Annotate a virtual method indicating it must be overriding a virtual
// method in the parent class.
// Use like:
//   virtual void foo() OVERRIDE;
#if defined(WIN32)
#define OVERRIDE override
#elif defined(__clang__)
// Clang defaults to C++03 and warns about using override. Squelch that.
// Intentionally no push/pop here so all users of OVERRIDE ignore the warning
// too. This is like passing -Wno-c++11-extensions, except that GCC won't die
// (because it won't see this pragma).
#pragma clang diagnostic ignored "-Wc++11-extensions"
#define OVERRIDE override
#elif defined(__GNUC__) && __cplusplus >= 201103 && \
    (__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40700
// GCC 4.7 supports explicit virtual overrides when C++11 support is enabled.
#define OVERRIDE override
#else
#define OVERRIDE
#endif

// Annotate a function indicating the caller must examine the return value.
// Use like:
//   int foo() WARN_UNUSED_RESULT;
// To explicitly ignore a result, see |ignore_result()| in <base/basictypes.h>.
// TODO(ajm): Hack to avoid multiple definitions until the base/ of webrtc and
// libjingle are merged.
#if !defined(WARN_UNUSED_RESULT)
#if defined(__GNUC__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define WARN_UNUSED_RESULT
#endif
#endif  // WARN_UNUSED_RESULT

#endif  // TALK_BASE_COMMON_H_    // NOLINT
