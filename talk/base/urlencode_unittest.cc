/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/base/common.h"
#include "talk/base/gunit.h"
#include "talk/base/thread.h"
#include "talk/base/urlencode.h"

TEST(Urlencode, SourceTooLong) {
  char source[] = "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"
      "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^";
  char dest[1];
  ASSERT_EQ(0, UrlEncode(source, dest, ARRAY_SIZE(dest)));
  ASSERT_EQ('\0', dest[0]);

  dest[0] = 'a';
  ASSERT_EQ(0, UrlEncode(source, dest, 0));
  ASSERT_EQ('a', dest[0]);
}

TEST(Urlencode, OneCharacterConversion) {
  char source[] = "^";
  char dest[4];
  ASSERT_EQ(3, UrlEncode(source, dest, ARRAY_SIZE(dest)));
  ASSERT_STREQ("%5E", dest);
}

TEST(Urlencode, ShortDestinationNoEncoding) {
  // In this case we have a destination that would not be
  // big enough to hold an encoding but is big enough to
  // hold the text given.
  char source[] = "aa";
  char dest[3];
  ASSERT_EQ(2, UrlEncode(source, dest, ARRAY_SIZE(dest)));
  ASSERT_STREQ("aa", dest);
}

TEST(Urlencode, ShortDestinationEncoding) {
  // In this case we have a destination that is not
  // big enough to hold the encoding.
  char source[] = "&";
  char dest[3];
  ASSERT_EQ(0, UrlEncode(source, dest, ARRAY_SIZE(dest)));
  ASSERT_EQ('\0', dest[0]);
}

TEST(Urlencode, Encoding1) {
  char source[] = "A^ ";
  char dest[8];
  ASSERT_EQ(5, UrlEncode(source, dest, ARRAY_SIZE(dest)));
  ASSERT_STREQ("A%5E+", dest);
}

TEST(Urlencode, Encoding2) {
  char source[] = "A^ ";
  char dest[8];
  ASSERT_EQ(7, UrlEncodeWithoutEncodingSpaceAsPlus(source, dest,
                                                   ARRAY_SIZE(dest)));
  ASSERT_STREQ("A%5E%20", dest);
}

TEST(Urldecode, Decoding1) {
  char source[] = "A%5E+";
  char dest[8];
  ASSERT_EQ(3, UrlDecode(source, dest));
  ASSERT_STREQ("A^ ", dest);
}

TEST(Urldecode, Decoding2) {
  char source[] = "A%5E+";
  char dest[8];
  ASSERT_EQ(3, UrlDecodeWithoutEncodingSpaceAsPlus(source, dest));
  ASSERT_STREQ("A^+", dest);
}
