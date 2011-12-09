# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

MY_WEBRTC_ROOT_PATH := $(call my-dir)

# voice
include $(MY_WEBRTC_ROOT_PATH)/src/common_audio/resampler/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/common_audio/signal_processing/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/common_audio/vad/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/neteq/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/cng/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/g711/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/g722/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/pcm16b/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/ilbc/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/iSAC/fix/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/codecs/iSAC/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_coding/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_conference_mixer/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_device/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/aec/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/aecm/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/agc/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/modules/audio_processing/ns/Android.mk
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
include $(MY_WEBRTC_ROOT_PATH)/src/video_engine/Android.mk

# third party
include $(MY_WEBRTC_ROOT_PATH)/libvpx.mk

# build .so
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../external/webrtc/android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwebrtc_audio_preprocessing
LOCAL_MODULE_TAGS := optional

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libwebrtc_spl \
    libwebrtc_resampler \
    libwebrtc_apm \
    libwebrtc_apm_utility \
    libwebrtc_vad \
    libwebrtc_ns \
    libwebrtc_agc \
    libwebrtc_aec \
    libwebrtc_aecm \
    libwebrtc_system_wrappers

# Add Neon libraries.
ifneq (,$(filter '-DWEBRTC_DETECT_ARM_NEON',$(MY_WEBRTC_COMMON_DEFS)))
LOCAL_WHOLE_STATIC_LIBRARIES += \
    libwebrtc_aecm_neon \
    libwebrtc_ns_neon
else ifeq ($(ARCH_ARM_HAVE_NEON),true)
LOCAL_WHOLE_STATIC_LIBRARIES += \
    libwebrtc_aecm_neon \
    libwebrtc_ns_neon
endif

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-cpp-2.3.0-lite

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

LOCAL_PRELINK_MODULE := false

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_SHARED_LIBRARY)

###

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwebrtc
LOCAL_MODULE_TAGS := optional

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libwebrtc_system_wrappers \
    libwebrtc_audio_device \
    libwebrtc_pcm16b \
    libwebrtc_cng \
    libwebrtc_audio_coding \
    libwebrtc_rtp_rtcp \
    libwebrtc_media_file \
    libwebrtc_udp_transport \
    libwebrtc_utility \
    libwebrtc_neteq \
    libwebrtc_audio_conference_mixer \
    libwebrtc_isac \
    libwebrtc_ilbc \
    libwebrtc_isacfix \
    libwebrtc_g722 \
    libwebrtc_g711 \
    libwebrtc_voe_core \
    libwebrtc_video_render \
    libwebrtc_video_capture \
    libwebrtc_i420 \
    libwebrtc_video_coding \
    libwebrtc_video_processing \
    libwebrtc_vp8 \
    libwebrtc_vie_core \
    libwebrtc_vplib \
    libwebrtc_jpeg \
    libwebrtc_vpx

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport \
    libjpeg \
    libGLESv2 \
    libOpenSLES \
    libwebrtc_audio_preprocessing

LOCAL_PRELINK_MODULE := false

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_SHARED_LIBRARY)

# test apps, they're for test only; all these test apps have LOCAL_MODULE_TAGS:=tests
# voice engine test apps
include $(MY_WEBRTC_ROOT_PATH)/src/voice_engine/main/test/cmd_test/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/voice_engine/main/test/auto_test/Android.mk
# video engien test apps
include $(MY_WEBRTC_ROOT_PATH)/src/video_engine/test/auto_test/android/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/src/video_engine/main/test/android_test/Android.mk
