// Formatting library for C++ - optional wchar_t and exotic character support
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_XCHAR_H_
#define FMT_XCHAR_H_

#include <cwchar>

#include "format.h"

FMT_BEGIN_NAMESPACE
namespace detail {
template <typename T>
using is_exotic_char = bool_constant<!std::is_same<T, char>::value>;
}

FMT_MODULE_EXPORT_BEGIN

using wstring_view = basic_string_view<wchar_t>;
using wformat_parse_context = basic_format_parse_context<wchar_t>;
using wformat_context = buffer_context<wchar_t>;
using wformat_args = basic_format_args<wformat_context>;
using wmemory_buffer = basic_memory_buffer<wchar_t>;

#if FMT_GCC_VERSION && FMT_GCC_VERSION < 409
// Workaround broken conversion on older gcc.
template <typename... Args> using wformat_string = wstring_view;
inline auto runtime(wstring_view s) -> wstring_view { return s; }
#else
template <typename... Args>
using wformat_string = basic_format_string<wchar_t, type_identity_t<Args>...>;
inline auto runtime(wstring_view s) -> basic_runtime<wchar_t> { return {{s}}; }
#endif

template <> struct is_char<wchar_t> : std::true_type {};
template <> struct is_char<detail::char8_type> : std::true_type {};
template <> struct is_char<char16_t> : std::true_type {};
template <> struct is_char<char32_t> : std::true_type {};

template <typename... Args>
constexpr format_arg_store<wformat_context, Args...> make_wformat_args(
    const Args&... args) {
  return {args...};
}

inline namespace literals {
#if FMT_USE_USER_DEFINED_LITERALS && !FMT_USE_NONTYPE_TEMPLATE_ARGS
constexpr detail::udl_arg<wchar_t> operator"" _a(const wchar_t* s, size_t) {
  return {s};
}
#endif
}  // namespace literals

template <typename It, typename Sentinel>
auto join(It begin, Sentinel end, wstring_view sep)
    -> join_view<It, Sentinel, wchar_t> {
  return {begin, end, sep};
}

template <typename Range>
auto join(Range&& range, wstring_view sep)
    -> join_view<detail::iterator_t<Range>, detail::sentinel_t<Range>,
                 wchar_t> {
  return join(std::begin(range), std::end(range), sep);
}

template <typename T>
auto join(std::initializer_list<T> list, wstring_view sep)
    -> join_view<const T*, const T*, wchar_t> {
  return join(std::begin(list), std::end(list), sep);
}

template <typename Char, FMT_ENABLE_IF(!std::is_same<Char, char>::value)>
auto vformat(basic_string_view<Char> format_str,
             basic_format_args<buffer_context<type_identity_t<Char>>> args)
    -> std::basic_string<Char> {
  basic_memory_buffer<Char> buffer;
  detail::vformat_to(buffer, format_str, args);
  return to_string(buffer);
}

template <typename... T>
auto format(wformat_string<T...> fmt, T&&... args) -> std::wstring {
  return vformat(fmt::wstring_view(fmt), fmt::make_wformat_args(args...));
}

// Pass char_t as a default template parameter instead of using
// std::basic_string<char_t<S>> to reduce the symbol size.
template <typename S, typename... Args, typename Char = char_t<S>,
          FMT_ENABLE_IF(!std::is_same<Char, char>::value &&
                        !std::is_same<Char, wchar_t>::value)>
auto format(const S& format_str, Args&&... args) -> std::basic_string<Char> {
  return vformat(detail::to_string_view(format_str),
                 fmt::make_format_args<buffer_context<Char>>(args...));
}

template <typename Locale, typename S, typename Char = char_t<S>,
          FMT_ENABLE_IF(detail::is_locale<Locale>::value&&
                            detail::is_exotic_char<Char>::value)>
inline auto vformat(
    const Locale& loc, const S& format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args)
    -> std::basic_string<Char> {
  return detail::vformat(loc, detail::to_string_view(format_str), args);
}

template <typename Locale, typename S, typename... Args,
          typename Char = char_t<S>,
          FMT_ENABLE_IF(detail::is_locale<Locale>::value&&
                            detail::is_exotic_char<Char>::value)>
inline auto format(const Locale& loc, const S& format_str, Args&&... args)
    -> std::basic_string<Char> {
  return detail::vformat(loc, detail::to_string_view(format_str),
                         fmt::make_format_args<buffer_context<Char>>(args...));
}

