/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_STATS_RTCSTATS_H_
#define WEBRTC_API_STATS_RTCSTATS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "webrtc/base/checks.h"

namespace webrtc {

class RTCStatsMemberInterface;

// Abstract base class for RTCStats-derived dictionaries, see
// https://w3c.github.io/webrtc-stats/.
//
// All derived classes must have the following static variable defined:
//   static const char kType[];
// It is used as a unique class identifier and a string representation of the
// class type, see https://w3c.github.io/webrtc-stats/#rtcstatstype-str*.
// Use the |WEBRTC_RTCSTATS_IMPL| macro when implementing subclasses, see macro
// for details.
//
// Derived classes list their dictionary members, RTCStatsMember<T>, as public
// fields, allowing the following:
//
// RTCFooStats foo("fooId", GetCurrentTime());
// foo.bar = 42;
// foo.baz = std::vector<std::string>();
// foo.baz->push_back("hello world");
// uint32_t x = *foo.bar;
//
// Pointers to all the members are available with |Members|, allowing iteration:
//
// for (const RTCStatsMemberInterface* member : foo.Members()) {
//   printf("%s = %s\n", member->name(), member->ValueToString().c_str());
// }
class RTCStats {
 public:
  RTCStats(const std::string& id, int64_t timestamp_us)
      : id_(id), timestamp_us_(timestamp_us) {}
  RTCStats(std::string&& id, int64_t timestamp_us)
      : id_(std::move(id)), timestamp_us_(timestamp_us) {}
  virtual ~RTCStats() {}

  virtual std::unique_ptr<RTCStats> copy() const = 0;

  const std::string& id() const { return id_; }
  // Time relative to the UNIX epoch (Jan 1, 1970, UTC), in microseconds.
  int64_t timestamp_us() const { return timestamp_us_; }
  // Returns the static member variable |kType| of the implementing class.
  virtual const char* type() const = 0;
  // Returns a vector of pointers to all the RTCStatsMemberInterface members of
  // this class. This allows for iteration of members.
  std::vector<const RTCStatsMemberInterface*> Members() const;

  // Creates a human readable string representation of the report, listing all
  // of its members (names and values).
  std::string ToString() const;

  // Downcasts the stats object to an |RTCStats| subclass |T|. DCHECKs that the
  // object is of type |T|.
  template<typename T>
  const T& cast_to() const {
    RTC_DCHECK_EQ(type(), T::kType);
    return static_cast<const T&>(*this);
  }

 protected:
  // Gets a vector of all members of this |RTCStats| object, including members
  // derived from parent classes. |additional_capacity| is how many more members
  // shall be reserved in the vector (so that subclasses can allocate a vector
  // with room for both parent and child members without it having to resize).
  virtual std::vector<const RTCStatsMemberInterface*>
  MembersOfThisObjectAndAncestors(
      size_t additional_capacity) const;

  std::string const id_;
  int64_t timestamp_us_;
};

// All |RTCStats| classes should use this macro in a public section of the class
// definition.
//
// This macro declares the static |kType| and overrides methods as required by
// subclasses of |RTCStats|: |copy|, |type|, and
// |MembersOfThisObjectAndAncestors|. The |...| argument is a list of addresses
// to each member defined in the implementing class (list cannot be empty, must
// have at least one new member).
//
// (Since class names need to be known to implement these methods this cannot be
// part of the base |RTCStats|. While these methods could be implemented using
// templates, that would only work for immediate subclasses. Subclasses of
// subclasses also have to override these methods, resulting in boilerplate
// code. Using a macro avoids this and works for any |RTCStats| class, including
// grandchildren.)
//
// Sample usage:
//
// rtcfoostats.h:
//   class RTCFooStats : public RTCStats {
//    public:
//     RTCFooStats(const std::string& id, int64_t timestamp_us)
//         : RTCStats(id, timestamp_us),
//           foo("foo"),
//           bar("bar") {
//     }
//
//     WEBRTC_RTCSTATS_IMPL(RTCStats, RTCFooStats,
//         &foo,
//         &bar);
//
//     RTCStatsMember<int32_t> foo;
//     RTCStatsMember<int32_t> bar;
//   };
//
// rtcfoostats.cc:
//   const char RTCFooStats::kType[] = "foo-stats";
//
#define WEBRTC_RTCSTATS_IMPL(parent_class, this_class, ...)                    \
 public:                                                                       \
  static const char kType[];                                                   \
  std::unique_ptr<webrtc::RTCStats> copy() const override {                    \
    return std::unique_ptr<webrtc::RTCStats>(new this_class(*this));           \
  }                                                                            \
  const char* type() const override { return this_class::kType; }              \
 protected:                                                                    \
  std::vector<const webrtc::RTCStatsMemberInterface*>                          \
  MembersOfThisObjectAndAncestors(                                             \
      size_t local_var_additional_capacity) const override {                   \
    const webrtc::RTCStatsMemberInterface* local_var_members[] = {             \
      __VA_ARGS__                                                              \
    };                                                                         \
    size_t local_var_members_count =                                           \
        sizeof(local_var_members) / sizeof(local_var_members[0]);              \
    std::vector<const webrtc::RTCStatsMemberInterface*> local_var_members_vec =\
        parent_class::MembersOfThisObjectAndAncestors(                         \
            local_var_members_count + local_var_additional_capacity);          \
    RTC_DCHECK_GE(                                                             \
        local_var_members_vec.capacity() - local_var_members_vec.size(),       \
        local_var_members_count + local_var_additional_capacity);              \
    local_var_members_vec.insert(local_var_members_vec.end(),                  \
                                 &local_var_members[0],                        \
                                 &local_var_members[local_var_members_count]); \
    return local_var_members_vec;                                              \
  }                                                                            \
 public:

// Interface for |RTCStats| members, which have a name and a value of a type
// defined in a subclass. Only the types listed in |Type| are supported, these
// are implemented by |RTCStatsMember<T>|. The value of a member may be
// undefined, the value can only be read if |is_defined|.
class RTCStatsMemberInterface {
 public:
  // Member value types.
  enum Type {
    kInt32,                 // int32_t
    kUint32,                // uint32_t
    kInt64,                 // int64_t
    kUint64,                // uint64_t
    kDouble,                // double
    kString,                // std::string

