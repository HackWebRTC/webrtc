/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/event_log_visualizer/plot_protobuf.h"

#include <memory>

namespace webrtc {
namespace plotting {

ProtobufPlot::ProtobufPlot() {}

ProtobufPlot::~ProtobufPlot() {}

void ProtobufPlot::Draw() {}

void ProtobufPlot::ExportProtobuf(protobuf_plot::Plot* plot) {
  for (size_t i = 0; i < series_list_.size(); i++) {
    protobuf_plot::DataSet* data_set = plot->add_data_sets();
    for (const auto& point : series_list_[i].points) {
      data_set->add_xvalues(point.x);
    }
    for (const auto& point : series_list_[i].points) {
      data_set->add_yvalues(point.y);
    }

    if (series_list_[i].style == BAR_GRAPH) {
      data_set->set_style(protobuf_plot::BAR_GRAPH);
    } else if (series_list_[i].style == LINE_GRAPH) {
      data_set->set_style(protobuf_plot::LINE_GRAPH);
    } else if (series_list_[i].style == LINE_DOT_GRAPH) {
      data_set->set_style(protobuf_plot::LINE_DOT_GRAPH);
    } else {
      data_set->set_style(protobuf_plot::UNDEFINED);
    }

    data_set->set_label(series_list_[i].label);
  }

  plot->set_xaxis_min(xaxis_min_);
  plot->set_xaxis_max(xaxis_max_);
  plot->set_yaxis_min(yaxis_min_);
  plot->set_yaxis_max(yaxis_max_);
  plot->set_xaxis_label(xaxis_label_);
  plot->set_yaxis_label(yaxis_label_);
  plot->set_title(title_);
}

ProtobufPlotCollection::ProtobufPlotCollection() {}

ProtobufPlotCollection::~ProtobufPlotCollection() {}

void ProtobufPlotCollection::Draw() {}

void ProtobufPlotCollection::ExportProtobuf(
    protobuf_plot::PlotCollection* collection) {
  for (const auto& plot : plots_) {
    // TODO(terelius): Ensure that there is no way to insert plots other than
    // ProtobufPlots in a ProtobufPlotCollection. Needed to safely static_cast
    // here.
    protobuf_plot::Plot* protobuf_representation = collection->add_plots();
    static_cast<ProtobufPlot*>(plot.get())
        ->ExportProtobuf(protobuf_representation);
  }
}

Plot* ProtobufPlotCollection::AppendNewPlot() {
  Plot* plot = new ProtobufPlot();
  plots_.push_back(std::unique_ptr<Plot>(plot));
  return plot;
}

}  // namespace plotting
}  // namespace webrtc
