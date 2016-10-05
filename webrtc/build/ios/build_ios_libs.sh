#!/bin/bash

#  Copyright 2015 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# Generates static FAT libraries for ios in out_ios_libs.

# Exit on errors.
set -e

SCRIPT_DIR=$(cd $(dirname $0) && pwd)
WEBRTC_BASE_DIR=${SCRIPT_DIR}/../../..

SDK_OUTPUT_DIR=${WEBRTC_BASE_DIR}/out_ios_libs
SDK_LIB_NAME="librtc_sdk_objc.a"
GN_BASE_ARGS="target_os=\"ios\" is_debug=false ios_enable_code_signing=false \
rtc_libvpx_build_vp9=false"
GN_STATIC_TARGET_NAMES="rtc_sdk_peerconnection_objc field_trial_default \
metrics_default"

# TODO(tkchin): Restore functionality of old script to build dynamic framework,
# symbols and license file.

function build_static_webrtc {
  local arch=$1
  local xcode_arch=$2

  OUTPUT_DIR=${SDK_OUTPUT_DIR}/${arch}_libs
  OUTPUT_LIB=${OUTPUT_DIR}/${SDK_LIB_NAME}
  GN_ARGS="${GN_BASE_ARGS} target_cpu=\"${arch}\""
  gn gen ${OUTPUT_DIR} --args="${GN_ARGS}"
  ninja -C ${OUTPUT_DIR} ${GN_STATIC_TARGET_NAMES}
  # Combine the object files together into a single archive and strip debug
  # symbols.
  find ${OUTPUT_DIR}/obj -type f -name "*.o" |
     xargs ld -r -static -S -all_load -arch ${xcode_arch} -o ${OUTPUT_LIB}
}

# Build all the common architectures.
build_static_webrtc "arm" "armv7"
build_static_webrtc "arm64" "arm64"
build_static_webrtc "x86" "i386"
build_static_webrtc "x64" "x86_64"

# Combine the libraries.
lipo ${SDK_OUTPUT_DIR}/arm_libs/${SDK_LIB_NAME} \
     ${SDK_OUTPUT_DIR}/arm64_libs/${SDK_LIB_NAME} \
     ${SDK_OUTPUT_DIR}/x86_libs/${SDK_LIB_NAME} \
     ${SDK_OUTPUT_DIR}/x64_libs/${SDK_LIB_NAME} \
     -create -output ${SDK_OUTPUT_DIR}/${SDK_LIB_NAME}

echo "Done."
