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
#define FMT_VERSION 70001

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

#if defined(__INTEL_COMPILER)
#  define FMT_ICC_VERSION __INTEL_COMPILER
#else
#  define FMT_ICC_VERSION 0
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
#  define FMT_SUPPRESS_MSC_WARNING(n) __pragma(warning(suppress : n))
#else
#  define FMT_MSC_VER 0
#  define FMT_SUPPRESS_MSC_WARNING(n)
#endif
#ifdef __has_feature
#  define FMT_HAS_FEATURE(x) __has_feature(x)
#else
#  define FMT_HAS_FEATURE(x) 0
#endif

#if defined(__has_include) && !defined(__INTELLISENSE__) && \
    !(FMT_ICC_VERSION && FMT_ICC_VERSION < 1600)
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

// Check if relaxed C++14 constexpr is supported.
// GCC doesn't allow throw in constexpr until version 6 (bug 67371).
#ifndef FMT_USE_CONSTEXPR
#  define FMT_USE_CONSTEXPR                                           \
    (FMT_HAS_FEATURE(cxx_relaxed_constexpr) || FMT_MSC_VER >= 1910 || \
     (FMT_GCC_VERSION >= 600 && __cplusplus >= 201402L)) &&           \
        !FMT_NVCC && !FMT_ICC_VERSION
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
#if FMT_ICC_VERSION || defined(__PGI) || FMT_NVCC
#  define FMT_DEPRECATED_ALIAS
#else
#  define FMT_DEPRECATED_ALIAS FMT_DEPRECATED
#endif

#ifndef FMT_INLINE
#  if FMT_GCC_VERSION || FMT_CLANG_VERSION
#    define FMT_INLINE inline __attribute__((always_inline))
#  else
#    define FMT_INLINE inline
#  endif
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
      using namespace v7;     \
      }
#  endif
#  define FMT_BEGIN_NAMESPACE \
    namespace fmt {           \
    FMT_INLINE_NAMESPACE v7 {
#endif

#if !defined(FMT_HEADER_ONLY) && defined(_WIN32)
#  define FMT_CLASS_API FMT_SUPPRESS_MSC_WARNING(4275)
#  ifdef FMT_EXPORT
#    define FMT_API __declspec(dllexport)
#    define FMT_EXTERN_TEMPLATE_API FMT_API
#    define FMT_EXPORTED
#  elif defined(FMT_SHARED)
#    define FMT_API __declspec(dllimport)
#    define FMT_EXTERN_TEMPLATE_API FMT_API
#  endif
#else
#  define FMT_CLASS_API
#endif
#ifndef FMT_API
#  define FMT_API
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

namespace detail {

// A helper function to suppress bogus "conditional expression is constant"
// warnings.
template <typename T> constexpr T const_check(T value) { return value; }

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
           : ::fmt::detail::assert_fail(__FILE__, __LINE__, (message)))
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

FMT_SUPPRESS_MSC_WARNING(4566) constexpr unsigned char micro[] = "\u00B5";

template <typename Char> constexpr bool is_unicode() {
  return FMT_UNICODE || sizeof(Char) != 1 ||
         (sizeof(micro) == 3 && micro[0] == 0xC2 && micro[1] == 0xB5);
}

#ifdef __cpp_char8_t
using char8_type = char8_t;
#else
enum char8_type : unsigned char {};
#endif
}  // namespace detail

#ifdef FMT_USE_INTERNAL
namespace internal = detail;  // DEPRECATED
#endif

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
  using value_type = Char;
  using iterator = const Char*;

  constexpr basic_string_view() FMT_NOEXCEPT : data_(nullptr), size_(0) {}

  /** Constructs a string reference object from a C string and a size. */
  constexpr basic_string_view(const Char* s, size_t count) FMT_NOEXCEPT
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

  template <typename S, FMT_ENABLE_IF(std::is_same<
                                      S, detail::std_string_view<Char>>::value)>
  FMT_CONSTEXPR basic_string_view(S s) FMT_NOEXCEPT : data_(s.data()),
                                                      size_(s.size()) {}

  /** Returns a pointer to the string data. */
  constexpr const Char* data() const { return data_; }

  /** Returns the string size. */
  constexpr size_t size() const { return size_; }

  constexpr iterator begin() const { return data_; }
  constexpr iterator end() const { return data_ + size_; }

  constexpr const Char& operator[](size_t pos) const { return data_[pos]; }

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

/** Specifies if ``T`` is a character type. Can be specialized by users. */
template <typename T> struct is_char : std::false_type {};
template <> struct is_char<char> : std::true_type {};
template <> struct is_char<wchar_t> : std::true_type {};
template <> struct is_char<detail::char8_type> : std::true_type {};
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
          FMT_ENABLE_IF(!std::is_empty<detail::std_string_view<Char>>::value)>
inline basic_string_view<Char> to_string_view(detail::std_string_view<Char> s) {
  return s;
}

// A base class for compile-time strings. It is defined in the fmt namespace to
// make formatting functions visible via ADL, e.g. format(FMT_STRING("{}"), 42).
struct compile_string {};

template <typename S>
struct is_compile_string : std::is_base_of<compile_string, S> {};

template <typename S, FMT_ENABLE_IF(is_compile_string<S>::value)>
constexpr basic_string_view<typename S::char_type> to_string_view(const S& s) {
  return s;
}

namespace detail {
void to_string_view(...);
using fmt::v7::to_string_view;

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
  constexpr error_handler() = default;
  constexpr error_handler(const error_handler&) = default;

  // This function is intentionally not constexpr to give a compile-time error.
  FMT_NORETURN FMT_API void on_error(const char* message);
};
}  // namespace detail

