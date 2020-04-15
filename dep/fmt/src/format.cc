// Formatting library for C++
//
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#include "fmt/format-inl.h"

FMT_BEGIN_NAMESPACE
namespace internal {

template <typename T>
int format_float(char* buf, std::size_t size, const char* format, int precision,
                 T value) {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  if (precision > 100000)
    throw std::runtime_error(
        "fuzz mode - avoid large allocation inside snprintf");
#endif
  // Suppress the warning about nonliteral format string.
  int (*snprintf_ptr)(char*, size_t, const char*, ...) = FMT_SNPRINTF;
  return precision < 0 ? snprintf_ptr(buf, size, format, value)
                       : snprintf_ptr(buf, size, format, precision, value);
}
struct sprintf_specs {
  int precision;
  char type;
  bool alt : 1;

  template <typename Char>
  constexpr sprintf_specs(basic_format_specs<Char> specs)
      : precision(specs.precision), type(specs.type), alt(specs.alt) {}

  constexpr bool has_precision() const { return precision >= 0; }
};

// This is deprecated and is kept only to preserve ABI compatibility.
template <typename Double>
char* sprintf_format(Double value, internal::buffer<char>& buf,
                     sprintf_specs specs) {
  // Buffer capacity must be non-zero, otherwise MSVC's vsnprintf_s will fail.
  FMT_ASSERT(buf.capacity() != 0, "empty buffer");

  // Build format string.
  enum { max_format_size = 10 };  // longest format: %#-*.*Lg
  char format[max_format_size];
  char* format_ptr = format;
  *format_ptr++ = '%';
  if (specs.alt || !specs.type) *format_ptr++ = '#';
  if (specs.precision >= 0) {
    *format_ptr++ = '.';
    *format_ptr++ = '*';
  }
  if (std::is_same<Double, long double>::value) *format_ptr++ = 'L';

  char type = specs.type;

  if (type == '%')
    type = 'f';
  else if (type == 0 || type == 'n')
    type = 'g';
#if FMT_MSC_VER
  if (type == 'F') {
    // MSVC's printf doesn't support 'F'.
    type = 'f';
  }
#endif
  *format_ptr++ = type;
  *format_ptr = '\0';

  // Format using snprintf.
  char* start = nullptr;
  char* decimal_point_pos = nullptr;
  for (;;) {
    std::size_t buffer_size = buf.capacity();
    start = &buf[0];
    int result =
        format_float(start, buffer_size, format, specs.precision, value);
    if (result >= 0) {
      unsigned n = internal::to_unsigned(result);
      if (n < buf.capacity()) {
        // Find the decimal point.
        auto p = buf.data(), end = p + n;
        if (*p == '+' || *p == '-') ++p;
        if (specs.type != 'a' && specs.type != 'A') {
          while (p < end && *p >= '0' && *p <= '9') ++p;
          if (p < end && *p != 'e' && *p != 'E') {
            decimal_point_pos = p;
            if (!specs.type) {
              // Keep only one trailing zero after the decimal point.
              ++p;
              if (*p == '0') ++p;
              while (p != end && *p >= '1' && *p <= '9') ++p;
              char* where = p;
              while (p != end && *p == '0') ++p;
              if (p == end || *p < '0' || *p > '9') {
                if (p != end) std::memmove(where, p, to_unsigned(end - p));
                n -= static_cast<unsigned>(p - where);
              }
            }
          }
        }
        buf.resize(n);
        break;  // The buffer is large enough - continue with formatting.
      }
      buf.reserve(n + 1);
    } else {
      // If result is negative we ask to increase the capacity by at least 1,
      // but as std::vector, the buffer grows exponentially.
      buf.reserve(buf.capacity() + 1);
    }
  }
  return decimal_point_pos;
}
}  // namespace internal

template FMT_API char* internal::sprintf_format(double, internal::buffer<char>&,
                                                sprintf_specs);
template FMT_API char* internal::sprintf_format(long double,
                                                internal::buffer<char>&,
                                                sprintf_specs);

template struct FMT_INSTANTIATION_DEF_API internal::basic_data<void>;

// Workaround a bug in MSVC2013 that prevents instantiation of format_float.
int (*instantiate_format_float)(double, int, internal::float_specs,
                                internal::buffer<char>&) =
    internal::format_float;

#ifndef FMT_STATIC_THOUSANDS_SEPARATOR
template FMT_API internal::locale_ref::locale_ref(const std::locale& loc);
template FMT_API std::locale internal::locale_ref::get<std::locale>() const;
#endif

// Explicit instantiations for char.

template FMT_API std::string internal::grouping_impl<char>(locale_ref);
template FMT_API char internal::thousands_sep_impl(locale_ref);
template FMT_API char internal::decimal_point_impl(locale_ref);

template FMT_API void internal::buffer<char>::append(const char*, const char*);

template FMT_API void internal::arg_map<format_context>::init(
    const basic_format_args<format_context>& args);

template FMT_API std::string internal::vformat<char>(
    string_view, basic_format_args<format_context>);

template FMT_API format_context::iterator internal::vformat_to(
    internal::buffer<char>&, string_view, basic_format_args<format_context>);

template FMT_API int internal::snprintf_float(double, int,
                                              internal::float_specs,
                                              internal::buffer<char>&);
template FMT_API int internal::snprintf_float(long double, int,
                                              internal::float_specs,
                                              internal::buffer<char>&);
template FMT_API int internal::format_float(double, int, internal::float_specs,
                                            internal::buffer<char>&);
template FMT_API int internal::format_float(long double, int,
                                            internal::float_specs,
                                            internal::buffer<char>&);

// Explicit instantiations for wchar_t.

template FMT_API std::string internal::grouping_impl<wchar_t>(locale_ref);
template FMT_API wchar_t internal::thousands_sep_impl(locale_ref);
template FMT_API wchar_t internal::decimal_point_impl(locale_ref);

template FMT_API void internal::buffer<wchar_t>::append(const wchar_t*,
                                                        const wchar_t*);

template FMT_API std::wstring internal::vformat<wchar_t>(
    wstring_view, basic_format_args<wformat_context>);
FMT_END_NAMESPACE
