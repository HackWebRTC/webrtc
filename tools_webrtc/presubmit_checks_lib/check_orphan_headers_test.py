#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import unittest

import check_orphan_headers


class GetBuildGnPathFromFilePathTest(unittest.TestCase):

  def testGetBuildGnFromSameDirectory(self):
    file_path = '/home/projects/webrtc/base/foo.h'
    expected_build_path = '/home/projects/webrtc/base/BUILD.gn'
    file_exists = lambda p: p == '/home/projects/webrtc/base/BUILD.gn'
    src_dir_path = '/home/projects/webrtc'
    self.assertEqual(
      expected_build_path,
      check_orphan_headers.GetBuildGnPathFromFilePath(file_path,
                                                      file_exists,
                                                      src_dir_path))

  def testGetBuildPathFromParentDirectory(self):
    file_path = '/home/projects/webrtc/base/foo.h'
    expected_build_path = '/home/projects/webrtc/BUILD.gn'
    file_exists = lambda p: p == '/home/projects/webrtc/BUILD.gn'
    src_dir_path = '/home/projects/webrtc'
    self.assertEqual(
      expected_build_path,
      check_orphan_headers.GetBuildGnPathFromFilePath(file_path,
                                                      file_exists,
                                                      src_dir_path))

  def testExceptionIfNoBuildGnFilesAreFound(self):
    with self.assertRaises(check_orphan_headers.NoBuildGnFoundError):
      file_path = '/home/projects/webrtc/base/foo.h'
      file_exists = lambda p: False
      src_dir_path = '/home/projects/webrtc'
      check_orphan_headers.GetBuildGnPathFromFilePath(file_path,
                                                      file_exists,
                                                      src_dir_path)

  def testExceptionIfFilePathIsNotAnHeader(self):
    with self.assertRaises(check_orphan_headers.WrongFileTypeError):
      file_path = '/home/projects/webrtc/base/foo.cc'
      file_exists = lambda p: False
      src_dir_path = '/home/projects/webrtc'
      check_orphan_headers.GetBuildGnPathFromFilePath(file_path,
                                                      file_exists,
                                                      src_dir_path)


class GetHeadersInBuildGnFileSourcesTest(unittest.TestCase):

  def testEmptyFileReturnsEmptySet(self):
    self.assertEqual(
      set([]),
      check_orphan_headers.GetHeadersInBuildGnFileSources('', '/a/b'))

  def testReturnsSetOfHeadersFromFileContent(self):
    file_content = """
    # Some comments
    if (is_android) {
      import("//a/b/c.gni")
      import("//d/e/f.gni")
    }
    source_set("foo") {
      sources = ["foo.h"]
      deps = [":bar"]
    }
    rtc_static_library("bar") {
      sources = [
        "bar.h",
        "bar.cc",
      ]
      deps = [":bar"]
    }
    source_set("baz_foo") {
      sources = ["baz/foo.h"]
    }
    """
    self.assertEqual(
      set(['/a/b/foo.h', '/a/b/bar.h', '/a/b/baz/foo.h']),
      check_orphan_headers.GetHeadersInBuildGnFileSources(file_content,
                                                          '/a/b'))


if __name__ == '__main__':
  unittest.main()
