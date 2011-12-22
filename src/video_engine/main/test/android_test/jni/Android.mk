#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

INTERFACES_PATH := $(LOCAL_PATH)/../../../../../../../build/interface
LIBS_PATH := $(LOCAL_PATH)/../../../../../../../build/libraries

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := libwebrtc-video-demo-jni
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := vie_android_java_api.cc
LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_ANDROID'

LOCAL_C_INCLUDES := \
    external/gtest/include \
    $(LOCAL_PATH)/../../../../.. \
    $(LOCAL_PATH)/../../../../include \
    $(LOCAL_PATH)/../../../../../voice_engine/main/interface 

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libstlport \
    libandroid \
    libwebrtc \
    libGLESv2
LOCAL_LDLIBS := $(LIBS_PATH)/VideoEngine_android_gcc.a -llog -lgcc 

include $(BUILD_SHARED_LIBRARY)
