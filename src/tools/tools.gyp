# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
      {
      'target_name': 'frame_analyzer',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'sources': [
        'frame_analyzer/frame_analyzer.cc',
        'frame_analyzer/video_quality_analysis.h',
        'frame_analyzer/video_quality_analysis.cc',
        'simple_command_line_parser.h',
        'simple_command_line_parser.cc',
      ],
    },
    {
      'target_name': 'rgba_to_i420_converter',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'sources': [
        'converter/converter.h',
        'converter/converter.cc',
        'converter/rgba_to_i420_converter.cc',
        'simple_command_line_parser.h',
        'simple_command_line_parser.cc',
      ],
    },
  ],
}
