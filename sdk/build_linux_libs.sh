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
mkdir -p $OUTPUT_PATH/libs/linux/x64/ \
  $OUTPUT_PATH/libs/windows_linux/include/ \
  $OUTPUT_PATH/libs/symbols/linux_x64/

set -e

export PATH=$(pwd)/third_party/llvm-build/Release+Asserts/bin:$PATH

if [ "$2" != "--skip-build-ffmpeg" ]; then
  pushd third_party/ffmpeg

  git reset --hard
  git apply ../../sdk/ffmpeg_build.diff
  if [ ! -d 'ffmpeg-webrtc-build-scripts' ]; then
    git clone https://github.com/HackWebRTC/ffmpeg-webrtc-build-scripts.git
  fi

  python ffmpeg-webrtc-build-scripts/build_ffmpeg.py linux x64 --branding Chrome -- \
      --disable-asm \
      --disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
      --disable-protocols --enable-protocol=file \
      --disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
      --disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 --enable-demuxer=mpegts \
      --disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
      --disable-muxers --enable-muxer=matroska \
      --enable-swresample --enable-bsf=h264_mp4toannexb --enable-bsf=hevc_mp4toannexb

  ./chromium/scripts/copy_config.sh
  python ffmpeg-webrtc-build-scripts/generate_gn.py

  popd
fi

gn gen out/linux_release_x64 --args='target_os="linux" target_cpu="x64" is_clang=true use_custom_libcxx=true use_rtti=true is_debug=false treat_warnings_as_errors=false is_component_build=false enable_stripping=false rtc_include_tests=false rtc_libvpx_build_vp9=false rtc_use_h264=true rtc_include_internal_audio_device=true rtc_build_examples=false rtc_use_pipewire=false ffmpeg_branding="Chrome"'
ninja -C out/linux_release_x64 linux_pc_client
cp out/linux_release_x64/liblinux_pc_client.so $OUTPUT_PATH/libs/linux/x64/
cp out/linux_release_x64/liblinux_pc_client.so $OUTPUT_PATH/libs/symbols/linux_x64/
strip $OUTPUT_PATH/libs/linux/x64/liblinux_pc_client.so
cp sdk/desktop/lib_pc_client.h $OUTPUT_PATH/libs/windows_linux/include/