template <typename OutputIt, typename S, typename Char = char_t<S>,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value&&
                            detail::is_exotic_char<Char>::value)>
auto vformat_to(OutputIt out, const S& format_str,
                basic_format_args<buffer_context<type_identity_t<Char>>> args)
    -> OutputIt {
  auto&& buf = detail::get_buffer<Char>(out);
  detail::vformat_to(buf, detail::to_string_view(format_str), args);
  return detail::get_iterator(buf);
}

template <typename OutputIt, typename S, typename... Args,
          typename Char = char_t<S>,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value&&
                            detail::is_exotic_char<Char>::value)>
inline auto format_to(OutputIt out, const S& fmt, Args&&... args) -> OutputIt {
  return vformat_to(out, detail::to_string_view(fmt),
                    fmt::make_format_args<buffer_context<Char>>(args...));
}

template <typename Locale, typename S, typename OutputIt, typename... Args,
          typename Char = char_t<S>,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value&&
                            detail::is_locale<Locale>::value&&
                                detail::is_exotic_char<Char>::value)>
inline auto vformat_to(
    OutputIt out, const Locale& loc, const S& format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args) -> OutputIt {
  auto&& buf = detail::get_buffer<Char>(out);
  vformat_to(buf, detail::to_string_view(format_str), args,
             detail::locale_ref(loc));
  return detail::get_iterator(buf);
}

template <
    typename OutputIt, typename Locale, typename S, typename... Args,
    typename Char = char_t<S>,
    bool enable = detail::is_output_iterator<OutputIt, Char>::value&&
        detail::is_locale<Locale>::value&& detail::is_exotic_char<Char>::value>
inline auto format_to(OutputIt out, const Locale& loc, const S& format_str,
                      Args&&... args) ->
    typename std::enable_if<enable, OutputIt>::type {
  return vformat_to(out, loc, to_string_view(format_str),
                    fmt::make_format_args<buffer_context<Char>>(args...));
}

template <typename OutputIt, typename Char, typename... Args,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value&&
                            detail::is_exotic_char<Char>::value)>
inline auto vformat_to_n(
    OutputIt out, size_t n, basic_string_view<Char> format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args)
    -> format_to_n_result<OutputIt> {
  detail::iterator_buffer<OutputIt, Char, detail::fixed_buffer_traits> buf(out,
                                                                           n);
  detail::vformat_to(buf, format_str, args);
  return {buf.out(), buf.count()};
}

template <typename OutputIt, typename S, typename... Args,
          typename Char = char_t<S>,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value&&
                            detail::is_exotic_char<Char>::value)>
inline auto format_to_n(OutputIt out, size_t n, const S& fmt,
                        const Args&... args) -> format_to_n_result<OutputIt> {
  return vformat_to_n(out, n, detail::to_string_view(fmt),
                      fmt::make_format_args<buffer_context<Char>>(args...));
}

template <typename S, typename... Args, typename Char = char_t<S>,
          FMT_ENABLE_IF(detail::is_exotic_char<Char>::value)>
inline auto formatted_size(const S& fmt, Args&&... args) -> size_t {
  detail::counting_buffer<Char> buf;
  detail::vformat_to(buf, detail::to_string_view(fmt),
                     fmt::make_format_args<buffer_context<Char>>(args...));
  return buf.count();
}

inline void vprint(std::FILE* f, wstring_view fmt, wformat_args args) {
  wmemory_buffer buffer;
  detail::vformat_to(buffer, fmt, args);
  buffer.push_back(L'\0');
  if (std::fputws(buffer.data(), f) == -1)
    FMT_THROW(system_error(errno, FMT_STRING("cannot write to file")));
}

inline void vprint(wstring_view fmt, wformat_args args) {
  vprint(stdout, fmt, args);
}

template <typename... T>
void print(std::FILE* f, wformat_string<T...> fmt, T&&... args) {
  return vprint(f, wstring_view(fmt), fmt::make_wformat_args(args...));
}

template <typename... T> void print(wformat_string<T...> fmt, T&&... args) {
  return vprint(wstring_view(fmt), fmt::make_wformat_args(args...));
}

/**
  Converts *value* to ``std::wstring`` using the default format for type *T*.
 */
template <typename T> inline auto to_wstring(const T& value) -> std::wstring {
  return format(FMT_STRING(L"{}"), value);
}
FMT_MODULE_EXPORT_END
FMT_END_NAMESPACE

#endif  // FMT_XCHAR_H_
