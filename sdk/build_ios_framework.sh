#!/bin/bash

set -e

pushd third_party/ffmpeg

git reset --hard
git apply ../../sdk/ffmpeg-ios-build.diff

python chromium/scripts/build_ffmpeg.py ios arm64 --branding Chrome -- \
    --disable-asm \
    --disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
    --disable-protocols --enable-protocol=file \
    --disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
    --disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 --enable-demuxer=mpegts \
    --disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
    --disable-muxers --enable-muxer=matroska

python chromium/scripts/build_ffmpeg.py ios x64 --branding Chrome -- \
    --disable-asm \
    --disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
    --disable-protocols --enable-protocol=file \
    --disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
    --disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 --enable-demuxer=mpegts \
    --disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
    --disable-muxers --enable-muxer=matroska

./chromium/scripts/copy_config.sh
./chromium/scripts/generate_gn.py

popd

gn gen out/ios_release_arm64 --args='target_os = "ios" target_cpu = "arm64" ios_enable_code_signing = false use_xcode_clang = true is_component_build = false is_debug = false ios_deployment_target = "10.0" rtc_libvpx_build_vp9 = false enable_ios_bitcode = true use_goma = false enable_dsyms = true enable_stripping = true ffmpeg_branding="Chrome"'
ninja -C out/ios_release_arm64 framework_objc

gn gen out/ios_release_x64 --args='target_os = "ios" target_cpu = "x64" ios_enable_code_signing = false use_xcode_clang = true is_component_build = false is_debug = false ios_deployment_target = "10.0" rtc_libvpx_build_vp9 = false enable_ios_bitcode = true use_goma = false enable_dsyms = true enable_stripping = true ffmpeg_branding="Chrome"'
ninja -C out/ios_release_x64 framework_objc
