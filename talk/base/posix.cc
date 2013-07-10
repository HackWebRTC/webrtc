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

#include "talk/base/posix.h"

#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>

#ifdef LINUX
#include "talk/base/linuxfdwalk.h"
#endif
#include "talk/base/logging.h"

namespace talk_base {

#ifdef LINUX
static void closefds(void *close_errors, int fd) {
  if (fd <= 2) {
    // We leave stdin/out/err open to the browser's terminal, if any.
    return;
  }
  if (close(fd) < 0) {
    *static_cast<bool *>(close_errors) = true;
  }
}
#endif

enum {
  EXIT_FLAG_CHDIR_ERRORS       = 1 << 0,
#ifdef LINUX
  EXIT_FLAG_FDWALK_ERRORS      = 1 << 1,
  EXIT_FLAG_CLOSE_ERRORS       = 1 << 2,
#endif
  EXIT_FLAG_SECOND_FORK_FAILED = 1 << 3,
};

bool RunAsDaemon(const char *file, const char *const argv[]) {
  // Fork intermediate child to daemonize.
  pid_t pid = fork();
  if (pid < 0) {
    LOG_ERR(LS_ERROR) << "fork()";
    return false;
  } else if (!pid) {
    // Child.

    // We try to close all fds and change directory to /, but if that fails we
    // keep going because it's not critical.
    int exit_code = 0;
    if (chdir("/") < 0) {
      exit_code |= EXIT_FLAG_CHDIR_ERRORS;
    }
#ifdef LINUX
    bool close_errors = false;
    if (fdwalk(&closefds, &close_errors) < 0) {
      exit_code |= EXIT_FLAG_FDWALK_ERRORS;
    }
    if (close_errors) {
      exit_code |= EXIT_FLAG_CLOSE_ERRORS;
    }
#endif

    // Fork again to become a daemon.
    pid = fork();
    // It is important that everything here use _exit() and not exit(), because
    // exit() would call the destructors of all global variables in the whole
    // process, which is both unnecessary and unsafe.
    if (pid < 0) {
      exit_code |= EXIT_FLAG_SECOND_FORK_FAILED;
      _exit(exit_code);  // if second fork failed
    } else if (!pid) {
      // Child.
      // Successfully daemonized. Run command.
      // POSIX requires the args to be typed as non-const for historical
      // reasons, but it mandates that the actual implementation be const, so
      // the cast is safe.
      execvp(file, const_cast<char *const *>(argv));
      _exit(255);  // if execvp failed
    }

    // Parent.
    // Successfully spawned process, but report any problems to the parent where
    // we can log them.
    _exit(exit_code);
  }

  // Parent. Reap intermediate child.
  int status;
  pid_t child = waitpid(pid, &status, 0);
  if (child < 0) {
    LOG_ERR(LS_ERROR) << "Error in waitpid()";
    return false;
  }
  if (child != pid) {
    // Should never happen (see man page).
    LOG(LS_ERROR) << "waitpid() chose wrong child???";
    return false;
  }
  if (!WIFEXITED(status)) {
    LOG(LS_ERROR) << "Intermediate child killed uncleanly";  // Probably crashed
    return false;
  }

  int exit_code = WEXITSTATUS(status);
  if (exit_code & EXIT_FLAG_CHDIR_ERRORS) {
    LOG(LS_WARNING) << "Child reported probles calling chdir()";
  }
#ifdef LINUX
  if (exit_code & EXIT_FLAG_FDWALK_ERRORS) {
    LOG(LS_WARNING) << "Child reported problems calling fdwalk()";
  }
  if (exit_code & EXIT_FLAG_CLOSE_ERRORS) {
    LOG(LS_WARNING) << "Child reported problems calling close()";
  }
#endif
  if (exit_code & EXIT_FLAG_SECOND_FORK_FAILED) {
    LOG(LS_ERROR) << "Failed to daemonize";
    // This means the command was not launched, so failure.
    return false;
  }
  return true;
}

}  // namespace talk_base
