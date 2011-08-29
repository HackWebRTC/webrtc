/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <string>

#include "data_log.h"
#include "gtest/gtest.h"

using ::webrtc::DataLog;

// A class for storing the values expected from a log table column when
// verifying a log table file.
struct ExpectedValues {
 public:
  ExpectedValues()
    : values(NULL),
      multi_value_length(1) {
  }

  ExpectedValues(std::vector<std::string> expected_values,
                 int expected_multi_value_length)
    : values(expected_values),
      multi_value_length(expected_multi_value_length) {
  }

  std::vector<std::string> values;
  int multi_value_length;
};

typedef std::map<std::string, ExpectedValues> ExpectedValuesMap;

// A static class used for parsing and verifying data log files.
class DataLogParser {
 public:
  // Verifies that the log table stored in the file "log_file" corresponds to
  // the cells and columns specified in "columns".
  static int VerifyTable(FILE* log_file, const ExpectedValuesMap& columns) {
    int row = 0;
    char line_buffer[kMaxLineLength];
    char* ret = fgets(line_buffer, kMaxLineLength, log_file);
    EXPECT_FALSE(ret == NULL);
    if (ret == NULL)
      return -1;

    std::string line(line_buffer, kMaxLineLength);
    VerifyHeader(line, columns);
    while (fgets(line_buffer, kMaxLineLength, log_file) != NULL) {
      line = std::string(line_buffer, kMaxLineLength);
      size_t line_position = 0;

      for (ExpectedValuesMap::const_iterator it = columns.begin();
           it != columns.end(); ++it) {
        std::string str = ParseElement(line, &line_position,
                                       it->second.multi_value_length);
        EXPECT_EQ(str, it->second.values[row]);
        if (str != it->second.values[row])
          return -1;
      }
      ++row;
    }
    return 0;
  }

  // Verifies the table header stored in "line" to correspond with the header
  // specified in "columns".
  static int VerifyHeader(const std::string& line,
                          const ExpectedValuesMap& columns) {
    size_t line_position = 0;
    for (ExpectedValuesMap::const_iterator it = columns.begin();
         it != columns.end(); ++it) {
      std::string str = ParseElement(line, &line_position,
                                     it->second.multi_value_length);
      EXPECT_EQ(str, it->first);
      if (str != it->first)
        return -1;
    }
    return 0;
  }

  // Parses out and returns one element from the string "line", which contains
  // one line read from a log table file. An element can either be a column
  // header or a cell of a row.
  static std::string ParseElement(const std::string& line,
                                  size_t* line_position,
                                  int multi_value_length) {
    std::string parsed_cell;
    parsed_cell = "";
    for (int i = 0; i < multi_value_length; ++i) {
      size_t next_separator = line.find(',', *line_position);
      EXPECT_NE(next_separator, std::string::npos);
      if (next_separator == std::string::npos)
        break;
      parsed_cell += line.substr(*line_position,
                                 next_separator - *line_position + 1);
      *line_position = next_separator + 1;
    }
    return parsed_cell;
  }

  // This constant defines the maximum line length the DataLogParser can
  // parse.
  enum { kMaxLineLength = 100 };
};

TEST(TestDataLog, CreateReturnTest) {
  for (int i = 0; i < 10; ++i)
    ASSERT_EQ(DataLog::CreateLog(), 0);
  ASSERT_EQ(DataLog::AddTable("a proper table", "table.txt"), 0);
  for (int i = 0; i < 10; ++i)
    DataLog::ReturnLog();
  ASSERT_LT(DataLog::AddTable("table failure", "table.txt"), 0);
}

TEST(TestDataLog, VerifySingleTable) {
  DataLog::CreateLog();
  DataLog::AddTable("table1", "table1.txt");
  DataLog::AddColumn("table1", "arrival", 1);
  DataLog::AddColumn("table1", "timestamp", 1);
  DataLog::AddColumn("table1", "size", 5);
  WebRtc_UWord32 sizes[5] = {1400, 1500, 1600, 1700, 1800};
  for (int i = 0; i < 10; ++i) {
    DataLog::InsertCell("table1", "arrival", static_cast<double>(i));
    DataLog::InsertCell("table1", "timestamp",
                        static_cast<WebRtc_Word64>(4354 + i));
    DataLog::InsertCell("table1", "size", sizes, 5);
    DataLog::NextRow("table1");
  }
  DataLog::ReturnLog();
  // Verify file
  FILE* table = fopen("table1.txt", "r");
  ASSERT_FALSE(table == NULL);
  // Read the column names and verify with the expected columns.
  // Note that the columns are written to file in alphabetical order.
  // Data expected from parsing the file
  const int kNumberOfRows = 10;
  std::string string_arrival[kNumberOfRows] = {
    "0,", "1,", "2,", "3,", "4,",
    "5,", "6,", "7,", "8,", "9,"
  };
  std::string string_timestamp[kNumberOfRows] = {
    "4354,", "4355,", "4356,", "4357,",
    "4358,", "4359,", "4360,", "4361,",
    "4362,", "4363,"
  };
  std::string string_sizes = "1400,1500,1600,1700,1800,";
  ExpectedValuesMap expected;
  expected["arrival,"] = ExpectedValues(
                           std::vector<std::string>(string_arrival,
                                                    string_arrival +
                                                    kNumberOfRows),
                           1);
  expected["size[5],,,,,"] = ExpectedValues(
                               std::vector<std::string>(10, string_sizes), 5);
  expected["timestamp,"] = ExpectedValues(
                             std::vector<std::string>(string_timestamp,
                                                      string_timestamp +
                                                      kNumberOfRows),
                             1);
  ASSERT_EQ(DataLogParser::VerifyTable(table, expected), 0);
  fclose(table);
}

