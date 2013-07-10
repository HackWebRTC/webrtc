/*
 * libjingle
 * Copyright 2007, Google Inc.
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

#include "talk/base/pathutils.h"
#include "talk/base/gunit.h"

TEST(Pathname, ReturnsDotForEmptyPathname) {
  const std::string kCWD =
      std::string(".") + talk_base::Pathname::DefaultFolderDelimiter();

  talk_base::Pathname path("/", "");
  EXPECT_FALSE(path.empty());
  EXPECT_FALSE(path.folder().empty());
  EXPECT_TRUE (path.filename().empty());
  EXPECT_FALSE(path.pathname().empty());
  EXPECT_EQ(std::string("/"), path.pathname());

  path.SetPathname("", "foo");
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE (path.folder().empty());
  EXPECT_FALSE(path.filename().empty());
  EXPECT_FALSE(path.pathname().empty());
  EXPECT_EQ(std::string("foo"), path.pathname());

  path.SetPathname("", "");
  EXPECT_TRUE (path.empty());
  EXPECT_TRUE (path.folder().empty());
  EXPECT_TRUE (path.filename().empty());
  EXPECT_FALSE(path.pathname().empty());
  EXPECT_EQ(kCWD, path.pathname());

  path.SetPathname(kCWD, "");
  EXPECT_FALSE(path.empty());
  EXPECT_FALSE(path.folder().empty());
  EXPECT_TRUE (path.filename().empty());
  EXPECT_FALSE(path.pathname().empty());
  EXPECT_EQ(kCWD, path.pathname());

  talk_base::Pathname path2("c:/foo bar.txt");
  EXPECT_EQ(path2.url(), std::string("file:///c:/foo%20bar.txt"));
}
