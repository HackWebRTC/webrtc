# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

MY_WEBRTC_ROOT_PATH := $(call my-dir)

MY_WEBRTC_SRC_PATH := ../../../../../../..

include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/common_audio/resampler/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/common_audio/signal_processing_library/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/common_audio/vad/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_coding/NetEQ/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_coding/codecs/CNG/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_coding/codecs/G711/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_coding/codecs/G722/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_coding/codecs/PCM16B/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_coding/codecs/iLBC/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_coding/codecs/iSAC/fix/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_coding/codecs/iSAC/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_coding/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_conference_mixer/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_device/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_processing/aec/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_processing/aecm/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_processing/agc/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_processing/main/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_processing/ns/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/audio_processing/utility/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/media_file/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/rtp_rtcp/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/udp_transport/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/modules/utility/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/system_wrappers/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/$(MY_WEBRTC_SRC_PATH)/src/voice_engine/main/source/Android.mk

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

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
    libwebrtc_system_wrappers \

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
    libwebrtc_voe_core

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
