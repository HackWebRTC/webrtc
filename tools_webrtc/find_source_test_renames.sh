#!/bin/bash

# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Usage: find_source_test_renames.sh {header-renames}
#
# Given a file with header renames in the form:
#   d/hdr1.h --> d/hdr2.h
# Outputs a sorted sequence of renames that also include .cc and _unittest.cc
# renames that match the header renames.
#

line_regex="([^ ]+) --> ([^ ]+)"
while ((line_no++)); read line; do
  echo "$line"

  if ! [[ $line =~ $line_regex ]]; then
    (>&2 echo "$line_no: Skipping malformed line: $line")
    continue
  fi

  old_path="${BASH_REMATCH[1]}"
  new_path="${BASH_REMATCH[2]}"
  if ! [[ -f "$old_path" ]]; then
    (>&2 echo "$line_no: Skipping missing old path: $old_path")
    continue
  fi

  old_name="$(basename "$old_path" .h)"
  new_name="$(basename "$new_path" .h)"

  if [[ "$new_name" == "???" ]]; then
    (>&2 echo "$line_no: Skipping missing new name: $new_name")
    continue
  fi


  # Find source renames.
  for old_source_path in $(git ls-files "*/${old_name}.cc"); do
    new_source_path="$(dirname "$old_source_path")/${new_name}.cc"
    echo "$old_source_path --> $new_source_path"
  done

  # Find unittest renames.
  for old_test_path in $(git ls-files "*/${old_name}_unittest.cc"); do
    new_test_path="$(dirname "$old_test_path")/${new_name}_unittest.cc"
    echo "$old_test_path --> $new_test_path"
  done
done < "${1:-/dev/stdin}" | sort -u
