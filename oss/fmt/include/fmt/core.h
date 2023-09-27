// Formatting library for C++ - the core API for char/UTF-8
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_CORE_H_
#define FMT_CORE_H_

#include <cstddef>  // std::byte
#include <cstdio>   // std::FILE
#include <cstring>  // std::strlen
#include <iterator>
#include <limits>
#include <memory>  // std::addressof
#include <string>
#include <type_traits>

// The fmt library version in the form major * 10000 + minor * 100 + patch.
#define FMT_VERSION 100101

#if defined(__clang__) && !defined(__ibmxl__)
#  define FMT_CLANG_VERSION (__clang_major__ * 100 + __clang_minor__)
#else
#  define FMT_CLANG_VERSION 0
#endif

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && \
    !defined(__NVCOMPILER)
#  define FMT_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#  define FMT_GCC_VERSION 0
#endif

#ifndef FMT_GCC_PRAGMA
// Workaround _Pragma bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59884.
#  if FMT_GCC_VERSION >= 504
#    define FMT_GCC_PRAGMA(arg) _Pragma(arg)
#  else
#    define FMT_GCC_PRAGMA(arg)
#  endif
#endif

#ifdef __ICL
#  define FMT_ICC_VERSION __ICL
#elif defined(__INTEL_COMPILER)
#  define FMT_ICC_VERSION __INTEL_COMPILER
#else
#  define FMT_ICC_VERSION 0
#endif

#ifdef _MSC_VER
#  define FMT_MSC_VERSION _MSC_VER
#  define FMT_MSC_WARNING(...) __pragma(warning(__VA_ARGS__))
#else
#  define FMT_MSC_VERSION 0
#  define FMT_MSC_WARNING(...)
#endif

#ifdef _MSVC_LANG
#  define FMT_CPLUSPLUS _MSVC_LANG
#else
#  define FMT_CPLUSPLUS __cplusplus
#endif

#ifdef __has_feature
#  define FMT_HAS_FEATURE(x) __has_feature(x)
#else
#  define FMT_HAS_FEATURE(x) 0
#endif

#if defined(__has_include) || FMT_ICC_VERSION >= 1600 || FMT_MSC_VERSION > 1900
#  define FMT_HAS_INCLUDE(x) __has_include(x)
#else
#  define FMT_HAS_INCLUDE(x) 0
#endif

#ifdef __has_cpp_attribute
#  define FMT_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#  define FMT_HAS_CPP_ATTRIBUTE(x) 0
#endif

#define FMT_HAS_CPP14_ATTRIBUTE(attribute) \
  (FMT_CPLUSPLUS >= 201402L && FMT_HAS_CPP_ATTRIBUTE(attribute))

#define FMT_HAS_CPP17_ATTRIBUTE(attribute) \
  (FMT_CPLUSPLUS >= 201703L && FMT_HAS_CPP_ATTRIBUTE(attribute))

// Check if relaxed C++14 constexpr is supported.
// GCC doesn't allow throw in constexpr until version 6 (bug 67371).
#ifndef FMT_USE_CONSTEXPR
#  if (FMT_HAS_FEATURE(cxx_relaxed_constexpr) || FMT_MSC_VERSION >= 1912 || \
       (FMT_GCC_VERSION >= 600 && FMT_CPLUSPLUS >= 201402L)) &&             \
      !FMT_ICC_VERSION && (!defined(__NVCC__) || FMT_CPLUSPLUS >= 202002L)
#    define FMT_USE_CONSTEXPR 1
#  else
#    define FMT_USE_CONSTEXPR 0
#  endif
#endif
#if FMT_USE_CONSTEXPR
#  define FMT_CONSTEXPR constexpr
#else
#  define FMT_CONSTEXPR
#endif

#if ((FMT_CPLUSPLUS >= 202002L) &&                            \
     (!defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE > 9)) || \
    (FMT_CPLUSPLUS >= 201709L && FMT_GCC_VERSION >= 1002)
#  define FMT_CONSTEXPR20 constexpr
#else
#  define FMT_CONSTEXPR20
#endif

// Check if constexpr std::char_traits<>::{compare,length} are supported.
#if defined(__GLIBCXX__)
#  if FMT_CPLUSPLUS >= 201703L && defined(_GLIBCXX_RELEASE) && \
      _GLIBCXX_RELEASE >= 7  // GCC 7+ libstdc++ has _GLIBCXX_RELEASE.
#    define FMT_CONSTEXPR_CHAR_TRAITS constexpr
#  endif
#elif defined(_LIBCPP_VERSION) && FMT_CPLUSPLUS >= 201703L && \
    _LIBCPP_VERSION >= 4000
#  define FMT_CONSTEXPR_CHAR_TRAITS constexpr
#elif FMT_MSC_VERSION >= 1914 && FMT_CPLUSPLUS >= 201703L
#  define FMT_CONSTEXPR_CHAR_TRAITS constexpr
#endif
#ifndef FMT_CONSTEXPR_CHAR_TRAITS
#  define FMT_CONSTEXPR_CHAR_TRAITS
#endif

// Check if exceptions are disabled.
#ifndef FMT_EXCEPTIONS
#  if (defined(__GNUC__) && !defined(__EXCEPTIONS)) || \
      (FMT_MSC_VERSION && !_HAS_EXCEPTIONS)
#    define FMT_EXCEPTIONS 0
#  else
#    define FMT_EXCEPTIONS 1
#  endif
#endif

// Disable [[noreturn]] on MSVC/NVCC because of bogus unreachable code warnings.
#if FMT_EXCEPTIONS && FMT_HAS_CPP_ATTRIBUTE(noreturn) && !FMT_MSC_VERSION && \
    !defined(__NVCC__)
#  define FMT_NORETURN [[noreturn]]
#else
#  define FMT_NORETURN
#endif

#ifndef FMT_NODISCARD
#  if FMT_HAS_CPP17_ATTRIBUTE(nodiscard)
#    define FMT_NODISCARD [[nodiscard]]
#  else
#    define FMT_NODISCARD
#  endif
#endif

#ifndef FMT_INLINE
#  if FMT_GCC_VERSION || FMT_CLANG_VERSION
#    define FMT_INLINE inline __attribute__((always_inline))
#  else
#    define FMT_INLINE inline
#  endif
#endif

#ifdef _MSC_VER
#  define FMT_UNCHECKED_ITERATOR(It) \
    using _Unchecked_type = It  // Mark iterator as checked.
#else
#  define FMT_UNCHECKED_ITERATOR(It) using unchecked_type = It
#endif

#ifndef FMT_BEGIN_NAMESPACE
#  define FMT_BEGIN_NAMESPACE \
    namespace fmt {           \
    inline namespace v10 {
#  define FMT_END_NAMESPACE \
    }                       \
    }
#endif

#ifndef FMT_EXPORT
#  define FMT_EXPORT
#  define FMT_BEGIN_EXPORT
#  define FMT_END_EXPORT
#endif

#if !defined(FMT_HEADER_ONLY) && defined(_WIN32)
#  ifdef FMT_LIB_EXPORT
#    define FMT_API __declspec(dllexport)
#  elif defined(FMT_SHARED)
#    define FMT_API __declspec(dllimport)
#  endif
#else
#  if defined(FMT_LIB_EXPORT) || defined(FMT_SHARED)
#    if defined(__GNUC__) || defined(__clang__)
#      define FMT_API __attribute__((visibility("default")))
#    endif
#  endif
#endif
#ifndef FMT_API
#  define FMT_API
#endif

// libc++ supports string_view in pre-c++17.
#if FMT_HAS_INCLUDE(<string_view>) && \
    (FMT_CPLUSPLUS >= 201703L || defined(_LIBCPP_VERSION))
#  include <string_view>
#  define FMT_USE_STRING_VIEW
#elif FMT_HAS_INCLUDE("experimental/string_view") && FMT_CPLUSPLUS >= 201402L
#  include <experimental/string_view>
#  define FMT_USE_EXPERIMENTAL_STRING_VIEW
#endif

#ifndef FMT_UNICODE
#  define FMT_UNICODE !FMT_MSC_VERSION
#endif

#ifndef FMT_CONSTEVAL
#  if ((FMT_GCC_VERSION >= 1000 || FMT_CLANG_VERSION >= 1101) && \
       (!defined(__apple_build_version__) ||                     \
        __apple_build_version__ >= 14000029L) &&                 \
       FMT_CPLUSPLUS >= 202002L) ||                              \
      (defined(__cpp_consteval) &&                               \
       (!FMT_MSC_VERSION || _MSC_FULL_VER >= 193030704))
// consteval is broken in MSVC before VS2022 and Apple clang before 14.
#    define FMT_CONSTEVAL consteval
#    define FMT_HAS_CONSTEVAL
#  else
#    define FMT_CONSTEVAL
#  endif
#endif

#ifndef FMT_USE_NONTYPE_TEMPLATE_ARGS
#  if defined(__cpp_nontype_template_args) &&                  \
      ((FMT_GCC_VERSION >= 903 && FMT_CPLUSPLUS >= 201709L) || \
       __cpp_nontype_template_args >= 201911L) &&              \
      !defined(__NVCOMPILER) && !defined(__LCC__)
#    define FMT_USE_NONTYPE_TEMPLATE_ARGS 1
#  else
#    define FMT_USE_NONTYPE_TEMPLATE_ARGS 0
#  endif
#endif

// Enable minimal optimizations for more compact code in debug mode.
FMT_GCC_PRAGMA("GCC push_options")
#if !defined(__OPTIMIZE__) && !defined(__NVCOMPILER) && !defined(__LCC__) && \
    !defined(__CUDACC__)
FMT_GCC_PRAGMA("GCC optimize(\"Og\")")
#endif

FMT_BEGIN_NAMESPACE

// Implementations of enable_if_t and other metafunctions for older systems.
template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;
template <bool B, typename T, typename F>
using conditional_t = typename std::conditional<B, T, F>::type;
template <bool B> using bool_constant = std::integral_constant<bool, B>;
template <typename T>
using remove_reference_t = typename std::remove_reference<T>::type;
template <typename T>
using remove_const_t = typename std::remove_const<T>::type;
template <typename T>
using remove_cvref_t = typename std::remove_cv<remove_reference_t<T>>::type;
template <typename T> struct type_identity { using type = T; };
template <typename T> using type_identity_t = typename type_identity<T>::type;
template <typename T>
using underlying_t = typename std::underlying_type<T>::type;

// Checks whether T is a container with contiguous storage.
template <typename T> struct is_contiguous : std::false_type {};
template <typename Char>
struct is_contiguous<std::basic_string<Char>> : std::true_type {};

struct monostate {
  constexpr monostate() {}
};

// An enable_if helper to be used in template parameters which results in much
// shorter symbols: https://godbolt.org/z/sWw4vP. Extra parentheses are needed
// to workaround a bug in MSVC 2019 (see #1140 and #1186).
#ifdef FMT_DOC
#  define FMT_ENABLE_IF(...)
#else
#  define FMT_ENABLE_IF(...) fmt::enable_if_t<(__VA_ARGS__), int> = 0
#endif

// This is defined in core.h instead of format.h to avoid injecting in std.
// It is a template to avoid undesirable implicit conversions to std::byte.
#ifdef __cpp_lib_byte
template <typename T, FMT_ENABLE_IF(std::is_same<T, std::byte>::value)>
inline auto format_as(T b) -> unsigned char {
  return static_cast<unsigned char>(b);
}
#endif

