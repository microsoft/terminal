// Formatting library for C++ - std::locale support
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_LOCALE_H_
#define FMT_LOCALE_H_

#include <locale>

#include "format.h"

FMT_BEGIN_NAMESPACE

namespace detail {
template <typename Char>
std::basic_string<Char> vformat(
    const std::locale& loc, basic_string_view<Char> format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args) {
  basic_memory_buffer<Char> buffer;
  detail::vformat_to(buffer, format_str, args, detail::locale_ref(loc));
  return fmt::to_string(buffer);
}
}  // namespace detail

template <typename S, typename Char = char_t<S>>
inline std::basic_string<Char> vformat(
    const std::locale& loc, const S& format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args) {
  return detail::vformat(loc, to_string_view(format_str), args);
}

template <typename S, typename... Args, typename Char = char_t<S>>
inline std::basic_string<Char> format(const std::locale& loc,
                                      const S& format_str, Args&&... args) {
  return detail::vformat(loc, to_string_view(format_str),
                         fmt::make_args_checked<Args...>(format_str, args...));
}

template <typename S, typename OutputIt, typename... Args,
          typename Char = char_t<S>,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value)>
inline OutputIt vformat_to(
    OutputIt out, const std::locale& loc, const S& format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args) {
  decltype(detail::get_buffer<Char>(out)) buf(detail::get_buffer_init(out));
  vformat_to(buf, to_string_view(format_str), args, detail::locale_ref(loc));
  return detail::get_iterator(buf);
}

template <typename OutputIt, typename S, typename... Args,
          bool enable = detail::is_output_iterator<OutputIt, char_t<S>>::value>
inline auto format_to(OutputIt out, const std::locale& loc,
                      const S& format_str, Args&&... args) ->
    typename std::enable_if<enable, OutputIt>::type {
  const auto& vargs = fmt::make_args_checked<Args...>(format_str, args...);
  return vformat_to(out, loc, to_string_view(format_str), vargs);
}

FMT_END_NAMESPACE

#endif  // FMT_LOCALE_H_
