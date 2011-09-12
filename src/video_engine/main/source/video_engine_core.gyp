# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'video_engine_core',
      'type': '<(library)',
      'dependencies': [

        ## common_video
       '../../../common_video/vplib/main/source/vplib.gyp:webrtc_vplib',
       '../../../common_video/jpeg/main/source/jpeg.gyp:webrtc_jpeg',

        ## ModulesShared
        '../../../modules/media_file/source/media_file.gyp:media_file',
        '../../../modules/rtp_rtcp/source/rtp_rtcp.gyp:rtp_rtcp',
        '../../../modules/udp_transport/source/udp_transport.gyp:udp_transport',
        '../../../modules/utility/source/utility.gyp:webrtc_utility',

        ## ModulesVideo
        '../../../modules/video_coding/main/source/video_coding.gyp:webrtc_video_coding',
        '../../../modules/video_processing/main/source/video_processing.gyp:video_processing',
        '../../../modules/video_render/main/source/video_render.gyp:video_render_module',

        ## VoiceEngine
        '../../../voice_engine/main/source/voice_engine_core.gyp:voice_engine_core',

        ## system_wrappers_2005
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../../modules/video_capture/main/interface',
        '../../../modules/video_capture/main/source',
        '../../../modules/video_render/main/interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
        ],
      },
      'sources': [
        # interface
        '../interface/vie_base.h',
        '../interface/vie_capture.h',
        '../interface/vie_codec.h',
        '../interface/vie_encryption.h',
        '../interface/vie_errors.h',
        '../interface/vie_external_codec.h',
        '../interface/vie_file.h',
        '../interface/vie_image_process.h',
        '../interface/vie_network.h',
        '../interface/vie_render.h',
        '../interface/vie_rtp_rtcp.h',

        # headers
        'vie_base_impl.h',
        'vie_capture_impl.h',
        'vie_codec_impl.h',
        'vie_defines.h',
        'vie_encryption_impl.h',
        'vie_external_codec_impl.h',
        'vie_file_impl.h',
        'vie_image_process_impl.h',
        'vie_impl.h',
        'vie_network_impl.h',
        'vie_ref_count.h',
        'vie_render_impl.h',
        'vie_rtp_rtcp_impl.h',
        'vie_shared_data.h',
        'vie_capturer.h',
        'vie_channel.h',
        'vie_channel_manager.h',
        'vie_encoder.h',
        'vie_file_image.h',
        'vie_file_player.h',
        'vie_file_recorder.h',
        'vie_frame_provider_base.h',
        'vie_input_manager.h',
        'vie_manager_base.h',
        'vie_performance_monitor.h',
        'vie_receiver.h',
        'vie_renderer.h',
        'vie_render_manager.h',
        'vie_sender.h',
        'vie_sync_module.h',

        # ViE
        'vie_base_impl.cc',
        'vie_capture_impl.cc',
        'vie_codec_impl.cc',
        'vie_encryption_impl.cc',
        'vie_external_codec_impl.cc',
        'vie_file_impl.cc',
        'vie_image_process_impl.cc',
        'vie_impl.cc',
        'vie_network_impl.cc',
        'vie_ref_count.cc',
        'vie_render_impl.cc',
        'vie_rtp_rtcp_impl.cc',
        'vie_shared_data.cc',

        # ViE
        'vie_capturer.cc',
        'vie_channel.cc',
        'vie_channel_manager.cc',
        'vie_encoder.cc',
        'vie_file_image.cc',
        'vie_file_player.cc',
        'vie_file_recorder.cc',
        'vie_frame_provider_base.cc',
        'vie_input_manager.cc',
        'vie_manager_base.cc',
        'vie_performance_monitor.cc',
        'vie_receiver.cc',
        'vie_renderer.cc',
        'vie_render_manager.cc',
        'vie_sender.cc',
        'vie_sync_module.cc',
      ], # source
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