/** String's character type. */
template <typename S> using char_t = typename detail::char_t_impl<S>::type;

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
template <typename Char, typename ErrorHandler = detail::error_handler>
class basic_format_parse_context : private ErrorHandler {
 private:
  basic_string_view<Char> format_str_;
  int next_arg_id_;

 public:
  using char_type = Char;
  using iterator = typename basic_string_view<Char>::iterator;

  explicit constexpr basic_format_parse_context(
      basic_string_view<Char> format_str, ErrorHandler eh = {})
      : ErrorHandler(eh), format_str_(format_str), next_arg_id_(0) {}

  /**
    Returns an iterator to the beginning of the format string range being
    parsed.
   */
  constexpr iterator begin() const FMT_NOEXCEPT { return format_str_.begin(); }

  /**
    Returns an iterator past the end of the format string range being parsed.
   */
  constexpr iterator end() const FMT_NOEXCEPT { return format_str_.end(); }

  /** Advances the begin iterator to ``it``. */
  FMT_CONSTEXPR void advance_to(iterator it) {
    format_str_.remove_prefix(detail::to_unsigned(it - begin()));
  }

  /**
    Reports an error if using the manual argument indexing; otherwise returns
    the next argument index and switches to the automatic indexing.
   */
  FMT_CONSTEXPR int next_arg_id() {
    // Don't check if the argument id is valid to avoid overhead and because it
    // will be checked during formatting anyway.
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

  constexpr ErrorHandler error_handler() const { return *this; }
};

using format_parse_context = basic_format_parse_context<char>;
using wformat_parse_context = basic_format_parse_context<wchar_t>;

template <typename Context> class basic_format_arg;
template <typename Context> class basic_format_args;
template <typename Context> class dynamic_format_arg_store;

// A formatter for objects of type T.
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

namespace detail {

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
  FMT_SUPPRESS_MSC_WARNING(26495)
  buffer(size_t sz) FMT_NOEXCEPT : size_(sz), capacity_(sz) {}

  buffer(T* p = nullptr, size_t sz = 0, size_t cap = 0) FMT_NOEXCEPT
      : ptr_(p),
        size_(sz),
        capacity_(cap) {}

  /** Sets the buffer data and capacity. */
  void set(T* buf_data, size_t buf_capacity) FMT_NOEXCEPT {
    ptr_ = buf_data;
    capacity_ = buf_capacity;
  }

  /** Increases the buffer capacity to hold at least *capacity* elements. */
  virtual void grow(size_t capacity) = 0;

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
  size_t size() const FMT_NOEXCEPT { return size_; }

