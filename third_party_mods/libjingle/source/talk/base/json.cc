/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/json.h"

#include <errno.h>

#include <climits>
#include <cstdlib>
#include <sstream>

bool GetStringFromJson(const Json::Value& in, std::string* out) {
  if (!in.isString()) {
    std::ostringstream s;
    if (in.isBool()) {
      s << std::boolalpha << in.asBool();
    } else if (in.isInt()) {
      s << in.asInt();
    } else if (in.isUInt()) {
      s << in.asUInt();
    } else if (in.isDouble()) {
      s << in.asDouble();
    } else {
      return false;
    }
    *out = s.str();
  } else {
    *out = in.asString();
  }
  return true;
}

bool GetIntFromJson(const Json::Value& in, int* out) {
  bool ret;
  if (!in.isString()) {
    ret = in.isConvertibleTo(Json::intValue);
    if (ret) {
      *out = in.asInt();
    }
  } else {
    long val;  // NOLINT
    const char* c_str = in.asCString();
    char* end_ptr;
    errno = 0;
    val = strtol(c_str, &end_ptr, 10);  // NOLINT
    ret = (end_ptr != c_str && *end_ptr == '\0' && !errno &&
           val >= INT_MIN && val <= INT_MAX);
    *out = val;
  }
  return ret;
}

bool GetUIntFromJson(const Json::Value& in, unsigned int* out) {
  bool ret;
  if (!in.isString()) {
    ret = in.isConvertibleTo(Json::uintValue);
    if (ret) {
      *out = in.asUInt();
    }
  } else {
    unsigned long val;  // NOLINT
    const char* c_str = in.asCString();
    char* end_ptr;
    errno = 0;
    val = strtoul(c_str, &end_ptr, 10);  // NOLINT
    ret = (end_ptr != c_str && *end_ptr == '\0' && !errno &&
           val <= UINT_MAX);
    *out = val;
  }
  return ret;
}

bool GetBoolFromJson(const Json::Value& in, bool* out) {
  bool ret;
  if (!in.isString()) {
    ret = in.isConvertibleTo(Json::booleanValue);
    if (ret) {
      *out = in.asBool();
    }
  } else {
    if (in.asString() == "true") {
      *out = true;
      ret = true;
    } else if (in.asString() == "false") {
      *out = false;
      ret = true;
    } else {
      ret = false;
    }
  }
  return ret;
}

bool GetValueFromJsonArray(const Json::Value& in, size_t n,
                           Json::Value* out) {
  if (!in.isArray() || !in.isValidIndex(n)) {
    return false;
  }

  *out = in[n];
  return true;
}

bool GetIntFromJsonArray(const Json::Value& in, size_t n,
                         int* out) {
  Json::Value x;
  return GetValueFromJsonArray(in, n, &x) && GetIntFromJson(x, out);
}

bool GetUIntFromJsonArray(const Json::Value& in, size_t n,
                          unsigned int* out)  {
  Json::Value x;
  return GetValueFromJsonArray(in, n, &x) && GetUIntFromJson(x, out);
}

bool GetStringFromJsonArray(const Json::Value& in, size_t n,
                            std::string* out) {
  Json::Value x;
  return GetValueFromJsonArray(in, n, &x) && GetStringFromJson(x, out);
}

bool GetBoolFromJsonArray(const Json::Value& in, size_t n,
                          bool* out) {
  Json::Value x;
  return GetValueFromJsonArray(in, n, &x) && GetBoolFromJson(x, out);
}

bool GetValueFromJsonObject(const Json::Value& in, const std::string& k,
                            Json::Value* out) {
  if (!in.isObject() || !in.isMember(k)) {
    return false;
  }

  *out = in[k];
  return true;
}


bool GetIntFromJsonObject(const Json::Value& in, const std::string& k,
                          int* out) {
  Json::Value x;
  return GetValueFromJsonObject(in, k, &x) && GetIntFromJson(x, out);
}

bool GetUIntFromJsonObject(const Json::Value& in, const std::string& k,
                           unsigned int* out)  {
  Json::Value x;
  return GetValueFromJsonObject(in, k, &x) && GetUIntFromJson(x, out);
}

bool GetStringFromJsonObject(const Json::Value& in, const std::string& k,
                             std::string* out)  {
  Json::Value x;
  return GetValueFromJsonObject(in, k, &x) && GetStringFromJson(x, out);
}

bool GetBoolFromJsonObject(const Json::Value& in, const std::string& k,
                           bool* out) {
  Json::Value x;
  return GetValueFromJsonObject(in, k, &x) && GetBoolFromJson(x, out);
}

Json::Value StringVectorToJsonValue(const std::vector<std::string>& strings) {
  Json::Value result(Json::arrayValue);
  for (size_t i = 0; i < strings.size(); ++i) {
    result.append(Json::Value(strings[i]));
  }
  return result;
}

bool JsonValueToStringVector(const Json::Value& value,
                             std::vector<std::string> *strings) {
  strings->clear();
  if (!value.isArray()) {
    return false;
  }

  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i].isString()) {
      strings->push_back(value[i].asString());
    } else {
      return false;
    }
  }

  return true;
}

std::string JsonValueToString(const Json::Value& json) {
  Json::FastWriter w;
  std::string value = w.write(json);
  return value.substr(0, value.size() - 1);  // trim trailing newline
}
