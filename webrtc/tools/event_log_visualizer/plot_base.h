/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_BASE_H_
#define WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_BASE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace webrtc {
namespace plotting {

enum PlotStyle { LINE_GRAPH, BAR_GRAPH };

struct TimeSeriesPoint {
  TimeSeriesPoint(float x, float y) : x(x), y(y) {}
  float x;
  float y;
};

struct TimeSeries {
  TimeSeries() = default;
  TimeSeries(TimeSeries&& other)
      : label(std::move(other.label)),
        style(other.style),
        points(std::move(other.points)) {}
  TimeSeries& operator=(TimeSeries&& other) {
    label = std::move(other.label);
    style = other.style;
    points = std::move(other.points);
    return *this;
  }

  std::string label;
  PlotStyle style;
  std::vector<TimeSeriesPoint> points;
};

// This is basically a struct that represents of a general graph, with axes,
// title and one or more data series. We make it a class only to document that
// it also specifies an interface for the draw()ing objects.
class Plot {
 public:
  virtual ~Plot() {}
  virtual void draw() = 0;

  float xaxis_min;
  float xaxis_max;
  std::string xaxis_label;
  float yaxis_min;
  float yaxis_max;
  std::string yaxis_label;
  std::vector<TimeSeries> series;
  std::string title;
};

class PlotCollection {
 public:
  virtual ~PlotCollection() {}
  virtual void draw() = 0;
  virtual Plot* append_new_plot() = 0;

 protected:
  std::vector<std::unique_ptr<Plot> > plots;
};

}  // namespace plotting
}  // namespace webrtc

#endif  // WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_BASE_H_
