/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test.h"

#include <stdio.h>

#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"

namespace {

template <typename Container>
void OutputListToStream(std::ostream* ostream, const Container& values) {
  const char* sep = "";
  for (const auto& v : values) {
    (*ostream) << sep << v;
    sep = ",";
  }
}

struct PlottableCounter {
  std::string graph_name;
  std::string trace_name;
  webrtc::SamplesStatsCounter counter;
  std::string units;
};

class PerfResultsLogger {
 public:
  PerfResultsLogger() : crit_(), output_(stdout), graphs_() {}
  void ClearResults() {
    rtc::CritScope lock(&crit_);
    graphs_.clear();
  }
  void SetOutput(FILE* output) {
    rtc::CritScope lock(&crit_);
    output_ = output;
  }
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const webrtc::SamplesStatsCounter& counter,
                 const std::string& units,
                 const bool important,
                 webrtc::test::ImproveDirection improve_direction) {
    LogResultMeanAndError(
        graph_name, trace_name, counter.IsEmpty() ? 0 : counter.GetAverage(),
        counter.IsEmpty() ? 0 : counter.GetStandardDeviation(), units,
        important, improve_direction);

    rtc::CritScope lock(&crit_);
    plottable_counters_.push_back({graph_name, trace_name, counter, units});
  }
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important,
                 webrtc::test::ImproveDirection improve_direction) {
    RTC_CHECK(std::isfinite(value))
        << "Expected finite value for graph " << graph_name << ", trace name "
        << trace_name << ", units " << units << ", got " << value;

    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << value;
    LogResultsImpl(graph_name, trace_name, value_stream.str(), units, important,
                   improve_direction);

    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"scalar",)";
    json_stream << R"("value":)" << value << ',';
    json_stream << R"("units":")" << UnitWithDirection(units, improve_direction)
                << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }
  void LogResultMeanAndError(const std::string& graph_name,
                             const std::string& trace_name,
                             const double mean,
                             const double error,
                             const std::string& units,
                             const bool important,
                             webrtc::test::ImproveDirection improve_direction) {
    RTC_CHECK(std::isfinite(mean));
    RTC_CHECK(std::isfinite(error));

    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << '{' << mean << ',' << error << '}';
    LogResultsImpl(graph_name, trace_name, value_stream.str(), units, important,
                   improve_direction);

    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"list_of_scalar_values",)";
    json_stream << R"("values":[)" << mean << "],";
    json_stream << R"("std":)" << error << ',';
    json_stream << R"("units":")" << UnitWithDirection(units, improve_direction)
                << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }
  void LogResultList(const std::string& graph_name,
                     const std::string& trace_name,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     const bool important,
                     webrtc::test::ImproveDirection improve_direction) {
    for (double v : values) {
      RTC_CHECK(std::isfinite(v));
    }

    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << '[';
    OutputListToStream(&value_stream, values);
    value_stream << ']';
    LogResultsImpl(graph_name, trace_name, value_stream.str(), units, important,
                   improve_direction);

    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"list_of_scalar_values",)";
    json_stream << R"("values":)" << value_stream.str() << ',';
    json_stream << R"("units":")" << UnitWithDirection(units, improve_direction)
                << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }
  std::string ToJSON() const;
  void PrintPlottableCounters(
      const std::vector<std::string>& desired_graphs_raw) const {
    std::set<std::string> desired_graphs(desired_graphs_raw.begin(),
                                         desired_graphs_raw.end());
    rtc::CritScope lock(&crit_);
    for (auto& counter : plottable_counters_) {
      if (!desired_graphs.empty()) {
        auto it = desired_graphs.find(counter.graph_name);
        if (it == desired_graphs.end()) {
          continue;
        }
      }

      std::ostringstream value_stream;
      value_stream.precision(8);
      value_stream << R"({"graph_name":")" << counter.graph_name << R"(",)";
      value_stream << R"("trace_name":")" << counter.trace_name << R"(",)";
      value_stream << R"("units":")" << counter.units << R"(",)";
      if (!counter.counter.IsEmpty()) {
        value_stream << R"("mean":)" << counter.counter.GetAverage() << ',';
        value_stream << R"("std":)" << counter.counter.GetStandardDeviation()
                     << ',';
      }
      value_stream << R"("samples":[)";
      const char* sep = "";
      for (const auto& sample : counter.counter.GetTimedSamples()) {
        value_stream << sep << R"({"time":)" << sample.time.us() << ','
                     << R"("value":)" << sample.value << '}';
        sep = ",";
      }
      value_stream << "]}";

      fprintf(output_, "PLOTTABLE_DATA: %s\n", value_stream.str().c_str());
    }
  }

 private:
  void LogResultsImpl(const std::string& graph_name,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& units,
                      bool important,
                      webrtc::test::ImproveDirection improve_direction) {
    // <*>RESULT <graph_name>: <trace_name>= <value> <units>
    // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
    // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>
    rtc::CritScope lock(&crit_);

    if (important) {
      fprintf(output_, "*");
    }
    fprintf(output_, "RESULT %s: %s= %s %s\n", graph_name.c_str(),
            trace.c_str(), values.c_str(),
            UnitWithDirection(units, improve_direction).c_str());
  }

  std::string UnitWithDirection(
      const std::string& units,
      webrtc::test::ImproveDirection improve_direction) {
    switch (improve_direction) {
      case webrtc::test::ImproveDirection::kNone:
        return units;
      case webrtc::test::ImproveDirection::kSmallerIsBetter:
        return units + "_smallerIsBetter";
      case webrtc::test::ImproveDirection::kBiggerIsBetter:
        return units + "_biggerIsBetter";
    }
  }

  rtc::CriticalSection crit_;
  FILE* output_ RTC_GUARDED_BY(&crit_);
  std::map<std::string, std::vector<std::string>> graphs_
      RTC_GUARDED_BY(&crit_);
  std::vector<PlottableCounter> plottable_counters_ RTC_GUARDED_BY(&crit_);
};

