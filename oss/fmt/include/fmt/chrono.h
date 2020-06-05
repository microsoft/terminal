// Formatting library for C++ - chrono support
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_CHRONO_H_
#define FMT_CHRONO_H_

#include <chrono>
#include <ctime>
#include <locale>
#include <sstream>

#include "format.h"
#include "locale.h"

FMT_BEGIN_NAMESPACE

// Enable safe chrono durations, unless explicitly disabled.
#ifndef FMT_SAFE_DURATION_CAST
#  define FMT_SAFE_DURATION_CAST 1
#endif
#if FMT_SAFE_DURATION_CAST

// For conversion between std::chrono::durations without undefined
// behaviour or erroneous results.
// This is a stripped down version of duration_cast, for inclusion in fmt.
// See https://github.com/pauldreik/safe_duration_cast
//
// Copyright Paul Dreik 2019
namespace safe_duration_cast {

template <typename To, typename From,
          FMT_ENABLE_IF(!std::is_same<From, To>::value &&
                        std::numeric_limits<From>::is_signed ==
                            std::numeric_limits<To>::is_signed)>
FMT_CONSTEXPR To lossless_integral_conversion(const From from, int& ec) {
  ec = 0;
  using F = std::numeric_limits<From>;
  using T = std::numeric_limits<To>;
  static_assert(F::is_integer, "From must be integral");
  static_assert(T::is_integer, "To must be integral");

  // A and B are both signed, or both unsigned.
  if (F::digits <= T::digits) {
    // From fits in To without any problem.
  } else {
    // From does not always fit in To, resort to a dynamic check.
    if (from < T::min() || from > T::max()) {
      // outside range.
      ec = 1;
      return {};
    }
  }
  return static_cast<To>(from);
}

/**
 * converts From to To, without loss. If the dynamic value of from
 * can't be converted to To without loss, ec is set.
 */
template <typename To, typename From,
          FMT_ENABLE_IF(!std::is_same<From, To>::value &&
                        std::numeric_limits<From>::is_signed !=
                            std::numeric_limits<To>::is_signed)>
FMT_CONSTEXPR To lossless_integral_conversion(const From from, int& ec) {
  ec = 0;
  using F = std::numeric_limits<From>;
  using T = std::numeric_limits<To>;
  static_assert(F::is_integer, "From must be integral");
  static_assert(T::is_integer, "To must be integral");

  if (F::is_signed && !T::is_signed) {
    // From may be negative, not allowed!
    if (fmt::internal::is_negative(from)) {
      ec = 1;
      return {};
    }

    // From is positive. Can it always fit in To?
    if (F::digits <= T::digits) {
      // yes, From always fits in To.
    } else {
      // from may not fit in To, we have to do a dynamic check
      if (from > static_cast<From>(T::max())) {
        ec = 1;
        return {};
      }
    }
  }

  if (!F::is_signed && T::is_signed) {
    // can from be held in To?
    if (F::digits < T::digits) {
      // yes, From always fits in To.
    } else {
      // from may not fit in To, we have to do a dynamic check
      if (from > static_cast<From>(T::max())) {
        // outside range.
        ec = 1;
        return {};
      }
    }
  }

  // reaching here means all is ok for lossless conversion.
  return static_cast<To>(from);

}  // function

template <typename To, typename From,
          FMT_ENABLE_IF(std::is_same<From, To>::value)>
FMT_CONSTEXPR To lossless_integral_conversion(const From from, int& ec) {
  ec = 0;
  return from;
}  // function

// clang-format off
/**
 * converts From to To if possible, otherwise ec is set.
 *
 * input                            |    output
 * ---------------------------------|---------------
 * NaN                              | NaN
 * Inf                              | Inf
 * normal, fits in output           | converted (possibly lossy)
 * normal, does not fit in output   | ec is set
 * subnormal                        | best effort
 * -Inf                             | -Inf
 */
// clang-format on
template <typename To, typename From,
          FMT_ENABLE_IF(!std::is_same<From, To>::value)>
FMT_CONSTEXPR To safe_float_conversion(const From from, int& ec) {
  ec = 0;
  using T = std::numeric_limits<To>;
  static_assert(std::is_floating_point<From>::value, "From must be floating");
  static_assert(std::is_floating_point<To>::value, "To must be floating");

  // catch the only happy case
  if (std::isfinite(from)) {
    if (from >= T::lowest() && from <= T::max()) {
      return static_cast<To>(from);
    }
    // not within range.
    ec = 1;
    return {};
  }

  // nan and inf will be preserved
  return static_cast<To>(from);
}  // function

template <typename To, typename From,
          FMT_ENABLE_IF(std::is_same<From, To>::value)>
FMT_CONSTEXPR To safe_float_conversion(const From from, int& ec) {
  ec = 0;
  static_assert(std::is_floating_point<From>::value, "From must be floating");
  return from;
}

/**
 * safe duration cast between integral durations
 */
template <typename To, typename FromRep, typename FromPeriod,
          FMT_ENABLE_IF(std::is_integral<FromRep>::value),
          FMT_ENABLE_IF(std::is_integral<typename To::rep>::value)>
To safe_duration_cast(std::chrono::duration<FromRep, FromPeriod> from,
                      int& ec) {
  using From = std::chrono::duration<FromRep, FromPeriod>;
  ec = 0;
  // the basic idea is that we need to convert from count() in the from type
  // to count() in the To type, by multiplying it with this:
  struct Factor
      : std::ratio_divide<typename From::period, typename To::period> {};

  static_assert(Factor::num > 0, "num must be positive");
  static_assert(Factor::den > 0, "den must be positive");

  // the conversion is like this: multiply from.count() with Factor::num
  // /Factor::den and convert it to To::rep, all this without
  // overflow/underflow. let's start by finding a suitable type that can hold
  // both To, From and Factor::num
  using IntermediateRep =
      typename std::common_type<typename From::rep, typename To::rep,
                                decltype(Factor::num)>::type;

  // safe conversion to IntermediateRep
  IntermediateRep count =
      lossless_integral_conversion<IntermediateRep>(from.count(), ec);
  if (ec) {
    return {};
  }
  // multiply with Factor::num without overflow or underflow
  if (Factor::num != 1) {
    const auto max1 = internal::max_value<IntermediateRep>() / Factor::num;
    if (count > max1) {
      ec = 1;
      return {};
    }
    const auto min1 = std::numeric_limits<IntermediateRep>::min() / Factor::num;
    if (count < min1) {
      ec = 1;
      return {};
    }
    count *= Factor::num;
  }

  // this can't go wrong, right? den>0 is checked earlier.
  if (Factor::den != 1) {
    count /= Factor::den;
  }
  // convert to the to type, safely
  using ToRep = typename To::rep;
  const ToRep tocount = lossless_integral_conversion<ToRep>(count, ec);
  if (ec) {
    return {};
  }
  return To{tocount};
}

/**
 * safe duration_cast between floating point durations
 */
template <typename To, typename FromRep, typename FromPeriod,
          FMT_ENABLE_IF(std::is_floating_point<FromRep>::value),
          FMT_ENABLE_IF(std::is_floating_point<typename To::rep>::value)>
To safe_duration_cast(std::chrono::duration<FromRep, FromPeriod> from,
                      int& ec) {
  using From = std::chrono::duration<FromRep, FromPeriod>;
  ec = 0;
  if (std::isnan(from.count())) {
    // nan in, gives nan out. easy.
    return To{std::numeric_limits<typename To::rep>::quiet_NaN()};
  }
  // maybe we should also check if from is denormal, and decide what to do about
  // it.

  // +-inf should be preserved.
  if (std::isinf(from.count())) {
    return To{from.count()};
  }

  // the basic idea is that we need to convert from count() in the from type
  // to count() in the To type, by multiplying it with this:
  struct Factor
      : std::ratio_divide<typename From::period, typename To::period> {};

  static_assert(Factor::num > 0, "num must be positive");
  static_assert(Factor::den > 0, "den must be positive");

  // the conversion is like this: multiply from.count() with Factor::num
  // /Factor::den and convert it to To::rep, all this without
  // overflow/underflow. let's start by finding a suitable type that can hold
  // both To, From and Factor::num
  using IntermediateRep =
      typename std::common_type<typename From::rep, typename To::rep,
                                decltype(Factor::num)>::type;

  // force conversion of From::rep -> IntermediateRep to be safe,
  // even if it will never happen be narrowing in this context.
  IntermediateRep count =
      safe_float_conversion<IntermediateRep>(from.count(), ec);
  if (ec) {
    return {};
  }

  // multiply with Factor::num without overflow or underflow
  if (Factor::num != 1) {
    constexpr auto max1 = internal::max_value<IntermediateRep>() /
                          static_cast<IntermediateRep>(Factor::num);
    if (count > max1) {
      ec = 1;
      return {};
    }
    constexpr auto min1 = std::numeric_limits<IntermediateRep>::lowest() /
                          static_cast<IntermediateRep>(Factor::num);
    if (count < min1) {
      ec = 1;
      return {};
    }
    count *= static_cast<IntermediateRep>(Factor::num);
  }

  // this can't go wrong, right? den>0 is checked earlier.
  if (Factor::den != 1) {
    using common_t = typename std::common_type<IntermediateRep, intmax_t>::type;
    count /= static_cast<common_t>(Factor::den);
  }

  // convert to the to type, safely
  using ToRep = typename To::rep;

  const ToRep tocount = safe_float_conversion<ToRep>(count, ec);
  if (ec) {
    return {};
  }
  return To{tocount};
}
}  // namespace safe_duration_cast
#endif

