#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH:= $(call my-dir)

# apm test app

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= \
    process_test.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_LINUX' \
    '-DWEBRTC_THREAD_RR' \
    '-DWEBRTC_ANDROID' \
    '-DANDROID' 

LOCAL_CPPFLAGS := 
LOCAL_LDFLAGS :=
LOCAL_C_INCLUDES := \
    external/gtest/include \
    $(LOCAL_PATH)/../../../../../system_wrappers/interface \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../../../interface \
    $(LOCAL_PATH)/../../../../..

LOCAL_STATIC_LIBRARIES := \
    libgtest 

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libstlport \
    libwebrtc_audio_preprocessing 

LOCAL_MODULE:= webrtc_apm_process_test

include external/stlport/libstlport.mk
include $(BUILD_EXECUTABLE)
