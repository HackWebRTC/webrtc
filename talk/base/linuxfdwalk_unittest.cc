/*
 * libjingle
 * Copyright 2009, Google Inc.
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

#include <set>
#include <sstream>

#include "talk/base/gunit.h"
#include "talk/base/linuxfdwalk.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static const int kArbitraryLargeFdNumber = 424;

static void FdCheckVisitor(void *data, int fd) {
  std::set<int> *fds = static_cast<std::set<int> *>(data);
  EXPECT_EQ(1U, fds->erase(fd));
}

static void FdEnumVisitor(void *data, int fd) {
  std::set<int> *fds = static_cast<std::set<int> *>(data);
  EXPECT_TRUE(fds->insert(fd).second);
}

// Checks that the set of open fds is exactly the given list.
static void CheckOpenFdList(std::set<int> fds) {
  EXPECT_EQ(0, fdwalk(&FdCheckVisitor, &fds));
  EXPECT_EQ(0U, fds.size());
}

static void GetOpenFdList(std::set<int> *fds) {
  fds->clear();
  EXPECT_EQ(0, fdwalk(&FdEnumVisitor, fds));
}

TEST(LinuxFdWalk, TestFdWalk) {
  std::set<int> fds;
  GetOpenFdList(&fds);
  std::ostringstream str;
  // I have observed that the open set when starting a test is [0, 6]. Leaked
  // fds would change that, but so can (e.g.) running under a debugger, so we
  // can't really do an EXPECT. :(
  str << "File descriptors open in test executable:";
  for (std::set<int>::const_iterator i = fds.begin(); i != fds.end(); ++i) {
    str << " " << *i;
  }
  LOG(LS_INFO) << str.str();
  // Open some files.
  int fd1 = open("/dev/null", O_RDONLY);
  EXPECT_LE(0, fd1);
  int fd2 = open("/dev/null", O_WRONLY);
  EXPECT_LE(0, fd2);
  int fd3 = open("/dev/null", O_RDWR);
  EXPECT_LE(0, fd3);
  int fd4 = dup2(fd3, kArbitraryLargeFdNumber);
  EXPECT_LE(0, fd4);
  EXPECT_TRUE(fds.insert(fd1).second);
  EXPECT_TRUE(fds.insert(fd2).second);
  EXPECT_TRUE(fds.insert(fd3).second);
  EXPECT_TRUE(fds.insert(fd4).second);
  CheckOpenFdList(fds);
  EXPECT_EQ(0, close(fd1));
  EXPECT_EQ(0, close(fd2));
  EXPECT_EQ(0, close(fd3));
  EXPECT_EQ(0, close(fd4));
}
