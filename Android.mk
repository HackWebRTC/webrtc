# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

MY_WEBRTC_ROOT_PATH := $(call my-dir)

# voice
include $(MY_WEBRTC_ROOT_PATH)/webrtc/common_audio/signal_processing/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/webrtc/common_audio/vad/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/webrtc/modules/audio_processing/aec/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/webrtc/modules/audio_processing/aecm/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/webrtc/modules/audio_processing/agc/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/webrtc/modules/audio_processing/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/webrtc/modules/audio_processing/ns/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/webrtc/modules/audio_processing/utility/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/webrtc/modules/utility/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/webrtc/system_wrappers/source/Android.mk

# test support
include $(MY_WEBRTC_ROOT_PATH)/webrtc/test/Android.mk

# build .so
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../external/webrtc/android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwebrtc_audio_preprocessing
LOCAL_MODULE_TAGS := optional

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libwebrtc_spl \
    libwebrtc_apm \
    libwebrtc_apm_utility \
    libwebrtc_vad \
    libwebrtc_ns \
    libwebrtc_agc \
    libwebrtc_aec \
    libwebrtc_aecm \
    libwebrtc_system_wrappers

# Add Neon libraries.
ifeq ($(WEBRTC_BUILD_NEON_LIBS),true)
LOCAL_WHOLE_STATIC_LIBRARIES += \
    libwebrtc_aecm_neon \
    libwebrtc_ns_neon \
    libwebrtc_spl_neon
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
