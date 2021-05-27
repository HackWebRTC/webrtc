#!/bin/bash

set -e

py=$(python -c 'import sys; print(".".join(map(str, sys.version_info[0:1])))')
if [[ "$py" != "2" ]]; then
  echo "Please use py2 env"
  exit
fi

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

gn gen out/ios_release_arm64 --args='target_os="ios" target_cpu="arm64" rtc_enable_symbol_export=true ios_enable_code_signing=false is_component_build=false use_goma=false rtc_libvpx_build_vp9=false ffmpeg_branding="Chrome" is_debug=false enable_dsyms=true enable_stripping = true rtc_include_tests=false'
ninja -C out/ios_release_arm64 framework_objc

gn gen out/ios_release_x64 --args='target_os="ios" target_cpu="x64" rtc_enable_symbol_export=true ios_enable_code_signing=false is_component_build=false use_goma=false rtc_libvpx_build_vp9=false ffmpeg_branding="Chrome" is_debug=false enable_dsyms=true enable_stripping = true rtc_include_tests=false'
ninja -C out/ios_release_x64 framework_objc