// Prevents expansion of a preceding token as a function-style macro.
// Usage: f FMT_NOMACRO()
#define FMT_NOMACRO

namespace internal {
inline null<> localtime_r FMT_NOMACRO(...) { return null<>(); }
inline null<> localtime_s(...) { return null<>(); }
inline null<> gmtime_r(...) { return null<>(); }
inline null<> gmtime_s(...) { return null<>(); }
}  // namespace internal

// Thread-safe replacement for std::localtime
inline std::tm localtime(std::time_t time) {
  struct dispatcher {
    std::time_t time_;
    std::tm tm_;

    dispatcher(std::time_t t) : time_(t) {}

    bool run() {
      using namespace fmt::internal;
      return handle(localtime_r(&time_, &tm_));
    }

    bool handle(std::tm* tm) { return tm != nullptr; }

    bool handle(internal::null<>) {
      using namespace fmt::internal;
      return fallback(localtime_s(&tm_, &time_));
    }

    bool fallback(int res) { return res == 0; }

#if !FMT_MSC_VER
    bool fallback(internal::null<>) {
      using namespace fmt::internal;
      std::tm* tm = std::localtime(&time_);
      if (tm) tm_ = *tm;
      return tm != nullptr;
    }
#endif
  };
  dispatcher lt(time);
  // Too big time values may be unsupported.
  if (!lt.run()) FMT_THROW(format_error("time_t value out of range"));
  return lt.tm_;
}