TEST(TestDataLog, VerifyMultipleTables) {
  DataLog::CreateLog();
  DataLog::AddTable("table2", "table2.txt");
  DataLog::AddTable("table3", "table3.txt");
  DataLog::AddColumn("table2", "arrival", 1);
  DataLog::AddColumn("table2", "timestamp", 1);
  DataLog::AddColumn("table2", "size", 1);
  DataLog::AddTable("table4", "table4.txt");
  DataLog::AddColumn("table3", "timestamp", 1);
  DataLog::AddColumn("table3", "arrival", 1);
  DataLog::AddColumn("table4", "size", 1);
  for (WebRtc_Word32 i = 0; i < 10; ++i) {
    DataLog::InsertCell("table2", "arrival",
                        static_cast<WebRtc_Word32>(i));
    DataLog::InsertCell("table2", "timestamp",
                        static_cast<WebRtc_Word32>(4354 + i));
    DataLog::InsertCell("table2", "size",
                        static_cast<WebRtc_Word32>(1200 + 10 * i));
    DataLog::InsertCell("table3", "timestamp",
                        static_cast<WebRtc_Word32>(4354 + i));
    DataLog::InsertCell("table3", "arrival",
                        static_cast<WebRtc_Word32>(i));
    DataLog::InsertCell("table4", "size",
                        static_cast<WebRtc_Word32>(1200 + 10 * i));
    DataLog::NextRow("table4");
    DataLog::NextRow("table2");
    DataLog::NextRow("table3");
  }
  DataLog::ReturnLog();

  // Data expected from parsing the file
  const int kNumberOfRows = 10;
  std::string string_arrival[kNumberOfRows] = {
    "0,", "1,", "2,", "3,", "4,",
    "5,", "6,", "7,", "8,", "9,"
  };
  std::string string_timestamp[kNumberOfRows] = {
    "4354,", "4355,", "4356,", "4357,",
    "4358,", "4359,", "4360,", "4361,",
    "4362,", "4363,"
  };
  std::string string_size[kNumberOfRows] = {
    "1200,", "1210,", "1220,", "1230,",
    "1240,", "1250,", "1260,", "1270,",
    "1280,", "1290,"
  };

  // Verify table 2
  {
    FILE* table = fopen("table2.txt", "r");
    ASSERT_FALSE(table == NULL);
    ExpectedValuesMap expected;
    expected["arrival,"] = ExpectedValues(
                             std::vector<std::string>(string_arrival,
                                                      string_arrival +
                                                      kNumberOfRows),
                             1);
    expected["size,"] = ExpectedValues(
                          std::vector<std::string>(string_size,
                                                   string_size + kNumberOfRows),
                          1);
    expected["timestamp,"] = ExpectedValues(
                               std::vector<std::string>(string_timestamp,
                                                        string_timestamp +
                                                        kNumberOfRows),
                               1);
    ASSERT_EQ(DataLogParser::VerifyTable(table, expected), 0);
    fclose(table);
  }

  // Verify table 3
  {
    FILE* table = fopen("table3.txt", "r");
    ASSERT_FALSE(table == NULL);
    ExpectedValuesMap expected;
    expected["arrival,"] = ExpectedValues(
                             std::vector<std::string>(string_arrival,
                                                      string_arrival +
                                                      sizeof(string_arrival)
                                                      / sizeof(std::string)),
                             1);
    expected["timestamp,"] = ExpectedValues(
                             std::vector<std::string>(string_timestamp,
                                                      string_timestamp +
                                                      sizeof(string_timestamp) /
                                                      sizeof(std::string)),
                               1);
    ASSERT_EQ(DataLogParser::VerifyTable(table, expected), 0);
    fclose(table);
  }

  // Verify table 4
  {
    FILE* table = fopen("table4.txt", "r");
    ASSERT_FALSE(table == NULL);
    ExpectedValuesMap expected;
    expected["size,"] = ExpectedValues(
                          std::vector<std::string>(string_size,
                                                   string_size +
                                                   sizeof(string_size)
                                                   / sizeof(std::string)),
                          1);
    ASSERT_EQ(DataLogParser::VerifyTable(table, expected), 0);
    fclose(table);
  }
}
