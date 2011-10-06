# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'merged_lib_voice',
      'type': 'none',
      'variables': {
        'autotest_name': 'voe_auto_test',
      },
      'dependencies': [
        '../voice_engine/voice_engine.gyp:<(autotest_name)',
      ],
      'actions': [
        {
          'variables': {
            'output_lib_name': 'webrtc_voice_engine',
            'output_lib': '<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)<(output_lib_name)_<(OS)<(STATIC_LIB_SUFFIX)',
          },
          'action_name': 'merge_libs',
          'inputs': ['<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)<(autotest_name)<(EXECUTABLE_SUFFIX)'],
          'outputs': ['<(output_lib)'],
          'action': ['python',
                     'merge_libs.py',
                     '<(PRODUCT_DIR)',
                     '<(output_lib)'],
        },
      ],
    },
    {
      'target_name': 'merged_lib',
      'type': 'none',
      'variables': {
        'autotest_name': 'vie_auto_test',
      },
      'dependencies': [
        '../video_engine/video_engine.gyp:<(autotest_name)',
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
                     'merge_libs.py',
                     '<(PRODUCT_DIR)',
                     '<(output_lib)'],
        },
      ],
    },
  ],
}
