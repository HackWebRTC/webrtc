# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    { 'target_name': 'audio_network_adaptor',
      'type': 'static_library',
      'sources': [
        'audio_network_adaptor.cc',
        'audio_network_adaptor_impl.cc',
        'audio_network_adaptor_impl.h',
        'channel_controller.cc',
        'channel_controller.h',
        'controller.h',
        'controller.cc',
        'controller_manager.cc',
        'controller_manager.h',
        'dtx_controller.h',
        'dtx_controller.cc',
        'include/audio_network_adaptor.h'
      ], # source
    },
  ], # targets
}