std::string PerfResultsLogger::ToJSON() const {
  std::ostringstream json_stream;
  json_stream << R"({"format_version":"1.0",)";
  json_stream << R"("charts":{)";
  rtc::CritScope lock(&crit_);
  for (auto graphs_it = graphs_.begin(); graphs_it != graphs_.end();
       ++graphs_it) {
    if (graphs_it != graphs_.begin())
      json_stream << ',';
    json_stream << '"' << graphs_it->first << "\":";
    json_stream << '{';
    OutputListToStream(&json_stream, graphs_it->second);
    json_stream << '}';
  }
  json_stream << "}}";
  return json_stream.str();
}

PerfResultsLogger& GetPerfResultsLogger() {
  static PerfResultsLogger* const logger_ = new PerfResultsLogger();
  return *logger_;
}

}  // namespace

namespace webrtc {
namespace test {

void ClearPerfResults() {
  GetPerfResultsLogger().ClearResults();
}

void SetPerfResultsOutput(FILE* output) {
  GetPerfResultsLogger().SetOutput(output);
}

std::string GetPerfResultsJSON() {
  return GetPerfResultsLogger().ToJSON();
}

void PrintPlottableResults(const std::vector<std::string>& desired_graphs) {
  GetPerfResultsLogger().PrintPlottableCounters(desired_graphs);
}

void WritePerfResults(const std::string& output_path) {
  std::string json_results = GetPerfResultsJSON();
  std::fstream json_file(output_path, std::fstream::out);
  json_file << json_results;
  json_file.close();
}

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const double value,
                 const std::string& units,
                 bool important,
                 ImproveDirection improve_direction) {
  GetPerfResultsLogger().LogResult(measurement + modifier, trace, value, units,
                                   important, improve_direction);
}

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const SamplesStatsCounter& counter,
                 const std::string& units,
                 const bool important,
                 ImproveDirection improve_direction) {
  GetPerfResultsLogger().LogResult(measurement + modifier, trace, counter,
                                   units, important, improve_direction);
}

void PrintResultMeanAndError(const std::string& measurement,
                             const std::string& modifier,
                             const std::string& trace,
                             const double mean,
                             const double error,
                             const std::string& units,
                             bool important,
                             ImproveDirection improve_direction) {
  GetPerfResultsLogger().LogResultMeanAndError(measurement + modifier, trace,
                                               mean, error, units, important,
                                               improve_direction);
}

void PrintResultList(const std::string& measurement,
                     const std::string& modifier,
                     const std::string& trace,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     bool important,
                     ImproveDirection improve_direction) {
  GetPerfResultsLogger().LogResultList(measurement + modifier, trace, values,
                                       units, important, improve_direction);
}

}  // namespace test
}  // namespace webrtc
