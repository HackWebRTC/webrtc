# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Android makefile for webrtc VoiceEngine Java API wrapper
# This setup assumes two libs built outside Android makefile structure.

LOCAL_PATH := $(call my-dir)

WEBRTC_INTERFACES_PATH := $(LOCAL_PATH)/../../../../../../../../build/interface
WEBRTC_LIBS_PATH := $(LOCAL_PATH)/../../../../../../../../build/libraries
WEBRTC_AUTO_TEST_PATH := $(LOCAL_PATH)/../../../auto_test

include $(CLEAR_VARS)

LOCAL_MODULE     := android_test
LOCAL_SRC_FILES  := android_test.cc
LOCAL_CFLAGS     := -DWEBRTC_TARGET_PC # For typedefs.h
LOCAL_C_INCLUDES := $(WEBRTC_INTERFACES_PATH) $(WEBRTC_AUTO_TEST_PATH)
LOCAL_LDLIBS     := \
    $(WEBRTC_LIBS_PATH)/VoiceEngine_android_gcc.a \
    $(WEBRTC_AUTO_TEST_PATH)/auto_test_android_gcc.a \
    -llog -lgcc

include $(BUILD_SHARED_LIBRARY)
