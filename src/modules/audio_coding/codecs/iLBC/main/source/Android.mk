# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_ilbc
LOCAL_MODULE_TAGS := optional
LOCAL_GENERATED_SOURCES :=
LOCAL_SRC_FILES := abs_quant.c \
    abs_quant_loop.c \
    augmented_cb_corr.c \
    bw_expand.c \
    cb_construct.c \
    cb_mem_energy.c \
    cb_mem_energy_augmentation.c \
    cb_mem_energy_calc.c \
    cb_search.c \
    cb_search_core.c \
    cb_update_best_index.c \
    chebyshev.c \
    comp_corr.c \
    constants.c \
    create_augmented_vec.c \
    decode.c \
    decode_residual.c \
    decoder_interpolate_lsf.c \
    do_plc.c \
    encode.c \
    energy_inverse.c \
    enh_upsample.c \
    enhancer.c \
    enhancer_interface.c \
    filtered_cb_vecs.c \
    frame_classify.c \
    gain_dequant.c \
    gain_quant.c \
    get_cd_vec.c \
    get_lsp_poly.c \
    get_sync_seq.c \
    hp_input.c \
    hp_output.c \
    ilbc.c \
    index_conv_dec.c \
    index_conv_enc.c \
    init_decode.c \
    init_encode.c \
    interpolate.c \
    interpolate_samples.c \
    lpc_encode.c \
    lsf_check.c \
    lsf_interpolate_to_poly_dec.c \
    lsf_interpolate_to_poly_enc.c \
    lsf_to_lsp.c \
    lsf_to_poly.c \
    lsp_to_lsf.c \
    my_corr.c \
    nearest_neighbor.c \
    pack_bits.c \
    poly_to_lsf.c \
    poly_to_lsp.c \
    refiner.c \
    simple_interpolate_lsf.c \
    simple_lpc_analysis.c \
    simple_lsf_dequant.c \
    simple_lsf_quant.c \
    smooth.c \
    smooth_out_data.c \
    sort_sq.c \
    split_vq.c \
    state_construct.c \
    state_search.c \
    swap_bytes.c \
    unpack_bits.c \
    vq3.c \
    vq4.c \
    window32_w32.c \
    xcorr_coef.c

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
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../../../.. \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../../../../common_audio/signal_processing_library/main/interface 

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