// Thread-safe replacement for std::gmtime
inline std::tm gmtime(std::time_t time) {
  struct dispatcher {
    std::time_t time_;
    std::tm tm_;

    dispatcher(std::time_t t) : time_(t) {}

    bool run() {
      using namespace fmt::internal;
      return handle(gmtime_r(&time_, &tm_));
    }

    bool handle(std::tm* tm) { return tm != nullptr; }

    bool handle(internal::null<>) {
      using namespace fmt::internal;
      return fallback(gmtime_s(&tm_, &time_));
    }

    bool fallback(int res) { return res == 0; }

#if !FMT_MSC_VER
    bool fallback(internal::null<>) {
      std::tm* tm = std::gmtime(&time_);
      if (tm) tm_ = *tm;
      return tm != nullptr;
    }
#endif
  };
  dispatcher gt(time);
  // Too big time values may be unsupported.
  if (!gt.run()) FMT_THROW(format_error("time_t value out of range"));
  return gt.tm_;
}

namespace internal {
inline std::size_t strftime(char* str, std::size_t count, const char* format,
                            const std::tm* time) {
  return std::strftime(str, count, format, time);
}

inline std::size_t strftime(wchar_t* str, std::size_t count,
                            const wchar_t* format, const std::tm* time) {
  return std::wcsftime(str, count, format, time);
}
}  // namespace internal

template <typename Char> struct formatter<std::tm, Char> {
  template <typename ParseContext>
  auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it == ':') ++it;
    auto end = it;
    while (end != ctx.end() && *end != '}') ++end;
    tm_format.reserve(internal::to_unsigned(end - it + 1));
    tm_format.append(it, end);
    tm_format.push_back('\0');
    return end;
  }

  template <typename FormatContext>
  auto format(const std::tm& tm, FormatContext& ctx) -> decltype(ctx.out()) {
    basic_memory_buffer<Char> buf;
    std::size_t start = buf.size();
    for (;;) {
      std::size_t size = buf.capacity() - start;
      std::size_t count =
          internal::strftime(&buf[start], size, &tm_format[0], &tm);
      if (count != 0) {
        buf.resize(start + count);
        break;
      }
      if (size >= tm_format.size() * 256) {
        // If the buffer is 256 times larger than the format string, assume
        // that `strftime` gives an empty result. There doesn't seem to be a
        // better way to distinguish the two cases:
        // https://github.com/fmtlib/fmt/issues/367
        break;
      }
      const std::size_t MIN_GROWTH = 10;
      buf.reserve(buf.capacity() + (size > MIN_GROWTH ? size : MIN_GROWTH));
    }
    return std::copy(buf.begin(), buf.end(), ctx.out());
  }

  basic_memory_buffer<Char> tm_format;
};

