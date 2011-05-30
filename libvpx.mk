# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

MY_LIBVPX_DEC_SRC = \
     vpx/src/vpx_codec.c \
     vpx/src/vpx_decoder.c \
     vpx/src/vpx_image.c \
     vpx_mem/vpx_mem.c \
     vpx_scale/generic/vpxscale.c \
     vpx_scale/generic/yv12config.c \
     vpx_scale/generic/yv12extend.c \
     vpx_scale/generic/gen_scalers.c \
     vpx_scale/generic/scalesystemdependant.c \
     vp8/common/alloccommon.c \
     vp8/common/blockd.c \
     vp8/common/debugmodes.c \
     vp8/common/entropy.c \
     vp8/common/entropymode.c \
     vp8/common/entropymv.c \
     vp8/common/extend.c \
     vp8/common/filter.c \
     vp8/common/findnearmv.c \
     vp8/common/generic/systemdependent.c \
     vp8/common/idctllm.c \
     vp8/common/invtrans.c \
     vp8/common/loopfilter.c \
     vp8/common/loopfilter_filters.c \
     vp8/common/mbpitch.c \
     vp8/common/modecont.c \
     vp8/common/modecontext.c \
     vp8/common/quant_common.c \
     vp8/common/recon.c \
     vp8/common/reconinter.c \
     vp8/common/reconintra.c \
     vp8/common/reconintra4x4.c \
     vp8/common/setupintrarecon.c \
     vp8/common/swapyv12buffer.c \
     vp8/common/textblit.c \
     vp8/common/treecoder.c \
     vp8/vp8_cx_iface.c \
     vp8/vp8_dx_iface.c \
     vp8/decoder/generic/dsystemdependent.c \
     vp8/decoder/dboolhuff.c \
     vp8/decoder/decodemv.c \
     vp8/decoder/decodframe.c \
     vp8/decoder/dequantize.c \
     vp8/decoder/detokenize.c \
     vp8/decoder/onyxd_if.c \
     vp8/decoder/reconintra_mt.c \
     vp8/decoder/threading.c \
     vpx_config.c \
     vp8/decoder/idct_blk.c 

MY_LIBVPX_ENC_PATH = ../libvpx

LOCAL_SRC_FILES = \
     $(MY_LIBVPX_ENC_PATH)/vpx/src/vpx_encoder.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/bitstream.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/boolhuff.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/dct.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/encodeframe.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/encodeintra.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/encodemb.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/encodemv.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/ethreading.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/firstpass.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/arm/arm_csystemdependent.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/mcomp.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/modecosts.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/pickinter.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/picklpf.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/psnr.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/quantize.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/ratectrl.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/rdopt.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/sad_c.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/segmentation.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/tokenize.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/treewriter.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/onyx_if.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/temporal_filter.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/arm/variance_arm.c \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/arm/variance_arm.h \
     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/variance_c.c 

#     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/generic/csystemdependent.c 
#     $(MY_LIBVPX_ENC_PATH)/vp8/encoder/variance_c.c 
#     $(MY_LIBVPX_ENC_PATH)/vp8/decoder/idct_blk.c \
#     md5_utils.c 
#     args.c \
#     tools_common.c \
#     nestegg/halloc/src/halloc.c \
#     nestegg/src/nestegg.c \
#     vpxdec.c \
#     y4minput.c \
#     libmkv/EbmlWriter.c \
#     vpxenc.c \
#     simple_decoder.c \
#     postproc.c \
#     decode_to_md5.c \
#     simple_encoder.c \
#     twopass_encoder.c \
#     force_keyframe.c \
#     decode_with_drops.c \
#     error_resilient.c \
#     vp8_scalable_patterns.c \
#     vp8_set_maps.c \
#     vp8cx_set_ref.c 

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H=vpx_config.h \
	-include $(LOCAL_PATH)/third_party_mods/libvpx/source/config/android/vpx_config.h

LOCAL_MODULE := libwebrtc_vpx_enc

LOCAL_C_INCLUDES := \
    external/libvpx \
    external/libvpx/vpx_ports \
    external/libvpx/vp8/common \
    external/libvpx/vp8/encoder \
    external/libvpx/vp8 \
    external/libvpx/vpx_codec 

include $(BUILD_STATIC_LIBRARY)
