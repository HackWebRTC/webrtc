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
LOCAL_MODULE := libwebrtc_vie_core
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_GENERATED_SOURCES :=
LOCAL_SRC_FILES := vie_base_impl.cc \
    vie_capture_impl.cc \
    vie_codec_impl.cc \
    vie_encryption_impl.cc \
    vie_external_codec_impl.cc \
    vie_file_impl.cc \
    vie_image_process_impl.cc \
    vie_impl.cc \
    vie_network_impl.cc \
    vie_ref_count.cc \
    vie_render_impl.cc \
    vie_rtp_rtcp_impl.cc \
    vie_shared_data.cc \
    vie_capturer.cc \
    vie_channel.cc \
    vie_channel_manager.cc \
    vie_encoder.cc \
    vie_file_image.cc \
    vie_file_player.cc \
    vie_file_recorder.cc \
    vie_frame_provider_base.cc \
    vie_input_manager.cc \
    vie_manager_base.cc \
    vie_performance_monitor.cc \
    vie_receiver.cc \
    vie_renderer.cc \
    vie_render_manager.cc \
    vie_sender.cc \
    vie_sync_module.cc

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
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../modules/video_capture/main/interface \
    $(LOCAL_PATH)/../../../modules/video_render/main/interface \
    $(LOCAL_PATH)/../../../common_video/vplib/main/interface \
    $(LOCAL_PATH)/../../../common_video/jpeg/main/interface \
    $(LOCAL_PATH)/../../../modules/media_file/interface \
    $(LOCAL_PATH)/../../../modules/interface \
    $(LOCAL_PATH)/../../../modules/rtp_rtcp/interface \
    $(LOCAL_PATH)/../../../modules/udp_transport/interface \
    $(LOCAL_PATH)/../../../modules/utility/interface \
    $(LOCAL_PATH)/../../../modules/audio_coding/main/interface \
    $(LOCAL_PATH)/../../../modules/video_coding/main/interface \
    $(LOCAL_PATH)/../../../modules/video_coding/codecs/interface \
    $(LOCAL_PATH)/../../../modules/video_mixer/main/interface \
    $(LOCAL_PATH)/../../../modules/video_processing/main/interface \
    $(LOCAL_PATH)/../../../voice_engine/main/interface \
    $(LOCAL_PATH)/../../../system_wrappers/interface 

# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS := 

LOCAL_LDFLAGS :=

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES := libcutils \
    libdl \
    libstlport
LOCAL_ADDITIONAL_DEPENDENCIES :=

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl -lpthread
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)
