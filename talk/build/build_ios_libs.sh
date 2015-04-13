#!/bin/bash
#
# libjingle
# Copyright 2015 Google Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Generates static FAT libraries for ios in out_ios_libs.

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
WEBRTC_BASE_DIR=${SCRIPT_DIR}/../..
GYP_WEBRTC_SCRIPT=${WEBRTC_BASE_DIR}/webrtc/build/gyp_webrtc
if [[ ! -x ${GYP_WEBRTC_SCRIPT} ]]; then
  echo "Failed to find gyp generator." >&2
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
  TARGET_SUBARCH=$4
  if [[ ${TARGET_ARCH} = 'armv7' || ${TARGET_ARCH} = 'arm64' ]]; then
    FLAVOR="${FLAVOR}-iphoneos"
  else
    FLAVOR="${FLAVOR}-iphonesimulator"
  fi
  export GYP_DEFINES="OS=ios use_openssl=1"
  export GYP_DEFINES="${GYP_DEFINES} target_arch=${TARGET_ARCH}"
  if [[ -n ${TARGET_SUBARCH} ]]; then
    export GYP_DEFINES="${GYP_DEFINES} target_subarch=${TARGET_SUBARCH}"
  fi
  export GYP_GENERATORS="ninja"
  export GYP_GENERATOR_FLAGS="output_dir=${OUTPUT_DIR}"
  webrtc/build/gyp_webrtc talk/build/merge_ios_libs.gyp
  ninja -C ${OUTPUT_DIR}/${FLAVOR} libjingle_peerconnection_objc_no_op
  mkdir -p ${LIBRARY_BASE_DIR}/${TARGET_ARCH}
  mv ${OUTPUT_DIR}/${FLAVOR}/*.a ${LIBRARY_BASE_DIR}/${TARGET_ARCH}
}

# Build all the common architectures.
build_webrtc "out_ios_armv7" "Release" "armv7" "arm32"
build_webrtc "out_ios_arm64" "Release" "arm64" "arm64"
build_webrtc "out_ios_ia32" "Release" "ia32" "arm32"
build_webrtc "out_ios_x86_64" "Release" "x64" "arm64"

popd

# Merge the libraries together.
${MERGE_SCRIPT} ${WEBRTC_BASE_DIR}/${LIBRARY_BASE_DIR}
