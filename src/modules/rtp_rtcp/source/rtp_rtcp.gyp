# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'rtp_rtcp',
      'type': '<(library)',
      'dependencies': [
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../interface',
        ],
      },
      'sources': [
        # Common
        '../interface/rtp_rtcp.h',
        '../interface/rtp_rtcp_defines.h',
        'bitrate.cc',
        'Bitrate.h',
        'rtp_rtcp_config.h',
        'rtp_rtcp_impl.cc',
        'rtp_rtcp_impl.h',
        'rtp_rtcp_private.h',
        'rtcp_receiver.cc',
        'rtcp_receiver.h',
        'rtcp_receiver_help.cc',
        'rtcp_receiver_help.h',
        'rtcp_sender.cc',
        'rtcp_sender.h',
        'rtcp_utility.cc',
        'rtcp_utility.h',
        'rtp_receiver.cc',
        'rtp_receiver.h',
        'rtp_sender.cc',
        'rtp_sender.h',
        'rtp_utility.cc',
        'rtp_utility.h',
        'ssrc_database.cc',
        'ssrc_database.h',
        'tmmbr_help.cc',
        'tmmbr_help.h',
        # Audio Files
        'dtmf_queue.cc',
        'dtmf_queue.h',
        'rtp_receiver_audio.cc',
        'rtp_receiver_audio.h',
        'rtp_sender_audio.cc',
        'rtp_sender_audio.h',
        # Video Files
        'bandwidth_management.cc',
        'bandwidth_management.h',
        'bwe_defines.h',
        'fec_private_tables.h',
        'forward_error_correction.cc',
        'forward_error_correction.h',
        'forward_error_correction_internal.cc',
        'forward_error_correction_internal.h',
        'overuse_detector.cc',
        'overuse_detector.h',
        'h263_information.cc',
        'h263_information.h',
        'remote_rate_control.cc',
        'remote_rate_control.h',
        'rtp_receiver_video.cc',
        'rtp_receiver_video.h',
        'rtp_sender_video.cc',
        'rtp_sender_video.h',
        'receiver_fec.cc',
        'receiver_fec.h',
        'video_codec_information.h',
        'rtp_format_vp8.cc',
        'rtp_format_vp8.h',
      ], # source
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
