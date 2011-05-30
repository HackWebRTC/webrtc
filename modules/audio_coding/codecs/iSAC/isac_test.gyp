# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    # simple kenny
    {
      'target_name': 'iSACtest',
      'type': 'executable',
      'dependencies': [
        './main/source/isac.gyp:iSAC',
      ],
      'include_dirs': [
        './main/test',
        './main/interface',
        './main/util',
      ],
      'sources': [
        './main/test/simpleKenny.c',
        './main/util/utility.c',
      ],
    },
    # ReleaseTest-API
    {
      'target_name': 'iSACAPITest',
      'type': 'executable',
      'dependencies': [
        './main/source/isac.gyp:iSAC',
      ],
      'include_dirs': [
        './main/test',
        './main/interface',
        './main/util',
      ],
      'sources': [
        './main/test/ReleaseTest-API/ReleaseTest-API.cc',
        './main/util/utility.c',
      ],
    },
    # SwitchingSampRate
    {
      'target_name': 'iSACSwitchSampRateTest',
      'type': 'executable',
      'dependencies': [
        './main/source/isac.gyp:iSAC',
      ],
      'include_dirs': [
        './main/test',
        './main/interface',
        '../../../../common_audio/signal_processing_library/main/interface',
        './main/util',
      ],
      'sources': [
        './main/test/SwitchingSampRate/SwitchingSampRate.cc',
        './main/util/utility.c',
      ],    
    },

  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
