/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_PERF_TEST_H_
#define TEST_TESTSUPPORT_PERF_TEST_H_

#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "api/array_view.h"
#include "rtc_base/numerics/samples_stats_counter.h"

namespace webrtc {
namespace test {

// Metrics improver direction.
enum class ImproveDirection {
  // Direction is undefined.
  kNone,
  // Smaller value is better.
  kSmallerIsBetter,
  // Bigger value is better.
  kBiggerIsBetter,
};

// Prints numerical information to stdout in a controlled format, for
// post-processing. |measurement| is a description of the quantity being
// measured, e.g. "vm_peak"; |modifier| is provided as a convenience and
// will be appended directly to the name of the |measurement|, e.g.
// "_browser"; |trace| is a description of the particular data point, e.g.
// "reference"; |value| is the measured value; and |units| is a description
// of the units of measure, e.g. "bytes". If |important| is true, the output
// line will be specially marked, to notify the post-processor. The strings
// may be empty.  They should not contain any colons (:) or equals signs (=).
// A typical post-processing step would be to produce graphs of the data
// produced for various builds, using the combined |measurement| + |modifier|
// string to specify a particular graph and the |trace| to identify a trace
// (i.e., data series) on that graph.
void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const double value,
                 const std::string& units,
                 bool important,
                 ImproveDirection improve_direction = ImproveDirection::kNone);

// Like PrintResult(), but prints a (mean, standard deviation) result pair.
// The |<values>| should be two comma-separated numbers, the mean and
// standard deviation (or other error metric) of the measurement.
void PrintResultMeanAndError(
    const std::string& measurement,
    const std::string& modifier,
    const std::string& trace,
    const double mean,
    const double error,
    const std::string& units,
    bool important,
    ImproveDirection improve_direction = ImproveDirection::kNone);

// Like PrintResult(), but prints an entire list of results. The |values|
// will generally be a list of comma-separated numbers. A typical
// post-processing step might produce plots of their mean and standard
// deviation.
void PrintResultList(
    const std::string& measurement,
    const std::string& modifier,
    const std::string& trace,
    rtc::ArrayView<const double> values,
    const std::string& units,
    bool important,
    ImproveDirection improve_direction = ImproveDirection::kNone);

// Like PrintResult(), but prints a (mean, standard deviation) from stats
// counter. Also add specified metric to the plotable metrics output.
void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const SamplesStatsCounter& counter,
                 const std::string& units,
                 const bool important,
                 ImproveDirection improve_direction = ImproveDirection::kNone);

// If --write_histogram_proto_json=false, this returns all perf results to date
// in a JSON string formatted as described in dashboard/docs/data-format.md
// in https://github.com/catapult-project/catapult/blob/master/. If
// --write_histogram_proto_json=true, returns a string-encoded proto as
// described in tracing/tracing/proto/histogram.proto in
// https://github.com/catapult-project/catapult/blob/master/.
std::string GetPerfResults();

// Print into stdout plottable metrics for further post processing.
// |desired_graphs| - list of metrics, that should be plotted. If empty - all
// available metrics will be plotted. If some of |desired_graphs| are missing
// they will be skipped.
void PrintPlottableResults(const std::vector<std::string>& desired_graphs);

// Call GetPerfResults() and write its output to a file.
void WritePerfResults(const std::string& output_path);

// By default, perf results are printed to stdout. Set the FILE* to where they
// should be printing instead.
void SetPerfResultsOutput(FILE* output);

// Only for use by tests.
void ClearPerfResults();

}  // namespace test
}  // namespace webrtc

// Only for use by tests.
ABSL_DECLARE_FLAG(bool, write_histogram_proto_json);

#endif  // TEST_TESTSUPPORT_PERF_TEST_H_
