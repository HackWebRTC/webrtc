#!/bin/bash

set -e

py=$(python -c 'import sys; print(".".join(map(str, sys.version_info[0:1])))')
if [[ "$py" != "3" ]]; then
  echo "Please use py3 env"
  exit
fi

if [ ! -d "$1" ]; then
  echo "Please set valid target prebuilt_libs path"
  exit
fi
PREBUILT_PATH=$1

export PATH=$(pwd)/third_party/llvm-build/Release+Asserts/bin:$PATH

if [ "$1" != "--skip-build-ffmpeg" ]; then
  pushd third_party/ffmpeg

  git reset --hard
  git apply ../../sdk/ffmpeg_build.diff
  if [ ! -d 'ffmpeg-webrtc-build-scripts' ]; then
    git clone https://github.com/HackWebRTC/ffmpeg-webrtc-build-scripts.git
  fi

  python ffmpeg-webrtc-build-scripts/build_ffmpeg.py android arm-neon --branding Chrome -- \
      --disable-asm \
      --disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
      --disable-protocols --enable-protocol=file \
      --disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
      --disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 --enable-demuxer=mpegts \
      --disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
      --disable-muxers --enable-muxer=matroska \
      --enable-swresample

  python ffmpeg-webrtc-build-scripts/build_ffmpeg.py android ia32 --branding Chrome -- \
      --disable-asm \
      --disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
      --disable-protocols --enable-protocol=file \
      --disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
      --disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 --enable-demuxer=mpegts \
      --disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
      --disable-muxers --enable-muxer=matroska \
      --enable-swresample

  python ffmpeg-webrtc-build-scripts/build_ffmpeg.py android arm64 --branding Chrome -- \
      --disable-asm \
      --disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
      --disable-protocols --enable-protocol=file \
      --disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
      --disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 --enable-demuxer=mpegts \
      --disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
      --disable-muxers --enable-muxer=matroska \
      --enable-swresample

  python ffmpeg-webrtc-build-scripts/build_ffmpeg.py android x64 --branding Chrome -- \
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

gn gen out/android_release_arm --args='target_os="android" target_cpu="arm" proprietary_codecs=true ffmpeg_branding="Chrome" is_debug=false is_component_build=false rtc_include_tests=false enable_rust=true enable_rust_cxx=true'
ninja -C out/android_release_arm libjingle_peerconnection_so
cp out/android_release_arm/libjingle_peerconnection_so.so $PREBUILT_PATH/armeabi-v7a/

gn gen out/android_release_x86 --args='target_os="android" target_cpu="x86" proprietary_codecs=true ffmpeg_branding="Chrome" is_debug=false is_component_build=false rtc_include_tests=false enable_rust=true enable_rust_cxx=true'
ninja -C out/android_release_x86 libjingle_peerconnection_so
cp out/android_release_x86/libjingle_peerconnection_so.so $PREBUILT_PATH/x86/

gn gen out/android_release_arm64 --args='target_os="android" target_cpu="arm64" proprietary_codecs=true ffmpeg_branding="Chrome" is_debug=false is_component_build=false rtc_include_tests=false enable_rust=true enable_rust_cxx=true'
ninja -C out/android_release_arm64 libjingle_peerconnection_so
cp out/android_release_arm64/libjingle_peerconnection_so.so $PREBUILT_PATH/arm64-v8a/

gn gen out/android_release_x64 --args='target_os="android" target_cpu="x64" proprietary_codecs=true ffmpeg_branding="Chrome" is_debug=false is_component_build=false rtc_include_tests=false enable_rust=true enable_rust_cxx=true'
ninja -C out/android_release_x64 libjingle_peerconnection_so
cp out/android_release_x64/libjingle_peerconnection_so.so $PREBUILT_PATH/x86_64/
