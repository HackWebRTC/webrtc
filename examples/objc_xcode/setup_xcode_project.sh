#!/bin/bash

gn gen out/xcode_ios_arm64 --args='target_os="ios" target_cpu="arm64" ios_enable_code_signing=false use_xcode_clang=true is_component_build=false enable_ios_bitcode=false use_goma=false is_debug=true' --ide=xcode && \
ninja -C out/xcode_ios_arm64 framework_objc && \
cd examples/objc_xcode && \
xcodegen
