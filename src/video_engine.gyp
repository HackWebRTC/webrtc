# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'common_settings.gypi', # Common settings
    'video_engine/main/test/AutoTest/vie_auto_test.gypi',
  ],
  'variables': {
    'autotest_name': 'vie_auto_test',
  },
  'targets': [
    {
      'target_name': 'merged_lib',
      'type': 'none',
      'dependencies': [
        '<(autotest_name)',
      ],
      'actions': [
        {
          'variables': {
            'output_lib_name': 'webrtc',
            'output_lib': '<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)<(output_lib_name)_<(OS)<(STATIC_LIB_SUFFIX)',
          },
          'action_name': 'merge_libs',
          'inputs': ['<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)<(autotest_name)<(EXECUTABLE_SUFFIX)'],
          'outputs': ['<(output_lib)'],
          'action': ['python',
                     './build/merge_libs.py',
                     '<(PRODUCT_DIR)',
                     '<(output_lib)'],
        },
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
