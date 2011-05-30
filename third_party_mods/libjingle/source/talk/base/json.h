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

#ifndef TALK_BASE_JSON_H_
#define TALK_BASE_JSON_H_

#include <string>
#include <vector>

#include "json/json.h"

// TODO(juberti): Move to talk_base namespace

///////////////////////////////////////////////////////////////////////////////
// JSON Helpers
///////////////////////////////////////////////////////////////////////////////

// Robust conversion operators, better than the ones in JsonCpp.
bool GetIntFromJson(const Json::Value& in, int* out);
bool GetUIntFromJson(const Json::Value& in, unsigned int* out);
bool GetStringFromJson(const Json::Value& in, std::string* out);
bool GetBoolFromJson(const Json::Value& in, bool* out);

// Pull values out of a JSON array.
bool GetValueFromJsonArray(const Json::Value& in, size_t n,
                           Json::Value* out);
bool GetIntFromJsonArray(const Json::Value& in, size_t n,
                         int* out);
bool GetUIntFromJsonArray(const Json::Value& in, size_t n,
                          unsigned int* out);
bool GetStringFromJsonArray(const Json::Value& in, size_t n,
                            std::string* out);
bool GetBoolFromJsonArray(const Json::Value& in, size_t n,
                          bool* out);

// Pull values out of a JSON object.
bool GetValueFromJsonObject(const Json::Value& in, const std::string& k,
                            Json::Value* out);
bool GetIntFromJsonObject(const Json::Value& in, const std::string& k,
                          int* out);
bool GetUIntFromJsonObject(const Json::Value& in, const std::string& k,
                           unsigned int* out);
bool GetStringFromJsonObject(const Json::Value& in, const std::string& k,
                             std::string* out);
bool GetBoolFromJsonObject(const Json::Value& in, const std::string& k,
                           bool* out);

// Converts vectors of strings to/from JSON arrays.
Json::Value StringVectorToJsonValue(const std::vector<std::string>& strings);
bool JsonValueToStringVector(const Json::Value& value,
                             std::vector<std::string> *strings);

// Writes out a Json value as a string.
std::string JsonValueToString(const Json::Value& json);

#endif  // TALK_BASE_JSON_H_
