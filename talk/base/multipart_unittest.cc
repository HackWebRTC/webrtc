/*
 * libjingle
 * Copyright 2010, Google Inc.
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

#include <string>

#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/pathutils.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/multipart.h"

namespace talk_base {

static const std::string kTestMultipartBoundary = "123456789987654321";
static const std::string kTestContentType =
    "multipart/form-data; boundary=123456789987654321";
static const char kTestData[] = "This is a test.";
static const char kTestStreamContent[] = "This is a test stream.";

TEST(MultipartTest, TestBasicOperations) {
  MultipartStream multipart("multipart/form-data", kTestMultipartBoundary);
  std::string content_type;
  multipart.GetContentType(&content_type);
  EXPECT_EQ(kTestContentType, content_type);

  EXPECT_EQ(talk_base::SS_OPENING, multipart.GetState());

  // The multipart stream contains only --boundary--\r\n
  size_t end_part_size = multipart.GetEndPartSize();
  multipart.EndParts();
  EXPECT_EQ(talk_base::SS_OPEN, multipart.GetState());
  size_t size;
  EXPECT_TRUE(multipart.GetSize(&size));
  EXPECT_EQ(end_part_size, size);

  // Write is not supported.
  EXPECT_EQ(talk_base::SR_ERROR,
            multipart.Write(kTestData, sizeof(kTestData), NULL, NULL));

  multipart.Close();
  EXPECT_EQ(talk_base::SS_CLOSED, multipart.GetState());
  EXPECT_TRUE(multipart.GetSize(&size));
  EXPECT_EQ(0U, size);
}

TEST(MultipartTest, TestAddAndRead) {
  MultipartStream multipart("multipart/form-data", kTestMultipartBoundary);

  size_t part_size =
      multipart.GetPartSize(kTestData, "form-data; name=\"text\"", "text");
  EXPECT_TRUE(multipart.AddPart(kTestData, "form-data; name=\"text\"", "text"));
  size_t size;
  EXPECT_TRUE(multipart.GetSize(&size));
  EXPECT_EQ(part_size, size);

  talk_base::MemoryStream* stream =
      new talk_base::MemoryStream(kTestStreamContent);
  size_t stream_size = 0;
  EXPECT_TRUE(stream->GetSize(&stream_size));
  part_size +=
      multipart.GetPartSize("", "form-data; name=\"stream\"", "stream");
  part_size += stream_size;

  EXPECT_TRUE(multipart.AddPart(
      new talk_base::MemoryStream(kTestStreamContent),
      "form-data; name=\"stream\"",
      "stream"));
  EXPECT_TRUE(multipart.GetSize(&size));
  EXPECT_EQ(part_size, size);

  // In adding state, block read.
  char buffer[1024];
  EXPECT_EQ(talk_base::SR_BLOCK,
            multipart.Read(buffer, sizeof(buffer), NULL, NULL));
  // Write is not supported.
  EXPECT_EQ(talk_base::SR_ERROR,
            multipart.Write(buffer, sizeof(buffer), NULL, NULL));

  part_size += multipart.GetEndPartSize();
  multipart.EndParts();
  EXPECT_TRUE(multipart.GetSize(&size));
  EXPECT_EQ(part_size, size);

  // Read the multipart stream into StringStream
  std::string str;
  talk_base::StringStream str_stream(str);
  EXPECT_EQ(talk_base::SR_SUCCESS,
            Flow(&multipart, buffer, sizeof(buffer), &str_stream));
  EXPECT_EQ(size, str.length());

  // Search three boundaries and two parts in the order.
  size_t pos = 0;
  pos = str.find(kTestMultipartBoundary);
  EXPECT_NE(std::string::npos, pos);
  pos += kTestMultipartBoundary.length();

  pos = str.find(kTestData, pos);
  EXPECT_NE(std::string::npos, pos);
  pos += sizeof(kTestData);

  pos = str.find(kTestMultipartBoundary, pos);
  EXPECT_NE(std::string::npos, pos);
  pos += kTestMultipartBoundary.length();

  pos = str.find(kTestStreamContent, pos);
  EXPECT_NE(std::string::npos, pos);
  pos += sizeof(kTestStreamContent);

  pos = str.find(kTestMultipartBoundary, pos);
  EXPECT_NE(std::string::npos, pos);
  pos += kTestMultipartBoundary.length();

  pos = str.find(kTestMultipartBoundary, pos);
  EXPECT_EQ(std::string::npos, pos);
}

}  // namespace talk_base