  /** Returns the capacity of this buffer. */
  size_t capacity() const FMT_NOEXCEPT { return capacity_; }

  /** Returns a pointer to the buffer data. */
  T* data() FMT_NOEXCEPT { return ptr_; }

  /** Returns a pointer to the buffer data. */
  const T* data() const FMT_NOEXCEPT { return ptr_; }

  /**
    Resizes the buffer. If T is a POD type new elements may not be initialized.
   */
  void resize(size_t new_size) {
    reserve(new_size);
    size_ = new_size;
  }

  /** Clears this buffer. */
  void clear() { size_ = 0; }

  /** Reserves space to store at least *capacity* elements. */
  void reserve(size_t new_capacity) {
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
  void grow(size_t capacity) FMT_OVERRIDE {
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
  T args_[1 + (NUM_ARGS != 0 ? NUM_ARGS : 1)];
  named_arg_info<Char> named_args_[NUM_NAMED_ARGS];

  template <typename... U>
  arg_data(const U&... init) : args_{T(named_args_, NUM_NAMED_ARGS), init...} {}
  arg_data(const arg_data& other) = delete;
  const T* args() const { return args_ + 1; }
  named_arg_info<Char>* named_args() { return named_args_; }
};

template <typename T, typename Char, size_t NUM_ARGS>
struct arg_data<T, Char, NUM_ARGS, 0> {
  T args_[NUM_ARGS != 0 ? NUM_ARGS : 1];

  template <typename... U>
  FMT_INLINE arg_data(const U&... init) : args_{init...} {}
  FMT_INLINE const T* args() const { return args_; }
  FMT_INLINE std::nullptr_t named_args() { return nullptr; }
};

template <typename Char>
inline void init_named_args(named_arg_info<Char>*, int, int) {}

template <typename Char, typename T, typename... Tail>
void init_named_args(named_arg_info<Char>* named_args, int arg_count,
                     int named_arg_count, const T&, const Tail&... args) {
  init_named_args(named_args, arg_count + 1, named_arg_count, args...);
}

template <typename Char, typename T, typename... Tail>
void init_named_args(named_arg_info<Char>* named_args, int arg_count,
                     int named_arg_count, const named_arg<Char, T>& arg,
                     const Tail&... args) {
  named_args[named_arg_count++] = {arg.name, arg_count};
  init_named_args(named_args, arg_count + 1, named_arg_count, args...);
}

template <typename... Args>
FMT_INLINE void init_named_args(std::nullptr_t, int, int, const Args&...) {}

template <typename T> struct is_named_arg : std::false_type {};

template <typename T, typename Char>
struct is_named_arg<named_arg<Char, T>> : std::true_type {};

template <bool B = false> constexpr size_t count() { return B ? 1 : 0; }
template <bool B1, bool B2, bool... Tail> constexpr size_t count() {
  return (B1 ? 1 : 0) + count<B2, Tail...>();
}

template <typename... Args> constexpr size_t count_named_args() {
  return count<is_named_arg<Args>::value...>();
}

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

constexpr bool is_integral_type(type t) {
  return t > type::none_type && t <= type::last_integer_type;
}

constexpr bool is_arithmetic_type(type t) {
  return t > type::none_type && t <= type::last_numeric_type;
}

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
    named_arg_value<char_type> named_args;
  };

  constexpr FMT_INLINE value(int val = 0) : int_value(val) {}
  constexpr FMT_INLINE value(unsigned val) : uint_value(val) {}
  FMT_INLINE value(long long val) : long_long_value(val) {}
  FMT_INLINE value(unsigned long long val) : ulong_long_value(val) {}
  FMT_INLINE value(int128_t val) : int128_value(val) {}
  FMT_INLINE value(uint128_t val) : uint128_value(val) {}
  FMT_INLINE value(float val) : float_value(val) {}
  FMT_INLINE value(double val) : double_value(val) {}
  FMT_INLINE value(long double val) : long_double_value(val) {}
  FMT_INLINE value(bool val) : bool_value(val) {}
  FMT_INLINE value(char_type val) : char_value(val) {}
  FMT_INLINE value(const char_type* val) { string.data = val; }
  FMT_INLINE value(basic_string_view<char_type> val) {
    string.data = val.data();
    string.size = val.size();
  }
  FMT_INLINE value(const void* val) : pointer(val) {}
  FMT_INLINE value(const named_arg_info<char_type>* args, size_t size)
      : named_args{args, size} {}

