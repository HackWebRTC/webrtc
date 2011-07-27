# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

MY_WEBRTC_ROOT_PATH := $(call my-dir)

# These defines will apply to all source files
# Think again before changing it
MY_WEBRTC_COMMON_DEFS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_LINUX' \
    '-DWEBRTC_THREAD_RR' \
    '-DWEBRTC_CLOCK_TYPE_REALTIME' \
    '-DWEBRTC_ANDROID' \
    '-DWEBRTC_ANDROID_OPENSLES'
ifeq ($(TARGET_ARCH),arm)
MY_WEBRTC_COMMON_DEFS += \
    '-DWEBRTC_ARM_INLINE_CALLS' \
    '-DWEBRTC_ARCH_ARM'
endif

# voice
include $(MY_WEBRTC_ROOT_PATH)/src/common_audio/resampler/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/common_audio/signal_processing_library/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/common_audio/vad/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/NetEQ/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/CNG/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/G711/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/G722/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/PCM16B/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/iLBC/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/iSAC/fix/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/iSAC/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_conference_mixer/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_device/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/aec/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/aecm/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/agc/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/ns/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/utility/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/media_file/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/rtp_rtcp/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/udp_transport/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/utility/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/system_wrappers/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/voice_engine/main/source/Android.mk

# video
include $(MY_WEBRTC_ROOT_PATH)/src/common_video/jpeg/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/common_video/vplib/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/video_capture/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/video_coding/codecs/i420/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/video_coding/codecs/vp8/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/video_coding/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/video_processing/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/video_render/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/video_engine/main/source/Android.mk

# third party
include $(MY_WEBRTC_ROOT_PATH)/libvpx.mk

# build .so
include $(MY_WEBRTC_ROOT_PATH)/android-webrtc.mk

# test apps, they're for test only; all these test apps have LOCAL_MODULE_TAGS:=tests
# audio processing test apps
# include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/main/test/process_test/Android.mk
# include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/main/test/unit_test/Android.mk 
# voice engine test apps
# include $(MY_WEBRTC_ROOT_PATH)/src/voice_engine/main/test/cmd_test/Android.mk
# include $(MY_WEBRTC_ROOT_PATH)/src/voice_engine/main/test/auto_test/Android.mk
# video engien test apps
# include $(MY_WEBRTC_ROOT_PATH)/src/video_engine/main/test/AutoTest/android/Android.mk
# include $(MY_WEBRTC_ROOT_PATH)/src/video_engine/main/test/android_test/Android.mk
