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
LOCAL_MODULE := libwebrtc_audio_coding
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_GENERATED_SOURCES :=
LOCAL_SRC_FILES := acm_amr.cc \
    acm_amrwb.cc \
    acm_cng.cc \
    acm_codec_database.cc \
    acm_dtmf_detection.cc \
    acm_dtmf_playout.cc \
    acm_g722.cc \
    acm_g7221.cc \
    acm_g7221c.cc \
    acm_g729.cc \
    acm_g7291.cc \
    acm_generic_codec.cc \
    acm_gsmfr.cc \
    acm_ilbc.cc \
    acm_isac.cc \
    acm_neteq.cc \
    acm_opus.cc \
    acm_speex.cc \
    acm_pcm16b.cc \
    acm_pcma.cc \
    acm_pcmu.cc \
    acm_red.cc \
    acm_resampler.cc \
    audio_coding_module.cc \
    audio_coding_module_impl.cc

# Flags passed to both C and C++ files.
MY_CFLAGS :=  
MY_CFLAGS_C :=
MY_DEFS := '-DNO_TCMALLOC' \
    '-DNO_HEAPCHECKER' \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_LINUX' \
    '-DWEBRTC_THREAD_RR' \
    '-DWEBRTC_ANDROID' \
    '-DANDROID' 

LOCAL_CFLAGS := $(MY_CFLAGS_C) $(MY_CFLAGS) $(MY_DEFS)

# Include paths placed before CFLAGS/CPPFLAGS
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../interface \
    $(LOCAL_PATH)/../../codecs/CNG/main/interface \
    $(LOCAL_PATH)/../../codecs/G711/main/interface \
    $(LOCAL_PATH)/../../codecs/G722/main/interface \
    $(LOCAL_PATH)/../../codecs/iLBC/main/interface \
    $(LOCAL_PATH)/../../codecs/iSAC/main/interface \
    $(LOCAL_PATH)/../../codecs/iSAC/fix/interface \
    $(LOCAL_PATH)/../../codecs/PCM16B/main/interface \
    $(LOCAL_PATH)/../../NetEQ/main/interface \
    $(LOCAL_PATH)/../../../../common_audio/resampler/main/interface \
    $(LOCAL_PATH)/../../../../common_audio/signal_processing_library/main/interface \
    $(LOCAL_PATH)/../../../../common_audio/vad/main/interface \
    $(LOCAL_PATH)/../../../../system_wrappers/interface 

# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS := 

LOCAL_LDFLAGS :=

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES := libcutils \
    libdl \
    libstlport
LOCAL_ADDITIONAL_DEPENDENCIES :=

ifneq ($(MY_WEBRTC_NDK_BUILD),true)
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)
endif