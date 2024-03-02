// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_OSTREAM_OPERATORS_H_
#define BASE_NUMERICS_OSTREAM_OPERATORS_H_

#include <ostream>

namespace base {
namespace internal {

template <typename T>
class ClampedNumeric;
template <typename T>
class StrictNumeric;

// Overload the ostream output operator to make logging work nicely.
template <typename T>
std::ostream& operator<<(std::ostream& os, const StrictNumeric<T>& value) {
  os << static_cast<T>(value);
  return os;
}

// Overload the ostream output operator to make logging work nicely.
template <typename T>
std::ostream& operator<<(std::ostream& os, const ClampedNumeric<T>& value) {
  os << static_cast<T>(value);
  return os;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_NUMERICS_OSTREAM_OPERATORS_H_