namespace detail {
// Suppresses "unused variable" warnings with the method described in
// https://herbsutter.com/2009/10/18/mailbag-shutting-up-compiler-warnings/.
// (void)var does not work on many Intel compilers.
template <typename... T> FMT_CONSTEXPR void ignore_unused(const T&...) {}

constexpr FMT_INLINE auto is_constant_evaluated(
    bool default_value = false) noexcept -> bool {
// Workaround for incompatibility between libstdc++ consteval-based
// std::is_constant_evaluated() implementation and clang-14.
// https://github.com/fmtlib/fmt/issues/3247
#if FMT_CPLUSPLUS >= 202002L && defined(_GLIBCXX_RELEASE) && \
    _GLIBCXX_RELEASE >= 12 &&                                \
    (FMT_CLANG_VERSION >= 1400 && FMT_CLANG_VERSION < 1500)
  ignore_unused(default_value);
  return __builtin_is_constant_evaluated();
#elif defined(__cpp_lib_is_constant_evaluated)
  ignore_unused(default_value);
  return std::is_constant_evaluated();
#else
  return default_value;
#endif
}

// Suppresses "conditional expression is constant" warnings.
template <typename T> constexpr FMT_INLINE auto const_check(T value) -> T {
  return value;
}

FMT_NORETURN FMT_API void assert_fail(const char* file, int line,
                                      const char* message);

#ifndef FMT_ASSERT
#  ifdef NDEBUG
// FMT_ASSERT is not empty to avoid -Wempty-body.
#    define FMT_ASSERT(condition, message) \
      fmt::detail::ignore_unused((condition), (message))
#  else
#    define FMT_ASSERT(condition, message)                                    \
      ((condition) /* void() fails with -Winvalid-constexpr on clang 4.0.1 */ \
           ? (void)0                                                          \
           : fmt::detail::assert_fail(__FILE__, __LINE__, (message)))
#  endif
#endif

#if defined(FMT_USE_STRING_VIEW)
template <typename Char> using std_string_view = std::basic_string_view<Char>;
#elif defined(FMT_USE_EXPERIMENTAL_STRING_VIEW)
template <typename Char>
using std_string_view = std::experimental::basic_string_view<Char>;
#else
template <typename T> struct std_string_view {};
#endif

#ifdef FMT_USE_INT128
// Do nothing.
#elif defined(__SIZEOF_INT128__) && !defined(__NVCC__) && \
    !(FMT_CLANG_VERSION && FMT_MSC_VERSION)
#  define FMT_USE_INT128 1
using int128_opt = __int128_t;  // An optional native 128-bit integer.
using uint128_opt = __uint128_t;
template <typename T> inline auto convert_for_visit(T value) -> T {
  return value;
}
#else
#  define FMT_USE_INT128 0
#endif
#if !FMT_USE_INT128
enum class int128_opt {};
enum class uint128_opt {};
// Reduce template instantiations.
template <typename T> auto convert_for_visit(T) -> monostate { return {}; }
#endif

// Casts a nonnegative integer to unsigned.
template <typename Int>
FMT_CONSTEXPR auto to_unsigned(Int value) ->
    typename std::make_unsigned<Int>::type {
  FMT_ASSERT(std::is_unsigned<Int>::value || value >= 0, "negative value");
  return static_cast<typename std::make_unsigned<Int>::type>(value);
}

FMT_CONSTEXPR inline auto is_utf8() -> bool {
  FMT_MSC_WARNING(suppress : 4566) constexpr unsigned char section[] = "\u00A7";

  // Avoid buggy sign extensions in MSVC's constant evaluation mode (#2297).
  using uchar = unsigned char;
  return FMT_UNICODE || (sizeof(section) == 3 && uchar(section[0]) == 0xC2 &&
                         uchar(section[1]) == 0xA7);
}
}  // namespace detail

/**
  An implementation of ``std::basic_string_view`` for pre-C++17. It provides a
  subset of the API. ``fmt::basic_string_view`` is used for format strings even
  if ``std::string_view`` is available to prevent issues when a library is
  compiled with a different ``-std`` option than the client code (which is not
  recommended).
 */
FMT_EXPORT
template <typename Char> class basic_string_view {
 private:
  const Char* data_;
  size_t size_;

 public:
  using value_type = Char;
  using iterator = const Char*;

  constexpr basic_string_view() noexcept : data_(nullptr), size_(0) {}

  /** Constructs a string reference object from a C string and a size. */
  constexpr basic_string_view(const Char* s, size_t count) noexcept
      : data_(s), size_(count) {}

  /**
    \rst
    Constructs a string reference object from a C string computing
    the size with ``std::char_traits<Char>::length``.
    \endrst
   */
  FMT_CONSTEXPR_CHAR_TRAITS
  FMT_INLINE
  basic_string_view(const Char* s)
      : data_(s),
        size_(detail::const_check(std::is_same<Char, char>::value &&
                                  !detail::is_constant_evaluated(true))
                  ? std::strlen(reinterpret_cast<const char*>(s))
                  : std::char_traits<Char>::length(s)) {}

  /** Constructs a string reference from a ``std::basic_string`` object. */
  template <typename Traits, typename Alloc>
  FMT_CONSTEXPR basic_string_view(
      const std::basic_string<Char, Traits, Alloc>& s) noexcept
      : data_(s.data()), size_(s.size()) {}

  template <typename S, FMT_ENABLE_IF(std::is_same<
                                      S, detail::std_string_view<Char>>::value)>
  FMT_CONSTEXPR basic_string_view(S s) noexcept
      : data_(s.data()), size_(s.size()) {}

  /** Returns a pointer to the string data. */
  constexpr auto data() const noexcept -> const Char* { return data_; }

  /** Returns the string size. */
  constexpr auto size() const noexcept -> size_t { return size_; }

  constexpr auto begin() const noexcept -> iterator { return data_; }
  constexpr auto end() const noexcept -> iterator { return data_ + size_; }

  constexpr auto operator[](size_t pos) const noexcept -> const Char& {
    return data_[pos];
  }

  FMT_CONSTEXPR void remove_prefix(size_t n) noexcept {
    data_ += n;
    size_ -= n;
  }

  FMT_CONSTEXPR_CHAR_TRAITS bool starts_with(
      basic_string_view<Char> sv) const noexcept {
    return size_ >= sv.size_ &&
           std::char_traits<Char>::compare(data_, sv.data_, sv.size_) == 0;
  }
  FMT_CONSTEXPR_CHAR_TRAITS bool starts_with(Char c) const noexcept {
    return size_ >= 1 && std::char_traits<Char>::eq(*data_, c);
  }
  FMT_CONSTEXPR_CHAR_TRAITS bool starts_with(const Char* s) const {
    return starts_with(basic_string_view<Char>(s));
  }

  // Lexicographically compare this string reference to other.
  FMT_CONSTEXPR_CHAR_TRAITS auto compare(basic_string_view other) const -> int {
    size_t str_size = size_ < other.size_ ? size_ : other.size_;
    int result = std::char_traits<Char>::compare(data_, other.data_, str_size);
    if (result == 0)
      result = size_ == other.size_ ? 0 : (size_ < other.size_ ? -1 : 1);
    return result;
  }

  FMT_CONSTEXPR_CHAR_TRAITS friend auto operator==(basic_string_view lhs,
                                                   basic_string_view rhs)
      -> bool {
    return lhs.compare(rhs) == 0;
  }
  friend auto operator!=(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) != 0;
  }
  friend auto operator<(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) < 0;
  }
  friend auto operator<=(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) <= 0;
  }
  friend auto operator>(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) > 0;
  }
  friend auto operator>=(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) >= 0;
  }
};

FMT_EXPORT
using string_view = basic_string_view<char>;

/** Specifies if ``T`` is a character type. Can be specialized by users. */
FMT_EXPORT
template <typename T> struct is_char : std::false_type {};
template <> struct is_char<char> : std::true_type {};

namespace detail {

// A base class for compile-time strings.
struct compile_string {};

template <typename S>
struct is_compile_string : std::is_base_of<compile_string, S> {};

template <typename Char, FMT_ENABLE_IF(is_char<Char>::value)>
FMT_INLINE auto to_string_view(const Char* s) -> basic_string_view<Char> {
  return s;
}
template <typename Char, typename Traits, typename Alloc>
inline auto to_string_view(const std::basic_string<Char, Traits, Alloc>& s)
    -> basic_string_view<Char> {
  return s;
}
template <typename Char>
constexpr auto to_string_view(basic_string_view<Char> s)
    -> basic_string_view<Char> {
  return s;
}
template <typename Char,
          FMT_ENABLE_IF(!std::is_empty<std_string_view<Char>>::value)>
inline auto to_string_view(std_string_view<Char> s) -> basic_string_view<Char> {
  return s;
}
template <typename S, FMT_ENABLE_IF(is_compile_string<S>::value)>
constexpr auto to_string_view(const S& s)
    -> basic_string_view<typename S::char_type> {
  return basic_string_view<typename S::char_type>(s);
}
void to_string_view(...);

// Specifies whether S is a string type convertible to fmt::basic_string_view.
// It should be a constexpr function but MSVC 2017 fails to compile it in
// enable_if and MSVC 2015 fails to compile it as an alias template.
// ADL is intentionally disabled as to_string_view is not an extension point.
template <typename S>
struct is_string
    : std::is_class<decltype(detail::to_string_view(std::declval<S>()))> {};

template <typename S, typename = void> struct char_t_impl {};
template <typename S> struct char_t_impl<S, enable_if_t<is_string<S>::value>> {
  using result = decltype(to_string_view(std::declval<S>()));
  using type = typename result::value_type;
};

enum class type {
  none_type,
  // Integer types should go first,
  int_type,
  uint_type,
  long_long_type,
  ulong_long_type,
  int128_type,
  uint128_type,
  bool_type,
  char_type,
  last_integer_type = char_type,
  // followed by floating-point types.
  float_type,
  double_type,
  long_double_type,
  last_numeric_type = long_double_type,
  cstring_type,
  string_type,
  pointer_type,
  custom_type
};

// Maps core type T to the corresponding type enum constant.
template <typename T, typename Char>
struct type_constant : std::integral_constant<type, type::custom_type> {};

#define FMT_TYPE_CONSTANT(Type, constant) \
  template <typename Char>                \
  struct type_constant<Type, Char>        \
      : std::integral_constant<type, type::constant> {}

FMT_TYPE_CONSTANT(int, int_type);
FMT_TYPE_CONSTANT(unsigned, uint_type);
FMT_TYPE_CONSTANT(long long, long_long_type);
FMT_TYPE_CONSTANT(unsigned long long, ulong_long_type);
FMT_TYPE_CONSTANT(int128_opt, int128_type);
FMT_TYPE_CONSTANT(uint128_opt, uint128_type);
FMT_TYPE_CONSTANT(bool, bool_type);
FMT_TYPE_CONSTANT(Char, char_type);
FMT_TYPE_CONSTANT(float, float_type);
FMT_TYPE_CONSTANT(double, double_type);
FMT_TYPE_CONSTANT(long double, long_double_type);
FMT_TYPE_CONSTANT(const Char*, cstring_type);
FMT_TYPE_CONSTANT(basic_string_view<Char>, string_type);
FMT_TYPE_CONSTANT(const void*, pointer_type);

constexpr bool is_integral_type(type t) {
  return t > type::none_type && t <= type::last_integer_type;
}
constexpr bool is_arithmetic_type(type t) {
  return t > type::none_type && t <= type::last_numeric_type;
}

constexpr auto set(type rhs) -> int { return 1 << static_cast<int>(rhs); }
constexpr auto in(type t, int set) -> bool {
  return ((set >> static_cast<int>(t)) & 1) != 0;
}

// Bitsets of types.
enum {
  sint_set =
      set(type::int_type) | set(type::long_long_type) | set(type::int128_type),
  uint_set = set(type::uint_type) | set(type::ulong_long_type) |
             set(type::uint128_type),
  bool_set = set(type::bool_type),
  char_set = set(type::char_type),
  float_set = set(type::float_type) | set(type::double_type) |
              set(type::long_double_type),
  string_set = set(type::string_type),
  cstring_set = set(type::cstring_type),
  pointer_set = set(type::pointer_type)
};

FMT_NORETURN FMT_API void throw_format_error(const char* message);

struct error_handler {
  constexpr error_handler() = default;

  // This function is intentionally not constexpr to give a compile-time error.
  FMT_NORETURN void on_error(const char* message) {
    throw_format_error(message);
  }
};
}  // namespace detail

/** Throws ``format_error`` with a given message. */
using detail::throw_format_error;

/** String's character type. */
template <typename S> using char_t = typename detail::char_t_impl<S>::type;

/**
  \rst
  Parsing context consisting of a format string range being parsed and an
  argument counter for automatic indexing.
  You can use the ``format_parse_context`` type alias for ``char`` instead.
  \endrst
 */
FMT_EXPORT
template <typename Char> class basic_format_parse_context {
 private:
  basic_string_view<Char> format_str_;
  int next_arg_id_;

  FMT_CONSTEXPR void do_check_arg_id(int id);

 public:
  using char_type = Char;
  using iterator = const Char*;

  explicit constexpr basic_format_parse_context(
      basic_string_view<Char> format_str, int next_arg_id = 0)
      : format_str_(format_str), next_arg_id_(next_arg_id) {}

  /**
    Returns an iterator to the beginning of the format string range being
    parsed.
   */
  constexpr auto begin() const noexcept -> iterator {
    return format_str_.begin();
  }

  /**
    Returns an iterator past the end of the format string range being parsed.
   */
  constexpr auto end() const noexcept -> iterator { return format_str_.end(); }

  /** Advances the begin iterator to ``it``. */
  FMT_CONSTEXPR void advance_to(iterator it) {
    format_str_.remove_prefix(detail::to_unsigned(it - begin()));
  }

  /**
    Reports an error if using the manual argument indexing; otherwise returns
    the next argument index and switches to the automatic indexing.
   */
  FMT_CONSTEXPR auto next_arg_id() -> int {
    if (next_arg_id_ < 0) {
      detail::throw_format_error(
          "cannot switch from manual to automatic argument indexing");
      return 0;
    }
    int id = next_arg_id_++;
    do_check_arg_id(id);
    return id;
  }

  /**
    Reports an error if using the automatic argument indexing; otherwise
    switches to the manual indexing.
   */
  FMT_CONSTEXPR void check_arg_id(int id) {
    if (next_arg_id_ > 0) {
      detail::throw_format_error(
          "cannot switch from automatic to manual argument indexing");
      return;
    }
    next_arg_id_ = -1;
    do_check_arg_id(id);
  }
  FMT_CONSTEXPR void check_arg_id(basic_string_view<Char>) {}
  FMT_CONSTEXPR void check_dynamic_spec(int arg_id);
};

