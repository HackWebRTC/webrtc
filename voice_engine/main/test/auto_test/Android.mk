#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH:= $(call my-dir)

# voice engine test app

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= \
    voe_cpu_test.cc \
    voe_standard_test.cc \
    voe_stress_test.cc \
    voe_unit_test.cc \
    voe_extended_test.cc 
# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_LINUX' \
    '-DWEBRTC_ANDROID' \
    '-DANDROID' 
LOCAL_CPPFLAGS := 
LOCAL_LDFLAGS :=
LOCAL_C_INCLUDES := \
    external/gtest/include \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../../../../system_wrappers/interface

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libstlport \
    libwebrtc

LOCAL_MODULE:= webrtc_voe_autotest

include external/stlport/libstlport.mk
include $(BUILD_EXECUTABLE)
