#!/bin/bash

set -e

gn gen out/xcode_ios_arm64 --args='target_os = "ios" target_cpu = "arm64" ios_enable_code_signing = false use_xcode_clang = true is_component_build = false ios_deployment_target = "10.0" rtc_libvpx_build_vp9 = false enable_ios_bitcode = true use_goma = false ffmpeg_branding="Chrome" is_debug=true' --ide=xcode
ninja -C out/xcode_ios_arm64 framework_objc

pushd examples/objc_xcode
xcodegen
popd
