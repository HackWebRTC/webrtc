# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../../webrtc/build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'rbe_components',
      'type': 'static_library',

      'include_dirs': [
        '<(webrtc_root)/modules/remote_bitrate_estimator',
      ],
      'sources': [
        '<(webrtc_root)/modules/remote_bitrate_estimator/test/bwe_test_logging.cc',
        '<(webrtc_root)/modules/remote_bitrate_estimator/test/bwe_test_logging.h',
        'aimd_rate_control.cc',
        'aimd_rate_control.h',
        'inter_arrival.cc',
        'inter_arrival.h',
        'mimd_rate_control.cc',
        'mimd_rate_control.h',
        'overuse_detector.cc',
        'overuse_detector.h',
        'overuse_estimator.cc',
        'overuse_estimator.h',
        'remote_bitrate_estimator_abs_send_time.cc',
        'remote_bitrate_estimator_single_stream.cc',
        'remote_rate_control.cc',
        'remote_rate_control.h',
      ],
    },
  ],
}
