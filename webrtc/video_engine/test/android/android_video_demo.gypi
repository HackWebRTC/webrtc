# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'variables': {
    # NOTE: This laundry list of libs comes from jni/Android.mk and should be
    # kept in sync with that location.  Also note that the explicit library
    # names must be used (in addition to the names of their gyp targets) because
    # these are fed into the 'inputs' attribute of the action below, not to
    # 'dependencies' because these need to be real (build-time) dependencies,
    # not just order-only dependencies
    # (https://code.google.com/p/webrtc/issues/detail?id=1980).
    'android_mk_common_libs': [
      '<(PRODUCT_DIR)/libvoice_engine.a',
      '<(PRODUCT_DIR)/libvideo_engine_core.a',
      '<(PRODUCT_DIR)/libvideo_processing.a',
      '<(PRODUCT_DIR)/libwebrtc_video_coding.a',
      '<(PRODUCT_DIR)/libvideo_render_module.a',
      '<(PRODUCT_DIR)/libvideo_capture_module.a',
      '<(PRODUCT_DIR)/libaudio_coding_module.a',
      '<(PRODUCT_DIR)/libaudio_processing.a',
      '<(PRODUCT_DIR)/libPCM16B.a',
      '<(PRODUCT_DIR)/libCNG.a',
      '<(PRODUCT_DIR)/libNetEq.a',
      '<(PRODUCT_DIR)/libG722.a',
      '<(PRODUCT_DIR)/libiSAC.a',
      '<(PRODUCT_DIR)/libG711.a',
      '<(PRODUCT_DIR)/libiLBC.a',
      '<(PRODUCT_DIR)/libiSACFix.a',
      '<(PRODUCT_DIR)/libwebrtc_opus.a',
      '<(PRODUCT_DIR)/libopus.a',
      '<(PRODUCT_DIR)/libcommon_audio.a',
      '<(PRODUCT_DIR)/libbitrate_controller.a',
      '<(PRODUCT_DIR)/libcommon_video.a',
      '<(PRODUCT_DIR)/libcpu_features_android.a',
      '<(PRODUCT_DIR)/libaudio_device.a',
      '<(PRODUCT_DIR)/libremote_bitrate_estimator.a',
      '<(PRODUCT_DIR)/librtp_rtcp.a',
      '<(PRODUCT_DIR)/libmedia_file.a',
      '<(PRODUCT_DIR)/libchannel_transport.a',
      '<(PRODUCT_DIR)/libwebrtc_utility.a',
      '<(PRODUCT_DIR)/libaudio_conference_mixer.a',
      '<(PRODUCT_DIR)/libyuv.a',
      '<(PRODUCT_DIR)/libwebrtc_i420.a',
      '<(PRODUCT_DIR)/libwebrtc_vp8.a',
      '<(PRODUCT_DIR)/libvideo_coding_utility.a',
      '<(PRODUCT_DIR)/libsystem_wrappers.a',
      '<(PRODUCT_DIR)/libjpeg_turbo.a',
      '<(PRODUCT_DIR)/libaudioproc_debug_proto.a',
      '<(PRODUCT_DIR)/libprotobuf_lite.a',
      '<(PRODUCT_DIR)/libpaced_sender.a',
      '<(PRODUCT_DIR)/libvpx.a',
    ],
    'android_mk_x86_libs': [
      '<(PRODUCT_DIR)/libvideo_processing_sse2.a',
      '<(PRODUCT_DIR)/libaudio_processing_sse2.a',
      '<(PRODUCT_DIR)/libcommon_audio_sse2.a',
      '<(PRODUCT_DIR)/libvpx_intrinsics_mmx.a',
      '<(PRODUCT_DIR)/libvpx_intrinsics_sse2.a',
      '<(PRODUCT_DIR)/libvpx_intrinsics_ssse3.a',
    ],
    'android_mk_arm_libs': [
      '<(PRODUCT_DIR)/libaudio_processing_neon.a',
      '<(PRODUCT_DIR)/libisac_neon.a',
      '<(PRODUCT_DIR)/libcommon_audio_neon.a',
      '<(PRODUCT_DIR)/libvpx_arm_neon.a',
    ],
    'android_modules_java_jars': [
      '<(PRODUCT_DIR)/lib.java/audio_device_module_java.jar',
      '<(PRODUCT_DIR)/lib.java/video_capture_module_java.jar',
      '<(PRODUCT_DIR)/lib.java/video_render_module_java.jar',
    ],
  },

  'targets': [
    {
      'target_name': 'video_demo_apk',
      'type': 'none',
      'conditions': [
        ['target_arch=="x86"', {
          'inputs': [ '<@(android_mk_x86_libs)' ],
        }, {
          'inputs': [ '<@(android_mk_arm_libs)' ],
        }],
      ],
      'dependencies': [
        '<(webrtc_root)/modules/modules.gyp:*',
        '<(webrtc_root)/modules/modules_java.gyp:*',
        '<(webrtc_root)/test/test.gyp:channel_transport',
        '<(webrtc_root)/video_engine/video_engine.gyp:video_engine_core',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
      ],
      'actions': [
        {
          # TODO(leozwang): Convert building of the demo to a proper GYP target
          # so this action and the custom build script is not needed.
          'action_name': 'build_video_demo_apk',
          'inputs' : [
            '<(webrtc_root)/video_engine/test/android/src/org/webrtc/videoengine/ViEMediaCodecDecoder.java',
            '<(webrtc_root)/video_engine/test/android/src/org/webrtc/videoengineapp/IViEAndroidCallback.java',
            '<(webrtc_root)/video_engine/test/android/src/org/webrtc/videoengineapp/ViEAndroidJavaAPI.java',
            '<(webrtc_root)/video_engine/test/android/src/org/webrtc/videoengineapp/WebRTCDemo.java',
            '<@(android_mk_common_libs)',
            '<@(android_modules_java_jars)',
          ],
          'outputs': ['<(webrtc_root)'],
          'action': ['python',
                     '<(webrtc_root)/video_engine/test/android/build_demo.py',
                     '--arch', '<(target_arch)'],
        },
      ],
    },
  ],
}
