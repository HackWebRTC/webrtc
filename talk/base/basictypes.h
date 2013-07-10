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

#ifndef TALK_BASE_BASICTYPES_H_
#define TALK_BASE_BASICTYPES_H_

#include <stddef.h>  // for NULL, size_t

#if !(defined(_MSC_VER) && (_MSC_VER < 1600))
#include <stdint.h>  // for uintptr_t
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"  // NOLINT
#endif

#include "talk/base/constructormagic.h"

#if !defined(INT_TYPES_DEFINED)
#define INT_TYPES_DEFINED
#ifdef COMPILER_MSVC
typedef unsigned __int64 uint64;
typedef __int64 int64;
#ifndef INT64_C
#define INT64_C(x) x ## I64
#endif
#ifndef UINT64_C
#define UINT64_C(x) x ## UI64
#endif
#define INT64_F "I64"
#else  // COMPILER_MSVC
// On Mac OS X, cssmconfig.h defines uint64 as uint64_t
// TODO(fbarchard): Use long long for compatibility with chromium on BSD/OSX.
#if defined(OSX)
typedef uint64_t uint64;
typedef int64_t int64;
#ifndef INT64_C
#define INT64_C(x) x ## LL
#endif
#ifndef UINT64_C
#define UINT64_C(x) x ## ULL
#endif
#define INT64_F "l"
#elif defined(__LP64__)
typedef unsigned long uint64;  // NOLINT
typedef long int64;  // NOLINT
#ifndef INT64_C
#define INT64_C(x) x ## L
#endif
#ifndef UINT64_C
#define UINT64_C(x) x ## UL
#endif
#define INT64_F "l"
#else  // __LP64__
typedef unsigned long long uint64;  // NOLINT
typedef long long int64;  // NOLINT
#ifndef INT64_C
#define INT64_C(x) x ## LL
#endif
#ifndef UINT64_C
#define UINT64_C(x) x ## ULL
#endif
#define INT64_F "ll"
#endif  // __LP64__
#endif  // COMPILER_MSVC
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;  // NOLINT
typedef short int16;  // NOLINT
typedef unsigned char uint8;
typedef signed char int8;
#endif  // INT_TYPES_DEFINED

// Detect compiler is for x86 or x64.
#if defined(__x86_64__) || defined(_M_X64) || \
    defined(__i386__) || defined(_M_IX86)
#define CPU_X86 1
#endif
// Detect compiler is for arm.
#if defined(__arm__) || defined(_M_ARM)
#define CPU_ARM 1
#endif
#if defined(CPU_X86) && defined(CPU_ARM)
#error CPU_X86 and CPU_ARM both defined.
#endif
#if !defined(ARCH_CPU_BIG_ENDIAN) && !defined(ARCH_CPU_LITTLE_ENDIAN)
// x86, arm or GCC provided __BYTE_ORDER__ macros
#if CPU_X86 || CPU_ARM ||  \
  (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define ARCH_CPU_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ARCH_CPU_BIG_ENDIAN
#else
#error ARCH_CPU_BIG_ENDIAN or ARCH_CPU_LITTLE_ENDIAN should be defined.
#endif
#endif
#if defined(ARCH_CPU_BIG_ENDIAN) && defined(ARCH_CPU_LITTLE_ENDIAN)
#error ARCH_CPU_BIG_ENDIAN and ARCH_CPU_LITTLE_ENDIAN both defined.
#endif

#ifdef WIN32
typedef int socklen_t;
#endif

// The following only works for C++
#ifdef __cplusplus
namespace talk_base {
  template<class T> inline T _min(T a, T b) { return (a > b) ? b : a; }
  template<class T> inline T _max(T a, T b) { return (a < b) ? b : a; }

  // For wait functions that take a number of milliseconds, kForever indicates
  // unlimited time.
  const int kForever = -1;
}

#define ALIGNP(p, t) \
    (reinterpret_cast<uint8*>(((reinterpret_cast<uintptr_t>(p) + \
    ((t) - 1)) & ~((t) - 1))))
#define IS_ALIGNED(p, a) (!((uintptr_t)(p) & ((a) - 1)))

// Note: UNUSED is also defined in common.h
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

// Use these to declare and define a static local variable (static T;) so that
// it is leaked so that its destructors are not called at exit.
#define LIBJINGLE_DEFINE_STATIC_LOCAL(type, name, arguments) \
  static type& name = *new type arguments

#endif  // __cplusplus
#endif  // TALK_BASE_BASICTYPES_H_
