#!/bin/bash

py=$(python -c 'import sys; print(".".join(map(str, sys.version_info[0:1])))')
if [[ "$py" != "3" ]]; then
  echo "Please use py3 env"
  exit
fi

if [ ! -d "$1" ]; then
  echo "Please set valid target output path"
  exit
fi
OUTPUT_PATH=$1
rm -rf $OUTPUT_PATH/apple/WebRTC.xcframework \
  $OUTPUT_PATH/symbols/ios_arm64 \
  $OUTPUT_PATH/symbols/ios_sim_x64 \
  $OUTPUT_PATH/symbols/ios_sim_arm64 \
  $OUTPUT_PATH/symbols/mac_x64/ \
  $OUTPUT_PATH/symbols/mac_arm64/
mkdir -p out/ios_sim out/mac \
  $OUTPUT_PATH/symbols/ios_arm64/ \
  $OUTPUT_PATH/symbols/ios_sim_x64/ \
  $OUTPUT_PATH/symbols/ios_sim_arm64/ \
  $OUTPUT_PATH/symbols/mac_x64/ \
  $OUTPUT_PATH/symbols/mac_arm64/

set -e

export PATH=$(pwd)/third_party/llvm-build/Release+Asserts/bin:$PATH

if [ "$2" != "--skip-build-ffmpeg" ]; then
  pushd third_party/ffmpeg

  git reset --hard
  git apply ../../sdk/ffmpeg_build.diff
  if [ ! -d 'ffmpeg-webrtc-build-scripts' ]; then
    git clone https://github.com/HackWebRTC/ffmpeg-webrtc-build-scripts.git
  fi

  python ffmpeg-webrtc-build-scripts/build_ffmpeg.py ios arm64 --branding Chrome -- \
      --disable-asm \
      --disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
      --disable-protocols --enable-protocol=file \
      --disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
      --disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 --enable-demuxer=mpegts \
      --disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
      --disable-muxers --enable-muxer=matroska \
      --enable-swresample

  python ffmpeg-webrtc-build-scripts/build_ffmpeg.py ios x64 --branding Chrome -- \
      --disable-asm \
      --disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
      --disable-protocols --enable-protocol=file \
      --disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
      --disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 --enable-demuxer=mpegts \
      --disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
      --disable-muxers --enable-muxer=matroska \
      --enable-swresample

  ./chromium/scripts/copy_config.sh
  python ffmpeg-webrtc-build-scripts/generate_gn.py

  popd
fi

gn gen out/ios_release_arm64 --args='target_os="ios" target_cpu="arm64" target_environment="device" proprietary_codecs=true ffmpeg_branding="Chrome" rtc_libvpx_build_vp9=false rtc_enable_symbol_export=true ios_enable_code_signing=false is_component_build=false is_debug=false enable_dsyms=true enable_stripping=true rtc_include_tests=false'
ninja -C out/ios_release_arm64 framework_objc

gn gen out/ios_release_arm64_sim --args='target_os="ios" target_cpu="arm64" target_environment="simulator" proprietary_codecs=true ffmpeg_branding="Chrome" rtc_libvpx_build_vp9=false rtc_enable_symbol_export=true ios_enable_code_signing=false is_component_build=false is_debug=false enable_dsyms=true enable_stripping=true rtc_include_tests=false'
ninja -C out/ios_release_arm64_sim framework_objc

gn gen out/ios_release_x64 --args='target_os="ios" target_cpu="x64" target_environment="simulator" proprietary_codecs=true ffmpeg_branding="Chrome" rtc_libvpx_build_vp9=false rtc_enable_symbol_export=true ios_enable_code_signing=false is_component_build=false is_debug=false enable_dsyms=true enable_stripping=true rtc_include_tests=false'
ninja -C out/ios_release_x64 framework_objc

gn gen out/mac_release_x64 --args='target_os="mac" target_cpu="x64" rtc_libvpx_build_vp9=false rtc_enable_symbol_export=true ios_enable_code_signing=false is_component_build=false is_debug=false enable_dsyms=true enable_stripping=true rtc_include_tests=false'
ninja -C out/mac_release_x64 mac_framework_objc

gn gen out/mac_release_arm64 --args='target_os="mac" target_cpu="arm64" rtc_libvpx_build_vp9=false rtc_enable_symbol_export=true ios_enable_code_signing=false is_component_build=false is_debug=false enable_dsyms=true enable_stripping=true rtc_include_tests=false'
ninja -C out/mac_release_arm64 mac_framework_objc

rm -rf out/ios_sim/WebRTC.framework
cp -R out/ios_release_x64/WebRTC.framework out/ios_sim/
lipo -create out/ios_release_arm64_sim/WebRTC.framework/WebRTC \
    out/ios_release_x64/WebRTC.framework/WebRTC \
    -output out/ios_sim/WebRTC-ios-sim-arm64-x64
cp out/ios_sim/WebRTC-ios-sim-arm64-x64 out/ios_sim/WebRTC.framework/WebRTC

rm -rf out/mac/WebRTC.framework
cp -R out/mac_release_x64/WebRTC.framework out/mac/
lipo -create out/mac_release_x64/WebRTC.framework/Versions/A/WebRTC \
    out/mac_release_arm64/WebRTC.framework/Versions/A/WebRTC \
    -output out/mac/WebRTC-mac-x64-arm64
cp out/mac/WebRTC-mac-x64-arm64 out/mac/WebRTC.framework/Versions/A/WebRTC

rm -rf out/WebRTC.xcframework
xcodebuild -create-xcframework -framework out/ios_release_arm64/WebRTC.framework \
    -framework out/ios_sim/WebRTC.framework \
    -framework out/mac/WebRTC.framework \
    -output out/WebRTC.xcframework

cp -R out/WebRTC.xcframework $OUTPUT_PATH/apple/
cp -R out/ios_release_arm64/WebRTC.dSYM $OUTPUT_PATH/symbols/ios_arm64/
cp -R out/ios_release_x64/WebRTC.dSYM $OUTPUT_PATH/symbols/ios_sim_x64/
cp -R out/ios_release_arm64_sim/WebRTC.dSYM $OUTPUT_PATH/symbols/ios_sim_arm64/
cp -R out/mac_release_x64/WebRTC.dSYM $OUTPUT_PATH/symbols/mac_x64/
cp -R out/mac_release_arm64/WebRTC.dSYM $OUTPUT_PATH/symbols/mac_arm64/