namespace internal {
template <typename Period> FMT_CONSTEXPR const char* get_units() {
  return nullptr;
}
template <> FMT_CONSTEXPR const char* get_units<std::atto>() { return "as"; }
template <> FMT_CONSTEXPR const char* get_units<std::femto>() { return "fs"; }
template <> FMT_CONSTEXPR const char* get_units<std::pico>() { return "ps"; }
template <> FMT_CONSTEXPR const char* get_units<std::nano>() { return "ns"; }
template <> FMT_CONSTEXPR const char* get_units<std::micro>() { return "µs"; }
template <> FMT_CONSTEXPR const char* get_units<std::milli>() { return "ms"; }
template <> FMT_CONSTEXPR const char* get_units<std::centi>() { return "cs"; }
template <> FMT_CONSTEXPR const char* get_units<std::deci>() { return "ds"; }
template <> FMT_CONSTEXPR const char* get_units<std::ratio<1>>() { return "s"; }
template <> FMT_CONSTEXPR const char* get_units<std::deca>() { return "das"; }
template <> FMT_CONSTEXPR const char* get_units<std::hecto>() { return "hs"; }
template <> FMT_CONSTEXPR const char* get_units<std::kilo>() { return "ks"; }
template <> FMT_CONSTEXPR const char* get_units<std::mega>() { return "Ms"; }
template <> FMT_CONSTEXPR const char* get_units<std::giga>() { return "Gs"; }
template <> FMT_CONSTEXPR const char* get_units<std::tera>() { return "Ts"; }
template <> FMT_CONSTEXPR const char* get_units<std::peta>() { return "Ps"; }
template <> FMT_CONSTEXPR const char* get_units<std::exa>() { return "Es"; }
template <> FMT_CONSTEXPR const char* get_units<std::ratio<60>>() {
  return "m";
}
template <> FMT_CONSTEXPR const char* get_units<std::ratio<3600>>() {
  return "h";
}

enum class numeric_system {
  standard,
  // Alternative numeric system, e.g. 十二 instead of 12 in ja_JP locale.
  alternative
};

// Parses a put_time-like format string and invokes handler actions.
template <typename Char, typename Handler>
FMT_CONSTEXPR const Char* parse_chrono_format(const Char* begin,
                                              const Char* end,
                                              Handler&& handler) {
  auto ptr = begin;
  while (ptr != end) {
    auto c = *ptr;
    if (c == '}') break;
    if (c != '%') {
      ++ptr;
      continue;
    }
    if (begin != ptr) handler.on_text(begin, ptr);
    ++ptr;  // consume '%'
    if (ptr == end) FMT_THROW(format_error("invalid format"));
    c = *ptr++;
    switch (c) {
    case '%':
      handler.on_text(ptr - 1, ptr);
      break;
    case 'n': {
      const Char newline[] = {'\n'};
      handler.on_text(newline, newline + 1);
      break;
    }
    case 't': {
      const Char tab[] = {'\t'};
      handler.on_text(tab, tab + 1);
      break;
    }
    // Day of the week:
    case 'a':
      handler.on_abbr_weekday();
      break;
    case 'A':
      handler.on_full_weekday();
      break;
    case 'w':
      handler.on_dec0_weekday(numeric_system::standard);
      break;
    case 'u':
      handler.on_dec1_weekday(numeric_system::standard);
      break;
    // Month:
    case 'b':
      handler.on_abbr_month();
      break;
    case 'B':
      handler.on_full_month();
      break;
    // Hour, minute, second:
    case 'H':
      handler.on_24_hour(numeric_system::standard);
      break;
    case 'I':
      handler.on_12_hour(numeric_system::standard);
      break;
    case 'M':
      handler.on_minute(numeric_system::standard);
      break;
    case 'S':
      handler.on_second(numeric_system::standard);
      break;
    // Other:
    case 'c':
      handler.on_datetime(numeric_system::standard);
      break;
    case 'x':
      handler.on_loc_date(numeric_system::standard);
      break;
    case 'X':
      handler.on_loc_time(numeric_system::standard);
      break;
    case 'D':
      handler.on_us_date();
      break;
    case 'F':
      handler.on_iso_date();
      break;
    case 'r':
      handler.on_12_hour_time();
      break;
    case 'R':
      handler.on_24_hour_time();
      break;
    case 'T':
      handler.on_iso_time();
      break;
    case 'p':
      handler.on_am_pm();
      break;
    case 'Q':
      handler.on_duration_value();
      break;
    case 'q':
      handler.on_duration_unit();
      break;
    case 'z':
      handler.on_utc_offset();
      break;
    case 'Z':
      handler.on_tz_name();
      break;
    // Alternative representation:
    case 'E': {
      if (ptr == end) FMT_THROW(format_error("invalid format"));
      c = *ptr++;
      switch (c) {
      case 'c':
        handler.on_datetime(numeric_system::alternative);
        break;
      case 'x':
        handler.on_loc_date(numeric_system::alternative);
        break;
      case 'X':
        handler.on_loc_time(numeric_system::alternative);
        break;
      default:
        FMT_THROW(format_error("invalid format"));
      }
      break;
    }
    case 'O':
      if (ptr == end) FMT_THROW(format_error("invalid format"));
      c = *ptr++;
      switch (c) {
      case 'w':
        handler.on_dec0_weekday(numeric_system::alternative);
        break;
      case 'u':
        handler.on_dec1_weekday(numeric_system::alternative);
        break;
      case 'H':
        handler.on_24_hour(numeric_system::alternative);
        break;
      case 'I':
        handler.on_12_hour(numeric_system::alternative);
        break;
      case 'M':
        handler.on_minute(numeric_system::alternative);
        break;
      case 'S':
        handler.on_second(numeric_system::alternative);
        break;
      default:
        FMT_THROW(format_error("invalid format"));
      }
      break;
    default:
      FMT_THROW(format_error("invalid format"));
    }
    begin = ptr;
  }
  if (begin != ptr) handler.on_text(begin, ptr);
  return ptr;
}

struct chrono_format_checker {
  FMT_NORETURN void report_no_date() { FMT_THROW(format_error("no date")); }

