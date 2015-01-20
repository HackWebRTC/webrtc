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

# javac & jar wrapper helping to simplify gyp action specification.

set -e  # Exit on any error.

# Allow build-error parsers (such as emacs' compilation-mode) to find failing
# files easily.
echo "$0: Entering directory \``pwd`'"

JAVA_HOME="$1"; shift
JAR_NAME="$1"; shift
TMP_DIR="$1"; shift
CLASSPATH="$1"; shift

if [ -z "$1" ]; then
  echo "Usage: $0 jar-name temp-work-dir source-path-dir .so-to-bundle " \
    "classpath path/to/Source1.java path/to/Source2.java ..." >&2
  exit 1
fi

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

$JAVA_HOME/bin/javac -Xlint:deprecation -Xlint:unchecked -d "$TMP_DIR" \
  -classpath "$CLASSPATH" "$@"
$JAVA_HOME/bin/jar cf "$JAR_NAME" -C "$TMP_DIR" .