FMT_EXPORT
using format_parse_context = basic_format_parse_context<char>;

namespace detail {
// A parse context with extra data used only in compile-time checks.
template <typename Char>
class compile_parse_context : public basic_format_parse_context<Char> {
 private:
  int num_args_;
  const type* types_;
  using base = basic_format_parse_context<Char>;

 public:
  explicit FMT_CONSTEXPR compile_parse_context(
      basic_string_view<Char> format_str, int num_args, const type* types,
      int next_arg_id = 0)
      : base(format_str, next_arg_id), num_args_(num_args), types_(types) {}

  constexpr auto num_args() const -> int { return num_args_; }
  constexpr auto arg_type(int id) const -> type { return types_[id]; }

  FMT_CONSTEXPR auto next_arg_id() -> int {
    int id = base::next_arg_id();
    if (id >= num_args_) throw_format_error("argument not found");
    return id;
  }

  FMT_CONSTEXPR void check_arg_id(int id) {
    base::check_arg_id(id);
    if (id >= num_args_) throw_format_error("argument not found");
  }
  using base::check_arg_id;

  FMT_CONSTEXPR void check_dynamic_spec(int arg_id) {
    detail::ignore_unused(arg_id);
#if !defined(__LCC__)
    if (arg_id < num_args_ && types_ && !is_integral_type(types_[arg_id]))
      throw_format_error("width/precision is not integer");
#endif
  }
};

// Extracts a reference to the container from back_insert_iterator.
template <typename Container>
inline auto get_container(std::back_insert_iterator<Container> it)
    -> Container& {
  using base = std::back_insert_iterator<Container>;
  struct accessor : base {
    accessor(base b) : base(b) {}
    using base::container;
  };
  return *accessor(it).container;
}

template <typename Char, typename InputIt, typename OutputIt>
FMT_CONSTEXPR auto copy_str(InputIt begin, InputIt end, OutputIt out)
    -> OutputIt {
  while (begin != end) *out++ = static_cast<Char>(*begin++);
  return out;
}

template <typename Char, typename T, typename U,
          FMT_ENABLE_IF(
              std::is_same<remove_const_t<T>, U>::value&& is_char<U>::value)>
FMT_CONSTEXPR auto copy_str(T* begin, T* end, U* out) -> U* {
  if (is_constant_evaluated()) return copy_str<Char, T*, U*>(begin, end, out);
  auto size = to_unsigned(end - begin);
  if (size > 0) memcpy(out, begin, size * sizeof(U));
  return out + size;
}

/**
  \rst
  A contiguous memory buffer with an optional growing ability. It is an internal
  class and shouldn't be used directly, only via `~fmt::basic_memory_buffer`.
  \endrst
 */
template <typename T> class buffer {
 private:
  T* ptr_;
  size_t size_;
  size_t capacity_;

 protected:
  // Don't initialize ptr_ since it is not accessed to save a few cycles.
  FMT_MSC_WARNING(suppress : 26495)
  buffer(size_t sz) noexcept : size_(sz), capacity_(sz) {}

  FMT_CONSTEXPR20 buffer(T* p = nullptr, size_t sz = 0, size_t cap = 0) noexcept
      : ptr_(p), size_(sz), capacity_(cap) {}

  FMT_CONSTEXPR20 ~buffer() = default;
  buffer(buffer&&) = default;

  /** Sets the buffer data and capacity. */
  FMT_CONSTEXPR void set(T* buf_data, size_t buf_capacity) noexcept {
    ptr_ = buf_data;
    capacity_ = buf_capacity;
  }

  /** Increases the buffer capacity to hold at least *capacity* elements. */
  virtual FMT_CONSTEXPR20 void grow(size_t capacity) = 0;

 public:
  using value_type = T;
  using const_reference = const T&;

  buffer(const buffer&) = delete;
  void operator=(const buffer&) = delete;

  FMT_INLINE auto begin() noexcept -> T* { return ptr_; }
  FMT_INLINE auto end() noexcept -> T* { return ptr_ + size_; }

  FMT_INLINE auto begin() const noexcept -> const T* { return ptr_; }
  FMT_INLINE auto end() const noexcept -> const T* { return ptr_ + size_; }

  /** Returns the size of this buffer. */
  constexpr auto size() const noexcept -> size_t { return size_; }

  /** Returns the capacity of this buffer. */
  constexpr auto capacity() const noexcept -> size_t { return capacity_; }

  /** Returns a pointer to the buffer data (not null-terminated). */
  FMT_CONSTEXPR auto data() noexcept -> T* { return ptr_; }
  FMT_CONSTEXPR auto data() const noexcept -> const T* { return ptr_; }

  /** Clears this buffer. */
  void clear() { size_ = 0; }

  // Tries resizing the buffer to contain *count* elements. If T is a POD type
  // the new elements may not be initialized.
  FMT_CONSTEXPR20 void try_resize(size_t count) {
    try_reserve(count);
    size_ = count <= capacity_ ? count : capacity_;
  }

  // Tries increasing the buffer capacity to *new_capacity*. It can increase the
  // capacity by a smaller amount than requested but guarantees there is space
  // for at least one additional element either by increasing the capacity or by
  // flushing the buffer if it is full.
  FMT_CONSTEXPR20 void try_reserve(size_t new_capacity) {
    if (new_capacity > capacity_) grow(new_capacity);
  }

  FMT_CONSTEXPR20 void push_back(const T& value) {
    try_reserve(size_ + 1);
    ptr_[size_++] = value;
  }

  /** Appends data to the end of the buffer. */
  template <typename U> void append(const U* begin, const U* end);

  template <typename Idx> FMT_CONSTEXPR auto operator[](Idx index) -> T& {
    return ptr_[index];
  }
  template <typename Idx>
  FMT_CONSTEXPR auto operator[](Idx index) const -> const T& {
    return ptr_[index];
  }
};

struct buffer_traits {
  explicit buffer_traits(size_t) {}
  auto count() const -> size_t { return 0; }
  auto limit(size_t size) -> size_t { return size; }
};

class fixed_buffer_traits {
 private:
  size_t count_ = 0;
  size_t limit_;

 public:
  explicit fixed_buffer_traits(size_t limit) : limit_(limit) {}
  auto count() const -> size_t { return count_; }
  auto limit(size_t size) -> size_t {
    size_t n = limit_ > count_ ? limit_ - count_ : 0;
    count_ += size;
    return size < n ? size : n;
  }
};

// A buffer that writes to an output iterator when flushed.
template <typename OutputIt, typename T, typename Traits = buffer_traits>
class iterator_buffer final : public Traits, public buffer<T> {
 private:
  OutputIt out_;
  enum { buffer_size = 256 };
  T data_[buffer_size];

 protected:
  FMT_CONSTEXPR20 void grow(size_t) override {
    if (this->size() == buffer_size) flush();
  }

  void flush() {
    auto size = this->size();
    this->clear();
    out_ = copy_str<T>(data_, data_ + this->limit(size), out_);
  }

 public:
  explicit iterator_buffer(OutputIt out, size_t n = buffer_size)
      : Traits(n), buffer<T>(data_, 0, buffer_size), out_(out) {}
  iterator_buffer(iterator_buffer&& other)
      : Traits(other), buffer<T>(data_, 0, buffer_size), out_(other.out_) {}
  ~iterator_buffer() { flush(); }

  auto out() -> OutputIt {
    flush();
    return out_;
  }
  auto count() const -> size_t { return Traits::count() + this->size(); }
};

template <typename T>
class iterator_buffer<T*, T, fixed_buffer_traits> final
    : public fixed_buffer_traits,
      public buffer<T> {
 private:
  T* out_;
  enum { buffer_size = 256 };
  T data_[buffer_size];

 protected:
  FMT_CONSTEXPR20 void grow(size_t) override {
    if (this->size() == this->capacity()) flush();
  }

  void flush() {
    size_t n = this->limit(this->size());
    if (this->data() == out_) {
      out_ += n;
      this->set(data_, buffer_size);
    }
    this->clear();
  }

 public:
  explicit iterator_buffer(T* out, size_t n = buffer_size)
      : fixed_buffer_traits(n), buffer<T>(out, 0, n), out_(out) {}
  iterator_buffer(iterator_buffer&& other)
      : fixed_buffer_traits(other),
        buffer<T>(std::move(other)),
        out_(other.out_) {
    if (this->data() != out_) {
      this->set(data_, buffer_size);
      this->clear();
    }
  }
  ~iterator_buffer() { flush(); }

  auto out() -> T* {
    flush();
    return out_;
  }
  auto count() const -> size_t {
    return fixed_buffer_traits::count() + this->size();
  }
};

template <typename T> class iterator_buffer<T*, T> final : public buffer<T> {
 protected:
  FMT_CONSTEXPR20 void grow(size_t) override {}

 public:
  explicit iterator_buffer(T* out, size_t = 0) : buffer<T>(out, 0, ~size_t()) {}

  auto out() -> T* { return &*this->end(); }
};

// A buffer that writes to a container with the contiguous storage.
template <typename Container>
class iterator_buffer<std::back_insert_iterator<Container>,
                      enable_if_t<is_contiguous<Container>::value,
                                  typename Container::value_type>>
    final : public buffer<typename Container::value_type> {
 private:
  Container& container_;

 protected:
  FMT_CONSTEXPR20 void grow(size_t capacity) override {
    container_.resize(capacity);
    this->set(&container_[0], capacity);
  }

 public:
  explicit iterator_buffer(Container& c)
      : buffer<typename Container::value_type>(c.size()), container_(c) {}
  explicit iterator_buffer(std::back_insert_iterator<Container> out, size_t = 0)
      : iterator_buffer(get_container(out)) {}

  auto out() -> std::back_insert_iterator<Container> {
    return std::back_inserter(container_);
  }
};

// A buffer that counts the number of code units written discarding the output.
template <typename T = char> class counting_buffer final : public buffer<T> {
 private:
  enum { buffer_size = 256 };
  T data_[buffer_size];
  size_t count_ = 0;

 protected:
  FMT_CONSTEXPR20 void grow(size_t) override {
    if (this->size() != buffer_size) return;
    count_ += this->size();
    this->clear();
  }

 public:
  counting_buffer() : buffer<T>(data_, 0, buffer_size) {}

  auto count() -> size_t { return count_ + this->size(); }
};
}  // namespace detail

template <typename Char>
FMT_CONSTEXPR void basic_format_parse_context<Char>::do_check_arg_id(int id) {
  // Argument id is only checked at compile-time during parsing because
  // formatting has its own validation.
  if (detail::is_constant_evaluated() &&
      (!FMT_GCC_VERSION || FMT_GCC_VERSION >= 1200)) {
    using context = detail::compile_parse_context<Char>;
    if (id >= static_cast<context*>(this)->num_args())
      detail::throw_format_error("argument not found");
  }
}

template <typename Char>
FMT_CONSTEXPR void basic_format_parse_context<Char>::check_dynamic_spec(
    int arg_id) {
  if (detail::is_constant_evaluated() &&
      (!FMT_GCC_VERSION || FMT_GCC_VERSION >= 1200)) {
    using context = detail::compile_parse_context<Char>;
    static_cast<context*>(this)->check_dynamic_spec(arg_id);
  }
}

FMT_EXPORT template <typename Context> class basic_format_arg;
FMT_EXPORT template <typename Context> class basic_format_args;
FMT_EXPORT template <typename Context> class dynamic_format_arg_store;

// A formatter for objects of type T.
FMT_EXPORT
template <typename T, typename Char = char, typename Enable = void>
struct formatter {
  // A deleted default constructor indicates a disabled formatter.
  formatter() = delete;
};

// Specifies if T has an enabled formatter specialization. A type can be
// formattable even if it doesn't have a formatter e.g. via a conversion.
template <typename T, typename Context>
using has_formatter =
    std::is_constructible<typename Context::template formatter_type<T>>;

// An output iterator that appends to a buffer.
// It is used to reduce symbol sizes for the common case.
class appender : public std::back_insert_iterator<detail::buffer<char>> {
  using base = std::back_insert_iterator<detail::buffer<char>>;

 public:
  using std::back_insert_iterator<detail::buffer<char>>::back_insert_iterator;
  appender(base it) noexcept : base(it) {}
  FMT_UNCHECKED_ITERATOR(appender);

  auto operator++() noexcept -> appender& { return *this; }
  auto operator++(int) noexcept -> appender { return *this; }
};

