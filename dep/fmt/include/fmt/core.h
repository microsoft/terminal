// Formatting library for C++ - the core API
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_CORE_H_
#define FMT_CORE_H_

#include <cstdio>  // std::FILE
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// The fmt library version in the form major * 10000 + minor * 100 + patch.
#define FMT_VERSION 60200

#ifdef __has_feature
#  define FMT_HAS_FEATURE(x) __has_feature(x)
#else
#  define FMT_HAS_FEATURE(x) 0
#endif

#if defined(__has_include) && !defined(__INTELLISENSE__) && \
    !(defined(__INTEL_COMPILER) && __INTEL_COMPILER < 1600)
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
  (__cplusplus >= 201402L && FMT_HAS_CPP_ATTRIBUTE(attribute))

#define FMT_HAS_CPP17_ATTRIBUTE(attribute) \
  (__cplusplus >= 201703L && FMT_HAS_CPP_ATTRIBUTE(attribute))

#ifdef __clang__
#  define FMT_CLANG_VERSION (__clang_major__ * 100 + __clang_minor__)
#else
#  define FMT_CLANG_VERSION 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#  define FMT_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#  define FMT_GCC_VERSION 0
#endif

#if __cplusplus >= 201103L || defined(__GXX_EXPERIMENTAL_CXX0X__)
#  define FMT_HAS_GXX_CXX11 FMT_GCC_VERSION
#else
#  define FMT_HAS_GXX_CXX11 0
#endif

#ifdef __NVCC__
#  define FMT_NVCC __NVCC__
#else
#  define FMT_NVCC 0
#endif

#ifdef _MSC_VER
#  define FMT_MSC_VER _MSC_VER
#else
#  define FMT_MSC_VER 0
#endif

// Check if relaxed C++14 constexpr is supported.
// GCC doesn't allow throw in constexpr until version 6 (bug 67371).
#ifndef FMT_USE_CONSTEXPR
#  define FMT_USE_CONSTEXPR                                           \
    (FMT_HAS_FEATURE(cxx_relaxed_constexpr) || FMT_MSC_VER >= 1910 || \
     (FMT_GCC_VERSION >= 600 && __cplusplus >= 201402L)) &&           \
        !FMT_NVCC
#endif
#if FMT_USE_CONSTEXPR
#  define FMT_CONSTEXPR constexpr
#  define FMT_CONSTEXPR_DECL constexpr
#else
#  define FMT_CONSTEXPR inline
#  define FMT_CONSTEXPR_DECL
#endif

#ifndef FMT_OVERRIDE
#  if FMT_HAS_FEATURE(cxx_override) || \
      (FMT_GCC_VERSION >= 408 && FMT_HAS_GXX_CXX11) || FMT_MSC_VER >= 1900
#    define FMT_OVERRIDE override
#  else
#    define FMT_OVERRIDE
#  endif
#endif

// Check if exceptions are disabled.
#ifndef FMT_EXCEPTIONS
#  if (defined(__GNUC__) && !defined(__EXCEPTIONS)) || \
      FMT_MSC_VER && !_HAS_EXCEPTIONS
#    define FMT_EXCEPTIONS 0
#  else
#    define FMT_EXCEPTIONS 1
#  endif
#endif

// Define FMT_USE_NOEXCEPT to make fmt use noexcept (C++11 feature).
#ifndef FMT_USE_NOEXCEPT
#  define FMT_USE_NOEXCEPT 0
#endif

#if FMT_USE_NOEXCEPT || FMT_HAS_FEATURE(cxx_noexcept) || \
    (FMT_GCC_VERSION >= 408 && FMT_HAS_GXX_CXX11) || FMT_MSC_VER >= 1900
#  define FMT_DETECTED_NOEXCEPT noexcept
#  define FMT_HAS_CXX11_NOEXCEPT 1
#else
#  define FMT_DETECTED_NOEXCEPT throw()
#  define FMT_HAS_CXX11_NOEXCEPT 0
#endif

#ifndef FMT_NOEXCEPT
#  if FMT_EXCEPTIONS || FMT_HAS_CXX11_NOEXCEPT
#    define FMT_NOEXCEPT FMT_DETECTED_NOEXCEPT
#  else
#    define FMT_NOEXCEPT
#  endif
#endif

// [[noreturn]] is disabled on MSVC and NVCC because of bogus unreachable code
// warnings.
#if FMT_EXCEPTIONS && FMT_HAS_CPP_ATTRIBUTE(noreturn) && !FMT_MSC_VER && \
    !FMT_NVCC
#  define FMT_NORETURN [[noreturn]]
#else
#  define FMT_NORETURN
#endif

#ifndef FMT_MAYBE_UNUSED
#  if FMT_HAS_CPP17_ATTRIBUTE(maybe_unused)
#    define FMT_MAYBE_UNUSED [[maybe_unused]]
#  else
#    define FMT_MAYBE_UNUSED
#  endif
#endif

#ifndef FMT_DEPRECATED
#  if FMT_HAS_CPP14_ATTRIBUTE(deprecated) || FMT_MSC_VER >= 1900
#    define FMT_DEPRECATED [[deprecated]]
#  else
#    if defined(__GNUC__) || defined(__clang__)
#      define FMT_DEPRECATED __attribute__((deprecated))
#    elif FMT_MSC_VER
#      define FMT_DEPRECATED __declspec(deprecated)
#    else
#      define FMT_DEPRECATED /* deprecated */
#    endif
#  endif
#endif

// Workaround broken [[deprecated]] in the Intel, PGI and NVCC compilers.
#if defined(__INTEL_COMPILER) || defined(__PGI) || FMT_NVCC
#  define FMT_DEPRECATED_ALIAS
#else
#  define FMT_DEPRECATED_ALIAS FMT_DEPRECATED
#endif

#ifndef FMT_BEGIN_NAMESPACE
#  if FMT_HAS_FEATURE(cxx_inline_namespaces) || FMT_GCC_VERSION >= 404 || \
      FMT_MSC_VER >= 1900
#    define FMT_INLINE_NAMESPACE inline namespace
#    define FMT_END_NAMESPACE \
      }                       \
      }
#  else
#    define FMT_INLINE_NAMESPACE namespace
#    define FMT_END_NAMESPACE \
      }                       \
      using namespace v6;     \
      }
