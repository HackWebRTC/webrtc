/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
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

#ifndef TALK_BASE_GUNIT_H_
#define TALK_BASE_GUNIT_H_

#include "talk/base/logging.h"
#include "talk/base/thread.h"
#if defined(ANDROID) || defined(GTEST_RELATIVE_PATH)
#include "gtest/gtest.h"
#else
#include "testing/base/public/gunit.h"
#endif

// forward declarations
namespace talk_base {
class Pathname;
}

// Wait until "ex" is true, or "timeout" expires.
#define WAIT(ex, timeout) \
  for (uint32 start = talk_base::Time(); \
      !(ex) && talk_base::Time() < start + timeout;) \
    talk_base::Thread::Current()->ProcessMessages(1);

// This returns the result of the test in res, so that we don't re-evaluate
// the expression in the XXXX_WAIT macros below, since that causes problems
// when the expression is only true the first time you check it.
#define WAIT_(ex, timeout, res) \
  do { \
    uint32 start = talk_base::Time(); \
    res = (ex); \
    while (!res && talk_base::Time() < start + timeout) { \
      talk_base::Thread::Current()->ProcessMessages(1); \
      res = (ex); \
    } \
  } while (0);

// The typical EXPECT_XXXX and ASSERT_XXXXs, but done until true or a timeout.
#define EXPECT_TRUE_WAIT(ex, timeout) \
  do { \
    bool res; \
    WAIT_(ex, timeout, res); \
    if (!res) EXPECT_TRUE(ex); \
  } while (0);

#define EXPECT_EQ_WAIT(v1, v2, timeout) \
  do { \
    bool res; \
    WAIT_(v1 == v2, timeout, res); \
    if (!res) EXPECT_EQ(v1, v2); \
  } while (0);

#define ASSERT_TRUE_WAIT(ex, timeout) \
  do { \
    bool res; \
    WAIT_(ex, timeout, res); \
    if (!res) ASSERT_TRUE(ex); \
  } while (0);

#define ASSERT_EQ_WAIT(v1, v2, timeout) \
  do { \
    bool res; \
    WAIT_(v1 == v2, timeout, res); \
    if (!res) ASSERT_EQ(v1, v2); \
  } while (0);

// Version with a "soft" timeout and a margin. This logs if the timeout is
// exceeded, but it only fails if the expression still isn't true after the
// margin time passes.
#define EXPECT_TRUE_WAIT_MARGIN(ex, timeout, margin) \
  do { \
    bool res; \
    WAIT_(ex, timeout, res); \
    if (res) { \
      break; \
    } \
    LOG(LS_WARNING) << "Expression " << #ex << " still not true after " << \
        timeout << "ms; waiting an additional " << margin << "ms"; \
    WAIT_(ex, margin, res); \
    if (!res) { \
      EXPECT_TRUE(ex); \
    } \
  } while (0);

talk_base::Pathname GetTalkDirectory();

#endif  // TALK_BASE_GUNIT_H_