namespace detail {

template <typename Context, typename T>
constexpr auto has_const_formatter_impl(T*)
    -> decltype(typename Context::template formatter_type<T>().format(
                    std::declval<const T&>(), std::declval<Context&>()),
                true) {
  return true;
}
template <typename Context>
constexpr auto has_const_formatter_impl(...) -> bool {
  return false;
}
template <typename T, typename Context>
constexpr auto has_const_formatter() -> bool {
  return has_const_formatter_impl<Context>(static_cast<T*>(nullptr));
}

template <typename T>
using buffer_appender = conditional_t<std::is_same<T, char>::value, appender,
                                      std::back_insert_iterator<buffer<T>>>;

// Maps an output iterator to a buffer.
template <typename T, typename OutputIt>
auto get_buffer(OutputIt out) -> iterator_buffer<OutputIt, T> {
  return iterator_buffer<OutputIt, T>(out);
}
template <typename T, typename Buf,
          FMT_ENABLE_IF(std::is_base_of<buffer<char>, Buf>::value)>
auto get_buffer(std::back_insert_iterator<Buf> out) -> buffer<char>& {
  return get_container(out);
}

template <typename Buf, typename OutputIt>
FMT_INLINE auto get_iterator(Buf& buf, OutputIt) -> decltype(buf.out()) {
  return buf.out();
}
template <typename T, typename OutputIt>
auto get_iterator(buffer<T>&, OutputIt out) -> OutputIt {
  return out;
}

struct view {};

template <typename Char, typename T> struct named_arg : view {
  const Char* name;
  const T& value;
  named_arg(const Char* n, const T& v) : name(n), value(v) {}
};

template <typename Char> struct named_arg_info {
  const Char* name;
  int id;
};

template <typename T, typename Char, size_t NUM_ARGS, size_t NUM_NAMED_ARGS>
struct arg_data {
  // args_[0].named_args points to named_args_ to avoid bloating format_args.
  // +1 to workaround a bug in gcc 7.5 that causes duplicated-branches warning.
  T args_[1 + (NUM_ARGS != 0 ? NUM_ARGS : +1)];
  named_arg_info<Char> named_args_[NUM_NAMED_ARGS];

  template <typename... U>
  arg_data(const U&... init) : args_{T(named_args_, NUM_NAMED_ARGS), init...} {}
  arg_data(const arg_data& other) = delete;
  auto args() const -> const T* { return args_ + 1; }
  auto named_args() -> named_arg_info<Char>* { return named_args_; }
};

template <typename T, typename Char, size_t NUM_ARGS>
struct arg_data<T, Char, NUM_ARGS, 0> {
  // +1 to workaround a bug in gcc 7.5 that causes duplicated-branches warning.
  T args_[NUM_ARGS != 0 ? NUM_ARGS : +1];

  template <typename... U>
  FMT_CONSTEXPR FMT_INLINE arg_data(const U&... init) : args_{init...} {}
  FMT_CONSTEXPR FMT_INLINE auto args() const -> const T* { return args_; }
  FMT_CONSTEXPR FMT_INLINE auto named_args() -> std::nullptr_t {
    return nullptr;
  }
};

template <typename Char>
inline void init_named_args(named_arg_info<Char>*, int, int) {}

template <typename T> struct is_named_arg : std::false_type {};
template <typename T> struct is_statically_named_arg : std::false_type {};

template <typename T, typename Char>
struct is_named_arg<named_arg<Char, T>> : std::true_type {};

template <typename Char, typename T, typename... Tail,
          FMT_ENABLE_IF(!is_named_arg<T>::value)>
void init_named_args(named_arg_info<Char>* named_args, int arg_count,
                     int named_arg_count, const T&, const Tail&... args) {
  init_named_args(named_args, arg_count + 1, named_arg_count, args...);
}

template <typename Char, typename T, typename... Tail,
          FMT_ENABLE_IF(is_named_arg<T>::value)>
void init_named_args(named_arg_info<Char>* named_args, int arg_count,
                     int named_arg_count, const T& arg, const Tail&... args) {
  named_args[named_arg_count++] = {arg.name, arg_count};
  init_named_args(named_args, arg_count + 1, named_arg_count, args...);
}

template <typename... Args>
FMT_CONSTEXPR FMT_INLINE void init_named_args(std::nullptr_t, int, int,
                                              const Args&...) {}

template <bool B = false> constexpr auto count() -> size_t { return B ? 1 : 0; }
template <bool B1, bool B2, bool... Tail> constexpr auto count() -> size_t {
  return (B1 ? 1 : 0) + count<B2, Tail...>();
}

template <typename... Args> constexpr auto count_named_args() -> size_t {
  return count<is_named_arg<Args>::value...>();
}

template <typename... Args>
constexpr auto count_statically_named_args() -> size_t {
  return count<is_statically_named_arg<Args>::value...>();
}

struct unformattable {};
struct unformattable_char : unformattable {};
struct unformattable_pointer : unformattable {};

template <typename Char> struct string_value {
  const Char* data;
  size_t size;
};

template <typename Char> struct named_arg_value {
  const named_arg_info<Char>* data;
  size_t size;
};

template <typename Context> struct custom_value {
  using parse_context = typename Context::parse_context_type;
  void* value;
  void (*format)(void* arg, parse_context& parse_ctx, Context& ctx);
};

// A formatting argument value.
template <typename Context> class value {
 public:
  using char_type = typename Context::char_type;

  union {
    monostate no_value;
    int int_value;
    unsigned uint_value;
    long long long_long_value;
    unsigned long long ulong_long_value;
    int128_opt int128_value;
    uint128_opt uint128_value;
    bool bool_value;
    char_type char_value;
    float float_value;
    double double_value;
    long double long_double_value;
    const void* pointer;
    string_value<char_type> string;
    custom_value<Context> custom;
    named_arg_value<char_type> named_args;
  };

  constexpr FMT_INLINE value() : no_value() {}
  constexpr FMT_INLINE value(int val) : int_value(val) {}
  constexpr FMT_INLINE value(unsigned val) : uint_value(val) {}
  constexpr FMT_INLINE value(long long val) : long_long_value(val) {}
  constexpr FMT_INLINE value(unsigned long long val) : ulong_long_value(val) {}
  FMT_INLINE value(int128_opt val) : int128_value(val) {}
  FMT_INLINE value(uint128_opt val) : uint128_value(val) {}
  constexpr FMT_INLINE value(float val) : float_value(val) {}
  constexpr FMT_INLINE value(double val) : double_value(val) {}
  FMT_INLINE value(long double val) : long_double_value(val) {}
  constexpr FMT_INLINE value(bool val) : bool_value(val) {}
  constexpr FMT_INLINE value(char_type val) : char_value(val) {}
  FMT_CONSTEXPR FMT_INLINE value(const char_type* val) {
    string.data = val;
    if (is_constant_evaluated()) string.size = {};
  }
  FMT_CONSTEXPR FMT_INLINE value(basic_string_view<char_type> val) {
    string.data = val.data();
    string.size = val.size();
  }
  FMT_INLINE value(const void* val) : pointer(val) {}
  FMT_INLINE value(const named_arg_info<char_type>* args, size_t size)
      : named_args{args, size} {}

  template <typename T> FMT_CONSTEXPR20 FMT_INLINE value(T& val) {
    using value_type = remove_const_t<T>;
    custom.value = const_cast<value_type*>(std::addressof(val));
    // Get the formatter type through the context to allow different contexts
    // have different extension points, e.g. `formatter<T>` for `format` and
    // `printf_formatter<T>` for `printf`.
    custom.format = format_custom_arg<
        value_type, typename Context::template formatter_type<value_type>>;
  }
  value(unformattable);
  value(unformattable_char);
  value(unformattable_pointer);

 private:
  // Formats an argument of a custom type, such as a user-defined class.
  template <typename T, typename Formatter>
  static void format_custom_arg(void* arg,
                                typename Context::parse_context_type& parse_ctx,
                                Context& ctx) {
    auto f = Formatter();
    parse_ctx.advance_to(f.parse(parse_ctx));
    using qualified_type =
        conditional_t<has_const_formatter<T, Context>(), const T, T>;
    ctx.advance_to(f.format(*static_cast<qualified_type*>(arg), ctx));
  }
};

// To minimize the number of types we need to deal with, long is translated
// either to int or to long long depending on its size.
enum { long_short = sizeof(long) == sizeof(int) };
using long_type = conditional_t<long_short, int, long long>;
using ulong_type = conditional_t<long_short, unsigned, unsigned long long>;

template <typename T> struct format_as_result {
  template <typename U,
            FMT_ENABLE_IF(std::is_enum<U>::value || std::is_class<U>::value)>
  static auto map(U*) -> decltype(format_as(std::declval<U>()));
  static auto map(...) -> void;

  using type = decltype(map(static_cast<T*>(nullptr)));
};
template <typename T> using format_as_t = typename format_as_result<T>::type;

template <typename T>
struct has_format_as
    : bool_constant<!std::is_same<format_as_t<T>, void>::value> {};

// Maps formatting arguments to core types.
// arg_mapper reports errors by returning unformattable instead of using
// static_assert because it's used in the is_formattable trait.
template <typename Context> struct arg_mapper {
  using char_type = typename Context::char_type;

  FMT_CONSTEXPR FMT_INLINE auto map(signed char val) -> int { return val; }
  FMT_CONSTEXPR FMT_INLINE auto map(unsigned char val) -> unsigned {
    return val;
  }
  FMT_CONSTEXPR FMT_INLINE auto map(short val) -> int { return val; }
  FMT_CONSTEXPR FMT_INLINE auto map(unsigned short val) -> unsigned {
    return val;
  }
  FMT_CONSTEXPR FMT_INLINE auto map(int val) -> int { return val; }
  FMT_CONSTEXPR FMT_INLINE auto map(unsigned val) -> unsigned { return val; }
  FMT_CONSTEXPR FMT_INLINE auto map(long val) -> long_type { return val; }
  FMT_CONSTEXPR FMT_INLINE auto map(unsigned long val) -> ulong_type {
    return val;
  }
  FMT_CONSTEXPR FMT_INLINE auto map(long long val) -> long long { return val; }
  FMT_CONSTEXPR FMT_INLINE auto map(unsigned long long val)
      -> unsigned long long {
    return val;
  }
  FMT_CONSTEXPR FMT_INLINE auto map(int128_opt val) -> int128_opt {
    return val;
  }
  FMT_CONSTEXPR FMT_INLINE auto map(uint128_opt val) -> uint128_opt {
    return val;
  }
  FMT_CONSTEXPR FMT_INLINE auto map(bool val) -> bool { return val; }

  template <typename T, FMT_ENABLE_IF(std::is_same<T, char>::value ||
                                      std::is_same<T, char_type>::value)>
  FMT_CONSTEXPR FMT_INLINE auto map(T val) -> char_type {
    return val;
  }
  template <typename T, enable_if_t<(std::is_same<T, wchar_t>::value ||
#ifdef __cpp_char8_t
                                     std::is_same<T, char8_t>::value ||
#endif
                                     std::is_same<T, char16_t>::value ||
                                     std::is_same<T, char32_t>::value) &&
                                        !std::is_same<T, char_type>::value,
                                    int> = 0>
  FMT_CONSTEXPR FMT_INLINE auto map(T) -> unformattable_char {
    return {};
  }

  FMT_CONSTEXPR FMT_INLINE auto map(float val) -> float { return val; }
  FMT_CONSTEXPR FMT_INLINE auto map(double val) -> double { return val; }
  FMT_CONSTEXPR FMT_INLINE auto map(long double val) -> long double {
    return val;
  }

  FMT_CONSTEXPR FMT_INLINE auto map(char_type* val) -> const char_type* {
    return val;
  }
  FMT_CONSTEXPR FMT_INLINE auto map(const char_type* val) -> const char_type* {
    return val;
  }
  template <typename T,
            FMT_ENABLE_IF(is_string<T>::value && !std::is_pointer<T>::value &&
                          std::is_same<char_type, char_t<T>>::value)>
  FMT_CONSTEXPR FMT_INLINE auto map(const T& val)
      -> basic_string_view<char_type> {
    return to_string_view(val);
  }
  template <typename T,
            FMT_ENABLE_IF(is_string<T>::value && !std::is_pointer<T>::value &&
                          !std::is_same<char_type, char_t<T>>::value)>
  FMT_CONSTEXPR FMT_INLINE auto map(const T&) -> unformattable_char {
    return {};
  }

  FMT_CONSTEXPR FMT_INLINE auto map(void* val) -> const void* { return val; }
  FMT_CONSTEXPR FMT_INLINE auto map(const void* val) -> const void* {
    return val;
  }
  FMT_CONSTEXPR FMT_INLINE auto map(std::nullptr_t val) -> const void* {
    return val;
  }

  // Use SFINAE instead of a const T* parameter to avoid a conflict with the
  // array overload.
  template <
      typename T,
      FMT_ENABLE_IF(
          std::is_pointer<T>::value || std::is_member_pointer<T>::value ||
          std::is_function<typename std::remove_pointer<T>::type>::value ||
          (std::is_array<T>::value &&
           !std::is_convertible<T, const char_type*>::value))>
  FMT_CONSTEXPR auto map(const T&) -> unformattable_pointer {
    return {};
  }

  template <typename T, std::size_t N,
            FMT_ENABLE_IF(!std::is_same<T, wchar_t>::value)>
  FMT_CONSTEXPR FMT_INLINE auto map(const T (&values)[N]) -> const T (&)[N] {
    return values;
  }

  // Only map owning types because mapping views can be unsafe.
  template <typename T, typename U = format_as_t<T>,
            FMT_ENABLE_IF(std::is_arithmetic<U>::value)>
  FMT_CONSTEXPR FMT_INLINE auto map(const T& val) -> decltype(this->map(U())) {
    return map(format_as(val));
  }

  template <typename T, typename U = remove_const_t<T>>
  struct formattable : bool_constant<has_const_formatter<U, Context>() ||
                                     (has_formatter<U, Context>::value &&
                                      !std::is_const<T>::value)> {};

  template <typename T, FMT_ENABLE_IF(formattable<T>::value)>
  FMT_CONSTEXPR FMT_INLINE auto do_map(T& val) -> T& {
    return val;
  }
  template <typename T, FMT_ENABLE_IF(!formattable<T>::value)>
  FMT_CONSTEXPR FMT_INLINE auto do_map(T&) -> unformattable {
    return {};
  }

  template <typename T, typename U = remove_const_t<T>,
            FMT_ENABLE_IF((std::is_class<U>::value || std::is_enum<U>::value ||
                           std::is_union<U>::value) &&
                          !is_string<U>::value && !is_char<U>::value &&
                          !is_named_arg<U>::value &&
                          !std::is_arithmetic<format_as_t<U>>::value)>
  FMT_CONSTEXPR FMT_INLINE auto map(T& val) -> decltype(this->do_map(val)) {
    return do_map(val);
  }

  template <typename T, FMT_ENABLE_IF(is_named_arg<T>::value)>
  FMT_CONSTEXPR FMT_INLINE auto map(const T& named_arg)
      -> decltype(this->map(named_arg.value)) {
    return map(named_arg.value);
  }

  auto map(...) -> unformattable { return {}; }
};

// A type constant after applying arg_mapper<Context>.
template <typename T, typename Context>
using mapped_type_constant =
    type_constant<decltype(arg_mapper<Context>().map(std::declval<const T&>())),
                  typename Context::char_type>;

enum { packed_arg_bits = 4 };
// Maximum number of arguments with packed types.
enum { max_packed_args = 62 / packed_arg_bits };
enum : unsigned long long { is_unpacked_bit = 1ULL << 63 };
enum : unsigned long long { has_named_args_bit = 1ULL << 62 };

template <typename Char, typename InputIt>
auto copy_str(InputIt begin, InputIt end, appender out) -> appender {
  get_container(out).append(begin, end);
  return out;
}
template <typename Char, typename InputIt>
auto copy_str(InputIt begin, InputIt end,
              std::back_insert_iterator<std::string> out)
    -> std::back_insert_iterator<std::string> {
  get_container(out).append(begin, end);
  return out;
}

template <typename Char, typename R, typename OutputIt>
FMT_CONSTEXPR auto copy_str(R&& rng, OutputIt out) -> OutputIt {
  return detail::copy_str<Char>(rng.begin(), rng.end(), out);
}

#if FMT_GCC_VERSION && FMT_GCC_VERSION < 500
// A workaround for gcc 4.8 to make void_t work in a SFINAE context.
template <typename...> struct void_t_impl { using type = void; };
template <typename... T> using void_t = typename void_t_impl<T...>::type;
#else
template <typename...> using void_t = void;
#endif

template <typename It, typename T, typename Enable = void>
struct is_output_iterator : std::false_type {};

template <typename It, typename T>
struct is_output_iterator<
    It, T,
    void_t<typename std::iterator_traits<It>::iterator_category,
           decltype(*std::declval<It>() = std::declval<T>())>>
    : std::true_type {};

template <typename It> struct is_back_insert_iterator : std::false_type {};
template <typename Container>
struct is_back_insert_iterator<std::back_insert_iterator<Container>>
    : std::true_type {};

// A type-erased reference to an std::locale to avoid a heavy <locale> include.
class locale_ref {
 private:
  const void* locale_;  // A type-erased pointer to std::locale.

