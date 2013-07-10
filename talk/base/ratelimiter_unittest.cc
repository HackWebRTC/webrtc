/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/base/gunit.h"
#include "talk/base/ratelimiter.h"

namespace talk_base {

TEST(RateLimiterTest, TestCanUse) {
  // Diet: Can eat 2,000 calories per day.
  RateLimiter limiter = RateLimiter(2000, 1.0);

  double monday = 1.0;
  double tuesday = 2.0;
  double thursday = 4.0;

  EXPECT_TRUE(limiter.CanUse(0, monday));
  EXPECT_TRUE(limiter.CanUse(1000, monday));
  EXPECT_TRUE(limiter.CanUse(1999, monday));
  EXPECT_TRUE(limiter.CanUse(2000, monday));
  EXPECT_FALSE(limiter.CanUse(2001, monday));

  limiter.Use(1000, monday);

  EXPECT_TRUE(limiter.CanUse(0, monday));
  EXPECT_TRUE(limiter.CanUse(999, monday));
  EXPECT_TRUE(limiter.CanUse(1000, monday));
  EXPECT_FALSE(limiter.CanUse(1001, monday));

  limiter.Use(1000, monday);

  EXPECT_TRUE(limiter.CanUse(0, monday));
  EXPECT_FALSE(limiter.CanUse(1, monday));

  EXPECT_TRUE(limiter.CanUse(0, tuesday));
  EXPECT_TRUE(limiter.CanUse(1, tuesday));
  EXPECT_TRUE(limiter.CanUse(1999, tuesday));
  EXPECT_TRUE(limiter.CanUse(2000, tuesday));
  EXPECT_FALSE(limiter.CanUse(2001, tuesday));

  limiter.Use(1000, tuesday);

  EXPECT_TRUE(limiter.CanUse(1000, tuesday));
  EXPECT_FALSE(limiter.CanUse(1001, tuesday));

  limiter.Use(1000, thursday);

  EXPECT_TRUE(limiter.CanUse(1000, tuesday));
  EXPECT_FALSE(limiter.CanUse(1001, tuesday));
}

}  // namespace talk_base
