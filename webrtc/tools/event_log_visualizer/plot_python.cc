/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/event_log_visualizer/plot_python.h"

#include <stdio.h>
#include <memory>

namespace webrtc {
namespace plotting {

PythonPlot::PythonPlot() {}

PythonPlot::~PythonPlot() {}

void PythonPlot::draw() {
  // Write python commands to stdout. Intended program usage is
  // ./event_log_visualizer event_log160330.dump | python

  if (!series.empty()) {
    printf("color_count = %zu\n", series.size());
    printf(
        "hls_colors = [(i*1.0/color_count, 0.25+i*0.5/color_count, 0.8) for i "
        "in range(color_count)]\n");
    printf("rgb_colors = [colorsys.hls_to_rgb(*hls) for hls in hls_colors]\n");

    for (size_t i = 0; i < series.size(); i++) {
      // List x coordinates
      printf("x%zu = [", i);
      if (series[i].points.size() > 0)
        printf("%G", series[i].points[0].x);
      for (size_t j = 1; j < series[i].points.size(); j++)
        printf(", %G", series[i].points[j].x);
      printf("]\n");

      // List y coordinates
      printf("y%zu = [", i);
      if (series[i].points.size() > 0)
        printf("%G", series[i].points[0].y);
      for (size_t j = 1; j < series[i].points.size(); j++)
        printf(", %G", series[i].points[j].y);
      printf("]\n");

      if (series[i].style == BAR_GRAPH) {
        // There is a plt.bar function that draws bar plots,
        // but it is *way* too slow to be useful.
        printf(
            "plt.vlines(x%zu, map(lambda t: min(t,0), y%zu), map(lambda t: "
            "max(t,0), y%zu), color=rgb_colors[%zu], "
            "label=\'%s\')\n",
            i, i, i, i, series[i].label.c_str());
      } else if (series[i].style == LINE_GRAPH) {
        printf("plt.plot(x%zu, y%zu, color=rgb_colors[%zu], label=\'%s\')\n", i,
               i, i, series[i].label.c_str());
      } else if (series[i].style == LINE_DOT_GRAPH) {
        printf(
            "plt.plot(x%zu, y%zu, color=rgb_colors[%zu], label=\'%s\', "
            "marker='.')\n",
            i, i, i, series[i].label.c_str());
      } else {
        printf("raise Exception(\"Unknown graph type\")\n");
      }
    }
  }

  printf("plt.xlim(%f, %f)\n", xaxis_min, xaxis_max);
  printf("plt.ylim(%f, %f)\n", yaxis_min, yaxis_max);
  printf("plt.xlabel(\'%s\')\n", xaxis_label.c_str());
  printf("plt.ylabel(\'%s\')\n", yaxis_label.c_str());
  printf("plt.title(\'%s\')\n", title.c_str());
  if (!series.empty()) {
    printf("plt.legend(loc=\'best\', fontsize=\'small\')\n");
  }
}

PythonPlotCollection::PythonPlotCollection() {}

PythonPlotCollection::~PythonPlotCollection() {}

void PythonPlotCollection::draw() {
  printf("import matplotlib.pyplot as plt\n");
  printf("import colorsys\n");
  for (size_t i = 0; i < plots.size(); i++) {
    printf("plt.figure(%zu)\n", i);
    plots[i]->draw();
  }
  printf("plt.show()\n");
}

Plot* PythonPlotCollection::append_new_plot() {
  Plot* plot = new PythonPlot();
  plots.push_back(std::unique_ptr<Plot>(plot));
  return plot;
}

}  // namespace plotting
}  // namespace webrtc