 public:
  constexpr FMT_INLINE locale_ref() : locale_(nullptr) {}
  template <typename Locale> explicit locale_ref(const Locale& loc);

  explicit operator bool() const noexcept { return locale_ != nullptr; }

  template <typename Locale> auto get() const -> Locale;
};

template <typename> constexpr auto encode_types() -> unsigned long long {
  return 0;
}

template <typename Context, typename Arg, typename... Args>
constexpr auto encode_types() -> unsigned long long {
  return static_cast<unsigned>(mapped_type_constant<Arg, Context>::value) |
         (encode_types<Context, Args...>() << packed_arg_bits);
}

#if defined(__cpp_if_constexpr)
// This type is intentionally undefined, only used for errors
template <typename T, typename Char> struct type_is_unformattable_for;
#endif

template <bool PACKED, typename Context, typename T, FMT_ENABLE_IF(PACKED)>
FMT_CONSTEXPR FMT_INLINE auto make_arg(T& val) -> value<Context> {
  using arg_type = remove_cvref_t<decltype(arg_mapper<Context>().map(val))>;

  constexpr bool formattable_char =
      !std::is_same<arg_type, unformattable_char>::value;
  static_assert(formattable_char, "Mixing character types is disallowed.");

  // Formatting of arbitrary pointers is disallowed. If you want to format a
  // pointer cast it to `void*` or `const void*`. In particular, this forbids
  // formatting of `[const] volatile char*` printed as bool by iostreams.
  constexpr bool formattable_pointer =
      !std::is_same<arg_type, unformattable_pointer>::value;
  static_assert(formattable_pointer,
                "Formatting of non-void pointers is disallowed.");

  constexpr bool formattable = !std::is_same<arg_type, unformattable>::value;
#if defined(__cpp_if_constexpr)
  if constexpr (!formattable) {
    type_is_unformattable_for<T, typename Context::char_type> _;
  }
#endif
  static_assert(
      formattable,
      "Cannot format an argument. To make type T formattable provide a "
      "formatter<T> specialization: https://fmt.dev/latest/api.html#udt");
  return {arg_mapper<Context>().map(val)};
}

template <typename Context, typename T>
FMT_CONSTEXPR auto make_arg(T& val) -> basic_format_arg<Context> {
  auto arg = basic_format_arg<Context>();
  arg.type_ = mapped_type_constant<T, Context>::value;
  arg.value_ = make_arg<true, Context>(val);
  return arg;
}

template <bool PACKED, typename Context, typename T, FMT_ENABLE_IF(!PACKED)>
FMT_CONSTEXPR inline auto make_arg(T& val) -> basic_format_arg<Context> {
  return make_arg<Context>(val);
}
}  // namespace detail
FMT_BEGIN_EXPORT

// A formatting argument. It is a trivially copyable/constructible type to
// allow storage in basic_memory_buffer.
template <typename Context> class basic_format_arg {
 private:
  detail::value<Context> value_;
  detail::type type_;

  template <typename ContextType, typename T>
  friend FMT_CONSTEXPR auto detail::make_arg(T& value)
      -> basic_format_arg<ContextType>;

  template <typename Visitor, typename Ctx>
  friend FMT_CONSTEXPR auto visit_format_arg(Visitor&& vis,
                                             const basic_format_arg<Ctx>& arg)
      -> decltype(vis(0));

  friend class basic_format_args<Context>;
  friend class dynamic_format_arg_store<Context>;

  using char_type = typename Context::char_type;

  template <typename T, typename Char, size_t NUM_ARGS, size_t NUM_NAMED_ARGS>
  friend struct detail::arg_data;

  basic_format_arg(const detail::named_arg_info<char_type>* args, size_t size)
      : value_(args, size) {}

 public:
  class handle {
   public:
    explicit handle(detail::custom_value<Context> custom) : custom_(custom) {}

    void format(typename Context::parse_context_type& parse_ctx,
                Context& ctx) const {
      custom_.format(custom_.value, parse_ctx, ctx);
    }

   private:
    detail::custom_value<Context> custom_;
  };

  constexpr basic_format_arg() : type_(detail::type::none_type) {}

  constexpr explicit operator bool() const noexcept {
    return type_ != detail::type::none_type;
  }

  auto type() const -> detail::type { return type_; }

  auto is_integral() const -> bool { return detail::is_integral_type(type_); }
  auto is_arithmetic() const -> bool {
    return detail::is_arithmetic_type(type_);
  }
};

/**
  \rst
  Visits an argument dispatching to the appropriate visit method based on
  the argument type. For example, if the argument type is ``double`` then
  ``vis(value)`` will be called with the value of type ``double``.
  \endrst
 */
// DEPRECATED!
template <typename Visitor, typename Context>
FMT_CONSTEXPR FMT_INLINE auto visit_format_arg(
    Visitor&& vis, const basic_format_arg<Context>& arg) -> decltype(vis(0)) {
  switch (arg.type_) {
  case detail::type::none_type:
    break;
  case detail::type::int_type:
    return vis(arg.value_.int_value);
  case detail::type::uint_type:
    return vis(arg.value_.uint_value);
  case detail::type::long_long_type:
    return vis(arg.value_.long_long_value);
  case detail::type::ulong_long_type:
    return vis(arg.value_.ulong_long_value);
  case detail::type::int128_type:
    return vis(detail::convert_for_visit(arg.value_.int128_value));
  case detail::type::uint128_type:
    return vis(detail::convert_for_visit(arg.value_.uint128_value));
  case detail::type::bool_type:
    return vis(arg.value_.bool_value);
  case detail::type::char_type:
    return vis(arg.value_.char_value);
  case detail::type::float_type:
    return vis(arg.value_.float_value);
  case detail::type::double_type:
    return vis(arg.value_.double_value);
  case detail::type::long_double_type:
    return vis(arg.value_.long_double_value);
  case detail::type::cstring_type:
    return vis(arg.value_.string.data);
  case detail::type::string_type:
    using sv = basic_string_view<typename Context::char_type>;
    return vis(sv(arg.value_.string.data, arg.value_.string.size));
  case detail::type::pointer_type:
    return vis(arg.value_.pointer);
  case detail::type::custom_type:
    return vis(typename basic_format_arg<Context>::handle(arg.value_.custom));
  }
  return vis(monostate());
}

// Formatting context.
template <typename OutputIt, typename Char> class basic_format_context {
 private:
  OutputIt out_;
  basic_format_args<basic_format_context> args_;
  detail::locale_ref loc_;

 public:
  using iterator = OutputIt;
  using format_arg = basic_format_arg<basic_format_context>;
  using format_args = basic_format_args<basic_format_context>;
  using parse_context_type = basic_format_parse_context<Char>;
  template <typename T> using formatter_type = formatter<T, Char>;

  /** The character type for the output. */
  using char_type = Char;

  basic_format_context(basic_format_context&&) = default;
  basic_format_context(const basic_format_context&) = delete;
  void operator=(const basic_format_context&) = delete;
  /**
    Constructs a ``basic_format_context`` object. References to the arguments
    are stored in the object so make sure they have appropriate lifetimes.
   */
  constexpr basic_format_context(OutputIt out, format_args ctx_args,
                                 detail::locale_ref loc = {})
      : out_(out), args_(ctx_args), loc_(loc) {}

  constexpr auto arg(int id) const -> format_arg { return args_.get(id); }
  FMT_CONSTEXPR auto arg(basic_string_view<Char> name) -> format_arg {
    return args_.get(name);
  }
  FMT_CONSTEXPR auto arg_id(basic_string_view<Char> name) -> int {
    return args_.get_id(name);
  }
  auto args() const -> const format_args& { return args_; }

  FMT_CONSTEXPR auto error_handler() -> detail::error_handler { return {}; }
  void on_error(const char* message) { error_handler().on_error(message); }

  // Returns an iterator to the beginning of the output range.
  FMT_CONSTEXPR auto out() -> iterator { return out_; }

  // Advances the begin iterator to ``it``.
  void advance_to(iterator it) {
    if (!detail::is_back_insert_iterator<iterator>()) out_ = it;
  }

  FMT_CONSTEXPR auto locale() -> detail::locale_ref { return loc_; }
};

template <typename Char>
using buffer_context =
    basic_format_context<detail::buffer_appender<Char>, Char>;
using format_context = buffer_context<char>;

template <typename T, typename Char = char>
using is_formattable = bool_constant<!std::is_base_of<
    detail::unformattable, decltype(detail::arg_mapper<buffer_context<Char>>()
                                        .map(std::declval<T&>()))>::value>;

/**
  \rst
  An array of references to arguments. It can be implicitly converted into
  `~fmt::basic_format_args` for passing into type-erased formatting functions
  such as `~fmt::vformat`.
  \endrst
 */
template <typename Context, typename... Args>
class format_arg_store
#if FMT_GCC_VERSION && FMT_GCC_VERSION < 409
    // Workaround a GCC template argument substitution bug.
    : public basic_format_args<Context>
