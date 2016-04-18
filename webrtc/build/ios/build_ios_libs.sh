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

# Globals.
SCRIPT_DIR=$(cd $(dirname $0) && pwd)
WEBRTC_BASE_DIR=${SCRIPT_DIR}/../../..
GYP_WEBRTC_SCRIPT=${WEBRTC_BASE_DIR}/webrtc/build/gyp_webrtc.py
EXPORT_HEADERS_SCRIPT=${SCRIPT_DIR}/export_headers.py
MERGE_SCRIPT=${SCRIPT_DIR}/merge_ios_libs.py

function check_preconditions {
  # Check for Darwin.
  if [[ ! $(uname) = "Darwin" ]]; then
    echo "OS/X required." >&2
    exit 1
  fi

  # Check for libtool.
  if [[ -z $(which libtool) ]]; then
    echo "Missing libtool binary." >&2
    exit 1
  fi

  # Check for GYP generator.
  if [[ ! -x ${GYP_WEBRTC_SCRIPT} ]]; then
    echo "Failed to find gyp generator." >&2
    exit 1
  fi

  # Check for export headers script.
  if [[ ! -x ${EXPORT_HEADERS_SCRIPT} ]]; then
    echo "Failed to find export headers script." >&2
    exit 1
  fi

  # Check for merge script.
  if [[ ! -x ${MERGE_SCRIPT} ]]; then
    echo "Failed to find library merging script." >&2
    exit 1
  fi
}

function build_webrtc {
  local base_output_dir=$1
  local flavor=$2
  local target_arch=$3
  local ninja_output_dir=${base_output_dir}/${target_arch}_ninja
  local library_output_dir=${base_output_dir}/${target_arch}_libs
  if [[ ${target_arch} = 'arm' || ${target_arch} = 'arm64' ]]; then
    flavor="${flavor}-iphoneos"
  else
    flavor="${flavor}-iphonesimulator"
  fi
  export GYP_DEFINES="OS=ios target_arch=${target_arch} use_objc_h264=1 \
clang_xcode=1 ios_override_visibility=1"
  export GYP_GENERATORS="ninja"
  export GYP_GENERATOR_FLAGS="output_dir=${ninja_output_dir}"

  # GYP generation requires relative path for some reason.
  pushd ${WEBRTC_BASE_DIR}
  ${GYP_WEBRTC_SCRIPT} webrtc/build/ios/merge_ios_libs.gyp
  popd
  if [[ ${USE_LEGACY_API} -eq 1 ]]; then
    ninja -C ${ninja_output_dir}/${flavor} libjingle_peerconnection_objc_no_op
  else
    ninja -C ${ninja_output_dir}/${flavor} webrtc_api_objc_no_op
  fi
  mkdir -p ${library_output_dir}

  for f in ${ninja_output_dir}/${flavor}/*.a
  do
    ln -sf "${f}" "${library_output_dir}/$(basename ${f})"
  done
}

function clean_artifacts {
  local output_dir=$1
  if [[ -d ${output_dir} ]]; then
    rm -r ${output_dir}
  fi
}

function usage {
  echo "WebRTC iOS FAT libraries build script."
  echo "Each architecture is compiled separately before being merged together."
  echo "By default, the fat libraries will be created in out_ios_libs/fat_libs."
  echo "The headers will be copied to out_ios_libs/include."
  echo "Usage: $0 [-h] [-c] [-o]"
  echo "  -h Print this help."
  echo "  -c Removes generated build output."
  echo "  -o Specifies a directory to output build artifacts to."
  echo "     If specified together with -c, deletes the dir."
  exit 0
}

check_preconditions

# Set default arguments.
# Output directory for build artifacts.
OUTPUT_DIR=${WEBRTC_BASE_DIR}/out_ios_libs
# Flag to build the new or legacy version of the API.
USE_LEGACY_API=0
PERFORM_CLEAN=0

# Parse arguments.
while getopts "hco:" opt; do
  case "${opt}" in
    h) usage;;
    c) PERFORM_CLEAN=1;;
    o) OUTPUT_DIR="${OPTARG}";;
    *)
      usage
      exit 1
      ;;
  esac
done

if [[ ${PERFORM_CLEAN} -ne 0 ]]; then
  clean_artifacts ${OUTPUT_DIR}
  exit 0
fi

# Build all the common architectures.
archs=( "arm" "arm64" "ia32" "x64" )
for arch in "${archs[@]}"
do
  echo "Building WebRTC arch: ${arch}"
  build_webrtc ${OUTPUT_DIR} "Profile" $arch
done

# Export header files.
${EXPORT_HEADERS_SCRIPT} ${OUTPUT_DIR} ${USE_LEGACY_API}

# Merge the libraries together.
${MERGE_SCRIPT} ${OUTPUT_DIR}
