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
LOCAL_MODULE := libwebrtc_spl
LOCAL_MODULE_TAGS := optional
LOCAL_GENERATED_SOURCES :=
LOCAL_SRC_FILES := add_sat_w16.c \
    add_sat_w32.c \
    auto_corr_to_refl_coef.c \
    auto_correlation.c \
    complex_fft.c \
    complex_ifft.c \
    complex_bit_reverse.c \
    copy_set_operations.c \
    cos_table.c \
    cross_correlation.c \
    division_operations.c \
    dot_product_with_scale.c \
    downsample_fast.c \
    energy.c \
    filter_ar.c \
    filter_ar_fast_q12.c \
    filter_ma_fast_q12.c \
    get_hanning_window.c \
    get_scaling_square.c \
    get_size_in_bits.c \
    hanning_table.c \
    ilbc_specific_functions.c \
    levinson_durbin.c \
    lpc_to_refl_coef.c \
    min_max_operations.c \
    norm_u32.c \
    norm_w16.c \
    norm_w32.c \
    randn_table.c \
    randomization_functions.c \
    refl_coef_to_lpc.c \
    resample.c \
    resample_48khz.c \
    resample_by_2.c \
    resample_by_2_internal.c \
    resample_fractional.c \
    sin_table.c \
    sin_table_1024.c \
    spl_sqrt.c \
    spl_version.c \
    splitting_filter.c \
    sqrt_of_one_minus_x_squared.c \
    sub_sat_w16.c \
    sub_sat_w32.c \
    vector_scaling_operations.c

# Flags passed to both C and C++ files.
MY_CFLAGS :=  
MY_CFLAGS_C :=
MY_DEFS := '-DNO_TCMALLOC' \
    '-DNO_HEAPCHECKER' \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_LINUX' 
ifeq ($(TARGET_ARCH),arm) 
MY_DEFS += \
    '-DWEBRTC_ANDROID' \
    '-DANDROID' 
endif
LOCAL_CFLAGS := $(MY_CFLAGS_C) $(MY_CFLAGS) $(MY_DEFS)

# Include paths placed before CFLAGS/CPPFLAGS
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../interface 

# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS := 

LOCAL_LDFLAGS :=

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES := libstlport

LOCAL_ADDITIONAL_DEPENDENCIES :=

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl -lpthread
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)
