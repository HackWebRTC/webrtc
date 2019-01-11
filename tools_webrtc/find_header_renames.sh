#!/bin/bash

# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Usage: find_header_renames.sh {dir}
#
# Looks for headers in the git repository and suggests renames to add
# underscores to files that are missing them.
#
# Outputs a sorted sequence of renames in the form:
#   d/hdr1.h --> d/hdr2.h
#
# If the rename could not be automatically deduced, the output will look like:
#   d/hdr1.h --> d/???.h
#

for old_path in $(git ls-files "${1:+$1/}*.h"); do
  # Extract the file name (without the .h).
  old_name=$(basename "$old_path" .h)

  # If there is an underscore already, assume it does not need to be renamed.
  if [[ "$old_name" =~ _ ]]; then
    continue
  fi

  # If there are capital letters it's likely an Objective C file which does
  # not need to be renamed.
  if [[ "$old_name" =~ [A-Z] ]]; then
    continue
  fi

  # We need to know where to put the underscores, so try a heuristic:
  # 1. Look for any sequence in the file that matches (case insensitively) the
  #    file name.
  # 2. Remove any results which are either all lower case or all upper case
  #    (these aren't going to help).
  # 3. Convert the results (in camel case) into snake case.
  # 4. Deduplicate.
  #
  # If there is only one result then we're good: there's an unambiguous
  # translation in the file into snake case. Otherwise, we throw up our hands
  # and defer to a human.
  candidates=$(
      cat "$old_path" |
      sed 's/^.*PROXY_MAP(\(.*\))$/class \1Proxy/' |
      grep -io "$old_name" |
      grep -v "$old_name" |
      grep -v $(echo "$old_name" | tr '[:lower:]' '[:upper:]') |
      perl -pe 's/([A-Z][a-z])/_$1/g' |
      perl -pe 's/^_?//' |
      tr '[:upper:]' '[:lower:]' |
      sort -u)

  if [[ $(echo "$candidates" | wc -w) -eq 1 ]]; then
    # We only have one candidate: great! This is most likely correct.
    # If the candidate is the same as the file name, then no need to rename.
    if [ "$old_name" == "$candidates" ]; then
      continue
    fi
    echo "$old_path --> $(dirname "$old_path")/${candidates}.h"
  else
    # Either got 0 candidates or more than 1, need human intervention.
    echo "$old_path --> $(dirname "$old_path")/???.h"
  fi
done | sort
