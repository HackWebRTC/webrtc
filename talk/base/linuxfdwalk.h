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

#ifndef TALK_BASE_LINUXFDWALK_H_
#define TALK_BASE_LINUXFDWALK_H_

#ifdef __cplusplus
extern "C" {
#endif

// Linux port of SunOS's fdwalk(3) call. It loops over all open file descriptors
// and calls func on each one. Additionally, it is safe to use from the child
// of a fork that hasn't exec'ed yet, so you can use it to close all open file
// descriptors prior to exec'ing a daemon.
// The return value is 0 if successful, or else -1 and errno is set. The
// possible errors include any error that can be returned by opendir(),
// readdir(), or closedir(), plus EBADF if there are problems parsing the
// contents of /proc/self/fd.
// The file descriptors that are enumerated will not include the file descriptor
// used for the enumeration itself.
int fdwalk(void (*func)(void *, int), void *opaque);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // TALK_BASE_LINUXFDWALK_H_
