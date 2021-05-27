#!/bin/bash

set -e

gn gen out/xcode_ios_arm64 --args='target_os="ios" target_cpu="arm64" target_environment="device" proprietary_codecs=true ffmpeg_branding="Chrome" rtc_libvpx_build_vp9=false rtc_enable_symbol_export=true ios_enable_code_signing=false is_component_build=false is_debug=true enable_dsyms=true enable_stripping=true rtc_include_tests=false' --ide=xcode
ninja -C out/xcode_ios_arm64 framework_objc

pushd examples/objc_xcode
xcodegen
popd
