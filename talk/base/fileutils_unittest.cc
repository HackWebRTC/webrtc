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

#include "talk/base/fileutils.h"
#include "talk/base/gunit.h"
#include "talk/base/pathutils.h"
#include "talk/base/stream.h"

namespace talk_base {

// Make sure we can get a temp folder for the later tests.
TEST(FilesystemTest, GetTemporaryFolder) {
  Pathname path;
  EXPECT_TRUE(Filesystem::GetTemporaryFolder(path, true, NULL));
}

// Test creating a temp file, reading it back in, and deleting it.
TEST(FilesystemTest, TestOpenFile) {
  Pathname path;
  EXPECT_TRUE(Filesystem::GetTemporaryFolder(path, true, NULL));
  path.SetPathname(Filesystem::TempFilename(path, "ut"));

  FileStream* fs;
  char buf[256];
  size_t bytes;

  fs = Filesystem::OpenFile(path, "wb");
  ASSERT_TRUE(fs != NULL);
  EXPECT_EQ(SR_SUCCESS, fs->Write("test", 4, &bytes, NULL));
  EXPECT_EQ(4U, bytes);
  delete fs;

  EXPECT_TRUE(Filesystem::IsFile(path));

  fs = Filesystem::OpenFile(path, "rb");
  ASSERT_TRUE(fs != NULL);
  EXPECT_EQ(SR_SUCCESS, fs->Read(buf, sizeof(buf), &bytes, NULL));
  EXPECT_EQ(4U, bytes);
  delete fs;

  EXPECT_TRUE(Filesystem::DeleteFile(path));
  EXPECT_FALSE(Filesystem::IsFile(path));
}

// Test opening a non-existent file.
TEST(FilesystemTest, TestOpenBadFile) {
  Pathname path;
  EXPECT_TRUE(Filesystem::GetTemporaryFolder(path, true, NULL));
  path.SetFilename("not an actual file");

  EXPECT_FALSE(Filesystem::IsFile(path));

  FileStream* fs = Filesystem::OpenFile(path, "rb");
  EXPECT_FALSE(fs != NULL);
}

// Test that CreatePrivateFile fails for existing files and succeeds for
// non-existent ones.
TEST(FilesystemTest, TestCreatePrivateFile) {
  Pathname path;
  EXPECT_TRUE(Filesystem::GetTemporaryFolder(path, true, NULL));
  path.SetFilename("private_file_test");

  // First call should succeed because the file doesn't exist yet.
  EXPECT_TRUE(Filesystem::CreatePrivateFile(path));
  // Next call should fail, because now it exists.
  EXPECT_FALSE(Filesystem::CreatePrivateFile(path));

  // Verify that we have permission to open the file for reading and writing.
  scoped_ptr<FileStream> fs(Filesystem::OpenFile(path, "wb"));
  EXPECT_TRUE(fs.get() != NULL);
  // Have to close the file on Windows before it will let us delete it.
  fs.reset();

  // Verify that we have permission to delete the file.
  EXPECT_TRUE(Filesystem::DeleteFile(path));
}

// Test checking for free disk space.
TEST(FilesystemTest, TestGetDiskFreeSpace) {
  // Note that we should avoid picking any file/folder which could be located
  // at the remotely mounted drive/device.
  Pathname path;
  ASSERT_TRUE(Filesystem::GetAppDataFolder(&path, true));

  int64 free1 = 0;
  EXPECT_TRUE(Filesystem::IsFolder(path));
  EXPECT_FALSE(Filesystem::IsFile(path));
  EXPECT_TRUE(Filesystem::GetDiskFreeSpace(path, &free1));
  EXPECT_GT(free1, 0);

  int64 free2 = 0;
  path.AppendFolder("this_folder_doesnt_exist");
  EXPECT_FALSE(Filesystem::IsFolder(path));
  EXPECT_TRUE(Filesystem::IsAbsent(path));
  EXPECT_TRUE(Filesystem::GetDiskFreeSpace(path, &free2));
  // These should be the same disk, and disk free space should not have changed
  // by more than 1% between the two calls.
  EXPECT_LT(static_cast<int64>(free1 * .9), free2);
  EXPECT_LT(free2, static_cast<int64>(free1 * 1.1));

  int64 free3 = 0;
  path.clear();
  EXPECT_TRUE(path.empty());
  EXPECT_TRUE(Filesystem::GetDiskFreeSpace(path, &free3));
  // Current working directory may not be where exe is.
  // EXPECT_LT(static_cast<int64>(free1 * .9), free3);
  // EXPECT_LT(free3, static_cast<int64>(free1 * 1.1));
  EXPECT_GT(free3, 0);
}

// Tests that GetCurrentDirectory() returns something.
TEST(FilesystemTest, TestGetCurrentDirectory) {
  EXPECT_FALSE(Filesystem::GetCurrentDirectory().empty());
}

// Tests that GetAppPathname returns something.
TEST(FilesystemTest, TestGetAppPathname) {
  Pathname path;
  EXPECT_TRUE(Filesystem::GetAppPathname(&path));
  EXPECT_FALSE(path.empty());
}

}  // namespace talk_base