  template <typename Char> void on_text(const Char*, const Char*) {}
  FMT_NORETURN void on_abbr_weekday() { report_no_date(); }
  FMT_NORETURN void on_full_weekday() { report_no_date(); }
  FMT_NORETURN void on_dec0_weekday(numeric_system) { report_no_date(); }
  FMT_NORETURN void on_dec1_weekday(numeric_system) { report_no_date(); }
  FMT_NORETURN void on_abbr_month() { report_no_date(); }
  FMT_NORETURN void on_full_month() { report_no_date(); }
  void on_24_hour(numeric_system) {}
  void on_12_hour(numeric_system) {}
  void on_minute(numeric_system) {}
  void on_second(numeric_system) {}
  FMT_NORETURN void on_datetime(numeric_system) { report_no_date(); }
  FMT_NORETURN void on_loc_date(numeric_system) { report_no_date(); }
  FMT_NORETURN void on_loc_time(numeric_system) { report_no_date(); }
  FMT_NORETURN void on_us_date() { report_no_date(); }
  FMT_NORETURN void on_iso_date() { report_no_date(); }
  void on_12_hour_time() {}
  void on_24_hour_time() {}
  void on_iso_time() {}
  void on_am_pm() {}
  void on_duration_value() {}
  void on_duration_unit() {}
  FMT_NORETURN void on_utc_offset() { report_no_date(); }
  FMT_NORETURN void on_tz_name() { report_no_date(); }
};

template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
inline bool isnan(T) {
  return false;
}
template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value)>
inline bool isnan(T value) {
  return std::isnan(value);
}

template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
inline bool isfinite(T) {
  return true;
}
template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value)>
inline bool isfinite(T value) {
  return std::isfinite(value);
}

// Converts value to int and checks that it's in the range [0, upper).
template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
inline int to_nonnegative_int(T value, int upper) {
  FMT_ASSERT(value >= 0 && value <= upper, "invalid value");
  (void)upper;
  return static_cast<int>(value);
}
template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
inline int to_nonnegative_int(T value, int upper) {
  FMT_ASSERT(
      std::isnan(value) || (value >= 0 && value <= static_cast<T>(upper)),
      "invalid value");
  (void)upper;
  return static_cast<int>(value);
}

template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
inline T mod(T x, int y) {
  return x % static_cast<T>(y);
}
template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value)>
inline T mod(T x, int y) {
  return std::fmod(x, static_cast<T>(y));
}

// If T is an integral type, maps T to its unsigned counterpart, otherwise
// leaves it unchanged (unlike std::make_unsigned).
template <typename T, bool INTEGRAL = std::is_integral<T>::value>
struct make_unsigned_or_unchanged {
  using type = T;
};

template <typename T> struct make_unsigned_or_unchanged<T, true> {
  using type = typename std::make_unsigned<T>::type;
};

#if FMT_SAFE_DURATION_CAST
// throwing version of safe_duration_cast
template <typename To, typename FromRep, typename FromPeriod>
To fmt_safe_duration_cast(std::chrono::duration<FromRep, FromPeriod> from) {
  int ec;
  To to = safe_duration_cast::safe_duration_cast<To>(from, ec);
  if (ec) FMT_THROW(format_error("cannot format duration"));
  return to;
}
#endif

