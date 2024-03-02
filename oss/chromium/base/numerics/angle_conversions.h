// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_ANGLE_CONVERSIONS_H_
#define BASE_NUMERICS_ANGLE_CONVERSIONS_H_

#include <concepts>
#include <numbers>

namespace base {

template <typename T>
  requires std::floating_point<T>
constexpr T DegToRad(T deg) {
  return deg * std::numbers::pi_v<T> / 180;
}

template <typename T>
  requires std::floating_point<T>
constexpr T RadToDeg(T rad) {
  return rad * 180 / std::numbers::pi_v<T>;
}

}  // namespace base

#endif  // BASE_NUMERICS_ANGLE_CONVERSIONS_H_