  template <typename T> FMT_INLINE value(const T& val) {
    custom.value = &val;
    // Get the formatter type through the context to allow different contexts
    // have different extension points, e.g. `formatter<T>` for `format` and
    // `printf_formatter<T>` for `printf`.
    custom.format = format_custom_arg<
        T, conditional_t<has_formatter<T, Context>::value,
                         typename Context::template formatter_type<T>,
                         fallback_formatter<T, char_type>>>;
  }

 private:
  // Formats an argument of a custom type, such as a user-defined class.
  template <typename T, typename Formatter>
  static void format_custom_arg(const void* arg,
                                typename Context::parse_context_type& parse_ctx,
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
  FMT_CONSTEXPR const char* map(signed char* val) {
    const auto* const_val = val;
    return map(const_val);
  }
  FMT_CONSTEXPR const char* map(unsigned char* val) {
    const auto* const_val = val;
    return map(const_val);
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
  FMT_CONSTEXPR auto map(const named_arg<char_type, T>& val)
      -> decltype(std::declval<arg_mapper>().map(val.value)) {
    return map(val.value);
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

enum { packed_arg_bits = 4 };
// Maximum number of arguments with packed types.
enum { max_packed_args = 62 / packed_arg_bits };
enum : unsigned long long { is_unpacked_bit = 1ULL << 63 };
enum : unsigned long long { has_named_args_bit = 1ULL << 62 };
}  // namespace detail

// A formatting argument. It is a trivially copyable/constructible type to
// allow storage in basic_memory_buffer.
template <typename Context> class basic_format_arg {
 private:
  detail::value<Context> value_;
  detail::type type_;

  template <typename ContextType, typename T>
  friend FMT_CONSTEXPR basic_format_arg<ContextType> detail::make_arg(
      const T& value);

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

  constexpr explicit operator bool() const FMT_NOEXCEPT {
    return type_ != detail::type::none_type;
  }

  detail::type type() const { return type_; }

  bool is_integral() const { return detail::is_integral_type(type_); }
  bool is_arithmetic() const { return detail::is_arithmetic_type(type_); }
};

/**
  \rst
  Visits an argument dispatching to the appropriate visit method based on
  the argument type. For example, if the argument type is ``double`` then
  ``vis(value)`` will be called with the value of type ``double``.
  \endrst
 */
template <typename Visitor, typename Context>
FMT_CONSTEXPR_DECL FMT_INLINE auto visit_format_arg(
    Visitor&& vis, const basic_format_arg<Context>& arg) -> decltype(vis(0)) {
  using char_type = typename Context::char_type;
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
#if FMT_USE_INT128
  case detail::type::int128_type:
    return vis(arg.value_.int128_value);
  case detail::type::uint128_type:
    return vis(arg.value_.uint128_value);
#else
  case detail::type::int128_type:
  case detail::type::uint128_type:
    break;
#endif
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
    return vis(basic_string_view<char_type>(arg.value_.string.data,
                                            arg.value_.string.size));
  case detail::type::pointer_type:
    return vis(arg.value_.pointer);
  case detail::type::custom_type:
    return vis(typename basic_format_arg<Context>::handle(arg.value_.custom));
  }
  return vis(monostate());
}

// Checks whether T is a container with contiguous storage.
template <typename T> struct is_contiguous : std::false_type {};
template <typename Char>
struct is_contiguous<std::basic_string<Char>> : std::true_type {};
template <typename Char>
struct is_contiguous<detail::buffer<Char>> : std::true_type {};

namespace detail {

template <typename OutputIt>
struct is_back_insert_iterator : std::false_type {};
template <typename Container>
struct is_back_insert_iterator<std::back_insert_iterator<Container>>
    : std::true_type {};

template <typename OutputIt>
struct is_contiguous_back_insert_iterator : std::false_type {};
template <typename Container>
struct is_contiguous_back_insert_iterator<std::back_insert_iterator<Container>>
    : is_contiguous<Container> {};

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

// The type template parameter is there to avoid an ODR violation when using
// a fallback formatter in one translation unit and an implicit conversion in
// another (not recommended).
template <bool IS_PACKED, typename Context, type, typename T,
          FMT_ENABLE_IF(IS_PACKED)>
inline value<Context> make_arg(const T& val) {
  return arg_mapper<Context>().map(val);
}

template <bool IS_PACKED, typename Context, type, typename T,
          FMT_ENABLE_IF(!IS_PACKED)>
inline basic_format_arg<Context> make_arg(const T& value) {
  return make_arg<Context>(value);
}

template <typename T> struct is_reference_wrapper : std::false_type {};
template <typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T> const T& unwrap(const T& v) { return v; }
template <typename T> const T& unwrap(const std::reference_wrapper<T>& v) {
  return static_cast<const T&>(v);
}

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
    auto new_node = std::unique_ptr<typed_node<T>>(new typed_node<T>(arg));
    auto& value = new_node->value;
    new_node->next = std::move(head_);
    head_ = std::move(new_node);
    return value;
  }
};
}  // namespace detail

// Formatting context.
template <typename OutputIt, typename Char> class basic_format_context {
 public:
  /** The character type for the output. */
  using char_type = Char;

 private:
  OutputIt out_;
  basic_format_args<basic_format_context> args_;
  detail::locale_ref loc_;

 public:
  using iterator = OutputIt;
  using format_arg = basic_format_arg<basic_format_context>;
  using parse_context_type = basic_format_parse_context<Char>;
  template <typename T> using formatter_type = formatter<T, char_type>;

  basic_format_context(const basic_format_context&) = delete;
  void operator=(const basic_format_context&) = delete;
  /**
   Constructs a ``basic_format_context`` object. References to the arguments are
   stored in the object so make sure they have appropriate lifetimes.
   */
  basic_format_context(OutputIt out,
                       basic_format_args<basic_format_context> ctx_args,
                       detail::locale_ref loc = detail::locale_ref())
      : out_(out), args_(ctx_args), loc_(loc) {}

  format_arg arg(int id) const { return args_.get(id); }
  format_arg arg(basic_string_view<char_type> name) { return args_.get(name); }
  int arg_id(basic_string_view<char_type> name) { return args_.get_id(name); }
  const basic_format_args<basic_format_context>& args() const { return args_; }

  detail::error_handler error_handler() { return {}; }
  void on_error(const char* message) { error_handler().on_error(message); }

  // Returns an iterator to the beginning of the output range.
  iterator out() { return out_; }

  // Advances the begin iterator to ``it``.
  void advance_to(iterator it) {
    if (!detail::is_back_insert_iterator<iterator>()) out_ = it;
  }

  detail::locale_ref locale() { return loc_; }
};

template <typename Char>
using buffer_context =
    basic_format_context<std::back_insert_iterator<detail::buffer<Char>>, Char>;
using format_context = buffer_context<char>;
using wformat_context = buffer_context<wchar_t>;

// Workaround a bug in gcc: https://stackoverflow.com/q/62767544/471164.
#define FMT_BUFFER_CONTEXT(Char) \
  basic_format_context<std::back_insert_iterator<detail::buffer<Char>>, Char>

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
  static const size_t num_named_args = detail::count_named_args<Args...>();
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
  format_arg_store(const Args&... args)
      :
#if FMT_GCC_VERSION && FMT_GCC_VERSION < 409
        basic_format_args<Context>(*this),
#endif
        data_{detail::make_arg<
            is_packed, Context,
            detail::mapped_type_constant<Args, Context>::value>(args)...} {
    detail::init_named_args(data_.named_args(), 0, 0, args...);
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
  Returns a named argument to be used in a formatting function. It should only
  be used in a call to a formatting function.

  **Example**::

    fmt::print("Elapsed time: {s:.2f} seconds", fmt::arg("s", 1.23));
  \endrst
 */
template <typename Char, typename T>
inline detail::named_arg<Char, T> arg(const Char* name, const T& arg) {
  static_assert(!detail::is_named_arg<T>(), "nested named arguments");
  return {name, arg};
}

/**
  \rst
  A dynamic version of `fmt::format_arg_store`.
  It's equipped with a storage to potentially temporary objects which lifetimes
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
    static constexpr detail::type mapped_type =
        detail::mapped_type_constant<T, Context>::value;

    enum {
      value = !(detail::is_reference_wrapper<T>::value ||
                std::is_same<T, basic_string_view<char_type>>::value ||
                std::is_same<T, detail::std_string_view<char_type>>::value ||
                (mapped_type != detail::type::cstring_type &&
                 mapped_type != detail::type::string_type &&
                 mapped_type != detail::type::custom_type))
    };
  };

  template <typename T>
  using stored_type = conditional_t<detail::is_string<T>::value,
                                    std::basic_string<char_type>, T>;

  // Storage of basic_format_arg must be contiguous.
  std::vector<basic_format_arg<Context>> data_;
  std::vector<detail::named_arg_info<char_type>> named_info_;

  // Storage of arguments not fitting into basic_format_arg must grow
  // without relocation because items in data_ refer to it.
  detail::dynamic_arg_list dynamic_args_;

  friend class basic_format_args<Context>;

  unsigned long long get_types() const {
    return detail::is_unpacked_bit | data_.size() |
           (named_info_.empty()
                ? 0ULL
                : static_cast<unsigned long long>(detail::has_named_args_bit));
  }

  const basic_format_arg<Context>* data() const {
    return named_info_.empty() ? data_.data() : data_.data() + 1;
  }

  template <typename T> void emplace_arg(const T& arg) {
    data_.emplace_back(detail::make_arg<Context>(arg));
  }

  template <typename T>
  void emplace_arg(const detail::named_arg<char_type, T>& arg) {
    if (named_info_.empty()) {
      constexpr const detail::named_arg_info<char_type>* zero_ptr{nullptr};
      data_.insert(data_.begin(), {zero_ptr, 0});
    }
    data_.emplace_back(detail::make_arg<Context>(detail::unwrap(arg.value)));
    auto pop_one = [](std::vector<basic_format_arg<Context>>* data) {
      data->pop_back();
    };
    std::unique_ptr<std::vector<basic_format_arg<Context>>, decltype(pop_one)>
        guard{&data_, pop_one};
    named_info_.push_back({arg.name, static_cast<int>(data_.size() - 2u)});
    data_[0].value_.named_args = {named_info_.data(), named_info_.size()};
    guard.release();
  }

 public:
  /**
    \rst
    Adds an argument into the dynamic store for later passing to a formatting
    function.

    Note that custom types and string types (but not string views) are copied
    into the store dynamically allocating memory if necessary.

    **Example**::

      fmt::dynamic_format_arg_store<fmt::format_context> store;
      store.push_back(42);
      store.push_back("abc");
      store.push_back(1.5f);
      std::string result = fmt::vformat("{} and {} and {}", store);
    \endrst
  */
  template <typename T> void push_back(const T& arg) {
    if (detail::const_check(need_copy<T>::value))
      emplace_arg(dynamic_args_.push<stored_type<T>>(arg));
    else
      emplace_arg(detail::unwrap(arg));
  }

  /**
    \rst
    Adds a reference to the argument into the dynamic store for later passing to
    a formatting function. Supports named arguments wrapped in
    ``std::reference_wrapper`` via ``std::ref()``/``std::cref()``.

    **Example**::

      fmt::dynamic_format_arg_store<fmt::format_context> store;
      char str[] = "1234567890";
      store.push_back(std::cref(str));
      int a1_val{42};
      auto a1 = fmt::arg("a1_", a1_val);
      store.push_back(std::cref(a1));

      // Changing str affects the output but only for string and custom types.
      str[0] = 'X';

      std::string result = fmt::vformat("{} and {a1_}");
      assert(result == "X234567890 and 42");
    \endrst
  */
  template <typename T> void push_back(std::reference_wrapper<T> arg) {
    static_assert(
        detail::is_named_arg<typename std::remove_cv<T>::type>::value ||
            need_copy<T>::value,
        "objects of built-in types and string views are always copied");
    emplace_arg(arg.get());
  }

  /**
    Adds named argument into the dynamic store for later passing to a formatting
    function. ``std::reference_wrapper`` is supported to avoid copying of the
    argument.
  */
  template <typename T>
  void push_back(const detail::named_arg<char_type, T>& arg) {
    const char_type* arg_name =
        dynamic_args_.push<std::basic_string<char_type>>(arg.name).c_str();
    if (detail::const_check(need_copy<T>::value)) {
      emplace_arg(
          fmt::arg(arg_name, dynamic_args_.push<stored_type<T>>(arg.value)));
    } else {
      emplace_arg(fmt::arg(arg_name, arg.value));
    }
  }

  /** Erase all elements from the store */
  void clear() {
    data_.clear();
    named_info_.clear();
    dynamic_args_ = detail::dynamic_arg_list();
  }

  /**
    \rst
    Reserves space to store at least *new_cap* arguments including
    *new_cap_named* named arguments.
    \endrst
  */
  void reserve(size_t new_cap, size_t new_cap_named) {
    FMT_ASSERT(new_cap >= new_cap_named,
               "Set of arguments includes set of named arguments");
    data_.reserve(new_cap);
    named_info_.reserve(new_cap_named);
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

  bool is_packed() const { return (desc_ & detail::is_unpacked_bit) == 0; }
  bool has_named_args() const {
    return (desc_ & detail::has_named_args_bit) != 0;
  }

  detail::type type(int index) const {
    int shift = index * detail::packed_arg_bits;
    unsigned int mask = (1 << detail::packed_arg_bits) - 1;
    return static_cast<detail::type>((desc_ >> shift) & mask);
  }

  basic_format_args(unsigned long long desc,
                    const detail::value<Context>* values)
      : desc_(desc), values_(values) {}
  basic_format_args(unsigned long long desc, const format_arg* args)
      : desc_(desc), args_(args) {}

 public:
  basic_format_args() : desc_(0) {}

  /**
   \rst
   Constructs a `basic_format_args` object from `~fmt::format_arg_store`.
   \endrst
   */
  template <typename... Args>
  FMT_INLINE basic_format_args(const format_arg_store<Context, Args...>& store)
      : basic_format_args(store.desc, store.data_.args()) {}

  /**
   \rst
   Constructs a `basic_format_args` object from
   `~fmt::dynamic_format_arg_store`.
   \endrst
   */
  FMT_INLINE basic_format_args(const dynamic_format_arg_store<Context>& store)
      : basic_format_args(store.get_types(), store.data()) {}

  /**
   \rst
   Constructs a `basic_format_args` object from a dynamic set of arguments.
   \endrst
   */
  basic_format_args(const format_arg* args, int count)
      : basic_format_args(detail::is_unpacked_bit | detail::to_unsigned(count),
                          args) {}

  /** Returns the argument with the specified id. */
  format_arg get(int id) const {
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

  template <typename Char> format_arg get(basic_string_view<Char> name) const {
    int id = get_id(name);
    return id >= 0 ? get(id) : format_arg();
  }

  template <typename Char> int get_id(basic_string_view<Char> name) const {
    if (!has_named_args()) return {};
    const auto& named_args =
        (is_packed() ? values_[-1] : args_[-1].value_).named_args;
    for (size_t i = 0; i < named_args.size; ++i) {
      if (named_args.data[i].name == name) return named_args.data[i].id;
    }
    return -1;
  }

  int max_size() const {
    unsigned long long max_packed = detail::max_packed_args;
    return static_cast<int>(is_packed() ? max_packed
                                        : desc_ & ~detail::is_unpacked_bit);
  }
};

/** An alias to ``basic_format_args<context>``. */
// It is a separate type rather than an alias to make symbols readable.
struct format_args : basic_format_args<format_context> {
  template <typename... Args>
  FMT_INLINE format_args(const Args&... args) : basic_format_args(args...) {}
};
struct wformat_args : basic_format_args<wformat_context> {
  using basic_format_args::basic_format_args;
};

namespace detail {

// Reports a compile-time error if S is not a valid format string.
template <typename..., typename S, FMT_ENABLE_IF(!is_compile_string<S>::value)>
FMT_INLINE void check_format_string(const S&) {
#ifdef FMT_ENFORCE_COMPILE_STRING
  static_assert(is_compile_string<S>::value,
                "FMT_ENFORCE_COMPILE_STRING requires all format strings to use "
                "FMT_STRING.");
#endif
}
template <typename..., typename S, FMT_ENABLE_IF(is_compile_string<S>::value)>
void check_format_string(S);

template <typename... Args, typename S, typename Char = char_t<S>>
inline format_arg_store<buffer_context<Char>, remove_reference_t<Args>...>
make_args_checked(const S& format_str,
                  const remove_reference_t<Args>&... args) {
  static_assert(count<(std::is_base_of<view, remove_reference_t<Args>>::value &&
                       std::is_reference<Args>::value)...>() == 0,
                "passing views as lvalues is disallowed");
  check_format_string<Args...>(format_str);
  return {args...};
}

template <typename Char, FMT_ENABLE_IF(!std::is_same<Char, char>::value)>
std::basic_string<Char> vformat(
    basic_string_view<Char> format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args);

FMT_API std::string vformat(string_view format_str, format_args args);

template <typename Char>
typename FMT_BUFFER_CONTEXT(Char)::iterator vformat_to(
    buffer<Char>& buf, basic_string_view<Char> format_str,
    basic_format_args<FMT_BUFFER_CONTEXT(type_identity_t<Char>)> args);

template <typename Char, typename Args,
          FMT_ENABLE_IF(!std::is_same<Char, char>::value)>
inline void vprint_mojibake(std::FILE*, basic_string_view<Char>, const Args&) {}

FMT_API void vprint_mojibake(std::FILE*, string_view, format_args);
#ifndef _WIN32
inline void vprint_mojibake(std::FILE*, string_view, format_args) {}
#endif
}  // namespace detail

/** Formats a string and writes the output to ``out``. */
// GCC 8 and earlier cannot handle std::back_insert_iterator<Container> with
// vformat_to<ArgFormatter>(...) overload, so SFINAE on iterator type instead.
template <
    typename OutputIt, typename S, typename Char = char_t<S>,
    FMT_ENABLE_IF(detail::is_contiguous_back_insert_iterator<OutputIt>::value)>
OutputIt vformat_to(
    OutputIt out, const S& format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args) {
  auto& c = detail::get_container(out);
  detail::container_buffer<remove_reference_t<decltype(c)>> buf(c);
  detail::vformat_to(buf, to_string_view(format_str), args);
  return out;
}

template <typename Container, typename S, typename... Args,
          FMT_ENABLE_IF(
              is_contiguous<Container>::value&& detail::is_string<S>::value)>
inline std::back_insert_iterator<Container> format_to(
    std::back_insert_iterator<Container> out, const S& format_str,
    Args&&... args) {
  return vformat_to(out, to_string_view(format_str),
                    detail::make_args_checked<Args...>(format_str, args...));
}

template <typename S, typename Char = char_t<S>>
FMT_INLINE std::basic_string<Char> vformat(
    const S& format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args) {
  return detail::vformat(to_string_view(format_str), args);
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
FMT_INLINE std::basic_string<Char> format(const S& format_str, Args&&... args) {
  const auto& vargs = detail::make_args_checked<Args...>(format_str, args...);
  return detail::vformat(to_string_view(format_str), vargs);
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
  const auto& vargs = detail::make_args_checked<Args...>(format_str, args...);
  return detail::is_unicode<Char>()
             ? vprint(f, to_string_view(format_str), vargs)
             : detail::vprint_mojibake(f, to_string_view(format_str), vargs);
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
  const auto& vargs = detail::make_args_checked<Args...>(format_str, args...);
  return detail::is_unicode<Char>()
             ? vprint(to_string_view(format_str), vargs)
             : detail::vprint_mojibake(stdout, to_string_view(format_str),
                                       vargs);
}
FMT_END_NAMESPACE

#endif  // FMT_CORE_H_