#endif
{
 private:
  static const size_t num_args = sizeof...(Args);
  static constexpr size_t num_named_args = detail::count_named_args<Args...>();
  static const bool is_packed = num_args <= detail::max_packed_args;

  using value_type = conditional_t<is_packed, detail::value<Context>,
                                   basic_format_arg<Context>>;

  detail::arg_data<value_type, typename Context::char_type, num_args,
                   num_named_args>
      data_;

  friend class basic_format_args<Context>;

  static constexpr unsigned long long desc =
      (is_packed ? detail::encode_types<Context, Args...>()
                 : detail::is_unpacked_bit | num_args) |
      (num_named_args != 0
           ? static_cast<unsigned long long>(detail::has_named_args_bit)
           : 0);

 public:
  template <typename... T>
  FMT_CONSTEXPR FMT_INLINE format_arg_store(T&... args)
      :
#if FMT_GCC_VERSION && FMT_GCC_VERSION < 409
        basic_format_args<Context>(*this),
#endif
        data_{detail::make_arg<is_packed, Context>(args)...} {
    if (detail::const_check(num_named_args != 0))
      detail::init_named_args(data_.named_args(), 0, 0, args...);
  }
};

/**
  \rst
  Constructs a `~fmt::format_arg_store` object that contains references to
  arguments and can be implicitly converted to `~fmt::format_args`. `Context`
  can be omitted in which case it defaults to `~fmt::format_context`.
  See `~fmt::arg` for lifetime considerations.
  \endrst
 */
// Arguments are taken by lvalue references to avoid some lifetime issues.
template <typename Context = format_context, typename... T>
constexpr auto make_format_args(T&... args)
    -> format_arg_store<Context, remove_cvref_t<T>...> {
  return {args...};
}

/**
  \rst
  Returns a named argument to be used in a formatting function.
  It should only be used in a call to a formatting function or
  `dynamic_format_arg_store::push_back`.

  **Example**::

    fmt::print("Elapsed time: {s:.2f} seconds", fmt::arg("s", 1.23));
  \endrst
 */
template <typename Char, typename T>
inline auto arg(const Char* name, const T& arg) -> detail::named_arg<Char, T> {
  static_assert(!detail::is_named_arg<T>(), "nested named arguments");
  return {name, arg};
}
FMT_END_EXPORT

/**
  \rst
  A view of a collection of formatting arguments. To avoid lifetime issues it
  should only be used as a parameter type in type-erased functions such as
  ``vformat``::

    void vlog(string_view format_str, format_args args);  // OK
    format_args args = make_format_args();  // Error: dangling reference
  \endrst
 */
template <typename Context> class basic_format_args {
 public:
  using size_type = int;
  using format_arg = basic_format_arg<Context>;

 private:
  // A descriptor that contains information about formatting arguments.
  // If the number of arguments is less or equal to max_packed_args then
  // argument types are passed in the descriptor. This reduces binary code size
  // per formatting function call.
  unsigned long long desc_;
  union {
    // If is_packed() returns true then argument values are stored in values_;
    // otherwise they are stored in args_. This is done to improve cache
    // locality and reduce compiled code size since storing larger objects
    // may require more code (at least on x86-64) even if the same amount of
    // data is actually copied to stack. It saves ~10% on the bloat test.
    const detail::value<Context>* values_;
    const format_arg* args_;
  };

  constexpr auto is_packed() const -> bool {
    return (desc_ & detail::is_unpacked_bit) == 0;
  }
  auto has_named_args() const -> bool {
    return (desc_ & detail::has_named_args_bit) != 0;
  }

  FMT_CONSTEXPR auto type(int index) const -> detail::type {
    int shift = index * detail::packed_arg_bits;
    unsigned int mask = (1 << detail::packed_arg_bits) - 1;
    return static_cast<detail::type>((desc_ >> shift) & mask);
  }

  constexpr FMT_INLINE basic_format_args(unsigned long long desc,
                                         const detail::value<Context>* values)
      : desc_(desc), values_(values) {}
  constexpr basic_format_args(unsigned long long desc, const format_arg* args)
      : desc_(desc), args_(args) {}

 public:
  constexpr basic_format_args() : desc_(0), args_(nullptr) {}

  /**
   \rst
   Constructs a `basic_format_args` object from `~fmt::format_arg_store`.
   \endrst
   */
  template <typename... Args>
  constexpr FMT_INLINE basic_format_args(
      const format_arg_store<Context, Args...>& store)
      : basic_format_args(format_arg_store<Context, Args...>::desc,
                          store.data_.args()) {}

  /**
   \rst
   Constructs a `basic_format_args` object from
   `~fmt::dynamic_format_arg_store`.
   \endrst
   */
  constexpr FMT_INLINE basic_format_args(
      const dynamic_format_arg_store<Context>& store)
      : basic_format_args(store.get_types(), store.data()) {}

  /**
   \rst
   Constructs a `basic_format_args` object from a dynamic set of arguments.
   \endrst
   */
  constexpr basic_format_args(const format_arg* args, int count)
      : basic_format_args(detail::is_unpacked_bit | detail::to_unsigned(count),
                          args) {}

  /** Returns the argument with the specified id. */
  FMT_CONSTEXPR auto get(int id) const -> format_arg {
    format_arg arg;
    if (!is_packed()) {
      if (id < max_size()) arg = args_[id];
      return arg;
    }
    if (id >= detail::max_packed_args) return arg;
    arg.type_ = type(id);
    if (arg.type_ == detail::type::none_type) return arg;
    arg.value_ = values_[id];
    return arg;
  }

  template <typename Char>
  auto get(basic_string_view<Char> name) const -> format_arg {
    int id = get_id(name);
    return id >= 0 ? get(id) : format_arg();
  }

  template <typename Char>
  auto get_id(basic_string_view<Char> name) const -> int {
    if (!has_named_args()) return -1;
    const auto& named_args =
        (is_packed() ? values_[-1] : args_[-1].value_).named_args;
    for (size_t i = 0; i < named_args.size; ++i) {
      if (named_args.data[i].name == name) return named_args.data[i].id;
    }
    return -1;
  }

  auto max_size() const -> int {
    unsigned long long max_packed = detail::max_packed_args;
    return static_cast<int>(is_packed() ? max_packed
                                        : desc_ & ~detail::is_unpacked_bit);
  }
};

/** An alias to ``basic_format_args<format_context>``. */
// A separate type would result in shorter symbols but break ABI compatibility
// between clang and gcc on ARM (#1919).
FMT_EXPORT using format_args = basic_format_args<format_context>;

// We cannot use enum classes as bit fields because of a gcc bug, so we put them
// in namespaces instead (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61414).
// Additionally, if an underlying type is specified, older gcc incorrectly warns
// that the type is too small. Both bugs are fixed in gcc 9.3.
#if FMT_GCC_VERSION && FMT_GCC_VERSION < 903
#  define FMT_ENUM_UNDERLYING_TYPE(type)
#else
#  define FMT_ENUM_UNDERLYING_TYPE(type) : type
#endif
namespace align {
enum type FMT_ENUM_UNDERLYING_TYPE(unsigned char){none, left, right, center,
                                                  numeric};
}
using align_t = align::type;
namespace sign {
enum type FMT_ENUM_UNDERLYING_TYPE(unsigned char){none, minus, plus, space};
}
using sign_t = sign::type;

namespace detail {

// Workaround an array initialization issue in gcc 4.8.
template <typename Char> struct fill_t {
 private:
  enum { max_size = 4 };
  Char data_[max_size] = {Char(' '), Char(0), Char(0), Char(0)};
  unsigned char size_ = 1;

 public:
  FMT_CONSTEXPR void operator=(basic_string_view<Char> s) {
    auto size = s.size();
    FMT_ASSERT(size <= max_size, "invalid fill");
    for (size_t i = 0; i < size; ++i) data_[i] = s[i];
    size_ = static_cast<unsigned char>(size);
  }

  constexpr auto size() const -> size_t { return size_; }
  constexpr auto data() const -> const Char* { return data_; }

  FMT_CONSTEXPR auto operator[](size_t index) -> Char& { return data_[index]; }
  FMT_CONSTEXPR auto operator[](size_t index) const -> const Char& {
    return data_[index];
  }
};
}  // namespace detail

enum class presentation_type : unsigned char {
  none,
  dec,             // 'd'
  oct,             // 'o'
  hex_lower,       // 'x'
  hex_upper,       // 'X'
  bin_lower,       // 'b'
  bin_upper,       // 'B'
  hexfloat_lower,  // 'a'
  hexfloat_upper,  // 'A'
  exp_lower,       // 'e'
  exp_upper,       // 'E'
  fixed_lower,     // 'f'
  fixed_upper,     // 'F'
  general_lower,   // 'g'
  general_upper,   // 'G'
  chr,             // 'c'
  string,          // 's'
  pointer,         // 'p'
  debug            // '?'
};

// Format specifiers for built-in and string types.
template <typename Char = char> struct format_specs {
  int width;
  int precision;
  presentation_type type;
  align_t align : 4;
  sign_t sign : 3;
  bool alt : 1;  // Alternate form ('#').
  bool localized : 1;
  detail::fill_t<Char> fill;

  constexpr format_specs()
      : width(0),
        precision(-1),
        type(presentation_type::none),
        align(align::none),
        sign(sign::none),
        alt(false),
        localized(false) {}
};

