# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

MY_LIBVPX_PATH = ../libvpx

LOCAL_SRC_FILES = \
     $(MY_LIBVPX_PATH)/vp8/common/alloccommon.c \
     $(MY_LIBVPX_PATH)/vp8/common/blockd.c \
     $(MY_LIBVPX_PATH)/vp8/common/debugmodes.c \
     $(MY_LIBVPX_PATH)/vp8/common/entropy.c \
     $(MY_LIBVPX_PATH)/vp8/common/entropymode.c \
     $(MY_LIBVPX_PATH)/vp8/common/entropymv.c \
     $(MY_LIBVPX_PATH)/vp8/common/extend.c \
     $(MY_LIBVPX_PATH)/vp8/common/filter.c \
     $(MY_LIBVPX_PATH)/vp8/common/findnearmv.c \
     $(MY_LIBVPX_PATH)/vp8/common/generic/systemdependent.c \
     $(MY_LIBVPX_PATH)/vp8/common/idctllm.c \
     $(MY_LIBVPX_PATH)/vp8/common/invtrans.c \
     $(MY_LIBVPX_PATH)/vp8/common/loopfilter.c \
     $(MY_LIBVPX_PATH)/vp8/common/loopfilter_filters.c \
     $(MY_LIBVPX_PATH)/vp8/common/mbpitch.c \
     $(MY_LIBVPX_PATH)/vp8/common/modecont.c \
     $(MY_LIBVPX_PATH)/vp8/common/modecontext.c \
     $(MY_LIBVPX_PATH)/vp8/common/quant_common.c \
     $(MY_LIBVPX_PATH)/vp8/common/recon.c \
     $(MY_LIBVPX_PATH)/vp8/common/reconinter.c \
     $(MY_LIBVPX_PATH)/vp8/common/reconintra.c \
     $(MY_LIBVPX_PATH)/vp8/common/reconintra4x4.c \
     $(MY_LIBVPX_PATH)/vp8/common/setupintrarecon.c \
     $(MY_LIBVPX_PATH)/vp8/common/swapyv12buffer.c \
     $(MY_LIBVPX_PATH)/vp8/common/textblit.c \
     $(MY_LIBVPX_PATH)/vp8/common/treecoder.c \
     $(MY_LIBVPX_PATH)/vp8/vp8_cx_iface.c \
     $(MY_LIBVPX_PATH)/vp8/vp8_dx_iface.c \
     $(MY_LIBVPX_PATH)/vpx_config.c \
     $(MY_LIBVPX_PATH)/vpx/src/vpx_codec.c \
     $(MY_LIBVPX_PATH)/vpx/src/vpx_decoder.c \
     $(MY_LIBVPX_PATH)/vpx/src/vpx_image.c \
     $(MY_LIBVPX_PATH)/vpx_mem/vpx_mem.c \
     $(MY_LIBVPX_PATH)/vpx_scale/generic/vpxscale.c \
     $(MY_LIBVPX_PATH)/vpx_scale/generic/yv12config.c \
     $(MY_LIBVPX_PATH)/vpx_scale/generic/yv12extend.c \
     $(MY_LIBVPX_PATH)/vpx_scale/generic/gen_scalers.c \
     $(MY_LIBVPX_PATH)/vpx_scale/generic/scalesystemdependent.c \
     $(MY_LIBVPX_PATH)/vpx/src/vpx_encoder.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/bitstream.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/boolhuff.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/dct.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/encodeframe.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/encodeintra.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/encodemb.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/encodemv.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/ethreading.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/firstpass.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/generic/csystemdependent.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/mcomp.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/modecosts.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/pickinter.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/picklpf.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/psnr.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/quantize.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/ratectrl.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/rdopt.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/sad_c.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/segmentation.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/tokenize.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/treewriter.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/onyx_if.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/temporal_filter.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/variance_c.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/dboolhuff.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/decodemv.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/decodframe.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/dequantize.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/detokenize.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/generic/dsystemdependent.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/onyxd_if.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/reconintra_mt.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/threading.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/idct_blk.c \
     $(MY_LIBVPX_PATH)/vp8/common/arm/arm_systemdependent.c \
     $(MY_LIBVPX_PATH)/vp8/encoder/arm/arm_csystemdependent.c \
     $(MY_LIBVPX_PATH)/vp8/decoder/arm/arm_dsystemdependent.c \

LOCAL_CFLAGS := \
    -DHAVE_CONFIG_H=vpx_config.h \
    -include $(LOCAL_PATH)/third_party/libvpx/source/config/android/vpx_config.h

LOCAL_MODULE := libwebrtc_vpx

LOCAL_C_INCLUDES := \
    external/libvpx \
    external/libvpx/vpx_ports \
    external/libvpx/vp8/common \
    external/libvpx/vp8/encoder \
    external/libvpx/vp8 \
    external/libvpx/vpx_codec 

include $(BUILD_STATIC_LIBRARY)
