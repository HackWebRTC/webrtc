#!/bin/bash

#  Copyright 2015 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# Generates static FAT libraries for ios in out_ios_libs.

# Flag to build the new or legacy version of the API.
USE_LEGACY_API=0

# Check for Darwin.
if [[ ! $(uname) = "Darwin" ]]; then
  echo "OS/X required." >&2
fi

# Check for libtool.
if [[ -z $(which libtool) ]]; then
  echo "Missing libtool binary." >&2
fi

# Check for GYP generator.
SCRIPT_DIR=$(dirname $0)
WEBRTC_BASE_DIR=${SCRIPT_DIR}/../../..
GYP_WEBRTC_SCRIPT=${WEBRTC_BASE_DIR}/webrtc/build/gyp_webrtc
if [[ ! -x ${GYP_WEBRTC_SCRIPT} ]]; then
  echo "Failed to find gyp generator." >&2
  exit 1
fi
# Check for export headers script.
EXPORT_HEADERS_SCRIPT=${SCRIPT_DIR}/export_headers
if [[ ! -x ${EXPORT_HEADERS_SCRIPT} ]]; then
  echo "Failed to find export headers script." >&2
  exit 1
fi
# Check for merge script.
MERGE_SCRIPT=${SCRIPT_DIR}/merge_ios_libs
if [[ ! -x ${MERGE_SCRIPT} ]]; then
  echo "Failed to find library merging script." >&2
  exit 1
fi

pushd ${WEBRTC_BASE_DIR}
LIBRARY_BASE_DIR="out_ios_libs"

function build_webrtc {
  OUTPUT_DIR=$1
  FLAVOR=$2
  TARGET_ARCH=$3
  if [[ ${TARGET_ARCH} = 'arm' || ${TARGET_ARCH} = 'arm64' ]]; then
    FLAVOR="${FLAVOR}-iphoneos"
  else
    FLAVOR="${FLAVOR}-iphonesimulator"
  fi
  export GYP_DEFINES="OS=ios target_arch=${TARGET_ARCH} use_objc_h264=1 \
clang_xcode=1 ios_override_visibility=1"
  export GYP_GENERATORS="ninja"
  export GYP_GENERATOR_FLAGS="output_dir=${OUTPUT_DIR}"
  webrtc/build/gyp_webrtc webrtc/build/ios/merge_ios_libs.gyp
  if [[ ${USE_LEGACY_API} -eq 1 ]]; then
    ninja -C ${OUTPUT_DIR}/${FLAVOR} libjingle_peerconnection_objc_no_op
  else
    ninja -C ${OUTPUT_DIR}/${FLAVOR} webrtc_api_objc_no_op
  fi
  mkdir -p ${LIBRARY_BASE_DIR}/${TARGET_ARCH}
  mv ${OUTPUT_DIR}/${FLAVOR}/*.a ${LIBRARY_BASE_DIR}/${TARGET_ARCH}
}

# Build all the common architectures.
build_webrtc "out_ios_arm" "Release" "arm"
build_webrtc "out_ios_arm64" "Release" "arm64"
build_webrtc "out_ios_ia32" "Release" "ia32"
build_webrtc "out_ios_x86_64" "Release" "x64"

popd

# Export header files.
${EXPORT_HEADERS_SCRIPT} ${WEBRTC_BASE_DIR}/${LIBRARY_BASE_DIR} \
    ${USE_LEGACY_API}

# Merge the libraries together.
${MERGE_SCRIPT} ${WEBRTC_BASE_DIR}/${LIBRARY_BASE_DIR}
