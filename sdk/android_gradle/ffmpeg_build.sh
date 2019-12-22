#!/bin/bash

# ffmpeg 4.1.4

# Android

WEBRTC_NDK=/Users/piasy/src/media/webrtc_repo/webrtc_android/src/third_party/android_ndk
TOOLCHAIN=${WEBRTC_NDK}/toolchains/llvm/prebuilt/darwin-x86_64

./configure \
--ar=${TOOLCHAIN}/bin/arm-linux-androideabi-ar \
--as=${TOOLCHAIN}/bin/arm-linux-androideabi-as \
--cc=${TOOLCHAIN}/bin/armv7a-linux-androideabi16-clang \
--cxx=${TOOLCHAIN}/bin/armv7a-linux-androideabi16-clang++ \
--nm=${TOOLCHAIN}/bin/arm-linux-androideabi-nm \
--ranlib=${TOOLCHAIN}/bin/arm-linux-androideabi-ranlib \
--strip=${TOOLCHAIN}/bin/arm-linux-androideabi-strip \
--enable-cross-compile \
--target-os=android \
--arch=armv7a \
--disable-shared --disable-doc --disable-programs \
--enable-debug --disable-symver --disable-asm \
--disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
--disable-protocols --enable-protocol=file \
--disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
--disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 \
--disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
--disable-muxers --enable-muxer=matroska \
--prefix=`pwd`/out_armv7a && \
make -j16 install && \
make clean && \
./configure \
--ar=${TOOLCHAIN}/bin/aarch64-linux-android-ar \
--as=${TOOLCHAIN}/bin/aarch64-linux-android-as \
--cc=${TOOLCHAIN}/bin/aarch64-linux-android21-clang \
--cxx=${TOOLCHAIN}/bin/aarch64-linux-android21-clang++ \
--nm=${TOOLCHAIN}/bin/aarch64-linux-android-nm \
--ranlib=${TOOLCHAIN}/bin/aarch64-linux-android-ranlib \
--strip=${TOOLCHAIN}/bin/aarch64-linux-android-strip \
--enable-cross-compile \
--target-os=android \
--arch=aarch64 \
--disable-shared --disable-doc --disable-programs \
--enable-debug --disable-symver --disable-asm \
--disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
--disable-protocols --enable-protocol=file \
--disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
--disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 \
--disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
--disable-muxers --enable-muxer=matroska \
--prefix=`pwd`/out_arm64 && \
make -j16 install && \
make clean && \
./configure \
--ar=${TOOLCHAIN}/bin/i686-linux-android-ar \
--as=${TOOLCHAIN}/bin/i686-linux-android-as \
--cc=${TOOLCHAIN}/bin/i686-linux-android16-clang \
--cxx=${TOOLCHAIN}/bin/i686-linux-android16-clang++ \
--nm=${TOOLCHAIN}/bin/i686-linux-android-nm \
--ranlib=${TOOLCHAIN}/bin/i686-linux-android-ranlib \
--strip=${TOOLCHAIN}/bin/i686-linux-android-strip \
--enable-cross-compile \
--target-os=android \
--arch=i686 \
--disable-shared --disable-doc --disable-programs \
--enable-debug --disable-symver --disable-asm \
--disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
--disable-protocols --enable-protocol=file \
--disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
--disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 \
--disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
--disable-muxers --enable-muxer=matroska \
--prefix=`pwd`/out_x86 && \
make -j16 install && \
make clean && \
./configure \
--ar=${TOOLCHAIN}/bin/x86_64-linux-android-ar \
--as=${TOOLCHAIN}/bin/x86_64-linux-android-as \
--cc=${TOOLCHAIN}/bin/x86_64-linux-android21-clang \
--cxx=${TOOLCHAIN}/bin/x86_64-linux-android21-clang++ \
--nm=${TOOLCHAIN}/bin/x86_64-linux-android-nm \
--ranlib=${TOOLCHAIN}/bin/x86_64-linux-android-ranlib \
--strip=${TOOLCHAIN}/bin/x86_64-linux-android-strip \
--enable-cross-compile \
--target-os=android \
--arch=x86_64 \
--disable-shared --disable-doc --disable-programs \
--enable-debug --disable-symver --disable-asm \
--disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
--disable-protocols --enable-protocol=file \
--disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
--disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 \
--disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
--disable-muxers --enable-muxer=matroska \
--prefix=`pwd`/out_x86_64 && \
make -j16 install && \
make clean



# iOS, note: libs built in case-sensitive fs are unusable
# use [FFmpeg iOS build script](https://github.com/kewlbear/FFmpeg-iOS-build-script)
# with the following configure flags
#CONFIGURE_FLAGS="--enable-cross-compile --enable-debug --disable-doc \
#--enable-pic \
#--disable-programs \
#--disable-encoders --disable-hwaccels --disable-bsfs --disable-devices --disable-filters \
#--disable-protocols --enable-protocol=file \
#--disable-parsers --enable-parser=mpegaudio --enable-parser=h264 --enable-parser=hevc \
#--disable-demuxers --enable-demuxer=mov --enable-demuxer=mp3 \
#--disable-decoders --enable-decoder=mp3 --enable-decoder=aac \
#--disable-muxers --enable-muxer=matroska"
