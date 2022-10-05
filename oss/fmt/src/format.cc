// Formatting library for C++
//
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#include "fmt/format-inl.h"

FMT_BEGIN_NAMESPACE
namespace detail {

template FMT_API auto dragonbox::to_decimal(float x) noexcept
    -> dragonbox::decimal_fp<float>;
template FMT_API auto dragonbox::to_decimal(double x) noexcept
    -> dragonbox::decimal_fp<double>;

#ifndef FMT_STATIC_THOUSANDS_SEPARATOR
template FMT_API locale_ref::locale_ref(const std::locale& loc);
template FMT_API auto locale_ref::get<std::locale>() const -> std::locale;
#endif

// Explicit instantiations for char.

template FMT_API auto thousands_sep_impl(locale_ref)
    -> thousands_sep_result<char>;
template FMT_API auto decimal_point_impl(locale_ref) -> char;

template FMT_API void buffer<char>::append(const char*, const char*);

// DEPRECATED!
// There is no correspondent extern template in format.h because of
// incompatibility between clang and gcc (#2377).
template FMT_API void vformat_to(buffer<char>&, string_view,
                                 basic_format_args<FMT_BUFFER_CONTEXT(char)>,
                                 locale_ref);

// Explicit instantiations for wchar_t.

template FMT_API auto thousands_sep_impl(locale_ref)
    -> thousands_sep_result<wchar_t>;
template FMT_API auto decimal_point_impl(locale_ref) -> wchar_t;

template FMT_API void buffer<wchar_t>::append(const wchar_t*, const wchar_t*);

}  // namespace detail
FMT_END_NAMESPACE