#  endif
#  define FMT_BEGIN_NAMESPACE \
    namespace fmt {           \
    FMT_INLINE_NAMESPACE v6 {
#endif

#if !defined(FMT_HEADER_ONLY) && defined(_WIN32)
#  if FMT_MSC_VER
#    define FMT_NO_W4275 __pragma(warning(suppress : 4275))
#  else
#    define FMT_NO_W4275
#  endif
#  define FMT_CLASS_API FMT_NO_W4275
#  ifdef FMT_EXPORT
#    define FMT_API __declspec(dllexport)
#  elif defined(FMT_SHARED)
#    define FMT_API __declspec(dllimport)
#    define FMT_EXTERN_TEMPLATE_API FMT_API
#  endif
#endif
#ifndef FMT_CLASS_API
#  define FMT_CLASS_API
#endif
#ifndef FMT_API
#  if FMT_GCC_VERSION || FMT_CLANG_VERSION
#    define FMT_API __attribute__((visibility("default")))
#    define FMT_EXTERN_TEMPLATE_API FMT_API
#    define FMT_INSTANTIATION_DEF_API
#  else
#    define FMT_API
#  endif
#endif
#ifndef FMT_EXTERN_TEMPLATE_API
#  define FMT_EXTERN_TEMPLATE_API
#endif
#ifndef FMT_INSTANTIATION_DEF_API
#  define FMT_INSTANTIATION_DEF_API FMT_API
#endif

#ifndef FMT_HEADER_ONLY
#  define FMT_EXTERN extern
#else
#  define FMT_EXTERN
#endif

// libc++ supports string_view in pre-c++17.
#if (FMT_HAS_INCLUDE(<string_view>) &&                       \
     (__cplusplus > 201402L || defined(_LIBCPP_VERSION))) || \
    (defined(_MSVC_LANG) && _MSVC_LANG > 201402L && _MSC_VER >= 1910)
#  include <string_view>
#  define FMT_USE_STRING_VIEW
#elif FMT_HAS_INCLUDE("experimental/string_view") && __cplusplus >= 201402L
#  include <experimental/string_view>
#  define FMT_USE_EXPERIMENTAL_STRING_VIEW
#endif

#ifndef FMT_UNICODE
#  define FMT_UNICODE !FMT_MSC_VER
#endif
#if FMT_UNICODE && FMT_MSC_VER
#  pragma execution_character_set("utf-8")
#endif

FMT_BEGIN_NAMESPACE

// Implementations of enable_if_t and other metafunctions for older systems.
template <bool B, class T = void>
using enable_if_t = typename std::enable_if<B, T>::type;
template <bool B, class T, class F>
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

struct monostate {};

// An enable_if helper to be used in template parameters which results in much
// shorter symbols: https://godbolt.org/z/sWw4vP. Extra parentheses are needed
// to workaround a bug in MSVC 2019 (see #1140 and #1186).
#define FMT_ENABLE_IF(...) enable_if_t<(__VA_ARGS__), int> = 0

namespace internal {

// A helper function to suppress bogus "conditional expression is constant"
// warnings.
template <typename T> FMT_CONSTEXPR T const_check(T value) { return value; }

// A workaround for gcc 4.8 to make void_t work in a SFINAE context.
template <typename... Ts> struct void_t_impl { using type = void; };

FMT_NORETURN FMT_API void assert_fail(const char* file, int line,
                                      const char* message);

#ifndef FMT_ASSERT
#  ifdef NDEBUG
// FMT_ASSERT is not empty to avoid -Werror=empty-body.
#    define FMT_ASSERT(condition, message) ((void)0)
#  else
#    define FMT_ASSERT(condition, message)                                    \
      ((condition) /* void() fails with -Winvalid-constexpr on clang 4.0.1 */ \
           ? (void)0                                                          \
           : ::fmt::internal::assert_fail(__FILE__, __LINE__, (message)))
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
#elif defined(__SIZEOF_INT128__) && !FMT_NVCC
#  define FMT_USE_INT128 1
using int128_t = __int128_t;
using uint128_t = __uint128_t;
#else
#  define FMT_USE_INT128 0
#endif
#if !FMT_USE_INT128
struct int128_t {};
struct uint128_t {};
#endif

// Casts a nonnegative integer to unsigned.
template <typename Int>
FMT_CONSTEXPR typename std::make_unsigned<Int>::type to_unsigned(Int value) {
  FMT_ASSERT(value >= 0, "negative value");
  return static_cast<typename std::make_unsigned<Int>::type>(value);
}

constexpr unsigned char micro[] = "\u00B5";

template <typename Char> constexpr bool is_unicode() {
  return FMT_UNICODE || sizeof(Char) != 1 ||
         (sizeof(micro) == 3 && micro[0] == 0xC2 && micro[1] == 0xB5);
}

#ifdef __cpp_char8_t
using char8_type = char8_t;
#else
enum char8_type : unsigned char {};
#endif
}  // namespace internal

template <typename... Ts>
using void_t = typename internal::void_t_impl<Ts...>::type;

/**
  An implementation of ``std::basic_string_view`` for pre-C++17. It provides a
  subset of the API. ``fmt::basic_string_view`` is used for format strings even
  if ``std::string_view`` is available to prevent issues when a library is
  compiled with a different ``-std`` option than the client code (which is not
  recommended).
 */
template <typename Char> class basic_string_view {
 private:
  const Char* data_;
  size_t size_;

 public:
  using char_type FMT_DEPRECATED_ALIAS = Char;
  using value_type = Char;
  using iterator = const Char*;

  FMT_CONSTEXPR basic_string_view() FMT_NOEXCEPT : data_(nullptr), size_(0) {}

  /** Constructs a string reference object from a C string and a size. */
  FMT_CONSTEXPR basic_string_view(const Char* s, size_t count) FMT_NOEXCEPT
      : data_(s),
        size_(count) {}

  /**
    \rst
    Constructs a string reference object from a C string computing
    the size with ``std::char_traits<Char>::length``.
    \endrst
   */
#if __cplusplus >= 201703L  // C++17's char_traits::length() is constexpr.
  FMT_CONSTEXPR
#endif
  basic_string_view(const Char* s)
      : data_(s), size_(std::char_traits<Char>::length(s)) {}

  /** Constructs a string reference from a ``std::basic_string`` object. */
  template <typename Traits, typename Alloc>
  FMT_CONSTEXPR basic_string_view(
      const std::basic_string<Char, Traits, Alloc>& s) FMT_NOEXCEPT
      : data_(s.data()),
        size_(s.size()) {}

  template <
      typename S,
      FMT_ENABLE_IF(std::is_same<S, internal::std_string_view<Char>>::value)>
  FMT_CONSTEXPR basic_string_view(S s) FMT_NOEXCEPT : data_(s.data()),
                                                      size_(s.size()) {}

  /** Returns a pointer to the string data. */
  FMT_CONSTEXPR const Char* data() const { return data_; }

  /** Returns the string size. */
  FMT_CONSTEXPR size_t size() const { return size_; }

  FMT_CONSTEXPR iterator begin() const { return data_; }
  FMT_CONSTEXPR iterator end() const { return data_ + size_; }

  FMT_CONSTEXPR const Char& operator[](size_t pos) const { return data_[pos]; }

  FMT_CONSTEXPR void remove_prefix(size_t n) {
    data_ += n;
    size_ -= n;
  }

  // Lexicographically compare this string reference to other.
  int compare(basic_string_view other) const {
    size_t str_size = size_ < other.size_ ? size_ : other.size_;
    int result = std::char_traits<Char>::compare(data_, other.data_, str_size);
    if (result == 0)
      result = size_ == other.size_ ? 0 : (size_ < other.size_ ? -1 : 1);
    return result;
  }

  friend bool operator==(basic_string_view lhs, basic_string_view rhs) {
    return lhs.compare(rhs) == 0;
  }
  friend bool operator!=(basic_string_view lhs, basic_string_view rhs) {
    return lhs.compare(rhs) != 0;
  }
  friend bool operator<(basic_string_view lhs, basic_string_view rhs) {
    return lhs.compare(rhs) < 0;
  }
  friend bool operator<=(basic_string_view lhs, basic_string_view rhs) {
    return lhs.compare(rhs) <= 0;
  }
  friend bool operator>(basic_string_view lhs, basic_string_view rhs) {
    return lhs.compare(rhs) > 0;
  }
  friend bool operator>=(basic_string_view lhs, basic_string_view rhs) {
    return lhs.compare(rhs) >= 0;
  }
};

using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;

#ifndef __cpp_char8_t
// char8_t is deprecated; use char instead.
using char8_t FMT_DEPRECATED_ALIAS = internal::char8_type;
#endif

/** Specifies if ``T`` is a character type. Can be specialized by users. */
template <typename T> struct is_char : std::false_type {};
template <> struct is_char<char> : std::true_type {};
template <> struct is_char<wchar_t> : std::true_type {};
template <> struct is_char<internal::char8_type> : std::true_type {};
template <> struct is_char<char16_t> : std::true_type {};
template <> struct is_char<char32_t> : std::true_type {};

/**
  \rst
  Returns a string view of `s`. In order to add custom string type support to
  {fmt} provide an overload of `to_string_view` for it in the same namespace as
  the type for the argument-dependent lookup to work.

  **Example**::

    namespace my_ns {
    inline string_view to_string_view(const my_string& s) {
      return {s.data(), s.length()};
    }
    }
    std::string message = fmt::format(my_string("The answer is {}"), 42);
  \endrst
 */
template <typename Char, FMT_ENABLE_IF(is_char<Char>::value)>
inline basic_string_view<Char> to_string_view(const Char* s) {
  return s;
}

template <typename Char, typename Traits, typename Alloc>
inline basic_string_view<Char> to_string_view(
    const std::basic_string<Char, Traits, Alloc>& s) {
  return s;
}

template <typename Char>
inline basic_string_view<Char> to_string_view(basic_string_view<Char> s) {
  return s;
}

template <typename Char,
          FMT_ENABLE_IF(!std::is_empty<internal::std_string_view<Char>>::value)>
inline basic_string_view<Char> to_string_view(
    internal::std_string_view<Char> s) {
  return s;
}

// A base class for compile-time strings. It is defined in the fmt namespace to
// make formatting functions visible via ADL, e.g. format(fmt("{}"), 42).
struct compile_string {};

template <typename S>
struct is_compile_string : std::is_base_of<compile_string, S> {};

template <typename S, FMT_ENABLE_IF(is_compile_string<S>::value)>
constexpr basic_string_view<typename S::char_type> to_string_view(const S& s) {
  return s;
}

namespace internal {
void to_string_view(...);
using fmt::v6::to_string_view;

// Specifies whether S is a string type convertible to fmt::basic_string_view.
// It should be a constexpr function but MSVC 2017 fails to compile it in
// enable_if and MSVC 2015 fails to compile it as an alias template.
template <typename S>
struct is_string : std::is_class<decltype(to_string_view(std::declval<S>()))> {
};

template <typename S, typename = void> struct char_t_impl {};
template <typename S> struct char_t_impl<S, enable_if_t<is_string<S>::value>> {
  using result = decltype(to_string_view(std::declval<S>()));
  using type = typename result::value_type;
};

struct error_handler {
  FMT_CONSTEXPR error_handler() = default;
  FMT_CONSTEXPR error_handler(const error_handler&) = default;

  // This function is intentionally not constexpr to give a compile-time error.
  FMT_NORETURN FMT_API void on_error(const char* message);
};
}  // namespace internal

/** String's character type. */
template <typename S> using char_t = typename internal::char_t_impl<S>::type;

/**
  \rst
  Parsing context consisting of a format string range being parsed and an
  argument counter for automatic indexing.

  You can use one of the following type aliases for common character types:

  +-----------------------+-------------------------------------+
  | Type                  | Definition                          |
  +=======================+=====================================+
  | format_parse_context  | basic_format_parse_context<char>    |
  +-----------------------+-------------------------------------+
  | wformat_parse_context | basic_format_parse_context<wchar_t> |
  +-----------------------+-------------------------------------+
  \endrst
 */
template <typename Char, typename ErrorHandler = internal::error_handler>
class basic_format_parse_context : private ErrorHandler {
 private:
  basic_string_view<Char> format_str_;
  int next_arg_id_;

 public:
  using char_type = Char;
  using iterator = typename basic_string_view<Char>::iterator;

  explicit FMT_CONSTEXPR basic_format_parse_context(
      basic_string_view<Char> format_str, ErrorHandler eh = ErrorHandler())
      : ErrorHandler(eh), format_str_(format_str), next_arg_id_(0) {}

  /**
    Returns an iterator to the beginning of the format string range being
    parsed.
   */
  FMT_CONSTEXPR iterator begin() const FMT_NOEXCEPT {
    return format_str_.begin();
  }

  /**
    Returns an iterator past the end of the format string range being parsed.
   */
  FMT_CONSTEXPR iterator end() const FMT_NOEXCEPT { return format_str_.end(); }

  /** Advances the begin iterator to ``it``. */
  FMT_CONSTEXPR void advance_to(iterator it) {
    format_str_.remove_prefix(internal::to_unsigned(it - begin()));
  }

  /**
    Reports an error if using the manual argument indexing; otherwise returns
    the next argument index and switches to the automatic indexing.
   */
  FMT_CONSTEXPR int next_arg_id() {
    if (next_arg_id_ >= 0) return next_arg_id_++;
    on_error("cannot switch from manual to automatic argument indexing");
    return 0;
  }

  /**
    Reports an error if using the automatic argument indexing; otherwise
    switches to the manual indexing.
   */
  FMT_CONSTEXPR void check_arg_id(int) {
    if (next_arg_id_ > 0)
      on_error("cannot switch from automatic to manual argument indexing");
    else
      next_arg_id_ = -1;
  }

  FMT_CONSTEXPR void check_arg_id(basic_string_view<Char>) {}

  FMT_CONSTEXPR void on_error(const char* message) {
    ErrorHandler::on_error(message);
  }

  FMT_CONSTEXPR ErrorHandler error_handler() const { return *this; }
};

using format_parse_context = basic_format_parse_context<char>;
using wformat_parse_context = basic_format_parse_context<wchar_t>;

template <typename Char, typename ErrorHandler = internal::error_handler>
using basic_parse_context FMT_DEPRECATED_ALIAS =
    basic_format_parse_context<Char, ErrorHandler>;
using parse_context FMT_DEPRECATED_ALIAS = basic_format_parse_context<char>;
using wparse_context FMT_DEPRECATED_ALIAS = basic_format_parse_context<wchar_t>;

template <typename Context> class basic_format_arg;
template <typename Context> class basic_format_args;

// A formatter for objects of type T.
template <typename T, typename Char = char, typename Enable = void>
struct formatter {
  // A deleted default constructor indicates a disabled formatter.
  formatter() = delete;
};

template <typename T, typename Char, typename Enable = void>
struct FMT_DEPRECATED convert_to_int
    : bool_constant<!std::is_arithmetic<T>::value &&
                    std::is_convertible<T, int>::value> {};

// Specifies if T has an enabled formatter specialization. A type can be
// formattable even if it doesn't have a formatter e.g. via a conversion.
template <typename T, typename Context>
using has_formatter =
    std::is_constructible<typename Context::template formatter_type<T>>;

namespace internal {

/** A contiguous memory buffer with an optional growing ability. */
template <typename T> class buffer {
 private:
  T* ptr_;
  std::size_t size_;
  std::size_t capacity_;

 protected:
  // Don't initialize ptr_ since it is not accessed to save a few cycles.
  buffer(std::size_t sz) FMT_NOEXCEPT : size_(sz), capacity_(sz) {}

  buffer(T* p = nullptr, std::size_t sz = 0, std::size_t cap = 0) FMT_NOEXCEPT
      : ptr_(p),
        size_(sz),
        capacity_(cap) {}

  /** Sets the buffer data and capacity. */
  void set(T* buf_data, std::size_t buf_capacity) FMT_NOEXCEPT {
    ptr_ = buf_data;
    capacity_ = buf_capacity;
  }

  /** Increases the buffer capacity to hold at least *capacity* elements. */
  virtual void grow(std::size_t capacity) = 0;

 public:
  using value_type = T;
  using const_reference = const T&;

  buffer(const buffer&) = delete;
  void operator=(const buffer&) = delete;
  virtual ~buffer() = default;

  T* begin() FMT_NOEXCEPT { return ptr_; }
  T* end() FMT_NOEXCEPT { return ptr_ + size_; }

  const T* begin() const FMT_NOEXCEPT { return ptr_; }
  const T* end() const FMT_NOEXCEPT { return ptr_ + size_; }

  /** Returns the size of this buffer. */
  std::size_t size() const FMT_NOEXCEPT { return size_; }

  /** Returns the capacity of this buffer. */
  std::size_t capacity() const FMT_NOEXCEPT { return capacity_; }

  /** Returns a pointer to the buffer data. */
  T* data() FMT_NOEXCEPT { return ptr_; }

  /** Returns a pointer to the buffer data. */
  const T* data() const FMT_NOEXCEPT { return ptr_; }

  /**
    Resizes the buffer. If T is a POD type new elements may not be initialized.
   */
  void resize(std::size_t new_size) {
    reserve(new_size);
    size_ = new_size;
  }

  /** Clears this buffer. */
  void clear() { size_ = 0; }

  /** Reserves space to store at least *capacity* elements. */
  void reserve(std::size_t new_capacity) {
    if (new_capacity > capacity_) grow(new_capacity);
  }

  void push_back(const T& value) {
    reserve(size_ + 1);
    ptr_[size_++] = value;
  }

  /** Appends data to the end of the buffer. */
  template <typename U> void append(const U* begin, const U* end);

  template <typename I> T& operator[](I index) { return ptr_[index]; }
  template <typename I> const T& operator[](I index) const {
    return ptr_[index];
  }
};

// A container-backed buffer.
template <typename Container>
class container_buffer : public buffer<typename Container::value_type> {
 private:
  Container& container_;

 protected:
  void grow(std::size_t capacity) FMT_OVERRIDE {
    container_.resize(capacity);
    this->set(&container_[0], capacity);
  }

 public:
  explicit container_buffer(Container& c)
      : buffer<typename Container::value_type>(c.size()), container_(c) {}
};

// Extracts a reference to the container from back_insert_iterator.
template <typename Container>
inline Container& get_container(std::back_insert_iterator<Container> it) {
  using bi_iterator = std::back_insert_iterator<Container>;
  struct accessor : bi_iterator {
    accessor(bi_iterator iter) : bi_iterator(iter) {}
    using bi_iterator::container;
  };
  return *accessor(it).container;
}

template <typename T, typename Char = char, typename Enable = void>
struct fallback_formatter {
  fallback_formatter() = delete;
};

// Specifies if T has an enabled fallback_formatter specialization.
template <typename T, typename Context>
using has_fallback_formatter =
    std::is_constructible<fallback_formatter<T, typename Context::char_type>>;

template <typename Char> struct named_arg_base;
template <typename T, typename Char> struct named_arg;

enum class type {
  none_type,
  named_arg_type,
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

FMT_TYPE_CONSTANT(const named_arg_base<Char>&, named_arg_type);
FMT_TYPE_CONSTANT(int, int_type);
FMT_TYPE_CONSTANT(unsigned, uint_type);
FMT_TYPE_CONSTANT(long long, long_long_type);
FMT_TYPE_CONSTANT(unsigned long long, ulong_long_type);
FMT_TYPE_CONSTANT(int128_t, int128_type);
FMT_TYPE_CONSTANT(uint128_t, uint128_type);
FMT_TYPE_CONSTANT(bool, bool_type);
FMT_TYPE_CONSTANT(Char, char_type);
FMT_TYPE_CONSTANT(float, float_type);
FMT_TYPE_CONSTANT(double, double_type);
FMT_TYPE_CONSTANT(long double, long_double_type);
FMT_TYPE_CONSTANT(const Char*, cstring_type);
FMT_TYPE_CONSTANT(basic_string_view<Char>, string_type);
FMT_TYPE_CONSTANT(const void*, pointer_type);

FMT_CONSTEXPR bool is_integral_type(type t) {
  FMT_ASSERT(t != type::named_arg_type, "invalid argument type");
  return t > type::none_type && t <= type::last_integer_type;
}

FMT_CONSTEXPR bool is_arithmetic_type(type t) {
  FMT_ASSERT(t != type::named_arg_type, "invalid argument type");
  return t > type::none_type && t <= type::last_numeric_type;
}

template <typename Char> struct string_value {
  const Char* data;
  std::size_t size;
};

template <typename Context> struct custom_value {
  using parse_context = basic_format_parse_context<typename Context::char_type>;
  const void* value;
  void (*format)(const void* arg, parse_context& parse_ctx, Context& ctx);
};

// A formatting argument value.
template <typename Context> class value {
 public:
  using char_type = typename Context::char_type;

  union {
    int int_value;
    unsigned uint_value;
    long long long_long_value;
    unsigned long long ulong_long_value;
    int128_t int128_value;
    uint128_t uint128_value;
    bool bool_value;
    char_type char_value;
    float float_value;
    double double_value;
    long double long_double_value;
    const void* pointer;
    string_value<char_type> string;
    custom_value<Context> custom;
    const named_arg_base<char_type>* named_arg;
  };

  FMT_CONSTEXPR value(int val = 0) : int_value(val) {}
  FMT_CONSTEXPR value(unsigned val) : uint_value(val) {}
  value(long long val) : long_long_value(val) {}
  value(unsigned long long val) : ulong_long_value(val) {}
  value(int128_t val) : int128_value(val) {}
  value(uint128_t val) : uint128_value(val) {}
  value(float val) : float_value(val) {}
  value(double val) : double_value(val) {}
  value(long double val) : long_double_value(val) {}
  value(bool val) : bool_value(val) {}
  value(char_type val) : char_value(val) {}
  value(const char_type* val) { string.data = val; }
  value(basic_string_view<char_type> val) {
    string.data = val.data();
    string.size = val.size();
  }
  value(const void* val) : pointer(val) {}

  template <typename T> value(const T& val) {
    custom.value = &val;
    // Get the formatter type through the context to allow different contexts
    // have different extension points, e.g. `formatter<T>` for `format` and
    // `printf_formatter<T>` for `printf`.
    custom.format = format_custom_arg<
        T, conditional_t<has_formatter<T, Context>::value,
                         typename Context::template formatter_type<T>,
                         fallback_formatter<T, char_type>>>;
  }

  value(const named_arg_base<char_type>& val) { named_arg = &val; }

 private:
  // Formats an argument of a custom type, such as a user-defined class.
  template <typename T, typename Formatter>
  static void format_custom_arg(
      const void* arg, basic_format_parse_context<char_type>& parse_ctx,
      Context& ctx) {
    Formatter f;
    parse_ctx.advance_to(f.parse(parse_ctx));
    ctx.advance_to(f.format(*static_cast<const T*>(arg), ctx));
  }
};

template <typename Context, typename T>
FMT_CONSTEXPR basic_format_arg<Context> make_arg(const T& value);

// To minimize the number of types we need to deal with, long is translated
// either to int or to long long depending on its size.
enum { long_short = sizeof(long) == sizeof(int) };
using long_type = conditional_t<long_short, int, long long>;
using ulong_type = conditional_t<long_short, unsigned, unsigned long long>;

// Maps formatting arguments to core types.
template <typename Context> struct arg_mapper {
  using char_type = typename Context::char_type;

  FMT_CONSTEXPR int map(signed char val) { return val; }
  FMT_CONSTEXPR unsigned map(unsigned char val) { return val; }
  FMT_CONSTEXPR int map(short val) { return val; }
  FMT_CONSTEXPR unsigned map(unsigned short val) { return val; }
  FMT_CONSTEXPR int map(int val) { return val; }
  FMT_CONSTEXPR unsigned map(unsigned val) { return val; }
  FMT_CONSTEXPR long_type map(long val) { return val; }
  FMT_CONSTEXPR ulong_type map(unsigned long val) { return val; }
  FMT_CONSTEXPR long long map(long long val) { return val; }
  FMT_CONSTEXPR unsigned long long map(unsigned long long val) { return val; }
  FMT_CONSTEXPR int128_t map(int128_t val) { return val; }
  FMT_CONSTEXPR uint128_t map(uint128_t val) { return val; }
  FMT_CONSTEXPR bool map(bool val) { return val; }

  template <typename T, FMT_ENABLE_IF(is_char<T>::value)>
  FMT_CONSTEXPR char_type map(T val) {
    static_assert(
        std::is_same<T, char>::value || std::is_same<T, char_type>::value,
        "mixing character types is disallowed");
    return val;
  }

  FMT_CONSTEXPR float map(float val) { return val; }
  FMT_CONSTEXPR double map(double val) { return val; }
  FMT_CONSTEXPR long double map(long double val) { return val; }

  FMT_CONSTEXPR const char_type* map(char_type* val) { return val; }
  FMT_CONSTEXPR const char_type* map(const char_type* val) { return val; }
  template <typename T, FMT_ENABLE_IF(is_string<T>::value)>
  FMT_CONSTEXPR basic_string_view<char_type> map(const T& val) {
    static_assert(std::is_same<char_type, char_t<T>>::value,
                  "mixing character types is disallowed");
    return to_string_view(val);
  }
  template <typename T,
            FMT_ENABLE_IF(
                std::is_constructible<basic_string_view<char_type>, T>::value &&
                !is_string<T>::value && !has_formatter<T, Context>::value &&
                !has_fallback_formatter<T, Context>::value)>
  FMT_CONSTEXPR basic_string_view<char_type> map(const T& val) {
    return basic_string_view<char_type>(val);
  }
  template <
      typename T,
      FMT_ENABLE_IF(
          std::is_constructible<std_string_view<char_type>, T>::value &&
          !std::is_constructible<basic_string_view<char_type>, T>::value &&
          !is_string<T>::value && !has_formatter<T, Context>::value &&
          !has_fallback_formatter<T, Context>::value)>
  FMT_CONSTEXPR basic_string_view<char_type> map(const T& val) {
    return std_string_view<char_type>(val);
  }
  FMT_CONSTEXPR const char* map(const signed char* val) {
    static_assert(std::is_same<char_type, char>::value, "invalid string type");
    return reinterpret_cast<const char*>(val);
  }
  FMT_CONSTEXPR const char* map(const unsigned char* val) {
    static_assert(std::is_same<char_type, char>::value, "invalid string type");
    return reinterpret_cast<const char*>(val);
  }

  FMT_CONSTEXPR const void* map(void* val) { return val; }
  FMT_CONSTEXPR const void* map(const void* val) { return val; }
  FMT_CONSTEXPR const void* map(std::nullptr_t val) { return val; }
  template <typename T> FMT_CONSTEXPR int map(const T*) {
    // Formatting of arbitrary pointers is disallowed. If you want to output
    // a pointer cast it to "void *" or "const void *". In particular, this
    // forbids formatting of "[const] volatile char *" which is printed as bool
    // by iostreams.
    static_assert(!sizeof(T), "formatting of non-void pointers is disallowed");
    return 0;
  }

  template <typename T,
            FMT_ENABLE_IF(std::is_enum<T>::value &&
                          !has_formatter<T, Context>::value &&
                          !has_fallback_formatter<T, Context>::value)>
  FMT_CONSTEXPR auto map(const T& val)
      -> decltype(std::declval<arg_mapper>().map(
          static_cast<typename std::underlying_type<T>::type>(val))) {
    return map(static_cast<typename std::underlying_type<T>::type>(val));
  }
  template <typename T,
            FMT_ENABLE_IF(!is_string<T>::value && !is_char<T>::value &&
                          (has_formatter<T, Context>::value ||
                           has_fallback_formatter<T, Context>::value))>
  FMT_CONSTEXPR const T& map(const T& val) {
    return val;
  }

  template <typename T>
  FMT_CONSTEXPR const named_arg_base<char_type>& map(
      const named_arg<T, char_type>& val) {
    auto arg = make_arg<Context>(val.value);
    std::memcpy(val.data, &arg, sizeof(arg));
    return val;
  }

  int map(...) {
    constexpr bool formattable = sizeof(Context) == 0;
    static_assert(
        formattable,
        "Cannot format argument. To make type T formattable provide a "
        "formatter<T> specialization: "
        "https://fmt.dev/latest/api.html#formatting-user-defined-types");
    return 0;
  }
};

// A type constant after applying arg_mapper<Context>.
template <typename T, typename Context>
using mapped_type_constant =
    type_constant<decltype(arg_mapper<Context>().map(std::declval<const T&>())),
                  typename Context::char_type>;

enum { packed_arg_bits = 5 };
// Maximum number of arguments with packed types.
enum { max_packed_args = 63 / packed_arg_bits };
enum : unsigned long long { is_unpacked_bit = 1ULL << 63 };

template <typename Context> class arg_map;
}  // namespace internal

// A formatting argument. It is a trivially copyable/constructible type to
// allow storage in basic_memory_buffer.
template <typename Context> class basic_format_arg {
 private:
  internal::value<Context> value_;
  internal::type type_;

  template <typename ContextType, typename T>
  friend FMT_CONSTEXPR basic_format_arg<ContextType> internal::make_arg(
      const T& value);

  template <typename Visitor, typename Ctx>
  friend FMT_CONSTEXPR auto visit_format_arg(Visitor&& vis,
                                             const basic_format_arg<Ctx>& arg)
      -> decltype(vis(0));

  friend class basic_format_args<Context>;
  friend class internal::arg_map<Context>;

  using char_type = typename Context::char_type;

 public:
  class handle {
   public:
    explicit handle(internal::custom_value<Context> custom) : custom_(custom) {}

    void format(basic_format_parse_context<char_type>& parse_ctx,
                Context& ctx) const {
      custom_.format(custom_.value, parse_ctx, ctx);
    }

   private:
    internal::custom_value<Context> custom_;
  };

  FMT_CONSTEXPR basic_format_arg() : type_(internal::type::none_type) {}

  FMT_CONSTEXPR explicit operator bool() const FMT_NOEXCEPT {
    return type_ != internal::type::none_type;
  }

  internal::type type() const { return type_; }

  bool is_integral() const { return internal::is_integral_type(type_); }
  bool is_arithmetic() const { return internal::is_arithmetic_type(type_); }
};

/**
  \rst
  Visits an argument dispatching to the appropriate visit method based on
  the argument type. For example, if the argument type is ``double`` then
  ``vis(value)`` will be called with the value of type ``double``.
  \endrst
 */
template <typename Visitor, typename Context>
FMT_CONSTEXPR auto visit_format_arg(Visitor&& vis,
                                    const basic_format_arg<Context>& arg)
    -> decltype(vis(0)) {
  using char_type = typename Context::char_type;
  switch (arg.type_) {
  case internal::type::none_type:
    break;
  case internal::type::named_arg_type:
    FMT_ASSERT(false, "invalid argument type");
    break;
  case internal::type::int_type:
    return vis(arg.value_.int_value);
  case internal::type::uint_type:
    return vis(arg.value_.uint_value);
  case internal::type::long_long_type:
    return vis(arg.value_.long_long_value);
  case internal::type::ulong_long_type:
    return vis(arg.value_.ulong_long_value);
#if FMT_USE_INT128
  case internal::type::int128_type:
    return vis(arg.value_.int128_value);
  case internal::type::uint128_type:
    return vis(arg.value_.uint128_value);
#else
  case internal::type::int128_type:
  case internal::type::uint128_type:
    break;
#endif
  case internal::type::bool_type:
    return vis(arg.value_.bool_value);
  case internal::type::char_type:
    return vis(arg.value_.char_value);
  case internal::type::float_type:
    return vis(arg.value_.float_value);
  case internal::type::double_type:
    return vis(arg.value_.double_value);
  case internal::type::long_double_type:
    return vis(arg.value_.long_double_value);
  case internal::type::cstring_type:
    return vis(arg.value_.string.data);
  case internal::type::string_type:
    return vis(basic_string_view<char_type>(arg.value_.string.data,
                                            arg.value_.string.size));
  case internal::type::pointer_type:
    return vis(arg.value_.pointer);
  case internal::type::custom_type:
    return vis(typename basic_format_arg<Context>::handle(arg.value_.custom));
  }
  return vis(monostate());
}

namespace internal {
// A map from argument names to their values for named arguments.
template <typename Context> class arg_map {
 private:
  using char_type = typename Context::char_type;

  struct entry {
    basic_string_view<char_type> name;
    basic_format_arg<Context> arg;
  };

  entry* map_;
  unsigned size_;

  void push_back(value<Context> val) {
    const auto& named = *val.named_arg;
    map_[size_] = {named.name, named.template deserialize<Context>()};
    ++size_;
  }

 public:
  arg_map(const arg_map&) = delete;
  void operator=(const arg_map&) = delete;
  arg_map() : map_(nullptr), size_(0) {}
  void init(const basic_format_args<Context>& args);
  ~arg_map() { delete[] map_; }

  basic_format_arg<Context> find(basic_string_view<char_type> name) const {
    // The list is unsorted, so just return the first matching name.
    for (entry *it = map_, *end = map_ + size_; it != end; ++it) {
      if (it->name == name) return it->arg;
    }
    return {};
  }
};

// A type-erased reference to an std::locale to avoid heavy <locale> include.
class locale_ref {
 private:
  const void* locale_;  // A type-erased pointer to std::locale.

 public:
  locale_ref() : locale_(nullptr) {}
  template <typename Locale> explicit locale_ref(const Locale& loc);

  explicit operator bool() const FMT_NOEXCEPT { return locale_ != nullptr; }

  template <typename Locale> Locale get() const;
};

template <typename> constexpr unsigned long long encode_types() { return 0; }

template <typename Context, typename Arg, typename... Args>
constexpr unsigned long long encode_types() {
  return static_cast<unsigned>(mapped_type_constant<Arg, Context>::value) |
         (encode_types<Context, Args...>() << packed_arg_bits);
}

template <typename Context, typename T>
FMT_CONSTEXPR basic_format_arg<Context> make_arg(const T& value) {
  basic_format_arg<Context> arg;
  arg.type_ = mapped_type_constant<T, Context>::value;
  arg.value_ = arg_mapper<Context>().map(value);
  return arg;
}

template <bool IS_PACKED, typename Context, typename T,
          FMT_ENABLE_IF(IS_PACKED)>
inline value<Context> make_arg(const T& val) {
  return arg_mapper<Context>().map(val);
}

template <bool IS_PACKED, typename Context, typename T,
          FMT_ENABLE_IF(!IS_PACKED)>
inline basic_format_arg<Context> make_arg(const T& value) {
  return make_arg<Context>(value);
}

template <typename T> struct is_reference_wrapper : std::false_type {};

template <typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

class dynamic_arg_list {
  // Workaround for clang's -Wweak-vtables. Unlike for regular classes, for
  // templates it doesn't complain about inability to deduce single translation
  // unit for placing vtable. So storage_node_base is made a fake template.
  template <typename = void> struct node {
    virtual ~node() = default;
    std::unique_ptr<node<>> next;
  };

  template <typename T> struct typed_node : node<> {
    T value;

    template <typename Arg>
    FMT_CONSTEXPR typed_node(const Arg& arg) : value(arg) {}

    template <typename Char>
    FMT_CONSTEXPR typed_node(const basic_string_view<Char>& arg)
        : value(arg.data(), arg.size()) {}
  };

  std::unique_ptr<node<>> head_;

 public:
  template <typename T, typename Arg> const T& push(const Arg& arg) {
    auto node = std::unique_ptr<typed_node<T>>(new typed_node<T>(arg));
    auto& value = node->value;
    node->next = std::move(head_);
    head_ = std::move(node);
    return value;
  }
};
}  // namespace internal

// Formatting context.
template <typename OutputIt, typename Char> class basic_format_context {
 public:
  /** The character type for the output. */
  using char_type = Char;

 private:
  OutputIt out_;
  basic_format_args<basic_format_context> args_;
  internal::arg_map<basic_format_context> map_;
  internal::locale_ref loc_;

 public:
  using iterator = OutputIt;
  using format_arg = basic_format_arg<basic_format_context>;
  template <typename T> using formatter_type = formatter<T, char_type>;

  basic_format_context(const basic_format_context&) = delete;
  void operator=(const basic_format_context&) = delete;
  /**
   Constructs a ``basic_format_context`` object. References to the arguments are
   stored in the object so make sure they have appropriate lifetimes.
   */
  basic_format_context(OutputIt out,
                       basic_format_args<basic_format_context> ctx_args,
                       internal::locale_ref loc = internal::locale_ref())
      : out_(out), args_(ctx_args), loc_(loc) {}

  format_arg arg(int id) const { return args_.get(id); }

  // Checks if manual indexing is used and returns the argument with the
  // specified name.
  format_arg arg(basic_string_view<char_type> name);

  internal::error_handler error_handler() { return {}; }
  void on_error(const char* message) { error_handler().on_error(message); }

  // Returns an iterator to the beginning of the output range.
  iterator out() { return out_; }

  // Advances the begin iterator to ``it``.
  void advance_to(iterator it) { out_ = it; }

  internal::locale_ref locale() { return loc_; }
};

template <typename Char>
using buffer_context =
    basic_format_context<std::back_insert_iterator<internal::buffer<Char>>,
                         Char>;
using format_context = buffer_context<char>;
using wformat_context = buffer_context<wchar_t>;

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
  static const bool is_packed = num_args < internal::max_packed_args;

  using value_type = conditional_t<is_packed, internal::value<Context>,
                                   basic_format_arg<Context>>;

  // If the arguments are not packed, add one more element to mark the end.
  value_type data_[num_args + (num_args == 0 ? 1 : 0)];

  friend class basic_format_args<Context>;

 public:
  static constexpr unsigned long long types =
      is_packed ? internal::encode_types<Context, Args...>()
                : internal::is_unpacked_bit | num_args;

  format_arg_store(const Args&... args)
      :
#if FMT_GCC_VERSION && FMT_GCC_VERSION < 409
        basic_format_args<Context>(*this),
#endif
        data_{internal::make_arg<is_packed, Context>(args)...} {
  }
};

/**
  \rst
  Constructs an `~fmt::format_arg_store` object that contains references to
  arguments and can be implicitly converted to `~fmt::format_args`. `Context`
  can be omitted in which case it defaults to `~fmt::context`.
  See `~fmt::arg` for lifetime considerations.
  \endrst
 */
template <typename Context = format_context, typename... Args>
inline format_arg_store<Context, Args...> make_format_args(
    const Args&... args) {
  return {args...};
}

/**
  \rst
  A dynamic version of `fmt::format_arg_store<>`.
  It's equipped with a storage to potentially temporary objects which lifetime
  could be shorter than the format arguments object.

  It can be implicitly converted into `~fmt::basic_format_args` for passing
  into type-erased formatting functions such as `~fmt::vformat`.
  \endrst
 */
template <typename Context>
class dynamic_format_arg_store
#if FMT_GCC_VERSION && FMT_GCC_VERSION < 409
    // Workaround a GCC template argument substitution bug.
    : public basic_format_args<Context>
#endif
{
 private:
  using char_type = typename Context::char_type;

  template <typename T> struct need_copy {
    static constexpr internal::type mapped_type =
        internal::mapped_type_constant<T, Context>::value;

    enum {
      value = !(internal::is_reference_wrapper<T>::value ||
                std::is_same<T, basic_string_view<char_type>>::value ||
                std::is_same<T, internal::std_string_view<char_type>>::value ||
                (mapped_type != internal::type::cstring_type &&
                 mapped_type != internal::type::string_type &&
                 mapped_type != internal::type::custom_type &&
                 mapped_type != internal::type::named_arg_type))
    };
  };

  template <typename T>
  using stored_type = conditional_t<internal::is_string<T>::value,
                                    std::basic_string<char_type>, T>;

  // Storage of basic_format_arg must be contiguous.
  std::vector<basic_format_arg<Context>> data_;

  // Storage of arguments not fitting into basic_format_arg must grow
  // without relocation because items in data_ refer to it.
  internal::dynamic_arg_list dynamic_args_;

  friend class basic_format_args<Context>;

  unsigned long long get_types() const {
    return internal::is_unpacked_bit | data_.size();
  }

  template <typename T> void emplace_arg(const T& arg) {
    data_.emplace_back(internal::make_arg<Context>(arg));
  }

 public:
  /**
    \rst
    Adds an argument into the dynamic store for later passing to a formating
    function.

    Note that custom types and string types (but not string views!) are copied
    into the store with dynamic memory (in addition to resizing vector).

    **Example**::

      fmt::dynamic_format_arg_store<fmt::format_context> store;
      store.push_back(42);
      store.push_back("abc");
      store.push_back(1.5f);
      std::string result = fmt::vformat("{} and {} and {}", store);
    \endrst
  */
  template <typename T> void push_back(const T& arg) {
    static_assert(
        !std::is_base_of<internal::named_arg_base<char_type>, T>::value,
        "named arguments are not supported yet");
    if (internal::const_check(need_copy<T>::value))
      emplace_arg(dynamic_args_.push<stored_type<T>>(arg));
    else
      emplace_arg(arg);
  }

  /**
    Adds a reference to the argument into the dynamic store for later passing to
    a formating function.
  */
  template <typename T> void push_back(std::reference_wrapper<T> arg) {
    static_assert(
        need_copy<T>::value,
        "objects of built-in types and string views are always copied");
    emplace_arg(arg.get());
  }
};

/**
  \rst
  A view of a collection of formatting arguments. To avoid lifetime issues it
  should only be used as a parameter type in type-erased functions such as
  ``vformat``::

    void vlog(string_view format_str, format_args args);  // OK
    format_args args = make_format_args(42);  // Error: dangling reference
  \endrst
 */
template <typename Context> class basic_format_args {
 public:
  using size_type = int;
  using format_arg = basic_format_arg<Context>;

 private:
  // To reduce compiled code size per formatting function call, types of first
  // max_packed_args arguments are passed in the types_ field.
  unsigned long long types_;
  union {
    // If the number of arguments is less than max_packed_args, the argument
    // values are stored in values_, otherwise they are stored in args_.
    // This is done to reduce compiled code size as storing larger objects
    // may require more code (at least on x86-64) even if the same amount of
    // data is actually copied to stack. It saves ~10% on the bloat test.
    const internal::value<Context>* values_;
    const format_arg* args_;
  };

  bool is_packed() const { return (types_ & internal::is_unpacked_bit) == 0; }

  internal::type type(int index) const {
    int shift = index * internal::packed_arg_bits;
    unsigned int mask = (1 << internal::packed_arg_bits) - 1;
    return static_cast<internal::type>((types_ >> shift) & mask);
  }

  friend class internal::arg_map<Context>;

  void set_data(const internal::value<Context>* values) { values_ = values; }
  void set_data(const format_arg* args) { args_ = args; }

  format_arg do_get(int index) const {
    format_arg arg;
    if (!is_packed()) {
      auto num_args = max_size();
      if (index < num_args) arg = args_[index];
      return arg;
    }
    if (index > internal::max_packed_args) return arg;
    arg.type_ = type(index);
    if (arg.type_ == internal::type::none_type) return arg;
    internal::value<Context>& val = arg.value_;
    val = values_[index];
    return arg;
  }

 public:
  basic_format_args() : types_(0) {}

  /**
   \rst
   Constructs a `basic_format_args` object from `~fmt::format_arg_store`.
   \endrst
   */
  template <typename... Args>
  basic_format_args(const format_arg_store<Context, Args...>& store)
      : types_(store.types) {
    set_data(store.data_);
  }

  /**
   \rst
   Constructs a `basic_format_args` object from
   `~fmt::dynamic_format_arg_store`.
   \endrst
   */
  basic_format_args(const dynamic_format_arg_store<Context>& store)
      : types_(store.get_types()) {
    set_data(store.data_.data());
  }

  /**
   \rst
   Constructs a `basic_format_args` object from a dynamic set of arguments.
   \endrst
   */
  basic_format_args(const format_arg* args, int count)
      : types_(internal::is_unpacked_bit | internal::to_unsigned(count)) {
    set_data(args);
  }

  /** Returns the argument at specified index. */
  format_arg get(int index) const {
    format_arg arg = do_get(index);
    if (arg.type_ == internal::type::named_arg_type)
      arg = arg.value_.named_arg->template deserialize<Context>();
    return arg;
  }

  int max_size() const {
    unsigned long long max_packed = internal::max_packed_args;
    return static_cast<int>(is_packed() ? max_packed
                                        : types_ & ~internal::is_unpacked_bit);
  }
};

/** An alias to ``basic_format_args<context>``. */
// It is a separate type rather than an alias to make symbols readable.
struct format_args : basic_format_args<format_context> {
  template <typename... Args>
  format_args(Args&&... args)
      : basic_format_args<format_context>(static_cast<Args&&>(args)...) {}
};
struct wformat_args : basic_format_args<wformat_context> {
  template <typename... Args>
  wformat_args(Args&&... args)
      : basic_format_args<wformat_context>(static_cast<Args&&>(args)...) {}
};

template <typename Container> struct is_contiguous : std::false_type {};

template <typename Char>
struct is_contiguous<std::basic_string<Char>> : std::true_type {};

template <typename Char>
struct is_contiguous<internal::buffer<Char>> : std::true_type {};

namespace internal {

template <typename OutputIt>
struct is_contiguous_back_insert_iterator : std::false_type {};
template <typename Container>
struct is_contiguous_back_insert_iterator<std::back_insert_iterator<Container>>
    : is_contiguous<Container> {};

template <typename Char> struct named_arg_base {
  basic_string_view<Char> name;

  // Serialized value<context>.
  mutable char data[sizeof(basic_format_arg<buffer_context<Char>>)];

  named_arg_base(basic_string_view<Char> nm) : name(nm) {}

  template <typename Context> basic_format_arg<Context> deserialize() const {
    basic_format_arg<Context> arg;
    std::memcpy(&arg, data, sizeof(basic_format_arg<Context>));
    return arg;
  }
};

struct view {};

template <typename T, typename Char>
struct named_arg : view, named_arg_base<Char> {
  const T& value;

  named_arg(basic_string_view<Char> name, const T& val)
      : named_arg_base<Char>(name), value(val) {}
};

template <typename..., typename S, FMT_ENABLE_IF(!is_compile_string<S>::value)>
inline void check_format_string(const S&) {
#if defined(FMT_ENFORCE_COMPILE_STRING)
  static_assert(is_compile_string<S>::value,
                "FMT_ENFORCE_COMPILE_STRING requires all format strings to "
                "utilize FMT_STRING() or fmt().");
#endif
}
template <typename..., typename S, FMT_ENABLE_IF(is_compile_string<S>::value)>
void check_format_string(S);

template <bool...> struct bool_pack;
template <bool... Args>
using all_true =
    std::is_same<bool_pack<Args..., true>, bool_pack<true, Args...>>;

template <typename... Args, typename S, typename Char = char_t<S>>
inline format_arg_store<buffer_context<Char>, remove_reference_t<Args>...>
make_args_checked(const S& format_str,
                  const remove_reference_t<Args>&... args) {
  static_assert(
      all_true<(!std::is_base_of<view, remove_reference_t<Args>>::value ||
                !std::is_reference<Args>::value)...>::value,
      "passing views as lvalues is disallowed");
  check_format_string<Args...>(format_str);
  return {args...};
}

template <typename Char>
std::basic_string<Char> vformat(
    basic_string_view<Char> format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args);

template <typename Char>
typename buffer_context<Char>::iterator vformat_to(
    buffer<Char>& buf, basic_string_view<Char> format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args);

template <typename Char, typename Args,
          FMT_ENABLE_IF(!std::is_same<Char, char>::value)>
inline void vprint_mojibake(std::FILE*, basic_string_view<Char>, const Args&) {}

FMT_API void vprint_mojibake(std::FILE*, string_view, format_args);
#ifndef _WIN32
inline void vprint_mojibake(std::FILE*, string_view, format_args) {}
#endif
}  // namespace internal

/**
  \rst
  Returns a named argument to be used in a formatting function. It should only
  be used in a call to a formatting function.

  **Example**::

    fmt::print("Elapsed time: {s:.2f} seconds", fmt::arg("s", 1.23));
  \endrst
 */
template <typename S, typename T, typename Char = char_t<S>>
inline internal::named_arg<T, Char> arg(const S& name, const T& arg) {
  static_assert(internal::is_string<S>::value, "");
  return {name, arg};
}

// Disable nested named arguments, e.g. ``arg("a", arg("b", 42))``.
template <typename S, typename T, typename Char>
void arg(S, internal::named_arg<T, Char>) = delete;

/** Formats a string and writes the output to ``out``. */
// GCC 8 and earlier cannot handle std::back_insert_iterator<Container> with
// vformat_to<ArgFormatter>(...) overload, so SFINAE on iterator type instead.
template <typename OutputIt, typename S, typename Char = char_t<S>,
          FMT_ENABLE_IF(
              internal::is_contiguous_back_insert_iterator<OutputIt>::value)>
OutputIt vformat_to(
    OutputIt out, const S& format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args) {
  using container = remove_reference_t<decltype(internal::get_container(out))>;
  internal::container_buffer<container> buf((internal::get_container(out)));
  internal::vformat_to(buf, to_string_view(format_str), args);
  return out;
}

template <typename Container, typename S, typename... Args,
          FMT_ENABLE_IF(
              is_contiguous<Container>::value&& internal::is_string<S>::value)>
inline std::back_insert_iterator<Container> format_to(
    std::back_insert_iterator<Container> out, const S& format_str,
    Args&&... args) {
  return vformat_to(out, to_string_view(format_str),
                    internal::make_args_checked<Args...>(format_str, args...));
}

template <typename S, typename Char = char_t<S>>
inline std::basic_string<Char> vformat(
    const S& format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args) {
  return internal::vformat(to_string_view(format_str), args);
}

/**
  \rst
  Formats arguments and returns the result as a string.

  **Example**::

    #include <fmt/core.h>
    std::string message = fmt::format("The answer is {}", 42);
  \endrst
*/
// Pass char_t as a default template parameter instead of using
// std::basic_string<char_t<S>> to reduce the symbol size.
template <typename S, typename... Args, typename Char = char_t<S>>
inline std::basic_string<Char> format(const S& format_str, Args&&... args) {
  return internal::vformat(
      to_string_view(format_str),
      internal::make_args_checked<Args...>(format_str, args...));
}

FMT_API void vprint(string_view, format_args);
FMT_API void vprint(std::FILE*, string_view, format_args);

/**
  \rst
  Formats ``args`` according to specifications in ``format_str`` and writes the
  output to the file ``f``. Strings are assumed to be Unicode-encoded unless the
  ``FMT_UNICODE`` macro is set to 0.

  **Example**::

    fmt::print(stderr, "Don't {}!", "panic");
  \endrst
 */
template <typename S, typename... Args, typename Char = char_t<S>>
inline void print(std::FILE* f, const S& format_str, Args&&... args) {
  return internal::is_unicode<Char>()
             ? vprint(f, to_string_view(format_str),
                      internal::make_args_checked<Args...>(format_str, args...))
             : internal::vprint_mojibake(
                   f, to_string_view(format_str),
                   internal::make_args_checked<Args...>(format_str, args...));
}

/**
  \rst
  Formats ``args`` according to specifications in ``format_str`` and writes
  the output to ``stdout``. Strings are assumed to be Unicode-encoded unless
  the ``FMT_UNICODE`` macro is set to 0.

  **Example**::

    fmt::print("Elapsed time: {0:.2f} seconds", 1.23);
  \endrst
 */
template <typename S, typename... Args, typename Char = char_t<S>>
inline void print(const S& format_str, Args&&... args) {
  return internal::is_unicode<Char>()
             ? vprint(to_string_view(format_str),
                      internal::make_args_checked<Args...>(format_str, args...))
             : internal::vprint_mojibake(
                   stdout, to_string_view(format_str),
                   internal::make_args_checked<Args...>(format_str, args...));
}
FMT_END_NAMESPACE

#endif  // FMT_CORE_H_
