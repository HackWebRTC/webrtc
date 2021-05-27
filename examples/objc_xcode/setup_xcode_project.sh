#!/bin/bash

set -e

gn gen out/xcode_ios_arm64 --args='target_os="ios" target_cpu="arm64" rtc_enable_symbol_export=true ios_enable_code_signing=false is_component_build=false use_goma=false ffmpeg_branding="Chrome" is_debug=true enable_dsyms=true rtc_include_tests=false' --ide=xcode
ninja -C out/xcode_ios_arm64 framework_objc

pushd examples/objc_xcode
xcodegen
popd
