#!/bin/bash
#
# libjingle
# Copyright 2013 Google Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Wrapper script for running the Java tests under this directory. This script
# will only work if it has been massaged by the build action and placed in
# the PRODUCT_DIR (e.g. out/Debug).

# Exit with error immediately if any subcommand fails.
set -e

# Change directory to the PRODUCT_DIR (e.g. out/Debug).
cd -P $(dirname $0)

if [ -z "$LD_PRELOAD" ]; then
  echo "LD_PRELOAD isn't set. It should be set to something like "
  echo "/usr/lib/x86_64-linux-gnu/libpulse.so.0. I will now refuse to run "
  echo "to protect you from the consequences of your folly."
  exit 1
fi

export CLASSPATH=`pwd`/junit-4.11.jar
CLASSPATH=$CLASSPATH:`pwd`/libjingle_peerconnection_test.jar
CLASSPATH=$CLASSPATH:`pwd`/libjingle_peerconnection.jar

# This sets java.library.path so lookup of libjingle_peerconnection_so.so works.
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`:`pwd`/lib:`pwd`/lib.target

# The RHS value is replaced by the build action that copies this script to
# <(PRODUCT_DIR), using search-and-replace by the build action.
export JAVA_HOME=GYP_JAVA_HOME

${JAVA_HOME}/bin/java -Xcheck:jni -classpath $CLASSPATH \
    junit.textui.TestRunner org.webrtc.PeerConnectionTestJava
