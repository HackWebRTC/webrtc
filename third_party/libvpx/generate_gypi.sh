#!/bin/bash -e
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to generate .gypi files needed to build libvpx.
# Every time libvpx source code is updated just run this script.
#
# For example:
# $ ./generate_gypi.sh
#
# And this will update all the .gypi files needed.
# However changes to asm_*_offsets.asm, vpx_config.asm and
# vpx_config.h are not updated and needs to modified manually
# for all platforms.

# This file is based upon the generate_gypi.sh file in Chromium,
# but with the following additional features enabled:
# CONFIG_ERROR_CONCEALMENT, CONFIG_POSTPROC
# http://src.chromium.org/svn/trunk/deps/third_party/libvpx/generate_gypi.sh

LIBVPX_SRC_DIR="source/libvpx"
COMMON_CONFIG="CONFIG_REALTIME_ONLY=yes CONFIG_GCC=yes CONFIG_ERROR_CONCEALMENT=yes CONFIG_POSTPROC=yes"
X86_CONFIG="ARCH_X86=yes HAVE_MMX=yes HAVE_SSE2=yes HAVE_SSE3=yes HAVE_SSSE3=yes HAVE_SSE4_1=yes CONFIG_RUNTIME_CPU_DETECT=yes"
X86_64_CONFIG="ARCH_X86_64=yes HAVE_MMX=yes HAVE_SSE2=yes HAVE_SSE3=yes HAVE_SSSE3=yes HAVE_SSE4_1=yes CONFIG_PIC=yes CONFIG_RUNTIME_CPU_DETECT=yes"
ARM_CONFIG="ARCH_ARM=yes HAVE_ARMV5TE=yes HAVE_ARMV6=yes"
ARM_NEON_CONFIG="ARCH_ARM=yes HAVE_ARMV5TE=yes HAVE_ARMV6=yes HAVE_ARMV7=yes"

function convert_srcs_to_gypi {
  # Do the following here:
  # 1. Filter .c, .h, .s, .S and .asm files.
  # 2. Exclude *_offsets.c.
  # 3. Exclude vpx_config.c.
  # 4. Repelace .asm.s to .asm because gyp will do the conversion.
  source_list=`grep -E '(\.c|\.h|\.S|\.s|\.asm)$' $1 | grep -v '_offsets\.c' | grep -v 'vpx_config\.c' | sed s/\.asm\.s$/.asm/`

  # Build the gypi file.
  echo "# This file is generated. Do not edit." > $2
  echo "# Copyright (c) 2011 The Chromium Authors. All rights reserved." >> $2
  echo "# Use of this source code is governed by a BSD-style license that can be" >> $2
  echo "# found in the LICENSE file." >> $2
  echo "" >> $2
  echo "{" >> $2
  echo "  'sources': [" >> $2
  for f in $source_list
  do
    echo "    '$LIBVPX_SRC_DIR/$f'," >> $2
  done
  echo "  ]," >> $2
  echo "}" >> $2
}

echo "Create temporary directory."
BASE_DIR=`pwd`
TEMP_DIR="$LIBVPX_SRC_DIR.temp"
cp -R $LIBVPX_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

echo "Prepare Makefile."
./configure --target=generic-gnu > /dev/null
make clean > /dev/null

echo "Generate X86 source list."
make clean > /dev/null
rm -f libvpx_srcs.txt
make libvpx_srcs.txt target=libs $COMMON_CONFIG $X86_CONFIG > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt ../../libvpx_srcs_x86.gypi

echo "Generate X86_64 source list."
make clean > /dev/null
rm -f libvpx_srcs.txt
make libvpx_srcs.txt target=libs $COMMON_CONFIG $X86_64_CONFIG > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt ../../libvpx_srcs_x86_64.gypi

echo "Generate ARM source list."
make clean > /dev/null
rm -f libvpx_srcs.txt
make libvpx_srcs.txt target=libs $COMMON_CONFIG $ARM_CONFIG > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt ../../libvpx_srcs_arm.gypi

echo "Generate ARM NEON source list."
make clean > /dev/null
rm -f libvpx_srcs.txt
make libvpx_srcs.txt target=libs $COMMON_CONFIG $ARM_NEON_CONFIG > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt ../../libvpx_srcs_arm_neon.gypi

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR
