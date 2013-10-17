#!/usr/bin/env python
# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
Copied from Chrome's src/tools/lsan/PRESUBMIT.py

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import re

def CheckChange(input_api, output_api):
  errors = []

  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith('suppressions.txt'):
      continue
    for line_num, line in enumerate(f.NewContents()):
      line = line.strip()
      if line.startswith('#') or not line:
        continue
      if not line.startswith('leak:'):
        errors.append('"%s" should be "leak:..." in %s line %d' %
                      (line, f.LocalPath(), line_num))
  if errors:
    return [output_api.PresubmitError('\n'.join(errors))]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)

def GetPreferredTrySlaves():
  return ['linux_asan']