namespace detail {

enum class arg_id_kind { none, index, name };

// An argument reference.
template <typename Char> struct arg_ref {
  FMT_CONSTEXPR arg_ref() : kind(arg_id_kind::none), val() {}

  FMT_CONSTEXPR explicit arg_ref(int index)
      : kind(arg_id_kind::index), val(index) {}
  FMT_CONSTEXPR explicit arg_ref(basic_string_view<Char> name)
      : kind(arg_id_kind::name), val(name) {}

  FMT_CONSTEXPR auto operator=(int idx) -> arg_ref& {
    kind = arg_id_kind::index;
    val.index = idx;
    return *this;
  }

  arg_id_kind kind;
  union value {
    FMT_CONSTEXPR value(int idx = 0) : index(idx) {}
    FMT_CONSTEXPR value(basic_string_view<Char> n) : name(n) {}

    int index;
    basic_string_view<Char> name;
  } val;
};

// Format specifiers with width and precision resolved at formatting rather
// than parsing time to allow reusing the same parsed specifiers with
// different sets of arguments (precompilation of format strings).
template <typename Char = char>
struct dynamic_format_specs : format_specs<Char> {
  arg_ref<Char> width_ref;
  arg_ref<Char> precision_ref;
};

// Converts a character to ASCII. Returns '\0' on conversion failure.
template <typename Char, FMT_ENABLE_IF(std::is_integral<Char>::value)>
constexpr auto to_ascii(Char c) -> char {
  return c <= 0xff ? static_cast<char>(c) : '\0';
}
template <typename Char, FMT_ENABLE_IF(std::is_enum<Char>::value)>
constexpr auto to_ascii(Char c) -> char {
  return c <= 0xff ? static_cast<char>(c) : '\0';
}

// Returns the number of code units in a code point or 1 on error.
template <typename Char>
FMT_CONSTEXPR auto code_point_length(const Char* begin) -> int {
  if (const_check(sizeof(Char) != 1)) return 1;
  auto c = static_cast<unsigned char>(*begin);
  return static_cast<int>((0x3a55000000000000ull >> (2 * (c >> 3))) & 0x3) + 1;
}

// Return the result via the out param to workaround gcc bug 77539.
template <bool IS_CONSTEXPR, typename T, typename Ptr = const T*>
FMT_CONSTEXPR auto find(Ptr first, Ptr last, T value, Ptr& out) -> bool {
  for (out = first; out != last; ++out) {
    if (*out == value) return true;
  }
  return false;
}

template <>
inline auto find<false, char>(const char* first, const char* last, char value,
                              const char*& out) -> bool {
  out = static_cast<const char*>(
      std::memchr(first, value, to_unsigned(last - first)));
  return out != nullptr;
}

// Parses the range [begin, end) as an unsigned integer. This function assumes
// that the range is non-empty and the first character is a digit.
template <typename Char>
FMT_CONSTEXPR auto parse_nonnegative_int(const Char*& begin, const Char* end,
                                         int error_value) noexcept -> int {
  FMT_ASSERT(begin != end && '0' <= *begin && *begin <= '9', "");
  unsigned value = 0, prev = 0;
  auto p = begin;
  do {
    prev = value;
    value = value * 10 + unsigned(*p - '0');
    ++p;
  } while (p != end && '0' <= *p && *p <= '9');
  auto num_digits = p - begin;
  begin = p;
  if (num_digits <= std::numeric_limits<int>::digits10)
    return static_cast<int>(value);
  // Check for overflow.
  const unsigned max = to_unsigned((std::numeric_limits<int>::max)());
  return num_digits == std::numeric_limits<int>::digits10 + 1 &&
                 prev * 10ull + unsigned(p[-1] - '0') <= max
             ? static_cast<int>(value)
             : error_value;
}

FMT_CONSTEXPR inline auto parse_align(char c) -> align_t {
  switch (c) {
  case '<':
    return align::left;
  case '>':
    return align::right;
  case '^':
    return align::center;
  }
  return align::none;
}

template <typename Char> constexpr auto is_name_start(Char c) -> bool {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

template <typename Char, typename Handler>
FMT_CONSTEXPR auto do_parse_arg_id(const Char* begin, const Char* end,
                                   Handler&& handler) -> const Char* {
  Char c = *begin;
  if (c >= '0' && c <= '9') {
    int index = 0;
    constexpr int max = (std::numeric_limits<int>::max)();
    if (c != '0')
      index = parse_nonnegative_int(begin, end, max);
    else
      ++begin;
    if (begin == end || (*begin != '}' && *begin != ':'))
      throw_format_error("invalid format string");
    else
      handler.on_index(index);
    return begin;
  }
  if (!is_name_start(c)) {
    throw_format_error("invalid format string");
    return begin;
  }
  auto it = begin;
  do {
    ++it;
  } while (it != end && (is_name_start(*it) || ('0' <= *it && *it <= '9')));
  handler.on_name({begin, to_unsigned(it - begin)});
  return it;
}

template <typename Char, typename Handler>
FMT_CONSTEXPR FMT_INLINE auto parse_arg_id(const Char* begin, const Char* end,
                                           Handler&& handler) -> const Char* {
  FMT_ASSERT(begin != end, "");
  Char c = *begin;
  if (c != '}' && c != ':') return do_parse_arg_id(begin, end, handler);
  handler.on_auto();
  return begin;
}

template <typename Char> struct dynamic_spec_id_handler {
  basic_format_parse_context<Char>& ctx;
  arg_ref<Char>& ref;

  FMT_CONSTEXPR void on_auto() {
    int id = ctx.next_arg_id();
    ref = arg_ref<Char>(id);
    ctx.check_dynamic_spec(id);
  }
  FMT_CONSTEXPR void on_index(int id) {
    ref = arg_ref<Char>(id);
    ctx.check_arg_id(id);
    ctx.check_dynamic_spec(id);
  }
  FMT_CONSTEXPR void on_name(basic_string_view<Char> id) {
    ref = arg_ref<Char>(id);
    ctx.check_arg_id(id);
  }
};

// Parses [integer | "{" [arg_id] "}"].
template <typename Char>
FMT_CONSTEXPR auto parse_dynamic_spec(const Char* begin, const Char* end,
                                      int& value, arg_ref<Char>& ref,
                                      basic_format_parse_context<Char>& ctx)
    -> const Char* {
  FMT_ASSERT(begin != end, "");
  if ('0' <= *begin && *begin <= '9') {
    int val = parse_nonnegative_int(begin, end, -1);
    if (val != -1)
      value = val;
    else
      throw_format_error("number is too big");
  } else if (*begin == '{') {
    ++begin;
    auto handler = dynamic_spec_id_handler<Char>{ctx, ref};
    if (begin != end) begin = parse_arg_id(begin, end, handler);
    if (begin != end && *begin == '}') return ++begin;
    throw_format_error("invalid format string");
  }
  return begin;
}

template <typename Char>
FMT_CONSTEXPR auto parse_precision(const Char* begin, const Char* end,
                                   int& value, arg_ref<Char>& ref,
                                   basic_format_parse_context<Char>& ctx)
    -> const Char* {
  ++begin;
  if (begin == end || *begin == '}') {
    throw_format_error("invalid precision");
    return begin;
  }
  return parse_dynamic_spec(begin, end, value, ref, ctx);
}

enum class state { start, align, sign, hash, zero, width, precision, locale };

// Parses standard format specifiers.
template <typename Char>
FMT_CONSTEXPR FMT_INLINE auto parse_format_specs(
    const Char* begin, const Char* end, dynamic_format_specs<Char>& specs,
    basic_format_parse_context<Char>& ctx, type arg_type) -> const Char* {
  auto c = '\0';
  if (end - begin > 1) {
    auto next = to_ascii(begin[1]);
    c = parse_align(next) == align::none ? to_ascii(*begin) : '\0';
  } else {
    if (begin == end) return begin;
    c = to_ascii(*begin);
  }

  struct {
    state current_state = state::start;
    FMT_CONSTEXPR void operator()(state s, bool valid = true) {
      if (current_state >= s || !valid)
        throw_format_error("invalid format specifier");
      current_state = s;
    }
  } enter_state;

  using pres = presentation_type;
  constexpr auto integral_set = sint_set | uint_set | bool_set | char_set;
  struct {
    const Char*& begin;
    dynamic_format_specs<Char>& specs;
    type arg_type;

    FMT_CONSTEXPR auto operator()(pres type, int set) -> const Char* {
      if (!in(arg_type, set)) throw_format_error("invalid format specifier");
      specs.type = type;
      return begin + 1;
    }
  } parse_presentation_type{begin, specs, arg_type};

  for (;;) {
    switch (c) {
    case '<':
    case '>':
    case '^':
      enter_state(state::align);
      specs.align = parse_align(c);
      ++begin;
      break;
    case '+':
    case '-':
    case ' ':
      enter_state(state::sign, in(arg_type, sint_set | float_set));
      switch (c) {
      case '+':
        specs.sign = sign::plus;
        break;
      case '-':
        specs.sign = sign::minus;
        break;
      case ' ':
        specs.sign = sign::space;
        break;
      }
      ++begin;
      break;
    case '#':
      enter_state(state::hash, is_arithmetic_type(arg_type));
      specs.alt = true;
      ++begin;
      break;
    case '0':
      enter_state(state::zero);
      if (!is_arithmetic_type(arg_type))
        throw_format_error("format specifier requires numeric argument");
      if (specs.align == align::none) {
        // Ignore 0 if align is specified for compatibility with std::format.
        specs.align = align::numeric;
        specs.fill[0] = Char('0');
      }
      ++begin;
      break;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '{':
      enter_state(state::width);
      begin = parse_dynamic_spec(begin, end, specs.width, specs.width_ref, ctx);
      break;
    case '.':
      enter_state(state::precision,
                  in(arg_type, float_set | string_set | cstring_set));
      begin = parse_precision(begin, end, specs.precision, specs.precision_ref,
                              ctx);
      break;
    case 'L':
      enter_state(state::locale, is_arithmetic_type(arg_type));
      specs.localized = true;
      ++begin;
      break;
    case 'd':
      return parse_presentation_type(pres::dec, integral_set);
    case 'o':
      return parse_presentation_type(pres::oct, integral_set);
    case 'x':
      return parse_presentation_type(pres::hex_lower, integral_set);
    case 'X':
      return parse_presentation_type(pres::hex_upper, integral_set);
    case 'b':
      return parse_presentation_type(pres::bin_lower, integral_set);
    case 'B':
      return parse_presentation_type(pres::bin_upper, integral_set);
    case 'a':
      return parse_presentation_type(pres::hexfloat_lower, float_set);
    case 'A':
      return parse_presentation_type(pres::hexfloat_upper, float_set);
    case 'e':
      return parse_presentation_type(pres::exp_lower, float_set);
    case 'E':
      return parse_presentation_type(pres::exp_upper, float_set);
    case 'f':
      return parse_presentation_type(pres::fixed_lower, float_set);
    case 'F':
      return parse_presentation_type(pres::fixed_upper, float_set);
    case 'g':
      return parse_presentation_type(pres::general_lower, float_set);
    case 'G':
      return parse_presentation_type(pres::general_upper, float_set);
    case 'c':
      return parse_presentation_type(pres::chr, integral_set);
    case 's':
      return parse_presentation_type(pres::string,
                                     bool_set | string_set | cstring_set);
    case 'p':
      return parse_presentation_type(pres::pointer, pointer_set | cstring_set);
    case '?':
      return parse_presentation_type(pres::debug,
                                     char_set | string_set | cstring_set);
    case '}':
      return begin;
    default: {
      if (*begin == '}') return begin;
      // Parse fill and alignment.
      auto fill_end = begin + code_point_length(begin);
      if (end - fill_end <= 0) {
        throw_format_error("invalid format specifier");
        return begin;
      }
      if (*begin == '{') {
        throw_format_error("invalid fill character '{'");
        return begin;
      }
      auto align = parse_align(to_ascii(*fill_end));
      enter_state(state::align, align != align::none);
      specs.fill = {begin, to_unsigned(fill_end - begin)};
      specs.align = align;
      begin = fill_end + 1;
    }
    }
    if (begin == end) return begin;
    c = to_ascii(*begin);
  }
}

template <typename Char, typename Handler>
FMT_CONSTEXPR auto parse_replacement_field(const Char* begin, const Char* end,
                                           Handler&& handler) -> const Char* {
  struct id_adapter {
    Handler& handler;
    int arg_id;

    FMT_CONSTEXPR void on_auto() { arg_id = handler.on_arg_id(); }
    FMT_CONSTEXPR void on_index(int id) { arg_id = handler.on_arg_id(id); }
    FMT_CONSTEXPR void on_name(basic_string_view<Char> id) {
      arg_id = handler.on_arg_id(id);
    }
  };

  ++begin;
  if (begin == end) return handler.on_error("invalid format string"), end;
  if (*begin == '}') {
    handler.on_replacement_field(handler.on_arg_id(), begin);
  } else if (*begin == '{') {
    handler.on_text(begin, begin + 1);
  } else {
    auto adapter = id_adapter{handler, 0};
    begin = parse_arg_id(begin, end, adapter);
    Char c = begin != end ? *begin : Char();
    if (c == '}') {
      handler.on_replacement_field(adapter.arg_id, begin);
    } else if (c == ':') {
      begin = handler.on_format_specs(adapter.arg_id, begin + 1, end);
      if (begin == end || *begin != '}')
        return handler.on_error("unknown format specifier"), end;
    } else {
      return handler.on_error("missing '}' in format string"), end;
    }
  }
  return begin + 1;
}

template <bool IS_CONSTEXPR, typename Char, typename Handler>
FMT_CONSTEXPR FMT_INLINE void parse_format_string(
    basic_string_view<Char> format_str, Handler&& handler) {
  auto begin = format_str.data();
  auto end = begin + format_str.size();
  if (end - begin < 32) {
    // Use a simple loop instead of memchr for small strings.
    const Char* p = begin;
    while (p != end) {
      auto c = *p++;
      if (c == '{') {
        handler.on_text(begin, p - 1);
        begin = p = parse_replacement_field(p - 1, end, handler);
      } else if (c == '}') {
        if (p == end || *p != '}')
          return handler.on_error("unmatched '}' in format string");
        handler.on_text(begin, p);
        begin = ++p;
      }
    }
    handler.on_text(begin, end);
    return;
  }
  struct writer {
    FMT_CONSTEXPR void operator()(const Char* from, const Char* to) {
      if (from == to) return;
      for (;;) {
        const Char* p = nullptr;
        if (!find<IS_CONSTEXPR>(from, to, Char('}'), p))
          return handler_.on_text(from, to);
        ++p;
        if (p == to || *p != '}')
          return handler_.on_error("unmatched '}' in format string");
        handler_.on_text(from, p);
        from = p + 1;
      }
    }
    Handler& handler_;
  } write = {handler};
  while (begin != end) {
    // Doing two passes with memchr (one for '{' and another for '}') is up to
    // 2.5x faster than the naive one-pass implementation on big format strings.
    const Char* p = begin;
    if (*begin != '{' && !find<IS_CONSTEXPR>(begin + 1, end, Char('{'), p))
      return write(begin, end);
    write(begin, p);
    begin = parse_replacement_field(p, end, handler);
  }
}

template <typename T, bool = is_named_arg<T>::value> struct strip_named_arg {
  using type = T;
};
template <typename T> struct strip_named_arg<T, true> {
  using type = remove_cvref_t<decltype(T::value)>;
};

template <typename T, typename ParseContext>
FMT_CONSTEXPR auto parse_format_specs(ParseContext& ctx)
    -> decltype(ctx.begin()) {
  using char_type = typename ParseContext::char_type;
  using context = buffer_context<char_type>;
  using mapped_type = conditional_t<
      mapped_type_constant<T, context>::value != type::custom_type,
      decltype(arg_mapper<context>().map(std::declval<const T&>())),
      typename strip_named_arg<T>::type>;
#if defined(__cpp_if_constexpr)
  if constexpr (std::is_default_constructible_v<
                    formatter<mapped_type, char_type>>) {
    return formatter<mapped_type, char_type>().parse(ctx);
  } else {
    type_is_unformattable_for<T, char_type> _;
    return ctx.begin();
  }
#else
  return formatter<mapped_type, char_type>().parse(ctx);
#endif
}

// Checks char specs and returns true iff the presentation type is char-like.
template <typename Char>
FMT_CONSTEXPR auto check_char_specs(const format_specs<Char>& specs) -> bool {
  if (specs.type != presentation_type::none &&
      specs.type != presentation_type::chr &&
      specs.type != presentation_type::debug) {
    return false;
  }
  if (specs.align == align::numeric || specs.sign != sign::none || specs.alt)
    throw_format_error("invalid format specifier for char");
  return true;
}

#if FMT_USE_NONTYPE_TEMPLATE_ARGS
template <int N, typename T, typename... Args, typename Char>
constexpr auto get_arg_index_by_name(basic_string_view<Char> name) -> int {
  if constexpr (is_statically_named_arg<T>()) {
    if (name == T::name) return N;
  }
  if constexpr (sizeof...(Args) > 0)
    return get_arg_index_by_name<N + 1, Args...>(name);
  (void)name;  // Workaround an MSVC bug about "unused" parameter.
  return -1;
}
#endif

template <typename... Args, typename Char>
FMT_CONSTEXPR auto get_arg_index_by_name(basic_string_view<Char> name) -> int {
#if FMT_USE_NONTYPE_TEMPLATE_ARGS
  if constexpr (sizeof...(Args) > 0)
    return get_arg_index_by_name<0, Args...>(name);
#endif
  (void)name;
  return -1;
}

template <typename Char, typename... Args> class format_string_checker {
 private:
  using parse_context_type = compile_parse_context<Char>;
  static constexpr int num_args = sizeof...(Args);

  // Format specifier parsing function.
  // In the future basic_format_parse_context will replace compile_parse_context
  // here and will use is_constant_evaluated and downcasting to access the data
  // needed for compile-time checks: https://godbolt.org/z/GvWzcTjh1.
  using parse_func = const Char* (*)(parse_context_type&);

  type types_[num_args > 0 ? static_cast<size_t>(num_args) : 1];
  parse_context_type context_;
  parse_func parse_funcs_[num_args > 0 ? static_cast<size_t>(num_args) : 1];

 public:
  explicit FMT_CONSTEXPR format_string_checker(basic_string_view<Char> fmt)
      : types_{mapped_type_constant<Args, buffer_context<Char>>::value...},
        context_(fmt, num_args, types_),
        parse_funcs_{&parse_format_specs<Args, parse_context_type>...} {}

  FMT_CONSTEXPR void on_text(const Char*, const Char*) {}

  FMT_CONSTEXPR auto on_arg_id() -> int { return context_.next_arg_id(); }
  FMT_CONSTEXPR auto on_arg_id(int id) -> int {
    return context_.check_arg_id(id), id;
  }
  FMT_CONSTEXPR auto on_arg_id(basic_string_view<Char> id) -> int {
#if FMT_USE_NONTYPE_TEMPLATE_ARGS
    auto index = get_arg_index_by_name<Args...>(id);
    if (index < 0) on_error("named argument is not found");
    return index;
#else
    (void)id;
    on_error("compile-time checks for named arguments require C++20 support");
    return 0;
#endif
  }

  FMT_CONSTEXPR void on_replacement_field(int id, const Char* begin) {
    on_format_specs(id, begin, begin);  // Call parse() on empty specs.
  }

  FMT_CONSTEXPR auto on_format_specs(int id, const Char* begin, const Char*)
      -> const Char* {
    context_.advance_to(begin);
    // id >= 0 check is a workaround for gcc 10 bug (#2065).
    return id >= 0 && id < num_args ? parse_funcs_[id](context_) : begin;
  }

  FMT_CONSTEXPR void on_error(const char* message) {
    throw_format_error(message);
  }
};

// Reports a compile-time error if S is not a valid format string.
template <typename..., typename S, FMT_ENABLE_IF(!is_compile_string<S>::value)>
FMT_INLINE void check_format_string(const S&) {
#ifdef FMT_ENFORCE_COMPILE_STRING
  static_assert(is_compile_string<S>::value,
                "FMT_ENFORCE_COMPILE_STRING requires all format strings to use "
                "FMT_STRING.");
#endif
}
template <typename... Args, typename S,
          FMT_ENABLE_IF(is_compile_string<S>::value)>
void check_format_string(S format_str) {
  using char_t = typename S::char_type;
  FMT_CONSTEXPR auto s = basic_string_view<char_t>(format_str);
  using checker = format_string_checker<char_t, remove_cvref_t<Args>...>;
  FMT_CONSTEXPR bool error = (parse_format_string<true>(s, checker(s)), true);
  ignore_unused(error);
}

template <typename Char = char> struct vformat_args {
  using type = basic_format_args<
      basic_format_context<std::back_insert_iterator<buffer<Char>>, Char>>;
};
template <> struct vformat_args<char> { using type = format_args; };

// Use vformat_args and avoid type_identity to keep symbols short.
template <typename Char>
void vformat_to(buffer<Char>& buf, basic_string_view<Char> fmt,
                typename vformat_args<Char>::type args, locale_ref loc = {});

FMT_API void vprint_mojibake(std::FILE*, string_view, format_args);
#ifndef _WIN32
inline void vprint_mojibake(std::FILE*, string_view, format_args) {}
#endif
}  // namespace detail

FMT_BEGIN_EXPORT

// A formatter specialization for natively supported types.
template <typename T, typename Char>
struct formatter<T, Char,
                 enable_if_t<detail::type_constant<T, Char>::value !=
                             detail::type::custom_type>> {
 private:
  detail::dynamic_format_specs<Char> specs_;

 public:
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> const Char* {
    auto type = detail::type_constant<T, Char>::value;
    auto end =
        detail::parse_format_specs(ctx.begin(), ctx.end(), specs_, ctx, type);
    if (type == detail::type::char_type) detail::check_char_specs(specs_);
    return end;
  }

  template <detail::type U = detail::type_constant<T, Char>::value,
            FMT_ENABLE_IF(U == detail::type::string_type ||
                          U == detail::type::cstring_type ||
                          U == detail::type::char_type)>
  FMT_CONSTEXPR void set_debug_format(bool set = true) {
    specs_.type = set ? presentation_type::debug : presentation_type::none;
  }

  template <typename FormatContext>
  FMT_CONSTEXPR auto format(const T& val, FormatContext& ctx) const
      -> decltype(ctx.out());
};

template <typename Char = char> struct runtime_format_string {
  basic_string_view<Char> str;
};

/** A compile-time format string. */
template <typename Char, typename... Args> class basic_format_string {
 private:
  basic_string_view<Char> str_;

 public:
  template <typename S,
            FMT_ENABLE_IF(
                std::is_convertible<const S&, basic_string_view<Char>>::value)>
  FMT_CONSTEVAL FMT_INLINE basic_format_string(const S& s) : str_(s) {
    static_assert(
        detail::count<
            (std::is_base_of<detail::view, remove_reference_t<Args>>::value &&
             std::is_reference<Args>::value)...>() == 0,
        "passing views as lvalues is disallowed");
#ifdef FMT_HAS_CONSTEVAL
    if constexpr (detail::count_named_args<Args...>() ==
                  detail::count_statically_named_args<Args...>()) {
      using checker =
          detail::format_string_checker<Char, remove_cvref_t<Args>...>;
      detail::parse_format_string<true>(str_, checker(s));
    }
#else
    detail::check_format_string<Args...>(s);
#endif
  }
  basic_format_string(runtime_format_string<Char> fmt) : str_(fmt.str) {}

  FMT_INLINE operator basic_string_view<Char>() const { return str_; }
  FMT_INLINE auto get() const -> basic_string_view<Char> { return str_; }
};

#if FMT_GCC_VERSION && FMT_GCC_VERSION < 409
// Workaround broken conversion on older gcc.
template <typename...> using format_string = string_view;
inline auto runtime(string_view s) -> string_view { return s; }
#else
template <typename... Args>
using format_string = basic_format_string<char, type_identity_t<Args>...>;
/**
  \rst
  Creates a runtime format string.

  **Example**::

    // Check format string at runtime instead of compile-time.
    fmt::print(fmt::runtime("{:d}"), "I am not a number");
  \endrst
 */
inline auto runtime(string_view s) -> runtime_format_string<> { return {{s}}; }
#endif

FMT_API auto vformat(string_view fmt, format_args args) -> std::string;

/**
  \rst
  Formats ``args`` according to specifications in ``fmt`` and returns the result
  as a string.

  **Example**::

    #include <fmt/core.h>
    std::string message = fmt::format("The answer is {}.", 42);
  \endrst
*/
template <typename... T>
FMT_NODISCARD FMT_INLINE auto format(format_string<T...> fmt, T&&... args)
    -> std::string {
  return vformat(fmt, fmt::make_format_args(args...));
}

/** Formats a string and writes the output to ``out``. */
template <typename OutputIt,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value)>
auto vformat_to(OutputIt out, string_view fmt, format_args args) -> OutputIt {
  auto&& buf = detail::get_buffer<char>(out);
  detail::vformat_to(buf, fmt, args, {});
  return detail::get_iterator(buf, out);
}

