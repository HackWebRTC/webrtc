# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_audio_device
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_GENERATED_SOURCES :=
LOCAL_SRC_FILES := audio_device_buffer.cc \
    audio_device_generic.cc \
    audio_device_utility.cc \
    audio_device_impl.cc \
    Android/audio_device_android_native.cc \
    Android/audio_device_utility_android.cc \
    Linux/audio_device_utility_linux.cc \
    Dummy/audio_device_dummy.cc

# Flags passed to both C and C++ files.
MY_CFLAGS :=  
MY_CFLAGS_C :=
MY_DEFS := '-DNO_TCMALLOC' \
    '-DNO_HEAPCHECKER' \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_THREAD_RR' \
    '-DWEBRTC_LINUX' \
    '-DWEBRTC_ANDROID' \
    '-DWEBRTC_ANDROID_NATIVE' \
    '-DANDROID' 

LOCAL_CFLAGS := $(MY_CFLAGS_C) $(MY_CFLAGS) $(MY_DEFS)

# Include paths placed before CFLAGS/CPPFLAGS
LOCAL_C_INCLUDES := \
    $(JNI_H_INCLUDE) \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/. \
    $(LOCAL_PATH)/../../../interface \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/Android \
    $(LOCAL_PATH)/Dummy \
    $(LOCAL_PATH)/Linux \
    $(LOCAL_PATH)/../../../../common_audio/resampler/main/interface \
    $(LOCAL_PATH)/../../../../common_audio/signal_processing_library/main/interface \
    $(LOCAL_PATH)/../../../../system_wrappers/interface \
    system/media/wilhelm/include/SLES

# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS := 

LOCAL_LDFLAGS :=

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := libcutils \
    libdl \
    libstlport \
    libOpenSLES

LOCAL_ADDITIONAL_DEPENDENCIES :=

include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)
