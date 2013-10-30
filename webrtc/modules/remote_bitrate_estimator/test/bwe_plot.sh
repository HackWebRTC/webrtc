#!/bin/bash

# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# To set up in e.g. Eclipse, run a separate shell and pipe the output from the
# test into this script.
#
# In Eclipse, that amounts to creating a Run Configuration which starts
# "/bin/bash" with the arguments "-c [trunk_path]/out/Debug/modules_unittests
# --gtest_filter=*Estimators* | [trunk_path]/webrtc/modules/
# remote_bitrate_estimator/bwe_plot.sh"

log=$(</dev/stdin)

function gen_gnuplot_input {
  colors=(a7001f 0a60c2 b2582b 21a66c d6604d 4393c3 f4a582 92c5de edcbb7 b1c5d0)
  data_sets=$(echo "$log" | grep "^PLOT" | cut -f 2 | sort | uniq)
  echo -n "reset; "
  echo -n "set terminal wxt size 1440,900 font \"Arial,9\"; "
  echo -n "set xtics 60; set xlabel \"Seconds\"; "
  echo -n "plot "
  i=0
  for set in $data_sets ; do
    (( i++ )) && echo -n ","
    echo -n "'-' with "
    echo -n "linespoints "
    echo -n "ps 0.5 "
    echo -n "lc rgbcolor \"#${colors[$(($i % 10))]}\" "
    echo -n "title \"$set\" "
  done
  echo
  for set in $data_sets ; do
    echo "$log" | grep "^PLOT.$set" | cut -f 3,4
    echo "e"
  done
}

gen_gnuplot_input "$log" | gnuplot -persist
