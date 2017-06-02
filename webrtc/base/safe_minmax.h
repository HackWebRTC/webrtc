/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Minimum and maximum
// ===================
//
//   rtc::SafeMin(x, y)
//   rtc::SafeMax(x, y)
//
// Accept two arguments of either any two integral or any two floating-point
// types, and return the smaller and larger value, respectively, with no
// truncation or wrap-around. If only one of the input types is statically
// guaranteed to be able to represent the result, the return type is that type;
// if either one would do, the result type is the smaller type. (One of these
// two cases always applies.)
//
// (The case with one floating-point and one integral type is not allowed,
// because the floating-point type will have greater range, but may not have
// sufficient precision to represent the integer value exactly.)
//
// Requesting a specific return type
// =================================
//
// Both functions allow callers to explicitly specify the return type as a
// template parameter, overriding the default return type. E.g.
//
//   rtc::SafeMin<int>(x, y)  // returns an int
//
// If the requested type is statically guaranteed to be able to represent the
// result, then everything's fine, and the return type is as requested. But if
// the requested type is too small, a static_assert is triggered.

#ifndef WEBRTC_BASE_SAFE_MINMAX_H_
#define WEBRTC_BASE_SAFE_MINMAX_H_

#include <limits>
#include <type_traits>

#include "webrtc/base/checks.h"
#include "webrtc/base/safe_compare.h"
#include "webrtc/base/type_traits.h"

namespace rtc {

namespace safe_minmax_impl {

// Make the range of a type available via something other than a constexpr
// function, to work around MSVC limitations. See
// https://blogs.msdn.microsoft.com/vcblog/2015/12/02/partial-support-for-expression-sfinae-in-vs-2015-update-1/
template <typename T>
struct Limits {
  static constexpr T lowest = std::numeric_limits<T>::lowest();
  static constexpr T max = std::numeric_limits<T>::max();
};

template <typename T, bool is_enum = std::is_enum<T>::value>
struct UnderlyingType;

template <typename T>
struct UnderlyingType<T, false> {
  using type = T;
};

template <typename T>
struct UnderlyingType<T, true> {
  using type = typename std::underlying_type<T>::type;
};

// Given two types T1 and T2, find types that can hold the smallest (in
// ::min_t) and the largest (in ::max_t) of the two values.
template <typename T1,
          typename T2,
          bool int1 = IsIntlike<T1>::value,
          bool int2 = IsIntlike<T2>::value>
struct MType {
  static_assert(int1 == int2,
                "You may not mix integral and floating-point arguments");
};

// Specialization for when neither type is integral (and therefore presumably
// floating-point).
template <typename T1, typename T2>
struct MType<T1, T2, false, false> {
  using min_t = typename std::common_type<T1, T2>::type;
  static_assert(std::is_same<min_t, T1>::value ||
                    std::is_same<min_t, T2>::value,
                "");

  using max_t = typename std::common_type<T1, T2>::type;
  static_assert(std::is_same<max_t, T1>::value ||
                    std::is_same<max_t, T2>::value,
                "");
};

// Specialization for when both types are integral.
template <typename T1, typename T2>
struct MType<T1, T2, true, true> {
  // The type with the lowest minimum value. In case of a tie, the type with
  // the lowest maximum value. In case that too is a tie, the types have the
  // same range, and we arbitrarily pick T1.
  using min_t = typename std::conditional<
      safe_cmp::Lt(Limits<T1>::lowest, Limits<T2>::lowest),
      T1,
      typename std::conditional<
          safe_cmp::Gt(Limits<T1>::lowest, Limits<T2>::lowest),
          T2,
          typename std::conditional<safe_cmp::Le(Limits<T1>::max,
                                                 Limits<T2>::max),
                                    T1,
                                    T2>::type>::type>::type;
  static_assert(std::is_same<min_t, T1>::value ||
                    std::is_same<min_t, T2>::value,
                "");

  // The type with the highest maximum value. In case of a tie, the types have
  // the same range (because in C++, integer types with the same maximum also
  // have the same minimum).
  static_assert(safe_cmp::Ne(Limits<T1>::max, Limits<T2>::max) ||
                    safe_cmp::Eq(Limits<T1>::lowest, Limits<T2>::lowest),
                "integer types with the same max should have the same min");
  using max_t = typename std::
      conditional<safe_cmp::Ge(Limits<T1>::max, Limits<T2>::max), T1, T2>::type;
  static_assert(std::is_same<max_t, T1>::value ||
                    std::is_same<max_t, T2>::value,
                "");
};

// A dummy type that we pass around at compile time but never actually use.
// Declared but not defined.
struct DefaultType;

// ::type is A, except we fall back to B if A is DefaultType. We static_assert
// that the chosen type can hold all values that B can hold.
template <typename A, typename B>
struct TypeOr {
  using type = typename std::
      conditional<std::is_same<A, DefaultType>::value, B, A>::type;
  static_assert(safe_cmp::Le(Limits<type>::lowest, Limits<B>::lowest) &&
                    safe_cmp::Ge(Limits<type>::max, Limits<B>::max),
                "The specified type isn't large enough");
  static_assert(IsIntlike<type>::value == IsIntlike<B>::value &&
                    std::is_floating_point<type>::value ==
                        std::is_floating_point<type>::value,
                "float<->int conversions not allowed");
};

}  // namespace safe_minmax_impl

template <
    typename R = safe_minmax_impl::DefaultType,
    typename T1 = safe_minmax_impl::DefaultType,
    typename T2 = safe_minmax_impl::DefaultType,
    typename R2 = typename safe_minmax_impl::TypeOr<
        R,
        typename safe_minmax_impl::MType<
            typename safe_minmax_impl::UnderlyingType<T1>::type,
            typename safe_minmax_impl::UnderlyingType<T2>::type>::min_t>::type>
constexpr R2 SafeMin(T1 a, T2 b) {
  static_assert(IsIntlike<T1>::value || std::is_floating_point<T1>::value,
                "The first argument must be integral or floating-point");
  static_assert(IsIntlike<T2>::value || std::is_floating_point<T2>::value,
                "The second argument must be integral or floating-point");
  return safe_cmp::Lt(a, b) ? static_cast<R2>(a) : static_cast<R2>(b);
}

template <
    typename R = safe_minmax_impl::DefaultType,
    typename T1 = safe_minmax_impl::DefaultType,
    typename T2 = safe_minmax_impl::DefaultType,
    typename R2 = typename safe_minmax_impl::TypeOr<
        R,
        typename safe_minmax_impl::MType<
            typename safe_minmax_impl::UnderlyingType<T1>::type,
            typename safe_minmax_impl::UnderlyingType<T2>::type>::max_t>::type>
constexpr R2 SafeMax(T1 a, T2 b) {
  static_assert(IsIntlike<T1>::value || std::is_floating_point<T1>::value,
                "The first argument must be integral or floating-point");
  static_assert(IsIntlike<T2>::value || std::is_floating_point<T2>::value,
                "The second argument must be integral or floating-point");
  return safe_cmp::Gt(a, b) ? static_cast<R2>(a) : static_cast<R2>(b);
}

}  // namespace rtc

#endif  // WEBRTC_BASE_SAFE_MINMAX_H_
