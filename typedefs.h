/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *
 * This file contains type definitions used in all WebRtc APIs.
 * 
 */

/* Reserved words definitions */
#define WEBRTC_EXTERN extern
#define G_CONST const
#define WEBRTC_INLINE extern __inline

#ifndef WEBRTC_TYPEDEFS_H
#define WEBRTC_TYPEDEFS_H

/* Define WebRtc preprocessor identifiers based on the current build platform */
#if defined(WIN32)
    // Windows & Windows Mobile
    #if !defined(WEBRTC_TARGET_PC)
        #define WEBRTC_TARGET_PC
    #endif
#elif defined(__APPLE__)
    // Mac OS X
    #if defined(__LITTLE_ENDIAN__ ) //TODO: is this used?
        #if !defined(WEBRTC_TARGET_MAC_INTEL)
            #define WEBRTC_TARGET_MAC_INTEL
        #endif  
    #else
        #if !defined(WEBRTC_TARGET_MAC)
            #define WEBRTC_TARGET_MAC
        #endif  
    #endif
#else
    // Linux etc.
    #if !defined(WEBRTC_TARGET_PC)
        #define WEBRTC_TARGET_PC
    #endif
#endif

#if defined(WEBRTC_TARGET_PC)

#if !defined(_MSC_VER)
  #include <stdint.h>
#else
    // Define C99 equivalent types.
    // Since MSVC doesn't include these headers, we have to write our own
    // version to provide a compatibility layer between MSVC and the WebRTC
    // headers.
    typedef signed char         int8_t;
    typedef signed short        int16_t;
    typedef signed int          int32_t;
    typedef signed long long    int64_t;
    typedef unsigned char       uint8_t;
    typedef unsigned short      uint16_t;
    typedef unsigned int        uint32_t;
    typedef unsigned long long  uint64_t;
#endif

#if defined(WIN32)
    typedef __int64             WebRtc_Word64;
    typedef unsigned __int64    WebRtc_UWord64;
#else
    typedef int64_t             WebRtc_Word64;
    typedef uint64_t            WebRtc_UWord64;
#endif
    typedef int32_t             WebRtc_Word32;
    typedef uint32_t            WebRtc_UWord32;
    typedef int16_t             WebRtc_Word16;
    typedef uint16_t            WebRtc_UWord16;
    typedef char                WebRtc_Word8;
    typedef uint8_t             WebRtc_UWord8;

    /* Define endian for the platform */
    #define WEBRTC_LITTLE_ENDIAN

#elif defined(WEBRTC_TARGET_MAC_INTEL)
    #include <stdint.h>

    typedef int64_t             WebRtc_Word64;
    typedef uint64_t            WebRtc_UWord64;
    typedef int32_t             WebRtc_Word32;
    typedef uint32_t            WebRtc_UWord32;
    typedef int16_t             WebRtc_Word16;
    typedef char                WebRtc_Word8;
    typedef uint16_t            WebRtc_UWord16;
    typedef uint8_t             WebRtc_UWord8;

    /* Define endian for the platform */
    #define WEBRTC_LITTLE_ENDIAN

#else

    #error "No platform defined for WebRtc type definitions (webrtc_typedefs.h)"

#endif


#endif // WEBRTC_TYPEDEFS_H
