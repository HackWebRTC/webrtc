#!/bin/bash
#
# libjingle
# Copyright 2013, Google Inc.
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

# Wrapper script for running the Java tests under this directory.

# Exit with error immediately if any subcommand fails.
set -e

# Change directory to the PRODUCT_DIR (e.g. out/Debug).
cd -P $(dirname $0)

export CLASSPATH=`pwd`/junit-4.11.jar
CLASSPATH=$CLASSPATH:`pwd`/libjingle_peerconnection_test.jar
CLASSPATH=$CLASSPATH:`pwd`/libjingle_peerconnection.jar

export LD_LIBRARY_PATH=`pwd`

# The RHS value is replaced by the build action that copies this script to
# <(PRODUCT_DIR).
export JAVA_HOME=GYP_JAVA_HOME

${JAVA_HOME}/bin/java -Xcheck:jni -classpath $CLASSPATH \
    junit.textui.TestRunner org.webrtc.PeerConnectionTest
