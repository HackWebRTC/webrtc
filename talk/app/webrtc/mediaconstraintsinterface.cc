/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#include "talk/app/webrtc/mediaconstraintsinterface.h"

#include "talk/base/stringencode.h"

namespace webrtc {

const char MediaConstraintsInterface::kValueTrue[] = "true";
const char MediaConstraintsInterface::kValueFalse[] = "false";

// Set |value| to the value associated with the first appearance of |key|, or
// return false if |key| is not found.
bool MediaConstraintsInterface::Constraints::FindFirst(
    const std::string& key, std::string* value) const {
  for (Constraints::const_iterator iter = begin(); iter != end(); ++iter) {
    if (iter->key == key) {
      *value = iter->value;
      return true;
    }
  }
  return false;
}

// Find the highest-priority instance of the boolean-valued constraint) named by
// |key| and return its value as |value|. |constraints| can be null.
// If |mandatory_constraints| is non-null, it is incremented if the key appears
// among the mandatory constraints.
// Returns true if the key was found and has a valid boolean value.
// If the key appears multiple times as an optional constraint, appearances
// after the first are ignored.
// Note: Because this uses FindFirst, repeated optional constraints whose
// first instance has an unrecognized value are not handled precisely in
// accordance with the specification.
bool FindConstraint(const MediaConstraintsInterface* constraints,
                    const std::string& key, bool* value,
                    size_t* mandatory_constraints) {
  std::string string_value;
  if (!constraints) {
    return false;
  }
  if (constraints->GetMandatory().FindFirst(key, &string_value)) {
    if (mandatory_constraints)
      ++*mandatory_constraints;
    return talk_base::FromString(string_value, value);
  }
  if (constraints->GetOptional().FindFirst(key, &string_value)) {
    return talk_base::FromString(string_value, value);
  }
  return false;
}

}  // namespace webrtc
