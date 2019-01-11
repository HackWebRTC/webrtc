#!/bin/bash

# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Run various operations according to the rename operations specified in the
# given rename file.
#
# The rename file must have the following format:
#
#   <old path 1> --> <new path 1>
#   <old path 2> --> <new path 2>
#
# For example:
#
#   a/old_name.h --> a/new_name.h
#   # Comments are allowed.
#   b/old.h --> b/new.h
#

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <move|update|install> <path to rename file>"
  exit 1
fi

cmd="$1"
rename_file="$2"

# replace_in_file $fn $old $new $path
#
# Replaces in the file at $path the result of applying $fn to $old with the
# result of applying $fn to $new.
function replace_in_file {
  fn="$1"
  old="$2"
  new="$3"
  path="$4"
  sed -i "s!$($fn "$old")!$($fn "$new")!g" "$path"
}

# Moves the file using git.
function move_file {
  old_path="$1"
  new_path="$2"
  git mv "$old_path" "$new_path"
  echo "Moved $old_path to $new_path"
}

# Outputs the path to the relevant BUILD.gn file for the given file path.
# Returns 0 if found, 1 otherwise.
function find_build_file {
  path="$1"
  file_name="$(basename "$path")"
  dir_name="$(dirname "$path")"
  while [ "$dir_name" != "." ]; do
    build_path="${dir_name}/BUILD.gn"
    if [ -f "$build_path" ] && grep "\"$file_name\"" "$build_path" \
        > /dev/null; then
      echo "$build_path"
      return 0
    fi
    file_name="$(basename "$dir_name")/$file_name"
    dir_name="$(dirname "$dir_name")"
  done
  return 1
}

# Update the relevant BUILD.gn file with the renamed file.
function rename_in_build_file {
  old_path="$1"
  new_path="$2"
  build_path=$(find_build_file "$old_path")
  if [[ $? -ne 0 ]]; then
    return 1
  fi
  build_dir="$(dirname "$build_path")/"
  old_name=$(echo "$old_path" | sed "s#$build_dir##")
  new_name=$(echo "$new_path" | sed "s#$build_dir##")
  sed -i "s#\"$old_name\"#\"$new_name\"#g" "$build_path"
  return 0
}

# Update the relevant DEPS files with the renamed file.
function rename_in_deps {
  old_path="$1"
  new_path="$2"

  # First, update other DEPS referencing this file.
  function deps_reference {
    echo "\"+$1\""
  }
  count=0
  while read -r referencer_path && [[ -n "$referencer_path" ]]; do
    replace_in_file deps_reference "$old_path" "$new_path" "$referencer_path"
    let count=count+1
  done <<< "$(git grep --files-with-matches $(deps_reference "$old_path") \
      '*/DEPS')"
  echo -n $count

  # Second, update DEPS specifying this file.
  function deps_entry {
    echo "\"$(basename "$1" .h)\\\\\\.h\":"
  }
  dir_name=$(dirname "$old_path")
  while [ "$dir_name" != "." ]; do
    deps_path="${dir_name}/DEPS"
    if [ -f "$deps_path" ]; then
      replace_in_file deps_entry "$old_path" "$new_path" "$deps_path"
      break
    fi
    dir_name=$(dirname "$dir_name")
  done
}

# Update all #include references from the old header path to the new path.
function update_all_includes {
  old_header_path="$1"
  new_header_path="$2"
  count=0
  while read -r includer_path && [[ -n "$includer_path" ]]; do
    sed -i "s!#include \"$old_header_path\"!#include \"$new_header_path\"!g" \
        "$includer_path"
    let count=count+1
  done <<< "$(git grep --files-with-matches "#include \"$old_header_path\"")"
  echo -n $count
}

# Echo out the header guard for a given file path.
# E.g., api/jsep.h turns into API_JSEP_H_ .
function header_guard {
  echo "${1}_" | perl -pe 's/[\/\.-]/_/g' | perl -pe 's/(.)/\U$1/g'
}

# Updates BUILD.gn and (if header) the include guard and all #include
# references.
function update_file {
  old_path="$1"
  new_path="$2"
  echo -n "Processing $old_path --> $new_path ... "
  echo -n " build file ... "
  if rename_in_build_file "$old_path" "$new_path"; then
    echo -n done
  else
    echo -n failed
  fi
  if [[ "$old_path" == *.h ]]; then
    echo -n " header guard ... "
    old_header_guard=$(header_guard "$old_path")
    new_header_guard=$(header_guard "$new_path")
    sed -i "s/${old_header_guard}/${new_header_guard}/g" "$new_path"
    echo -n done
    echo -n " includes ... "
    update_all_includes "$old_path" "$new_path"
    echo -n " done"
    echo -n " deps ... "
    rename_in_deps "$old_path" "$new_path"
    echo -n " done"
  fi
  echo
}

# Generate forwarding headers for the old header path that include the new
# header path.
function install_file {
  old_path="$1"
  new_path="$2"
  if ! [[ "$old_path" == *.h ]]; then
    return
  fi
  if ! [ -f "$old_path" ]; then
    # Add the old path to the BUILD.gn file.
    build_path="$(find_build_file "$new_path")"
    if [[ $? -eq 0 ]]; then
      build_dir="$(dirname "$build_path")/"
      old_name=$(echo "$old_path" | sed "s#$build_dir##")
      new_name=$(echo "$new_path" | sed "s#$build_dir##")
      sed -i "s!^\\([^#]*\\)\"$new_name\"!\1\"$new_name\",\"$old_name\"!g" \
          "$build_path"
    fi
  fi
  old_header_guard=$(header_guard "$old_path")
  cat << EOF > "$old_path"
/*
 *  Copyright $(date +%Y) The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef $old_header_guard
#define $old_header_guard

// TODO(bugs.webrtc.org/10159): Remove this files once downstream projects have
// been updated to include the new path.

#include "$new_path"

#endif  // $old_header_guard
EOF
  git add "$old_path"
  echo "Installed header at $old_path pointing to $new_path"
}

IFS=$'\n'
for rename_stanza in $(cat "$rename_file" | grep -v '^#'); do
  IFS=$' '
  arr=($rename_stanza)
  old_path=${arr[0]}
  new_path=${arr[2]}
  case "$cmd" in
  "move")
    move_file "$old_path" "$new_path"
    ;;
  "update")
    update_file "$old_path" "$new_path"
    ;;
  "install")
    install_file "$old_path" "$new_path"
    ;;
  *)
    echo "Unknown command: $cmd"
    exit 1
    ;;
  esac
done
