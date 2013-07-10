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

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "talk/base/linuxfdwalk.h"

// Parses a file descriptor number in base 10, requiring the strict format used
// in /proc/*/fd. Returns the value, or -1 if not a valid string.
static int parse_fd(const char *s) {
  if (!*s) {
    // Empty string is invalid.
    return -1;
  }
  int val = 0;
  do {
    if (*s < '0' || *s > '9') {
      // Non-numeric characters anywhere are invalid.
      return -1;
    }
    int digit = *s++ - '0';
    val = val * 10 + digit;
  } while (*s);
  return val;
}

int fdwalk(void (*func)(void *, int), void *opaque) {
  DIR *dir = opendir("/proc/self/fd");
  if (!dir) {
    return -1;
  }
  int opendirfd = dirfd(dir);
  int parse_errors = 0;
  struct dirent *ent;
  // Have to clear errno to distinguish readdir() completion from failure.
  while (errno = 0, (ent = readdir(dir)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 ||
        strcmp(ent->d_name, "..") == 0) {
      continue;
    }
    // We avoid atoi or strtol because those are part of libc and they involve
    // locale stuff, which is probably not safe from a post-fork context in a
    // multi-threaded app.
    int fd = parse_fd(ent->d_name);
    if (fd < 0) {
      parse_errors = 1;
      continue;
    }
    if (fd != opendirfd) {
      (*func)(opaque, fd);
    }
  }
  int saved_errno = errno;
  if (closedir(dir) < 0) {
    if (!saved_errno) {
      // Return the closedir error.
      return -1;
    }
    // Else ignore it because we have a more relevant error to return.
  }
  if (saved_errno) {
    errno = saved_errno;
    return -1;
  } else if (parse_errors) {
    errno = EBADF;
    return -1;
  } else {
    return 0;
  }
}