    kSequenceInt32,         // std::vector<int32_t>
    kSequenceUint32,        // std::vector<uint32_t>
    kSequenceInt64,         // std::vector<int64_t>
    kSequenceUint64,        // std::vector<uint64_t>
    kSequenceDouble,        // std::vector<double>
    kSequenceString,        // std::vector<std::string>
  };

  virtual ~RTCStatsMemberInterface() {}

  const char* name() const { return name_; }
  virtual Type type() const = 0;
  virtual bool is_sequence() const = 0;
  virtual bool is_string() const = 0;
  bool is_defined() const { return is_defined_; }
  virtual std::string ValueToString() const = 0;

  template<typename T>
  const T& cast_to() const {
    RTC_DCHECK_EQ(type(), T::kType);
    return static_cast<const T&>(*this);
  }

 protected:
  RTCStatsMemberInterface(const char* name, bool is_defined)
      : name_(name), is_defined_(is_defined) {}

  const char* const name_;
  bool is_defined_;
};

// Template implementation of |RTCStatsMemberInterface|. Every possible |T| is
// specialized in rtcstats.cc, using a different |T| results in a linker error
// (undefined reference to |kType|). The supported types are the ones described
// by |RTCStatsMemberInterface::Type|.
template<typename T>
class RTCStatsMember : public RTCStatsMemberInterface {
 public:
  static const Type kType;

  explicit RTCStatsMember(const char* name)
      : RTCStatsMemberInterface(name, false),
        value_() {}
  RTCStatsMember(const char* name, const T& value)
      : RTCStatsMemberInterface(name, true),
        value_(value) {}
  RTCStatsMember(const char* name, T&& value)
      : RTCStatsMemberInterface(name, true),
        value_(std::move(value)) {}
  explicit RTCStatsMember(const RTCStatsMember<T>& other)
      : RTCStatsMemberInterface(other.name_, other.is_defined_),
        value_(other.value_) {}
  explicit RTCStatsMember(RTCStatsMember<T>&& other)
      : RTCStatsMemberInterface(other.name_, other.is_defined_),
        value_(std::move(other.value_)) {}

  Type type() const override { return kType; }
  bool is_sequence() const override;
  bool is_string() const override;
  std::string ValueToString() const override;

  // Assignment operators.
  T& operator=(const T& value) {
    value_ = value;
    is_defined_ = true;
    return value_;
  }
  T& operator=(const T&& value) {
    value_ = std::move(value);
    is_defined_ = true;
    return value_;
  }
  T& operator=(const RTCStatsMember<T>& other) {
    RTC_DCHECK(other.is_defined_);
    value_ = other.is_defined_;
    is_defined_ = true;
    return value_;
  }

  // Value getters.
  T& operator*() {
    RTC_DCHECK(is_defined_);
    return value_;
  }
  const T& operator*() const {
    RTC_DCHECK(is_defined_);
    return value_;
  }

  // Value getters, arrow operator.
  T* operator->() {
    RTC_DCHECK(is_defined_);
    return &value_;
  }
  const T* operator->() const {
    RTC_DCHECK(is_defined_);
    return &value_;
  }

 private:
  T value_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_STATS_RTCSTATS_H_
