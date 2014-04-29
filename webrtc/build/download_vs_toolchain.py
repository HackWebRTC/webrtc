#!/usr/bin/env python
#
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This script is used to run the vs_toolchain.py script to download the
# Visual Studio toolchain. It's just a temporary measure while waiting for the
# Chrome team to move find_depot_tools into src/build to get rid of these
# workarounds (similar one in gyp_webrtc).

import os
import sys


script_dir = os.path.dirname(os.path.realpath(__file__))
checkout_root = os.path.abspath(os.path.join(script_dir, os.pardir, os.pardir))
sys.path.insert(0, os.path.join(checkout_root, 'build'))
sys.path.insert(0, os.path.join(checkout_root, 'tools', 'find_depot_tools'))


import vs_toolchain


if __name__ == '__main__':
  sys.exit(vs_toolchain.main())