template <typename Rep, typename Period,
          FMT_ENABLE_IF(std::is_integral<Rep>::value)>
inline std::chrono::duration<Rep, std::milli> get_milliseconds(
    std::chrono::duration<Rep, Period> d) {
  // this may overflow and/or the result may not fit in the
  // target type.
#if FMT_SAFE_DURATION_CAST
  using CommonSecondsType =
      typename std::common_type<decltype(d), std::chrono::seconds>::type;
  const auto d_as_common = fmt_safe_duration_cast<CommonSecondsType>(d);
  const auto d_as_whole_seconds =
      fmt_safe_duration_cast<std::chrono::seconds>(d_as_common);
  // this conversion should be nonproblematic
  const auto diff = d_as_common - d_as_whole_seconds;
  const auto ms =
      fmt_safe_duration_cast<std::chrono::duration<Rep, std::milli>>(diff);
  return ms;
#else
  auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
  return std::chrono::duration_cast<std::chrono::milliseconds>(d - s);
#endif
}

template <typename Rep, typename Period,
          FMT_ENABLE_IF(std::is_floating_point<Rep>::value)>
inline std::chrono::duration<Rep, std::milli> get_milliseconds(
    std::chrono::duration<Rep, Period> d) {
  using common_type = typename std::common_type<Rep, std::intmax_t>::type;
  auto ms = mod(d.count() * static_cast<common_type>(Period::num) /
                    static_cast<common_type>(Period::den) * 1000,
                1000);
  return std::chrono::duration<Rep, std::milli>(static_cast<Rep>(ms));
}

template <typename Char, typename Rep, typename OutputIt>
OutputIt format_duration_value(OutputIt out, Rep val, int precision) {
  const Char pr_f[] = {'{', ':', '.', '{', '}', 'f', '}', 0};
  if (precision >= 0) return format_to(out, pr_f, val, precision);
  const Char fp_f[] = {'{', ':', 'g', '}', 0};
  const Char format[] = {'{', '}', 0};
  return format_to(out, std::is_floating_point<Rep>::value ? fp_f : format,
                   val);
}

template <typename Char, typename Period, typename OutputIt>
OutputIt format_duration_unit(OutputIt out) {
  if (const char* unit = get_units<Period>()) {
    string_view s(unit);
    if (const_check(std::is_same<Char, wchar_t>())) {
      utf8_to_utf16 u(s);
      return std::copy(u.c_str(), u.c_str() + u.size(), out);
    }
    return std::copy(s.begin(), s.end(), out);
  }
  const Char num_f[] = {'[', '{', '}', ']', 's', 0};
  if (Period::den == 1) return format_to(out, num_f, Period::num);
  const Char num_def_f[] = {'[', '{', '}', '/', '{', '}', ']', 's', 0};
  return format_to(out, num_def_f, Period::num, Period::den);
}

template <typename FormatContext, typename OutputIt, typename Rep,
          typename Period>
struct chrono_formatter {
  FormatContext& context;
  OutputIt out;
  int precision;
  // rep is unsigned to avoid overflow.
  using rep =
      conditional_t<std::is_integral<Rep>::value && sizeof(Rep) < sizeof(int),
                    unsigned, typename make_unsigned_or_unchanged<Rep>::type>;
  rep val;
  using seconds = std::chrono::duration<rep>;
  seconds s;
  using milliseconds = std::chrono::duration<rep, std::milli>;
  bool negative;

  using char_type = typename FormatContext::char_type;

  explicit chrono_formatter(FormatContext& ctx, OutputIt o,
                            std::chrono::duration<Rep, Period> d)
      : context(ctx),
        out(o),
        val(static_cast<rep>(d.count())),
        negative(false) {
    if (d.count() < 0) {
      val = 0 - val;
      negative = true;
    }

    // this may overflow and/or the result may not fit in the
    // target type.
#if FMT_SAFE_DURATION_CAST
    // might need checked conversion (rep!=Rep)
    auto tmpval = std::chrono::duration<rep, Period>(val);
    s = fmt_safe_duration_cast<seconds>(tmpval);
#else
    s = std::chrono::duration_cast<seconds>(
        std::chrono::duration<rep, Period>(val));
#endif
  }

  // returns true if nan or inf, writes to out.
  bool handle_nan_inf() {
    if (isfinite(val)) {
      return false;
    }
    if (isnan(val)) {
      write_nan();
      return true;
    }
    // must be +-inf
    if (val > 0) {
      write_pinf();
    } else {
      write_ninf();
    }
    return true;
  }

  Rep hour() const { return static_cast<Rep>(mod((s.count() / 3600), 24)); }

