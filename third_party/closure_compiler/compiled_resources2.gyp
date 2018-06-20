# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Add your directory-specific .gyp file to this list for it to be continuously
# typechecked on the builder:
# http://build.chromium.org/p/chromium.fyi/builders/Closure%20Compilation%20Linux
#
# Also, see our guide to Closure compilation in chrome:
# https://chromium.googlesource.com/chromium/src/+/master/docs/closure_compilation.md
{
  'targets': [
    {
      'target_name': 'compiled_resources2',
      'type': 'none',
      'dependencies': [
      ],
    },
  ]
}
