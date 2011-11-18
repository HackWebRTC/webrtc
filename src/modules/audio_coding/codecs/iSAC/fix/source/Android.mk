# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../../../../android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_isacfix
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    arith_routines.c \
    arith_routines_hist.c \
    arith_routines_logist.c \
    bandwidth_estimator.c \
    decode.c \
    decode_bwe.c \
    decode_plc.c \
    encode.c \
    entropy_coding.c \
    fft.c \
    filterbank_tables.c \
    filterbanks.c \
    filters.c \
    initialize.c \
    isacfix.c \
    lattice.c \
    lpc_masking_model.c \
    lpc_tables.c \
    pitch_estimator.c \
    pitch_filter.c \
    pitch_gain_tables.c \
    pitch_lag_tables.c \
    spectrum_ar_model_tables.c \
    transform.c

ifeq ($(ARCH_ARM_HAVE_NEON),true)
LOCAL_SRC_FILES += \
    filters_neon.c
#    lattice_neon.c
LOCAL_CFLAGS += \
    $(MY_ARM_CFLAGS_NEON)
endif

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../../../.. \
    $(LOCAL_PATH)/../../../../../../common_audio/signal_processing/include

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)

# isac test app

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= ../test/kenny.c

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../../../..

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libwebrtc

LOCAL_MODULE:= webrtc_isac_test

ifdef NDK_ROOT
include $(BUILD_EXECUTABLE)
else
include $(BUILD_NATIVE_TEST)
endif