  Rep hour12() const {
    Rep hour = static_cast<Rep>(mod((s.count() / 3600), 12));
    return hour <= 0 ? 12 : hour;
  }

  Rep minute() const { return static_cast<Rep>(mod((s.count() / 60), 60)); }
  Rep second() const { return static_cast<Rep>(mod(s.count(), 60)); }

  std::tm time() const {
    auto time = std::tm();
    time.tm_hour = to_nonnegative_int(hour(), 24);
    time.tm_min = to_nonnegative_int(minute(), 60);
    time.tm_sec = to_nonnegative_int(second(), 60);
    return time;
  }

  void write_sign() {
    if (negative) {
      *out++ = '-';
      negative = false;
    }
  }

  void write(Rep value, int width) {
    write_sign();
    if (isnan(value)) return write_nan();
    uint32_or_64_or_128_t<int> n =
        to_unsigned(to_nonnegative_int(value, max_value<int>()));
    int num_digits = internal::count_digits(n);
    if (width > num_digits) out = std::fill_n(out, width - num_digits, '0');
    out = format_decimal<char_type>(out, n, num_digits);
  }

  void write_nan() { std::copy_n("nan", 3, out); }
  void write_pinf() { std::copy_n("inf", 3, out); }
  void write_ninf() { std::copy_n("-inf", 4, out); }

  void format_localized(const tm& time, char format, char modifier = 0) {
    if (isnan(val)) return write_nan();
    auto locale = context.locale().template get<std::locale>();
    auto& facet = std::use_facet<std::time_put<char_type>>(locale);
    std::basic_ostringstream<char_type> os;
    os.imbue(locale);
    facet.put(os, os, ' ', &time, format, modifier);
    auto str = os.str();
    std::copy(str.begin(), str.end(), out);
  }

  void on_text(const char_type* begin, const char_type* end) {
    std::copy(begin, end, out);
  }

  // These are not implemented because durations don't have date information.
  void on_abbr_weekday() {}
  void on_full_weekday() {}
  void on_dec0_weekday(numeric_system) {}
  void on_dec1_weekday(numeric_system) {}
  void on_abbr_month() {}
  void on_full_month() {}
  void on_datetime(numeric_system) {}
  void on_loc_date(numeric_system) {}
  void on_loc_time(numeric_system) {}
  void on_us_date() {}
  void on_iso_date() {}
  void on_utc_offset() {}
  void on_tz_name() {}

  void on_24_hour(numeric_system ns) {
    if (handle_nan_inf()) return;

    if (ns == numeric_system::standard) return write(hour(), 2);
    auto time = tm();
    time.tm_hour = to_nonnegative_int(hour(), 24);
    format_localized(time, 'H', 'O');
  }

  void on_12_hour(numeric_system ns) {
    if (handle_nan_inf()) return;

    if (ns == numeric_system::standard) return write(hour12(), 2);
    auto time = tm();
    time.tm_hour = to_nonnegative_int(hour12(), 12);
    format_localized(time, 'I', 'O');
  }

  void on_minute(numeric_system ns) {
    if (handle_nan_inf()) return;

    if (ns == numeric_system::standard) return write(minute(), 2);
    auto time = tm();
    time.tm_min = to_nonnegative_int(minute(), 60);
    format_localized(time, 'M', 'O');
  }

  void on_second(numeric_system ns) {
    if (handle_nan_inf()) return;

    if (ns == numeric_system::standard) {
      write(second(), 2);
#if FMT_SAFE_DURATION_CAST
      // convert rep->Rep
      using duration_rep = std::chrono::duration<rep, Period>;
      using duration_Rep = std::chrono::duration<Rep, Period>;
      auto tmpval = fmt_safe_duration_cast<duration_Rep>(duration_rep{val});
#else
      auto tmpval = std::chrono::duration<Rep, Period>(val);
#endif
      auto ms = get_milliseconds(tmpval);
      if (ms != std::chrono::milliseconds(0)) {
        *out++ = '.';
        write(ms.count(), 3);
      }
      return;
    }
    auto time = tm();
    time.tm_sec = to_nonnegative_int(second(), 60);
    format_localized(time, 'S', 'O');
  }

  void on_12_hour_time() {
    if (handle_nan_inf()) return;
    format_localized(time(), 'r');
  }

  void on_24_hour_time() {
    if (handle_nan_inf()) {
      *out++ = ':';
      handle_nan_inf();
      return;
    }

    write(hour(), 2);
    *out++ = ':';
    write(minute(), 2);
  }

  void on_iso_time() {
    on_24_hour_time();
    *out++ = ':';
    if (handle_nan_inf()) return;
    write(second(), 2);
  }

