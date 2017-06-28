/*
 * This is the header file for the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 *
 */

// Changes(fbarchard): Ported to C++ and Google style guide.
// Made context first parameter in MD5Final for consistency with Sha1.
// Changes(hellner): added rtc namespace
// Changes(pbos): Reverted types back to uint32(8)_t with _t suffix.

#ifndef WEBRTC_BASE_MD5_H_
#define WEBRTC_BASE_MD5_H_


// This header is deprecated and is just left here temporarily during
// refactoring. See https://bugs.webrtc.org/7634 for more details.
#include "webrtc/rtc_base/md5.h"

#endif  // WEBRTC_BASE_MD5_H_
