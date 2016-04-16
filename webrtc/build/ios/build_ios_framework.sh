#!/bin/bash

#  Copyright 2015 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# Generates dynamic FAT framework for iOS in out_ios_framework.

# Exit on errors.
set -e

# Globals.
SCRIPT_DIR=$(cd $(dirname $0) && pwd)
WEBRTC_BASE_DIR=${SCRIPT_DIR}/../../..
BUILD_WEBRTC_SCRIPT=${SCRIPT_DIR}/build_ios_libs.sh
FLATTEN_HEADERS_SCRIPT=${SCRIPT_DIR}/flatten_ios_headers.py
SDK_DIR=${SCRIPT_DIR}/SDK
FRAMEWORK_PROJECT_DIR=${SDK_DIR}/Framework
FRAMEWORK_PROJECT_PATH=${FRAMEWORK_PROJECT_DIR}/WebRTC.xcodeproj

function check_preconditions {
  # Check for Darwin.
  if [[ ! $(uname) = "Darwin" ]]; then
    echo "OS X required." >&2
    exit 1
  fi
  if [[ ! -x ${BUILD_WEBRTC_SCRIPT} ]]; then
    echo "Failed to find iOS library build script." >&2
    exit 1
  fi
  if [[ ! -x ${FLATTEN_HEADERS_SCRIPT} ]]; then
    echo "Failed to find flatten iOS headers script." >&2
    exit 1
  fi
  if [[ ! -x ${FRAMEWORK_PROJECT_PATH} ]]; then
    echo "Failed to find framework XCode project." >&2
    exit 1
  fi
}

function clean_artifacts {
  # Make XCode clean up after itself.
  xcodebuild -project ${FRAMEWORK_PROJECT_PATH} -scheme WebRTC \
      -configuration Release clean
  xcodebuild -project ${FRAMEWORK_PROJECT_PATH} -scheme WebRTC \
      -configuration Release clean \
      -destination "platform=iOS Simulator,name=iPhone 6"
  # Remove remaining directory that XCode doesn't delete.
  XCODE_BUILD_DIR=${FRAMEWORK_PROJECT_DIR}/build
  if [[ -d ${XCODE_BUILD_DIR} ]]; then
    rm -r ${XCODE_BUILD_DIR}
  fi

  # Remove the temporary framework header dir.
  if [[ -d ${FRAMEWORK_INCLUDE_DIR} ]]; then
    rm -r ${FRAMEWORK_INCLUDE_DIR}
  fi

  # Remove the generated framework.
  if [[ -d ${FRAMEWORK_OUTPUT_DIR} ]]; then
    rm -r ${FRAMEWORK_OUTPUT_DIR}
  fi

  # Let the other script clean up after itself.
  ${BUILD_WEBRTC_SCRIPT} -c
}

function usage {
  echo "WebRTC iOS Framework build script."
  echo "Builds a dynamic Framework for the WebRTC APIs."
  echo "Compiles various architectures and edits header paths as required."
  echo "Usage: $0 [-h] [-c]"
  echo "  -h Print this help."
  echo "  -c Removes generated build output."
  exit 0
}

check_preconditions

# Set the output directories for the various build artifacts.
# For convenience we'll output some generated files in the same directory
# as the one we used to output the generated statis libraries.
LIB_OUTPUT_DIR=${WEBRTC_BASE_DIR}/out_ios_libs
INCLUDE_OUTPUT_DIR=${LIB_OUTPUT_DIR}/include
FRAMEWORK_INCLUDE_DIR=${LIB_OUTPUT_DIR}/framework_include
FRAMEWORK_OUTPUT_DIR=${WEBRTC_BASE_DIR}/out_ios_framework
PERFORM_CLEAN=0

# Parse arguments.
while getopts "hc" opt; do
  case "${opt}" in
    h) usage;;
    c) PERFORM_CLEAN=1;;
    *)
      usage
      exit 1
      ;;
  esac
done

if [[ ${PERFORM_CLEAN} -ne 0 ]]; then
  clean_artifacts
  exit 0
fi

# Build static libraries for iOS.
${BUILD_WEBRTC_SCRIPT} -o ${LIB_OUTPUT_DIR}

# Flatten the directory structure for iOS headers generated from building the
# static libraries.
${FLATTEN_HEADERS_SCRIPT} ${INCLUDE_OUTPUT_DIR} ${FRAMEWORK_INCLUDE_DIR}

# Replace full paths for headers with framework paths.
SED_PATTERN='
  s/(\#import )\"webrtc\/api\/objc\/(.*)\"/\1<WebRTC\/\2>/g;
  s/(\#import )\"webrtc\/base\/objc\/(.*)\"/\1<WebRTC\/\2>/g;
  s/(\#include )\"webrtc\/base\/objc\/(.*)\"/\1<WebRTC\/\2>/g;
'
sed -E -i '' "$SED_PATTERN" ${FRAMEWORK_INCLUDE_DIR}/*.h

# Build the framework.
xcodebuild -project ${FRAMEWORK_PROJECT_PATH} -scheme WebRTC \
    -configuration Release build \
    CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
xcodebuild -project ${FRAMEWORK_PROJECT_PATH} -scheme WebRTC \
    -configuration Release build \
    -destination "platform=iOS Simulator,name=iPhone 6"

# XCode should output the build artifacts to the following directories.
DEVICE_BUILD_DIR=${FRAMEWORK_PROJECT_DIR}/build/Release-iphoneos
SIMULATOR_BUILD_DIR=${FRAMEWORK_PROJECT_DIR}/build/Release-iphonesimulator

# Copy podspec, framework, dSYM and LICENSE to FRAMEWORK_OUTPUT_DIR.
mkdir -p ${FRAMEWORK_OUTPUT_DIR}
cp ${SDK_DIR}/WebRTC.podspec ${FRAMEWORK_OUTPUT_DIR}/
cp -R ${DEVICE_BUILD_DIR}/WebRTC.framework ${FRAMEWORK_OUTPUT_DIR}/
cp -R ${DEVICE_BUILD_DIR}/WebRTC.framework.dSYM ${FRAMEWORK_OUTPUT_DIR}/
cp -R ${WEBRTC_BASE_DIR}/webrtc/LICENSE ${FRAMEWORK_OUTPUT_DIR}/

# Combine multiple architectures.
DYLIB_PATH=WebRTC.framework/WebRTC
DWARF_PATH=WebRTC.framework.dSYM/Contents/Resources/DWARF/WebRTC
lipo ${FRAMEWORK_OUTPUT_DIR}/${DYLIB_PATH} \
    ${SIMULATOR_BUILD_DIR}/${DYLIB_PATH} \
    -create -output ${FRAMEWORK_OUTPUT_DIR}/${DYLIB_PATH}
lipo ${FRAMEWORK_OUTPUT_DIR}/${DWARF_PATH} \
    ${SIMULATOR_BUILD_DIR}/${DWARF_PATH} \
    -create -output ${FRAMEWORK_OUTPUT_DIR}/${DWARF_PATH}
