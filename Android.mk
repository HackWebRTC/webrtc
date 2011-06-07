# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

MY_WEBRTC_ROOT_PATH := $(call my-dir)

# voice
include $(MY_WEBRTC_ROOT_PATH)/common_audio/resampler/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/common_audio/signal_processing_library/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/common_audio/vad/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_coding/NetEQ/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_coding/codecs/CNG/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_coding/codecs/G711/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_coding/codecs/G722/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_coding/codecs/PCM16B/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_coding/codecs/iLBC/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_coding/codecs/iSAC/fix/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_coding/codecs/iSAC/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_coding/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_conference_mixer/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_device/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_processing/aec/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_processing/aecm/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_processing/agc/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_processing/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_processing/ns/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/audio_processing/utility/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/media_file/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/rtp_rtcp/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/udp_transport/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/utility/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/system_wrappers/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/voice_engine/main/source/Android.mk

# video
include $(MY_WEBRTC_ROOT_PATH)/common_video/jpeg/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/common_video/vplib/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/video_capture/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/video_coding/codecs/i420/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/video_coding/codecs/vp8/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/video_coding/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/video_processing/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/modules/video_render/main/source/Android.mk
include $(MY_WEBRTC_ROOT_PATH)/video_engine/main/source/Android.mk

# third party
include $(MY_WEBRTC_ROOT_PATH)/libvpx.mk

# build .so
include $(MY_WEBRTC_ROOT_PATH)/android-webrtc.mk

# build test apps
#include $(MY_WEBRTC_ROOT_PATH)/modules/audio_processing/main/test/process_test/Android.mk
#include $(MY_WEBRTC_ROOT_PATH)/voice_engine/main/test/ui_linux_test/Android.mk
#include $(MY_WEBRTC_ROOT_PATH)/voice_engine/main/test/auto_test/Android.mk
#include $(MY_WEBRTC_ROOT_PATH)/video_engine/main/test/AutoTest/Android/Android.mk
#include $(MY_WEBRTC_ROOT_PATH)/video_engine/main/test/AndroidTest/Android.mk
