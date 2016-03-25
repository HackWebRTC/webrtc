#!/bin/bash

#  Copyright 2015 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# Generates dynamic FAT framework for iOS in out_ios_framework.

# Check for Darwin.
if [[ ! $(uname) = "Darwin" ]]; then
  echo "OS X required." >&2
fi

# Check for iOS library build script.
SCRIPT_DIR=$(dirname $0)
WEBRTC_BASE_DIR=${SCRIPT_DIR}/../../..
BUILD_WEBRTC_SCRIPT=${WEBRTC_BASE_DIR}/webrtc/build/ios/build_ios_libs.sh
if [[ ! -x ${BUILD_WEBRTC_SCRIPT} ]]; then
  echo "Failed to find iOS library build script." >&2
  exit 1
fi
# Check for flatten iOS headers script.
FLATTEN_HEADERS_SCRIPT=${WEBRTC_BASE_DIR}/webrtc/build/ios/flatten_ios_headers
if [[ ! -x ${FLATTEN_HEADERS_SCRIPT} ]]; then
  echo "Failed to find flatten iOS headers script." >&2
  exit 1
fi

pushd ${WEBRTC_BASE_DIR}
LIB_BASE_DIR=out_ios_libs
FRAMEWORK_BASE_DIR=out_ios_framework

# Build static libraries for iOS.
${BUILD_WEBRTC_SCRIPT}
if [ $? -ne 0 ]; then
  echo "Failed to build iOS static libraries." >&2
  exit 1
fi

# Flatten the directory structure for iOS headers.
${FLATTEN_HEADERS_SCRIPT} ${LIB_BASE_DIR} ${FRAMEWORK_BASE_DIR}
if [ $? -ne 0 ]; then
  echo "Failed to flatten iOS headers." >&2
  exit 1
fi

# Replace full paths for headers with framework paths.
SED_PATTERN='
  s/(\#import )\"webrtc\/api\/objc\/(.*)\"/\1<WebRTC\/\2>/g;
  s/(\#import )\"webrtc\/base\/objc\/(.*)\"/\1<WebRTC\/\2>/g;
  s/(\#include )\"webrtc\/base\/objc\/(.*)\"/\1<WebRTC\/\2>/g;
'
sed -E -i '' "$SED_PATTERN" ${FRAMEWORK_BASE_DIR}/include/*.h

SDK_DIR=webrtc/build/ios/SDK
PROJECT_DIR=${SDK_DIR}/Framework
# Build the framework.
pushd ${PROJECT_DIR}
xcodebuild -project WebRTC.xcodeproj -scheme WebRTC -configuration Release \
    build CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
xcodebuild -project WebRTC.xcodeproj -scheme WebRTC -configuration Release \
    build -destination 'platform=iOS Simulator,name=iPhone 6'
popd

# Copy podspec, framework, dSYM and LICENSE to FRAMEWORK_BASE_DIR
DEVICE_BUILD_DIR=${PROJECT_DIR}/build/Release-iphoneos
cp ${SDK_DIR}/WebRTC.podspec ${FRAMEWORK_BASE_DIR}/
cp -R ${DEVICE_BUILD_DIR}/WebRTC.framework ${FRAMEWORK_BASE_DIR}/
cp -R ${DEVICE_BUILD_DIR}/WebRTC.framework.dSYM ${FRAMEWORK_BASE_DIR}/
cp -R webrtc/LICENSE ${FRAMEWORK_BASE_DIR}/

# Combine multiple architectures
SIMULATOR_BUILD_DIR=${PROJECT_DIR}/build/Release-iphonesimulator
DYLIB_PATH=WebRTC.framework/WebRTC
DWARF_PATH=WebRTC.framework.dSYM/Contents/Resources/DWARF/WebRTC
lipo ${FRAMEWORK_BASE_DIR}/${DYLIB_PATH} ${SIMULATOR_BUILD_DIR}/${DYLIB_PATH} \
    -create -output ${FRAMEWORK_BASE_DIR}/${DYLIB_PATH}
lipo ${FRAMEWORK_BASE_DIR}/${DWARF_PATH} ${SIMULATOR_BUILD_DIR}/${DWARF_PATH} \
    -create -output ${FRAMEWORK_BASE_DIR}/${DWARF_PATH}

popd
