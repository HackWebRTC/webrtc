#!/bin/bash -e
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to generate .gypi files needed to build libvpx.
# Every time libvpx source code is updated just run this script.
#
# For example:
# $ ./generate_gypi.sh
#
# And this will update all the .gypi files needed.
#
# Configuration for building on each platform is taken from the
# corresponding vpx_config.h
#

BASE_DIR=`pwd`
LIBVPX_SRC_DIR="source/libvpx"
LIBVPX_CONFIG_DIR="source/config"

# Convert a list of source files into gypi file.
# $1 - Input file.
# $2 - Output gypi file.
function convert_srcs_to_gypi {
  # Do the following here:
  # 1. Filter .c, .h, .s, .S and .asm files.
  # 2. Exclude *_offsets.c.
  # 3. Exclude vpx_config.c.
  # 4. Repelace .asm.s to .asm because gyp will do the conversion.
  local source_list=`grep -E '(\.c|\.h|\.S|\.s|\.asm)$' $1 | grep -v '_offsets\.c' | grep -v 'vpx_config\.c' | sed s/\.asm\.s$/.asm/ | sort`

  # Build the gypi file.
  echo "# This file is generated. Do not edit." > $2
  echo "# Copyright (c) 2012 The Chromium Authors. All rights reserved." >> $2
  echo "# Use of this source code is governed by a BSD-style license that can be" >> $2
  echo "# found in the LICENSE file." >> $2
  echo "" >> $2
  echo "{" >> $2
  echo "  'sources': [" >> $2
  for f in $source_list
  do
    echo "    '<(libvpx_src_dir)/$f'," >> $2
  done
  echo "  ]," >> $2
  echo "}" >> $2
}

# Clean files from previous make.
function make_clean {
  make clean > /dev/null
  rm -f libvpx_srcs.txt
}

# Lint a pair of vpx_config.h and vpx_config.asm to make sure they match.
# $1 - Header file directory.
function lint_config {
  $BASE_DIR/lint_config.sh \
    -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
    -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm
}

# Print the configuration.
# $1 - Header file directory.
function print_config {
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
    -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm
}

# Generate vpx_rtcd.h.
# $1 - Header file directory.
# $2 - Architecture.
function gen_rtcd_header {
  echo "Generate $LIBVPX_CONFIG_DIR/$1/vpx_rtcd.h."

  rm -rf $BASE_DIR/$TEMP_DIR/libvpx.config
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
    -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm \
    -o $BASE_DIR/$TEMP_DIR/libvpx.config

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.sh \
    --arch=$2 \
    --sym=vpx_rtcd \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp8/common/rtcd_defs.sh \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_rtcd.h

  rm -rf $BASE_DIR/$TEMP_DIR/libvpx.config
}

echo "Lint libvpx configuration."
lint_config linux/ia32
lint_config linux/x64
lint_config linux/arm
lint_config linux/arm-neon
lint_config win/ia32
lint_config mac/ia32

echo "Create temporary directory."
TEMP_DIR="$LIBVPX_SRC_DIR.temp"
rm -rf $TEMP_DIR
cp -R $LIBVPX_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

gen_rtcd_header linux/ia32 x86
gen_rtcd_header linux/x64 x86_64
gen_rtcd_header linux/arm armv5te
gen_rtcd_header linux/arm-neon armv7
gen_rtcd_header win/ia32 x86
gen_rtcd_header mac/ia32 x86

echo "Prepare Makefile."
./configure --target=generic-gnu > /dev/null
make_clean

echo "Generate X86 source list."
config=$(print_config linux/ia32)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt $BASE_DIR/libvpx_srcs_x86.gypi

echo "Generate X86_64 source list."
config=$(print_config linux/x64)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt $BASE_DIR/libvpx_srcs_x86_64.gypi

echo "Generate ARM source list."
config=$(print_config linux/arm)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt $BASE_DIR/libvpx_srcs_arm.gypi

echo "Generate ARM NEON source list."
config=$(print_config linux/arm-neon)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt $BASE_DIR/libvpx_srcs_arm_neon.gypi

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR
