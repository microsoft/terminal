// Formatting library for C++ - formatters for standard library types
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_STD_H_
#define FMT_STD_H_

#include <atomic>
#include <bitset>
#include <cstdlib>
#include <exception>
#include <memory>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "format.h"
#include "ostream.h"

#if FMT_HAS_INCLUDE(<version>)
#  include <version>
#endif
// Checking FMT_CPLUSPLUS for warning suppression in MSVC.
#if FMT_CPLUSPLUS >= 201703L
#  if FMT_HAS_INCLUDE(<filesystem>)
#    include <filesystem>
#  endif
#  if FMT_HAS_INCLUDE(<variant>)
#    include <variant>
#  endif
#  if FMT_HAS_INCLUDE(<optional>)
#    include <optional>
#  endif
#endif

// GCC 4 does not support FMT_HAS_INCLUDE.
#if FMT_HAS_INCLUDE(<cxxabi.h>) || defined(__GLIBCXX__)
#  include <cxxabi.h>
// Android NDK with gabi++ library on some architectures does not implement
// abi::__cxa_demangle().
#  ifndef __GABIXX_CXXABI_H__
#    define FMT_HAS_ABI_CXA_DEMANGLE
#  endif
#endif

// Check if typeid is available.
#ifndef FMT_USE_TYPEID
// __RTTI is for EDG compilers. In MSVC typeid is available without RTTI.
#  if defined(__GXX_RTTI) || FMT_HAS_FEATURE(cxx_rtti) || FMT_MSC_VERSION || \
      defined(__INTEL_RTTI__) || defined(__RTTI)
#    define FMT_USE_TYPEID 1
#  else
#    define FMT_USE_TYPEID 0
#  endif
#endif

#ifdef __cpp_lib_filesystem
FMT_BEGIN_NAMESPACE

namespace detail {

template <typename Char> auto get_path_string(const std::filesystem::path& p) {
  return p.string<Char>();
}

template <typename Char>
void write_escaped_path(basic_memory_buffer<Char>& quoted,
                        const std::filesystem::path& p) {
  write_escaped_string<Char>(std::back_inserter(quoted), p.string<Char>());
}

#  ifdef _WIN32
template <>
inline auto get_path_string<char>(const std::filesystem::path& p) {
  return to_utf8<wchar_t>(p.native(), to_utf8_error_policy::replace);
}

template <>
inline void write_escaped_path<char>(memory_buffer& quoted,
                                     const std::filesystem::path& p) {
  auto buf = basic_memory_buffer<wchar_t>();
  write_escaped_string<wchar_t>(std::back_inserter(buf), p.native());
  bool valid = to_utf8<wchar_t>::convert(quoted, {buf.data(), buf.size()});
  FMT_ASSERT(valid, "invalid utf16");
}
#  endif  // _WIN32

template <>
inline void write_escaped_path<std::filesystem::path::value_type>(
    basic_memory_buffer<std::filesystem::path::value_type>& quoted,
    const std::filesystem::path& p) {
  write_escaped_string<std::filesystem::path::value_type>(
      std::back_inserter(quoted), p.native());
}

}  // namespace detail

FMT_EXPORT
template <typename Char> struct formatter<std::filesystem::path, Char> {
 private:
  format_specs<Char> specs_;
  detail::arg_ref<Char> width_ref_;
  bool debug_ = false;

 public:
  FMT_CONSTEXPR void set_debug_format(bool set = true) { debug_ = set; }

  template <typename ParseContext> FMT_CONSTEXPR auto parse(ParseContext& ctx) {
    auto it = ctx.begin(), end = ctx.end();
    if (it == end) return it;

    it = detail::parse_align(it, end, specs_);
    if (it == end) return it;

    it = detail::parse_dynamic_spec(it, end, specs_.width, width_ref_, ctx);
    if (it != end && *it == '?') {
      debug_ = true;
      ++it;
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const std::filesystem::path& p, FormatContext& ctx) const {
    auto specs = specs_;
    detail::handle_dynamic_spec<detail::width_checker>(specs.width, width_ref_,
                                                       ctx);
    if (!debug_) {
      auto s = detail::get_path_string<Char>(p);
      return detail::write(ctx.out(), basic_string_view<Char>(s), specs);
    }
    auto quoted = basic_memory_buffer<Char>();
    detail::write_escaped_path(quoted, p);
    return detail::write(ctx.out(),
                         basic_string_view<Char>(quoted.data(), quoted.size()),
                         specs);
  }
};
FMT_END_NAMESPACE
#endif

FMT_BEGIN_NAMESPACE
FMT_EXPORT
template <typename Char>
struct formatter<std::thread::id, Char> : basic_ostream_formatter<Char> {};
FMT_END_NAMESPACE

#ifdef __cpp_lib_optional
FMT_BEGIN_NAMESPACE
FMT_EXPORT
template <typename T, typename Char>
struct formatter<std::optional<T>, Char,
                 std::enable_if_t<is_formattable<T, Char>::value>> {
 private:
  formatter<T, Char> underlying_;
  static constexpr basic_string_view<Char> optional =
      detail::string_literal<Char, 'o', 'p', 't', 'i', 'o', 'n', 'a', 'l',
                             '('>{};
  static constexpr basic_string_view<Char> none =
      detail::string_literal<Char, 'n', 'o', 'n', 'e'>{};

  template <class U>
  FMT_CONSTEXPR static auto maybe_set_debug_format(U& u, bool set)
      -> decltype(u.set_debug_format(set)) {
    u.set_debug_format(set);
  }

  template <class U>
  FMT_CONSTEXPR static void maybe_set_debug_format(U&, ...) {}

 public:
  template <typename ParseContext> FMT_CONSTEXPR auto parse(ParseContext& ctx) {
    maybe_set_debug_format(underlying_, true);
    return underlying_.parse(ctx);
  }

  template <typename FormatContext>
  auto format(std::optional<T> const& opt, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    if (!opt) return detail::write<Char>(ctx.out(), none);

    auto out = ctx.out();
    out = detail::write<Char>(out, optional);
    ctx.advance_to(out);
    out = underlying_.format(*opt, ctx);
    return detail::write(out, ')');
  }
};
FMT_END_NAMESPACE
#endif  // __cpp_lib_optional

#ifdef __cpp_lib_variant
FMT_BEGIN_NAMESPACE
namespace detail {

template <typename T>
using variant_index_sequence =
    std::make_index_sequence<std::variant_size<T>::value>;

template <typename> struct is_variant_like_ : std::false_type {};
template <typename... Types>
struct is_variant_like_<std::variant<Types...>> : std::true_type {};

// formattable element check.
template <typename T, typename C> class is_variant_formattable_ {
  template <std::size_t... Is>
  static std::conjunction<
      is_formattable<std::variant_alternative_t<Is, T>, C>...>
      check(std::index_sequence<Is...>);

 public:
  static constexpr const bool value =
      decltype(check(variant_index_sequence<T>{}))::value;
};

template <typename Char, typename OutputIt, typename T>
auto write_variant_alternative(OutputIt out, const T& v) -> OutputIt {
  if constexpr (is_string<T>::value)
    return write_escaped_string<Char>(out, detail::to_string_view(v));
  else if constexpr (std::is_same_v<T, Char>)
    return write_escaped_char(out, v);
  else
    return write<Char>(out, v);
}

}  // namespace detail

template <typename T> struct is_variant_like {
  static constexpr const bool value = detail::is_variant_like_<T>::value;
};

template <typename T, typename C> struct is_variant_formattable {
  static constexpr const bool value =
      detail::is_variant_formattable_<T, C>::value;
};

FMT_EXPORT
template <typename Char> struct formatter<std::monostate, Char> {
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const std::monostate&, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return detail::write<Char>(ctx.out(), "monostate");
  }
};

FMT_EXPORT
template <typename Variant, typename Char>
struct formatter<
    Variant, Char,
    std::enable_if_t<std::conjunction_v<
        is_variant_like<Variant>, is_variant_formattable<Variant, Char>>>> {
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const Variant& value, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto out = ctx.out();

    out = detail::write<Char>(out, "variant(");
    FMT_TRY {
      std::visit(
          [&](const auto& v) {
            out = detail::write_variant_alternative<Char>(out, v);
          },
          value);
    }
    FMT_CATCH(const std::bad_variant_access&) {
      detail::write<Char>(out, "valueless by exception");
    }
    *out++ = ')';
    return out;
  }
};
FMT_END_NAMESPACE
#endif  // __cpp_lib_variant

FMT_BEGIN_NAMESPACE
FMT_EXPORT
template <typename Char> struct formatter<std::error_code, Char> {
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename FormatContext>
  FMT_CONSTEXPR auto format(const std::error_code& ec, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto out = ctx.out();
    out = detail::write_bytes(out, ec.category().name(), format_specs<Char>());
    out = detail::write<Char>(out, Char(':'));
    out = detail::write<Char>(out, ec.value());
    return out;
  }
};

FMT_EXPORT
template <typename T, typename Char>
struct formatter<
    T, Char,
    typename std::enable_if<std::is_base_of<std::exception, T>::value>::type> {
 private:
  bool with_typename_ = false;

 public:
  FMT_CONSTEXPR auto parse(basic_format_parse_context<Char>& ctx)
      -> decltype(ctx.begin()) {
    auto it = ctx.begin();
    auto end = ctx.end();
    if (it == end || *it == '}') return it;
    if (*it == 't') {
      ++it;
      with_typename_ = FMT_USE_TYPEID != 0;
    }
    return it;
  }

  template <typename OutputIt>
  auto format(const std::exception& ex,
              basic_format_context<OutputIt, Char>& ctx) const -> OutputIt {
    format_specs<Char> spec;
    auto out = ctx.out();
    if (!with_typename_)
      return detail::write_bytes(out, string_view(ex.what()), spec);

#if FMT_USE_TYPEID
    const std::type_info& ti = typeid(ex);
#  ifdef FMT_HAS_ABI_CXA_DEMANGLE
    int status = 0;
    std::size_t size = 0;
    std::unique_ptr<char, decltype(&std::free)> demangled_name_ptr(
        abi::__cxa_demangle(ti.name(), nullptr, &size, &status), &std::free);

    string_view demangled_name_view;
    if (demangled_name_ptr) {
      demangled_name_view = demangled_name_ptr.get();

      // Normalization of stdlib inline namespace names.
      // libc++ inline namespaces.
      //  std::__1::*       -> std::*
      //  std::__1::__fs::* -> std::*
      // libstdc++ inline namespaces.
      //  std::__cxx11::*             -> std::*
      //  std::filesystem::__cxx11::* -> std::filesystem::*
      if (demangled_name_view.starts_with("std::")) {
        char* begin = demangled_name_ptr.get();
        char* to = begin + 5;  // std::
        for (char *from = to, *end = begin + demangled_name_view.size();
             from < end;) {
          // This is safe, because demangled_name is NUL-terminated.
          if (from[0] == '_' && from[1] == '_') {
            char* next = from + 1;
            while (next < end && *next != ':') next++;
            if (next[0] == ':' && next[1] == ':') {
              from = next + 2;
              continue;
            }
          }
          *to++ = *from++;
        }
        demangled_name_view = {begin, detail::to_unsigned(to - begin)};
      }
    } else {
      demangled_name_view = string_view(ti.name());
    }
    out = detail::write_bytes(out, demangled_name_view, spec);
#  elif FMT_MSC_VERSION
    string_view demangled_name_view(ti.name());
    if (demangled_name_view.starts_with("class "))
      demangled_name_view.remove_prefix(6);
    else if (demangled_name_view.starts_with("struct "))
      demangled_name_view.remove_prefix(7);
    out = detail::write_bytes(out, demangled_name_view, spec);
#  else
    out = detail::write_bytes(out, string_view(ti.name()), spec);
#  endif
    *out++ = ':';
    *out++ = ' ';
    return detail::write_bytes(out, string_view(ex.what()), spec);
#endif
  }
};

namespace detail {

template <typename T, typename Enable = void>
struct has_flip : std::false_type {};

template <typename T>
struct has_flip<T, void_t<decltype(std::declval<T>().flip())>>
    : std::true_type {};

template <typename T> struct is_bit_reference_like {
  static constexpr const bool value =
      std::is_convertible<T, bool>::value &&
      std::is_nothrow_assignable<T, bool>::value && has_flip<T>::value;
};

#ifdef _LIBCPP_VERSION

// Workaround for libc++ incompatibility with C++ standard.
// According to the Standard, `bitset::operator[] const` returns bool.
template <typename C>
struct is_bit_reference_like<std::__bit_const_reference<C>> {
  static constexpr const bool value = true;
};

#endif

}  // namespace detail

// We can't use std::vector<bool, Allocator>::reference and
// std::bitset<N>::reference because the compiler can't deduce Allocator and N
// in partial specialization.
FMT_EXPORT
template <typename BitRef, typename Char>
struct formatter<BitRef, Char,
                 enable_if_t<detail::is_bit_reference_like<BitRef>::value>>
    : formatter<bool, Char> {
  template <typename FormatContext>
  FMT_CONSTEXPR auto format(const BitRef& v, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return formatter<bool, Char>::format(v, ctx);
  }
};

FMT_EXPORT
template <typename T, typename Char>
struct formatter<std::atomic<T>, Char,
                 enable_if_t<is_formattable<T, Char>::value>>
    : formatter<T, Char> {
  template <typename FormatContext>
  auto format(const std::atomic<T>& v, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return formatter<T, Char>::format(v.load(), ctx);
  }
};

#ifdef __cpp_lib_atomic_flag_test
FMT_EXPORT
template <typename Char>
struct formatter<std::atomic_flag, Char>
    : formatter<bool, Char> {
  template <typename FormatContext>
  auto format(const std::atomic_flag& v, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return formatter<bool, Char>::format(v.test(), ctx);
  }
};
#endif // __cpp_lib_atomic_flag_test

FMT_END_NAMESPACE
#endif  // FMT_STD_H_
