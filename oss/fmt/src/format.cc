// Formatting library for C++
//
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#include "fmt/format-inl.h"

FMT_BEGIN_NAMESPACE
namespace detail {

template <typename T>
int format_float(char* buf, std::size_t size, const char* format, int precision,
                 T value) {
#ifdef FMT_FUZZ
  if (precision > 100000)
    throw std::runtime_error(
        "fuzz mode - avoid large allocation inside snprintf");
#endif
  // Suppress the warning about nonliteral format string.
  int (*snprintf_ptr)(char*, size_t, const char*, ...) = FMT_SNPRINTF;
  return precision < 0 ? snprintf_ptr(buf, size, format, value)
                       : snprintf_ptr(buf, size, format, precision, value);
}

template FMT_API dragonbox::decimal_fp<float> dragonbox::to_decimal(float x)
    FMT_NOEXCEPT;
template FMT_API dragonbox::decimal_fp<double> dragonbox::to_decimal(double x)
    FMT_NOEXCEPT;

// DEPRECATED! This function exists for ABI compatibility.
template <typename Char>
typename basic_format_context<std::back_insert_iterator<buffer<Char>>,
                              Char>::iterator
vformat_to(buffer<Char>& buf, basic_string_view<Char> format_str,
           basic_format_args<basic_format_context<
               std::back_insert_iterator<buffer<type_identity_t<Char>>>,
               type_identity_t<Char>>>
               args) {
  using iterator = std::back_insert_iterator<buffer<char>>;
  using context = basic_format_context<
      std::back_insert_iterator<buffer<type_identity_t<Char>>>,
      type_identity_t<Char>>;
  auto out = iterator(buf);
  format_handler<iterator, Char, context> h(out, format_str, args, {});
  parse_format_string<false>(format_str, h);
  return out;
}
template basic_format_context<std::back_insert_iterator<buffer<char>>,
                              char>::iterator
vformat_to(buffer<char>&, string_view,
           basic_format_args<basic_format_context<
               std::back_insert_iterator<buffer<type_identity_t<char>>>,
               type_identity_t<char>>>);
}  // namespace detail

template struct FMT_INSTANTIATION_DEF_API detail::basic_data<void>;

// Workaround a bug in MSVC2013 that prevents instantiation of format_float.
int (*instantiate_format_float)(double, int, detail::float_specs,
                                detail::buffer<char>&) = detail::format_float;

#ifndef FMT_STATIC_THOUSANDS_SEPARATOR
template FMT_API detail::locale_ref::locale_ref(const std::locale& loc);
template FMT_API std::locale detail::locale_ref::get<std::locale>() const;
#endif

// Explicit instantiations for char.

template FMT_API std::string detail::grouping_impl<char>(locale_ref);
template FMT_API char detail::thousands_sep_impl(locale_ref);
template FMT_API char detail::decimal_point_impl(locale_ref);

template FMT_API void detail::buffer<char>::append(const char*, const char*);

template FMT_API void detail::vformat_to(
    detail::buffer<char>&, string_view,
    basic_format_args<FMT_BUFFER_CONTEXT(char)>, detail::locale_ref);

template FMT_API int detail::snprintf_float(double, int, detail::float_specs,
                                            detail::buffer<char>&);
template FMT_API int detail::snprintf_float(long double, int,
                                            detail::float_specs,
                                            detail::buffer<char>&);
template FMT_API int detail::format_float(double, int, detail::float_specs,
                                          detail::buffer<char>&);
template FMT_API int detail::format_float(long double, int, detail::float_specs,
                                          detail::buffer<char>&);

// Explicit instantiations for wchar_t.

template FMT_API std::string detail::grouping_impl<wchar_t>(locale_ref);
template FMT_API wchar_t detail::thousands_sep_impl(locale_ref);
template FMT_API wchar_t detail::decimal_point_impl(locale_ref);

template FMT_API void detail::buffer<wchar_t>::append(const wchar_t*,
                                                      const wchar_t*);
FMT_END_NAMESPACE
