# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

MY_APM_WHOLE_STATIC_LIBRARIES := \
	libwebrtc_spl \
	libwebrtc_resampler \
	libwebrtc_apm \
	libwebrtc_apm_utility \
	libwebrtc_vad \
	libwebrtc_ns \
	libwebrtc_agc \
	libwebrtc_aec \
	libwebrtc_aecm 

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwebrtc_audio_preprocessing
LOCAL_MODULE_TAGS := optional
LOCAL_LDFLAGS :=

LOCAL_WHOLE_STATIC_LIBRARIES := \
	$(MY_APM_WHOLE_STATIC_LIBRARIES) \
	libwebrtc_system_wrappers \

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libdl \
	libstlport 

LOCAL_ADDITIONAL_DEPENDENCIES :=

include external/stlport/libstlport.mk
include $(BUILD_SHARED_LIBRARY)

###

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwebrtc
LOCAL_MODULE_TAGS := optional
LOCAL_LDFLAGS :=

LOCAL_WHOLE_STATIC_LIBRARIES := \
	libwebrtc_system_wrappers \
	libwebrtc_audio_device \
	libwebrtc_pcm16b \
	libwebrtc_cng \
	libwebrtc_audio_coding \
	libwebrtc_rtp_rtcp \
	libwebrtc_media_file \
	libwebrtc_udp_transport \
	libwebrtc_utility \
	libwebrtc_neteq \
	libwebrtc_audio_conference_mixer \
	libwebrtc_isac \
	libwebrtc_ilbc \
	libwebrtc_isacfix \
	libwebrtc_g722 \
	libwebrtc_g711 \
	libwebrtc_vplib \
	libwebrtc_video_render \
	libwebrtc_video_capture \
	libwebrtc_i420 \
	libwebrtc_video_coding \
	libwebrtc_video_processing \
	libwebrtc_vp8 \
	libwebrtc_video_mixer \
	libwebrtc_voe_core \
	libwebrtc_vie_core \
	libwebrtc_vpx_enc \
	libwebrtc_jpeg \
	libvpx 

#LOCAL_LDLIBS := -ljpeg

LOCAL_STATIC_LIBRARIES := 	
LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libdl \
	libstlport \
	libjpeg \
	libGLESv2 \
	libOpenSLES \
	libwebrtc_audio_preprocessing

LOCAL_ADDITIONAL_DEPENDENCIES :=

include external/stlport/libstlport.mk
include $(BUILD_SHARED_LIBRARY)