  void on_am_pm() {
    if (handle_nan_inf()) return;
    format_localized(time(), 'p');
  }

  void on_duration_value() {
    if (handle_nan_inf()) return;
    write_sign();
    out = format_duration_value<char_type>(out, val, precision);
  }

  void on_duration_unit() {
    out = format_duration_unit<char_type, Period>(out);
  }
};
}  // namespace internal

template <typename Rep, typename Period, typename Char>
struct formatter<std::chrono::duration<Rep, Period>, Char> {
 private:
  basic_format_specs<Char> specs;
  int precision;
  using arg_ref_type = internal::arg_ref<Char>;
  arg_ref_type width_ref;
  arg_ref_type precision_ref;
  mutable basic_string_view<Char> format_str;
  using duration = std::chrono::duration<Rep, Period>;

  struct spec_handler {
    formatter& f;
    basic_format_parse_context<Char>& context;
    basic_string_view<Char> format_str;

    template <typename Id> FMT_CONSTEXPR arg_ref_type make_arg_ref(Id arg_id) {
      context.check_arg_id(arg_id);
      return arg_ref_type(arg_id);
    }

    FMT_CONSTEXPR arg_ref_type make_arg_ref(basic_string_view<Char> arg_id) {
      context.check_arg_id(arg_id);
      return arg_ref_type(arg_id);
    }

    FMT_CONSTEXPR arg_ref_type make_arg_ref(internal::auto_id) {
      return arg_ref_type(context.next_arg_id());
    }

    void on_error(const char* msg) { FMT_THROW(format_error(msg)); }
    void on_fill(basic_string_view<Char> fill) { f.specs.fill = fill; }
    void on_align(align_t align) { f.specs.align = align; }
    void on_width(int width) { f.specs.width = width; }
    void on_precision(int _precision) { f.precision = _precision; }
    void end_precision() {}

    template <typename Id> void on_dynamic_width(Id arg_id) {
      f.width_ref = make_arg_ref(arg_id);
    }

    template <typename Id> void on_dynamic_precision(Id arg_id) {
      f.precision_ref = make_arg_ref(arg_id);
    }
  };

  using iterator = typename basic_format_parse_context<Char>::iterator;
  struct parse_range {
    iterator begin;
    iterator end;
  };

  FMT_CONSTEXPR parse_range do_parse(basic_format_parse_context<Char>& ctx) {
    auto begin = ctx.begin(), end = ctx.end();
    if (begin == end || *begin == '}') return {begin, begin};
    spec_handler handler{*this, ctx, format_str};
    begin = internal::parse_align(begin, end, handler);
    if (begin == end) return {begin, begin};
    begin = internal::parse_width(begin, end, handler);
    if (begin == end) return {begin, begin};
    if (*begin == '.') {
      if (std::is_floating_point<Rep>::value)
        begin = internal::parse_precision(begin, end, handler);
      else
        handler.on_error("precision not allowed for this argument type");
    }
    end = parse_chrono_format(begin, end, internal::chrono_format_checker());
    return {begin, end};
  }

 public:
  formatter() : precision(-1) {}

  FMT_CONSTEXPR auto parse(basic_format_parse_context<Char>& ctx)
      -> decltype(ctx.begin()) {
    auto range = do_parse(ctx);
    format_str = basic_string_view<Char>(
        &*range.begin, internal::to_unsigned(range.end - range.begin));
    return range.end;
  }

  template <typename FormatContext>
  auto format(const duration& d, FormatContext& ctx) -> decltype(ctx.out()) {
    auto begin = format_str.begin(), end = format_str.end();
    // As a possible future optimization, we could avoid extra copying if width
    // is not specified.
    basic_memory_buffer<Char> buf;
    auto out = std::back_inserter(buf);
    using range = internal::output_range<decltype(ctx.out()), Char>;
    internal::basic_writer<range> w(range(ctx.out()));
    internal::handle_dynamic_spec<internal::width_checker>(specs.width,
                                                           width_ref, ctx);
    internal::handle_dynamic_spec<internal::precision_checker>(
        precision, precision_ref, ctx);
    if (begin == end || *begin == '}') {
      out = internal::format_duration_value<Char>(out, d.count(), precision);
      internal::format_duration_unit<Char, Period>(out);
    } else {
      internal::chrono_formatter<FormatContext, decltype(out), Rep, Period> f(
          ctx, out, d);
      f.precision = precision;
      parse_chrono_format(begin, end, f);
    }
    w.write(buf.data(), buf.size(), specs);
    return w.out();
  }
};

FMT_END_NAMESPACE

#endif  // FMT_CHRONO_H_