/**
 \rst
 Formats ``args`` according to specifications in ``fmt``, writes the result to
 the output iterator ``out`` and returns the iterator past the end of the output
 range. `format_to` does not append a terminating null character.

 **Example**::

   auto out = std::vector<char>();
   fmt::format_to(std::back_inserter(out), "{}", 42);
 \endrst
 */
template <typename OutputIt, typename... T,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value)>
FMT_INLINE auto format_to(OutputIt out, format_string<T...> fmt, T&&... args)
    -> OutputIt {
  return vformat_to(out, fmt, fmt::make_format_args(args...));
}

template <typename OutputIt> struct format_to_n_result {
  /** Iterator past the end of the output range. */
  OutputIt out;
  /** Total (not truncated) output size. */
  size_t size;
};

template <typename OutputIt, typename... T,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value)>
auto vformat_to_n(OutputIt out, size_t n, string_view fmt, format_args args)
    -> format_to_n_result<OutputIt> {
  using traits = detail::fixed_buffer_traits;
  auto buf = detail::iterator_buffer<OutputIt, char, traits>(out, n);
  detail::vformat_to(buf, fmt, args, {});
  return {buf.out(), buf.count()};
}

/**
  \rst
  Formats ``args`` according to specifications in ``fmt``, writes up to ``n``
  characters of the result to the output iterator ``out`` and returns the total
  (not truncated) output size and the iterator past the end of the output range.
  `format_to_n` does not append a terminating null character.
  \endrst
 */
template <typename OutputIt, typename... T,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value)>
FMT_INLINE auto format_to_n(OutputIt out, size_t n, format_string<T...> fmt,
                            T&&... args) -> format_to_n_result<OutputIt> {
  return vformat_to_n(out, n, fmt, fmt::make_format_args(args...));
}

/** Returns the number of chars in the output of ``format(fmt, args...)``. */
template <typename... T>
FMT_NODISCARD FMT_INLINE auto formatted_size(format_string<T...> fmt,
                                             T&&... args) -> size_t {
  auto buf = detail::counting_buffer<>();
  detail::vformat_to<char>(buf, fmt, fmt::make_format_args(args...), {});
  return buf.count();
}

FMT_API void vprint(string_view fmt, format_args args);
FMT_API void vprint(std::FILE* f, string_view fmt, format_args args);

/**
  \rst
  Formats ``args`` according to specifications in ``fmt`` and writes the output
  to ``stdout``.

  **Example**::

    fmt::print("Elapsed time: {0:.2f} seconds", 1.23);
  \endrst
 */
template <typename... T>
FMT_INLINE void print(format_string<T...> fmt, T&&... args) {
  const auto& vargs = fmt::make_format_args(args...);
  return detail::is_utf8() ? vprint(fmt, vargs)
                           : detail::vprint_mojibake(stdout, fmt, vargs);
}

/**
  \rst
  Formats ``args`` according to specifications in ``fmt`` and writes the
  output to the file ``f``.

  **Example**::

    fmt::print(stderr, "Don't {}!", "panic");
  \endrst
 */
template <typename... T>
FMT_INLINE void print(std::FILE* f, format_string<T...> fmt, T&&... args) {
  const auto& vargs = fmt::make_format_args(args...);
  return detail::is_utf8() ? vprint(f, fmt, vargs)
                           : detail::vprint_mojibake(f, fmt, vargs);
}

/**
  Formats ``args`` according to specifications in ``fmt`` and writes the
  output to the file ``f`` followed by a newline.
 */
template <typename... T>
FMT_INLINE void println(std::FILE* f, format_string<T...> fmt, T&&... args) {
  return fmt::print(f, "{}\n", fmt::format(fmt, std::forward<T>(args)...));
}

/**
  Formats ``args`` according to specifications in ``fmt`` and writes the output
  to ``stdout`` followed by a newline.
 */
template <typename... T>
FMT_INLINE void println(format_string<T...> fmt, T&&... args) {
  return fmt::println(stdout, fmt, std::forward<T>(args)...);
}

FMT_END_EXPORT
FMT_GCC_PRAGMA("GCC pop_options")
FMT_END_NAMESPACE

#ifdef FMT_HEADER_ONLY
#  include "format.h"
#endif
#endif  // FMT_CORE_H_
