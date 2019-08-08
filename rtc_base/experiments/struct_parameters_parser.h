/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_
#define RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
#include "rtc_base/string_encode.h"

namespace webrtc {
namespace struct_parser_impl {
inline std::string StringEncode(bool val) {
  return rtc::ToString(val);
}
inline std::string StringEncode(double val) {
  return rtc::ToString(val);
}
inline std::string StringEncode(int val) {
  return rtc::ToString(val);
}
inline std::string StringEncode(std::string val) {
  return val;
}
inline std::string StringEncode(DataRate val) {
  return ToString(val);
}
inline std::string StringEncode(DataSize val) {
  return ToString(val);
}
inline std::string StringEncode(TimeDelta val) {
  return ToString(val);
}

template <typename T>
inline std::string StringEncode(absl::optional<T> val) {
  if (val)
    return StringEncode(*val);
  return "";
}

template <typename T>
struct LambdaTraits : public LambdaTraits<decltype(&T::operator())> {};

template <typename ClassType, typename RetType, typename SourceType>
struct LambdaTraits<RetType* (ClassType::*)(SourceType*)const> {
  using ret = RetType;
  using src = SourceType;
};

void ParseConfigParams(
    absl::string_view config_str,
    std::map<std::string, std::function<bool(absl::string_view)>> field_map);

std::string EncodeStringStringMap(std::map<std::string, std::string> mapping);

template <typename StructType>
class StructParameterParser {
 public:
  virtual bool Parse(absl::string_view src, StructType* target) const = 0;
  virtual bool Changed(const StructType& src, const StructType& base) const = 0;
  virtual std::string Encode(const StructType& src) const = 0;
  virtual ~StructParameterParser() = default;
};

template <typename StructType, typename T>
class StructParameterImpl : public StructParameterParser<StructType> {
 public:
  explicit StructParameterImpl(std::function<T*(StructType*)> field_getter)
      : field_getter_(std::move(field_getter)) {}
  bool Parse(absl::string_view src, StructType* target) const override {
    auto parsed = ParseTypedParameter<T>(std::string(src));
    if (parsed.has_value())
      *field_getter_(target) = *parsed;
    return parsed.has_value();
  }
  bool Changed(const StructType& src, const StructType& base) const override {
    T base_value = *field_getter_(const_cast<StructType*>(&base));
    T value = *field_getter_(const_cast<StructType*>(&src));
    return value != base_value;
  }
  std::string Encode(const StructType& src) const override {
    T value = *field_getter_(const_cast<StructType*>(&src));
    return struct_parser_impl::StringEncode(value);
  }

 private:
  const std::function<T*(StructType*)> field_getter_;
};

template <typename StructType>
struct StructParameter {
  std::string key;
  StructParameterParser<StructType>* parser;
};

template <typename S,
          typename Closure,
          typename T = typename LambdaTraits<Closure>::ret>
void AddParameters(std::vector<StructParameter<S>>* out,
                   std::string key,
                   Closure getter) {
  auto* parser = new StructParameterImpl<S, T>(getter);
  out->push_back(StructParameter<S>{std::move(key), parser});
}

template <typename S,
          typename Closure,
          typename T = typename LambdaTraits<Closure>::ret,
          typename... Args>
void AddParameters(std::vector<StructParameter<S>>* out,
                   std::string key,
                   Closure getter,
                   Args... args) {
  AddParameters(out, key, getter);
  AddParameters<S>(out, args...);
}

}  // namespace struct_parser_impl

template <typename StructType>
class StructParametersParser {
 public:
  ~StructParametersParser() {
    for (auto& param : parameters_) {
      delete param.parser;
    }
  }

  void Parse(StructType* target, absl::string_view src) {
    std::map<std::string, std::function<bool(absl::string_view)>> field_parsers;
    for (const auto& param : parameters_) {
      field_parsers.emplace(param.key, [target, param](absl::string_view src) {
        return param.parser->Parse(src, target);
      });
    }
    struct_parser_impl::ParseConfigParams(src, std::move(field_parsers));
  }

  StructType Parse(absl::string_view src) {
    StructType res;
    Parse(&res, src);
    return res;
  }

  std::string EncodeChanged(const StructType& src) {
    static StructType base;
    std::map<std::string, std::string> pairs;
    for (const auto& param : parameters_) {
      if (param.parser->Changed(src, base))
        pairs[param.key] = param.parser->Encode(src);
    }
    return struct_parser_impl::EncodeStringStringMap(pairs);
  }

  std::string EncodeAll(const StructType& src) {
    std::map<std::string, std::string> pairs;
    for (const auto& param : parameters_) {
      pairs[param.key] = param.parser->Encode(src);
    }
    return struct_parser_impl::EncodeStringStringMap(pairs);
  }

 private:
  template <typename C, typename S, typename... Args>
  friend std::unique_ptr<StructParametersParser<S>>
  CreateStructParametersParser(std::string, C, Args...);

  explicit StructParametersParser(
      std::vector<struct_parser_impl::StructParameter<StructType>> parameters)
      : parameters_(parameters) {}

  std::vector<struct_parser_impl::StructParameter<StructType>> parameters_;
};

// Creates a struct parameters parser based on interleaved key and field
// accessor arguments, where the field accessor converts a struct pointer to a
// member pointer: FieldType*(StructType*). See the unit tests for example
// usage. Note that the struct type is inferred from the field getters. Beware
// of providing incorrect arguments to this, such as mixing the struct type or
// incorrect return values, as this will cause very confusing compile errors.
template <typename Closure,
          typename S = typename struct_parser_impl::LambdaTraits<Closure>::src,
          typename... Args>
std::unique_ptr<StructParametersParser<S>> CreateStructParametersParser(
    std::string first_key,
    Closure first_getter,
    Args... args) {
  std::vector<struct_parser_impl::StructParameter<S>> parameters;
  struct_parser_impl::AddParameters<S>(&parameters, std::move(first_key),
                                       first_getter, args...);
  // absl::make_unique can't be used since the StructParametersParser
  // constructor is only visible to this create function.
  return absl::WrapUnique(new StructParametersParser<S>(std::move(parameters)));
}
}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_
