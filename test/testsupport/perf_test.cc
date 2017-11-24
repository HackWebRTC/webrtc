/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// A stripped-down version of Chromium's chrome/test/perf/perf_test.cc.
// ResultsToString(), PrintResult(size_t value) and AppendResult(size_t value)
// have been modified. The remainder are identical to the Chromium version.

#include "test/testsupport/perf_test.h"

#include <sstream>
#include <stdio.h>

namespace {

std::string ResultsToString(const std::string& measurement,
                            const std::string& modifier,
                            const std::string& trace,
                            const std::string& values,
                            const std::string& prefix,
                            const std::string& suffix,
                            const std::string& units,
                            bool important) {
  // <*>RESULT <graph_name>: <trace_name>= <value> <units>
  // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
  // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>

  // TODO(ajm): Use of a stream here may violate the style guide (depending on
  // one's definition of "logging"). Consider adding StringPrintf-like
  // functionality as in the original Chromium implementation.
  std::ostringstream stream;
  if (important) {
    stream << "*";
  }
  stream << "RESULT " << measurement << modifier << ": " << trace << "= "
         << prefix << values << suffix << " " << units << std::endl;
  return stream.str();
}

void PrintResultsImpl(const std::string& measurement,
                      const std::string& modifier,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& prefix,
                      const std::string& suffix,
                      const std::string& units,
                      bool important) {
  printf("%s", ResultsToString(measurement, modifier, trace, values,
                               prefix, suffix, units, important).c_str());
}

}  // namespace

namespace webrtc {
namespace test {

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const double value,
                 const std::string& units,
                 bool important) {
  std::ostringstream value_stream;
  value_stream << value;
  PrintResultsImpl(measurement, modifier, trace, value_stream.str(), "", "",
                   units, important);
}

void PrintResultMeanAndError(const std::string& measurement,
                             const std::string& modifier,
                             const std::string& trace,
                             const double mean,
                             const double error,
                             const std::string& units,
                             bool important) {
  std::ostringstream value_stream;
  value_stream << '{' << mean << ',' << error << '}';
  PrintResultsImpl(measurement, modifier, trace, value_stream.str(), "", "",
                   units, important);
}

void PrintResultList(const std::string& measurement,
                     const std::string& modifier,
                     const std::string& trace,
                     const std::string& values,
                     const std::string& units,
                     bool important) {
  PrintResultsImpl(measurement, modifier, trace, values,
                   "[", "]", units, important);
}

}  // namespace test
}  // namespace webrtc
