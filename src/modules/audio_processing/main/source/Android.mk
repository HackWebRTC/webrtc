# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../../android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwebrtc_apm
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    $(call all-proto-files-under, .) \
    audio_buffer.cc \
    audio_processing_impl.cc \
    echo_cancellation_impl.cc \
    echo_control_mobile_impl.cc \
    gain_control_impl.cc \
    high_pass_filter_impl.cc \
    level_estimator_impl.cc \
    noise_suppression_impl.cc \
    splitting_filter.cc \
    processing_component.cc \
    voice_detection_impl.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
    '-DWEBRTC_NS_FIXED'
#   floating point
#   -DWEBRTC_NS_FLOAT'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../aec/main/interface \
    $(LOCAL_PATH)/../../aecm/main/interface \
    $(LOCAL_PATH)/../../agc/main/interface \
    $(LOCAL_PATH)/../../ns/main/interface \
    $(LOCAL_PATH)/../../../interface \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../../../../common_audio/signal_processing_library/main/interface \
    $(LOCAL_PATH)/../../../../common_audio/vad/main/interface \
    $(LOCAL_PATH)/../../../../system_wrappers/interface \
    external/protobuf/src

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport \
    libprotobuf-cpp-2.3.0-lite

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)

