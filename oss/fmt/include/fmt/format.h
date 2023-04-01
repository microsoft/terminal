/*
 Formatting library for C++

 Copyright (c) 2012 - present, Victor Zverovich

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 --- Optional exception to the license ---

 As an exception, if, as a result of your compiling your source code, portions
 of this Software are embedded into a machine-executable object form of such
 source code, you may redistribute such embedded portions in such object form
 without including the above copyright and permission notices.
 */

#ifndef FMT_FORMAT_H_
#define FMT_FORMAT_H_

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

#include "core.h"

#ifdef __INTEL_COMPILER
#  define FMT_ICC_VERSION __INTEL_COMPILER
#elif defined(__ICL)
#  define FMT_ICC_VERSION __ICL
#else
#  define FMT_ICC_VERSION 0
#endif

#ifdef __NVCC__
#  define FMT_CUDA_VERSION (__CUDACC_VER_MAJOR__ * 100 + __CUDACC_VER_MINOR__)
#else
#  define FMT_CUDA_VERSION 0
#endif

#ifdef __has_builtin
#  define FMT_HAS_BUILTIN(x) __has_builtin(x)
#else
#  define FMT_HAS_BUILTIN(x) 0
#endif

#if FMT_GCC_VERSION || FMT_CLANG_VERSION
#  define FMT_NOINLINE __attribute__((noinline))
#else
#  define FMT_NOINLINE
#endif

#if __cplusplus == 201103L || __cplusplus == 201402L
#  if defined(__INTEL_COMPILER) || defined(__PGI)
#    define FMT_FALLTHROUGH
#  elif defined(__clang__)
#    define FMT_FALLTHROUGH [[clang::fallthrough]]
#  elif FMT_GCC_VERSION >= 700 && \
      (!defined(__EDG_VERSION__) || __EDG_VERSION__ >= 520)
#    define FMT_FALLTHROUGH [[gnu::fallthrough]]
#  else
#    define FMT_FALLTHROUGH
#  endif
#elif FMT_HAS_CPP17_ATTRIBUTE(fallthrough) || \
    (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#  define FMT_FALLTHROUGH [[fallthrough]]
#else
#  define FMT_FALLTHROUGH
#endif

#ifndef FMT_MAYBE_UNUSED
#  if FMT_HAS_CPP17_ATTRIBUTE(maybe_unused)
#    define FMT_MAYBE_UNUSED [[maybe_unused]]
#  else
#    define FMT_MAYBE_UNUSED
#  endif
#endif

#ifndef FMT_THROW
#  if FMT_EXCEPTIONS
#    if FMT_MSC_VER || FMT_NVCC
FMT_BEGIN_NAMESPACE
namespace detail {
template <typename Exception> inline void do_throw(const Exception& x) {
  // Silence unreachable code warnings in MSVC and NVCC because these
  // are nearly impossible to fix in a generic code.
  volatile bool b = true;
  if (b) throw x;
}
}  // namespace detail
FMT_END_NAMESPACE
#      define FMT_THROW(x) detail::do_throw(x)
#    else
#      define FMT_THROW(x) throw x
#    endif
#  else
#    define FMT_THROW(x)              \
      do {                            \
        static_cast<void>(sizeof(x)); \
        FMT_ASSERT(false, "");        \
      } while (false)
#  endif
#endif

#if FMT_EXCEPTIONS
#  define FMT_TRY try
#  define FMT_CATCH(x) catch (x)
#else
#  define FMT_TRY if (true)
#  define FMT_CATCH(x) if (false)
#endif

#ifndef FMT_USE_USER_DEFINED_LITERALS
// EDG based compilers (Intel, NVIDIA, Elbrus, etc), GCC and MSVC support UDLs.
#  if (FMT_HAS_FEATURE(cxx_user_literals) || FMT_GCC_VERSION >= 407 || \
       FMT_MSC_VER >= 1900) &&                                         \
      (!defined(__EDG_VERSION__) || __EDG_VERSION__ >= /* UDL feature */ 480)
#    define FMT_USE_USER_DEFINED_LITERALS 1
#  else
#    define FMT_USE_USER_DEFINED_LITERALS 0
#  endif
#endif

#ifndef FMT_USE_UDL_TEMPLATE
// EDG frontend based compilers (icc, nvcc, PGI, etc) and GCC < 6.4 do not
// properly support UDL templates and GCC >= 9 warns about them.
#  if FMT_USE_USER_DEFINED_LITERALS &&                         \
      (!defined(__EDG_VERSION__) || __EDG_VERSION__ >= 501) && \
      ((FMT_GCC_VERSION >= 604 && __cplusplus >= 201402L) ||   \
       FMT_CLANG_VERSION >= 304) &&                            \
      !defined(__PGI) && !defined(__NVCC__)
#    define FMT_USE_UDL_TEMPLATE 1
#  else
#    define FMT_USE_UDL_TEMPLATE 0
#  endif
#endif

#ifndef FMT_USE_FLOAT
#  define FMT_USE_FLOAT 1
#endif

#ifndef FMT_USE_DOUBLE
#  define FMT_USE_DOUBLE 1
#endif

#ifndef FMT_USE_LONG_DOUBLE
#  define FMT_USE_LONG_DOUBLE 1
#endif

// Defining FMT_REDUCE_INT_INSTANTIATIONS to 1, will reduce the number of
// int_writer template instances to just one by only using the largest integer
// type. This results in a reduction in binary size but will cause a decrease in
// integer formatting performance.
#if !defined(FMT_REDUCE_INT_INSTANTIATIONS)
#  define FMT_REDUCE_INT_INSTANTIATIONS 0
#endif

// __builtin_clz is broken in clang with Microsoft CodeGen:
// https://github.com/fmtlib/fmt/issues/519
#if (FMT_GCC_VERSION || FMT_HAS_BUILTIN(__builtin_clz)) && !FMT_MSC_VER
#  define FMT_BUILTIN_CLZ(n) __builtin_clz(n)
#endif
#if (FMT_GCC_VERSION || FMT_HAS_BUILTIN(__builtin_clzll)) && !FMT_MSC_VER
#  define FMT_BUILTIN_CLZLL(n) __builtin_clzll(n)
#endif
#if (FMT_GCC_VERSION || FMT_HAS_BUILTIN(__builtin_ctz))
#  define FMT_BUILTIN_CTZ(n) __builtin_ctz(n)
#endif
#if (FMT_GCC_VERSION || FMT_HAS_BUILTIN(__builtin_ctzll))
#  define FMT_BUILTIN_CTZLL(n) __builtin_ctzll(n)
#endif

#if FMT_MSC_VER
#  include <intrin.h>  // _BitScanReverse[64], _BitScanForward[64], _umul128
#endif

// Some compilers masquerade as both MSVC and GCC-likes or otherwise support
// __builtin_clz and __builtin_clzll, so only define FMT_BUILTIN_CLZ using the
// MSVC intrinsics if the clz and clzll builtins are not available.
#if FMT_MSC_VER && !defined(FMT_BUILTIN_CLZLL) && \
    !defined(FMT_BUILTIN_CTZLL) && !defined(_MANAGED)
FMT_BEGIN_NAMESPACE
namespace detail {
// Avoid Clang with Microsoft CodeGen's -Wunknown-pragmas warning.
#  ifndef __clang__
#    pragma intrinsic(_BitScanForward)
#    pragma intrinsic(_BitScanReverse)
#  endif
#  if defined(_WIN64) && !defined(__clang__)
#    pragma intrinsic(_BitScanForward64)
#    pragma intrinsic(_BitScanReverse64)
#  endif

inline int clz(uint32_t x) {
  unsigned long r = 0;
  _BitScanReverse(&r, x);
  FMT_ASSERT(x != 0, "");
  // Static analysis complains about using uninitialized data
  // "r", but the only way that can happen is if "x" is 0,
  // which the callers guarantee to not happen.
  FMT_SUPPRESS_MSC_WARNING(6102)
  return 31 ^ static_cast<int>(r);
}
#  define FMT_BUILTIN_CLZ(n) detail::clz(n)

inline int clzll(uint64_t x) {
  unsigned long r = 0;
#  ifdef _WIN64
  _BitScanReverse64(&r, x);
#  else
  // Scan the high 32 bits.
  if (_BitScanReverse(&r, static_cast<uint32_t>(x >> 32))) return 63 ^ (r + 32);
  // Scan the low 32 bits.
  _BitScanReverse(&r, static_cast<uint32_t>(x));
#  endif
  FMT_ASSERT(x != 0, "");
  FMT_SUPPRESS_MSC_WARNING(6102)  // Suppress a bogus static analysis warning.
  return 63 ^ static_cast<int>(r);
}
#  define FMT_BUILTIN_CLZLL(n) detail::clzll(n)

inline int ctz(uint32_t x) {
  unsigned long r = 0;
  _BitScanForward(&r, x);
  FMT_ASSERT(x != 0, "");
  FMT_SUPPRESS_MSC_WARNING(6102)  // Suppress a bogus static analysis warning.
  return static_cast<int>(r);
}
#  define FMT_BUILTIN_CTZ(n) detail::ctz(n)

inline int ctzll(uint64_t x) {
  unsigned long r = 0;
  FMT_ASSERT(x != 0, "");
  FMT_SUPPRESS_MSC_WARNING(6102)  // Suppress a bogus static analysis warning.
#  ifdef _WIN64
  _BitScanForward64(&r, x);
#  else
  // Scan the low 32 bits.
  if (_BitScanForward(&r, static_cast<uint32_t>(x))) return static_cast<int>(r);
  // Scan the high 32 bits.
  _BitScanForward(&r, static_cast<uint32_t>(x >> 32));
  r += 32;
#  endif
  return static_cast<int>(r);
}
#  define FMT_BUILTIN_CTZLL(n) detail::ctzll(n)
}  // namespace detail
FMT_END_NAMESPACE
#endif

// Enable the deprecated numeric alignment.
#ifndef FMT_DEPRECATED_NUMERIC_ALIGN
#  define FMT_DEPRECATED_NUMERIC_ALIGN 0
#endif

FMT_BEGIN_NAMESPACE
namespace detail {

// An equivalent of `*reinterpret_cast<Dest*>(&source)` that doesn't have
// undefined behavior (e.g. due to type aliasing).
// Example: uint64_t d = bit_cast<uint64_t>(2.718);
template <typename Dest, typename Source>
inline Dest bit_cast(const Source& source) {
  static_assert(sizeof(Dest) == sizeof(Source), "size mismatch");
  Dest dest;
  std::memcpy(&dest, &source, sizeof(dest));
  return dest;
}

inline bool is_big_endian() {
  const auto u = 1u;
  struct bytes {
    char data[sizeof(u)];
  };
  return bit_cast<bytes>(u).data[0] == 0;
}

// A fallback implementation of uintptr_t for systems that lack it.
struct fallback_uintptr {
  unsigned char value[sizeof(void*)];

  fallback_uintptr() = default;
  explicit fallback_uintptr(const void* p) {
    *this = bit_cast<fallback_uintptr>(p);
    if (is_big_endian()) {
      for (size_t i = 0, j = sizeof(void*) - 1; i < j; ++i, --j)
        std::swap(value[i], value[j]);
    }
  }
};
#ifdef UINTPTR_MAX
using uintptr_t = ::uintptr_t;
inline uintptr_t to_uintptr(const void* p) { return bit_cast<uintptr_t>(p); }
#else
using uintptr_t = fallback_uintptr;
inline fallback_uintptr to_uintptr(const void* p) {
  return fallback_uintptr(p);
}
#endif

// Returns the largest possible value for type T. Same as
// std::numeric_limits<T>::max() but shorter and not affected by the max macro.
template <typename T> constexpr T max_value() {
  return (std::numeric_limits<T>::max)();
}
template <typename T> constexpr int num_bits() {
  return std::numeric_limits<T>::digits;
}
// std::numeric_limits<T>::digits may return 0 for 128-bit ints.
template <> constexpr int num_bits<int128_t>() { return 128; }
template <> constexpr int num_bits<uint128_t>() { return 128; }
template <> constexpr int num_bits<fallback_uintptr>() {
  return static_cast<int>(sizeof(void*) *
                          std::numeric_limits<unsigned char>::digits);
}

FMT_INLINE void assume(bool condition) {
  (void)condition;
#if FMT_HAS_BUILTIN(__builtin_assume)
  __builtin_assume(condition);
#endif
}

// An approximation of iterator_t for pre-C++20 systems.
template <typename T>
using iterator_t = decltype(std::begin(std::declval<T&>()));
template <typename T> using sentinel_t = decltype(std::end(std::declval<T&>()));

// A workaround for std::string not having mutable data() until C++17.
template <typename Char> inline Char* get_data(std::basic_string<Char>& s) {
  return &s[0];
}
template <typename Container>
inline typename Container::value_type* get_data(Container& c) {
  return c.data();
}

#if defined(_SECURE_SCL) && _SECURE_SCL
// Make a checked iterator to avoid MSVC warnings.
template <typename T> using checked_ptr = stdext::checked_array_iterator<T*>;
template <typename T> checked_ptr<T> make_checked(T* p, size_t size) {
  return {p, size};
}
#else
template <typename T> using checked_ptr = T*;
template <typename T> inline T* make_checked(T* p, size_t) { return p; }
#endif

template <typename Container, FMT_ENABLE_IF(is_contiguous<Container>::value)>
#if FMT_CLANG_VERSION
__attribute__((no_sanitize("undefined")))
#endif
inline checked_ptr<typename Container::value_type>
reserve(std::back_insert_iterator<Container> it, size_t n) {
  Container& c = get_container(it);
  size_t size = c.size();
  c.resize(size + n);
  return make_checked(get_data(c) + size, n);
}

template <typename T>
inline buffer_appender<T> reserve(buffer_appender<T> it, size_t n) {
  buffer<T>& buf = get_container(it);
  buf.try_reserve(buf.size() + n);
  return it;
}

template <typename Iterator> inline Iterator& reserve(Iterator& it, size_t) {
  return it;
}

template <typename T, typename OutputIt>
constexpr T* to_pointer(OutputIt, size_t) {
  return nullptr;
}
template <typename T> T* to_pointer(buffer_appender<T> it, size_t n) {
  buffer<T>& buf = get_container(it);
  auto size = buf.size();
  if (buf.capacity() < size + n) return nullptr;
  buf.try_resize(size + n);
  return buf.data() + size;
}

template <typename Container, FMT_ENABLE_IF(is_contiguous<Container>::value)>
inline std::back_insert_iterator<Container> base_iterator(
    std::back_insert_iterator<Container>& it,
    checked_ptr<typename Container::value_type>) {
  return it;
}

template <typename Iterator>
inline Iterator base_iterator(Iterator, Iterator it) {
  return it;
}

// An output iterator that counts the number of objects written to it and
// discards them.
class counting_iterator {
 private:
  size_t count_;

 public:
  using iterator_category = std::output_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = void;
  using _Unchecked_type = counting_iterator;  // Mark iterator as checked.

  struct value_type {
    template <typename T> void operator=(const T&) {}
  };

  counting_iterator() : count_(0) {}

  size_t count() const { return count_; }

  counting_iterator& operator++() {
    ++count_;
    return *this;
  }
  counting_iterator operator++(int) {
    auto it = *this;
    ++*this;
    return it;
  }

  friend counting_iterator operator+(counting_iterator it, difference_type n) {
    it.count_ += static_cast<size_t>(n);
    return it;
  }

  value_type operator*() const { return {}; }
};

template <typename OutputIt> class truncating_iterator_base {
 protected:
  OutputIt out_;
  size_t limit_;
  size_t count_;

  truncating_iterator_base(OutputIt out, size_t limit)
      : out_(out), limit_(limit), count_(0) {}

 public:
  using iterator_category = std::output_iterator_tag;
  using value_type = typename std::iterator_traits<OutputIt>::value_type;
  using difference_type = void;
  using pointer = void;
  using reference = void;
  using _Unchecked_type =
      truncating_iterator_base;  // Mark iterator as checked.

  OutputIt base() const { return out_; }
  size_t count() const { return count_; }
};

// An output iterator that truncates the output and counts the number of objects
// written to it.
template <typename OutputIt,
          typename Enable = typename std::is_void<
              typename std::iterator_traits<OutputIt>::value_type>::type>
class truncating_iterator;

template <typename OutputIt>
class truncating_iterator<OutputIt, std::false_type>
    : public truncating_iterator_base<OutputIt> {
  mutable typename truncating_iterator_base<OutputIt>::value_type blackhole_;

 public:
  using value_type = typename truncating_iterator_base<OutputIt>::value_type;

  truncating_iterator(OutputIt out, size_t limit)
      : truncating_iterator_base<OutputIt>(out, limit) {}

  truncating_iterator& operator++() {
    if (this->count_++ < this->limit_) ++this->out_;
    return *this;
  }

  truncating_iterator operator++(int) {
    auto it = *this;
    ++*this;
    return it;
  }

  value_type& operator*() const {
    return this->count_ < this->limit_ ? *this->out_ : blackhole_;
  }
};

template <typename OutputIt>
class truncating_iterator<OutputIt, std::true_type>
    : public truncating_iterator_base<OutputIt> {
 public:
  truncating_iterator(OutputIt out, size_t limit)
      : truncating_iterator_base<OutputIt>(out, limit) {}

  template <typename T> truncating_iterator& operator=(T val) {
    if (this->count_++ < this->limit_) *this->out_++ = val;
    return *this;
  }

  truncating_iterator& operator++() { return *this; }
  truncating_iterator& operator++(int) { return *this; }
  truncating_iterator& operator*() { return *this; }
};

template <typename Char>
inline size_t count_code_points(basic_string_view<Char> s) {
  return s.size();
}

// Counts the number of code points in a UTF-8 string.
inline size_t count_code_points(basic_string_view<char> s) {
  const char* data = s.data();
  size_t num_code_points = 0;
  for (size_t i = 0, size = s.size(); i != size; ++i) {
    if ((data[i] & 0xc0) != 0x80) ++num_code_points;
  }
  return num_code_points;
}

inline size_t count_code_points(basic_string_view<char8_type> s) {
  return count_code_points(basic_string_view<char>(
      reinterpret_cast<const char*>(s.data()), s.size()));
}

template <typename Char>
inline size_t code_point_index(basic_string_view<Char> s, size_t n) {
  size_t size = s.size();
  return n < size ? n : size;
}

// Calculates the index of the nth code point in a UTF-8 string.
inline size_t code_point_index(basic_string_view<char8_type> s, size_t n) {
  const char8_type* data = s.data();
  size_t num_code_points = 0;
  for (size_t i = 0, size = s.size(); i != size; ++i) {
    if ((data[i] & 0xc0) != 0x80 && ++num_code_points > n) {
      return i;
    }
  }
  return s.size();
}

template <typename InputIt, typename OutChar>
using needs_conversion = bool_constant<
    std::is_same<typename std::iterator_traits<InputIt>::value_type,
                 char>::value &&
    std::is_same<OutChar, char8_type>::value>;

template <typename OutChar, typename InputIt, typename OutputIt,
          FMT_ENABLE_IF(!needs_conversion<InputIt, OutChar>::value)>
OutputIt copy_str(InputIt begin, InputIt end, OutputIt it) {
  return std::copy(begin, end, it);
}

template <typename OutChar, typename InputIt, typename OutputIt,
          FMT_ENABLE_IF(needs_conversion<InputIt, OutChar>::value)>
OutputIt copy_str(InputIt begin, InputIt end, OutputIt it) {
  return std::transform(begin, end, it,
                        [](char c) { return static_cast<char8_type>(c); });
}

template <typename Char, typename InputIt>
inline counting_iterator copy_str(InputIt begin, InputIt end,
                                  counting_iterator it) {
  return it + (end - begin);
}

template <typename T>
using is_fast_float = bool_constant<std::numeric_limits<T>::is_iec559 &&
                                    sizeof(T) <= sizeof(double)>;

#ifndef FMT_USE_FULL_CACHE_DRAGONBOX
#  define FMT_USE_FULL_CACHE_DRAGONBOX 0
#endif

template <typename T>
template <typename U>
void buffer<T>::append(const U* begin, const U* end) {
  do {
    auto count = to_unsigned(end - begin);
    try_reserve(size_ + count);
    auto free_cap = capacity_ - size_;
    if (free_cap < count) count = free_cap;
    std::uninitialized_copy_n(begin, count, make_checked(ptr_ + size_, count));
    size_ += count;
    begin += count;
  } while (begin != end);
}

template <typename OutputIt, typename T, typename Traits>
void iterator_buffer<OutputIt, T, Traits>::flush() {
  out_ = std::copy_n(data_, this->limit(this->size()), out_);
  this->clear();
}
}  // namespace detail

// The number of characters to store in the basic_memory_buffer object itself
// to avoid dynamic memory allocation.
enum { inline_buffer_size = 500 };

/**
  \rst
  A dynamically growing memory buffer for trivially copyable/constructible types
  with the first ``SIZE`` elements stored in the object itself.

  You can use one of the following type aliases for common character types:

  +----------------+------------------------------+
  | Type           | Definition                   |
  +================+==============================+
  | memory_buffer  | basic_memory_buffer<char>    |
  +----------------+------------------------------+
  | wmemory_buffer | basic_memory_buffer<wchar_t> |
  +----------------+------------------------------+

  **Example**::

     fmt::memory_buffer out;
     format_to(out, "The answer is {}.", 42);

  This will append the following output to the ``out`` object:

  .. code-block:: none

     The answer is 42.

  The output can be converted to an ``std::string`` with ``to_string(out)``.
  \endrst
 */
template <typename T, size_t SIZE = inline_buffer_size,
          typename Allocator = std::allocator<T>>
class basic_memory_buffer final : public detail::buffer<T> {
 private:
  T store_[SIZE];

  // Don't inherit from Allocator avoid generating type_info for it.
  Allocator alloc_;

  // Deallocate memory allocated by the buffer.
  void deallocate() {
    T* data = this->data();
    if (data != store_) alloc_.deallocate(data, this->capacity());
  }

 protected:
  void grow(size_t size) final FMT_OVERRIDE;

 public:
  using value_type = T;
  using const_reference = const T&;

  explicit basic_memory_buffer(const Allocator& alloc = Allocator())
      : alloc_(alloc) {
    this->set(store_, SIZE);
  }
  ~basic_memory_buffer() { deallocate(); }

 private:
  // Move data from other to this buffer.
  void move(basic_memory_buffer& other) {
    alloc_ = std::move(other.alloc_);
    T* data = other.data();
    size_t size = other.size(), capacity = other.capacity();
    if (data == other.store_) {
      this->set(store_, capacity);
      std::uninitialized_copy(other.store_, other.store_ + size,
                              detail::make_checked(store_, capacity));
    } else {
      this->set(data, capacity);
      // Set pointer to the inline array so that delete is not called
      // when deallocating.
      other.set(other.store_, 0);
    }
    this->resize(size);
  }

 public:
  /**
    \rst
    Constructs a :class:`fmt::basic_memory_buffer` object moving the content
    of the other object to it.
    \endrst
   */
  basic_memory_buffer(basic_memory_buffer&& other) FMT_NOEXCEPT { move(other); }

  /**
    \rst
    Moves the content of the other ``basic_memory_buffer`` object to this one.
    \endrst
   */
  basic_memory_buffer& operator=(basic_memory_buffer&& other) FMT_NOEXCEPT {
    FMT_ASSERT(this != &other, "");
    deallocate();
    move(other);
    return *this;
  }

  // Returns a copy of the allocator associated with this buffer.
  Allocator get_allocator() const { return alloc_; }

  /**
    Resizes the buffer to contain *count* elements. If T is a POD type new
    elements may not be initialized.
   */
  void resize(size_t count) { this->try_resize(count); }

  /** Increases the buffer capacity to *new_capacity*. */
  void reserve(size_t new_capacity) { this->try_reserve(new_capacity); }

  // Directly append data into the buffer
  using detail::buffer<T>::append;
  template <typename ContiguousRange>
  void append(const ContiguousRange& range) {
    append(range.data(), range.data() + range.size());
  }
};

template <typename T, size_t SIZE, typename Allocator>
void basic_memory_buffer<T, SIZE, Allocator>::grow(size_t size) {
#ifdef FMT_FUZZ
  if (size > 5000) throw std::runtime_error("fuzz mode - won't grow that much");
#endif
  size_t old_capacity = this->capacity();
  size_t new_capacity = old_capacity + old_capacity / 2;
  if (size > new_capacity) new_capacity = size;
  T* old_data = this->data();
  T* new_data =
      std::allocator_traits<Allocator>::allocate(alloc_, new_capacity);
  // The following code doesn't throw, so the raw pointer above doesn't leak.
  std::uninitialized_copy(old_data, old_data + this->size(),
                          detail::make_checked(new_data, new_capacity));
  this->set(new_data, new_capacity);
  // deallocate must not throw according to the standard, but even if it does,
  // the buffer already uses the new storage and will deallocate it in
  // destructor.
  if (old_data != store_) alloc_.deallocate(old_data, old_capacity);
}

using memory_buffer = basic_memory_buffer<char>;
using wmemory_buffer = basic_memory_buffer<wchar_t>;

template <typename T, size_t SIZE, typename Allocator>
struct is_contiguous<basic_memory_buffer<T, SIZE, Allocator>> : std::true_type {
};

/** A formatting error such as invalid format string. */
FMT_CLASS_API
class FMT_API format_error : public std::runtime_error {
 public:
  explicit format_error(const char* message) : std::runtime_error(message) {}
  explicit format_error(const std::string& message)
      : std::runtime_error(message) {}
  format_error(const format_error&) = default;
  format_error& operator=(const format_error&) = default;
  format_error(format_error&&) = default;
  format_error& operator=(format_error&&) = default;
  ~format_error() FMT_NOEXCEPT FMT_OVERRIDE;
};

namespace detail {

template <typename T>
using is_signed =
    std::integral_constant<bool, std::numeric_limits<T>::is_signed ||
                                     std::is_same<T, int128_t>::value>;

// Returns true if value is negative, false otherwise.
// Same as `value < 0` but doesn't produce warnings if T is an unsigned type.
template <typename T, FMT_ENABLE_IF(is_signed<T>::value)>
FMT_CONSTEXPR bool is_negative(T value) {
  return value < 0;
}
template <typename T, FMT_ENABLE_IF(!is_signed<T>::value)>
FMT_CONSTEXPR bool is_negative(T) {
  return false;
}

template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value)>
FMT_CONSTEXPR bool is_supported_floating_point(T) {
  return (std::is_same<T, float>::value && FMT_USE_FLOAT) ||
         (std::is_same<T, double>::value && FMT_USE_DOUBLE) ||
         (std::is_same<T, long double>::value && FMT_USE_LONG_DOUBLE);
}

// Smallest of uint32_t, uint64_t, uint128_t that is large enough to
// represent all values of an integral type T.
template <typename T>
using uint32_or_64_or_128_t =
    conditional_t<num_bits<T>() <= 32 && !FMT_REDUCE_INT_INSTANTIATIONS,
                  uint32_t,
                  conditional_t<num_bits<T>() <= 64, uint64_t, uint128_t>>;

// 128-bit integer type used internally
struct FMT_EXTERN_TEMPLATE_API uint128_wrapper {
  uint128_wrapper() = default;

#if FMT_USE_INT128
  uint128_t internal_;

  uint128_wrapper(uint64_t high, uint64_t low) FMT_NOEXCEPT
      : internal_{static_cast<uint128_t>(low) |
                  (static_cast<uint128_t>(high) << 64)} {}

  uint128_wrapper(uint128_t u) : internal_{u} {}

  uint64_t high() const FMT_NOEXCEPT { return uint64_t(internal_ >> 64); }
  uint64_t low() const FMT_NOEXCEPT { return uint64_t(internal_); }

  uint128_wrapper& operator+=(uint64_t n) FMT_NOEXCEPT {
    internal_ += n;
    return *this;
  }
#else
  uint64_t high_;
  uint64_t low_;

  uint128_wrapper(uint64_t high, uint64_t low) FMT_NOEXCEPT : high_{high},
                                                              low_{low} {}

  uint64_t high() const FMT_NOEXCEPT { return high_; }
  uint64_t low() const FMT_NOEXCEPT { return low_; }

  uint128_wrapper& operator+=(uint64_t n) FMT_NOEXCEPT {
#  if defined(_MSC_VER) && defined(_M_X64)
    unsigned char carry = _addcarry_u64(0, low_, n, &low_);
    _addcarry_u64(carry, high_, 0, &high_);
    return *this;
#  else
    uint64_t sum = low_ + n;
    high_ += (sum < low_ ? 1 : 0);
    low_ = sum;
    return *this;
#  endif
  }
#endif
};

// Table entry type for divisibility test used internally
template <typename T> struct FMT_EXTERN_TEMPLATE_API divtest_table_entry {
  T mod_inv;
  T max_quotient;
};

// Static data is placed in this class template for the header-only config.
template <typename T = void> struct FMT_EXTERN_TEMPLATE_API basic_data {
  static const uint64_t powers_of_10_64[];
  static const uint32_t zero_or_powers_of_10_32_new[];
  static const uint64_t zero_or_powers_of_10_64_new[];
  static const uint64_t grisu_pow10_significands[];
  static const int16_t grisu_pow10_exponents[];
  static const divtest_table_entry<uint32_t> divtest_table_for_pow5_32[];
  static const divtest_table_entry<uint64_t> divtest_table_for_pow5_64[];
  static const uint64_t dragonbox_pow10_significands_64[];
  static const uint128_wrapper dragonbox_pow10_significands_128[];
  // log10(2) = 0x0.4d104d427de7fbcc...
  static const uint64_t log10_2_significand = 0x4d104d427de7fbcc;
#if !FMT_USE_FULL_CACHE_DRAGONBOX
  static const uint64_t powers_of_5_64[];
  static const uint32_t dragonbox_pow10_recovery_errors[];
#endif
  // GCC generates slightly better code for pairs than chars.
  using digit_pair = char[2];
  static const digit_pair digits[];
  static const char hex_digits[];
  static const char foreground_color[];
  static const char background_color[];
  static const char reset_color[5];
  static const wchar_t wreset_color[5];
  static const char signs[];
  static const char left_padding_shifts[5];
  static const char right_padding_shifts[5];

  // DEPRECATED! These are for ABI compatibility.
  static const uint32_t zero_or_powers_of_10_32[];
  static const uint64_t zero_or_powers_of_10_64[];
};

// Maps bsr(n) to ceil(log10(pow(2, bsr(n) + 1) - 1)).
// This is a function instead of an array to workaround a bug in GCC10 (#1810).
FMT_INLINE uint16_t bsr2log10(int bsr) {
  static constexpr uint16_t data[] = {
      1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,
      6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 10,
      10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15,
      15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20};
  return data[bsr];
}

#ifndef FMT_EXPORTED
FMT_EXTERN template struct basic_data<void>;
#endif

// This is a struct rather than an alias to avoid shadowing warnings in gcc.
struct data : basic_data<> {};

#ifdef FMT_BUILTIN_CLZLL
// Returns the number of decimal digits in n. Leading zeros are not counted
// except for n == 0 in which case count_digits returns 1.
inline int count_digits(uint64_t n) {
  // https://github.com/fmtlib/format-benchmark/blob/master/digits10
  auto t = bsr2log10(FMT_BUILTIN_CLZLL(n | 1) ^ 63);
  return t - (n < data::zero_or_powers_of_10_64_new[t]);
}
#else
// Fallback version of count_digits used when __builtin_clz is not available.
inline int count_digits(uint64_t n) {
  int count = 1;
  for (;;) {
    // Integer division is slow so do it for a group of four digits instead
    // of for every digit. The idea comes from the talk by Alexandrescu
    // "Three Optimization Tips for C++". See speed-test for a comparison.
    if (n < 10) return count;
    if (n < 100) return count + 1;
    if (n < 1000) return count + 2;
    if (n < 10000) return count + 3;
    n /= 10000u;
    count += 4;
  }
}
#endif

#if FMT_USE_INT128
inline int count_digits(uint128_t n) {
  int count = 1;
  for (;;) {
    // Integer division is slow so do it for a group of four digits instead
    // of for every digit. The idea comes from the talk by Alexandrescu
    // "Three Optimization Tips for C++". See speed-test for a comparison.
    if (n < 10) return count;
    if (n < 100) return count + 1;
    if (n < 1000) return count + 2;
    if (n < 10000) return count + 3;
    n /= 10000U;
    count += 4;
  }
}
#endif

// Counts the number of digits in n. BITS = log2(radix).
template <unsigned BITS, typename UInt> inline int count_digits(UInt n) {
  int num_digits = 0;
  do {
    ++num_digits;
  } while ((n >>= BITS) != 0);
  return num_digits;
}

template <> int count_digits<4>(detail::fallback_uintptr n);

#if FMT_GCC_VERSION || FMT_CLANG_VERSION
#  define FMT_ALWAYS_INLINE inline __attribute__((always_inline))
#elif FMT_MSC_VER
#  define FMT_ALWAYS_INLINE __forceinline
#else
#  define FMT_ALWAYS_INLINE inline
#endif

// To suppress unnecessary security cookie checks
#if FMT_MSC_VER && !FMT_CLANG_VERSION
#  define FMT_SAFEBUFFERS __declspec(safebuffers)
#else
#  define FMT_SAFEBUFFERS
#endif

#ifdef FMT_BUILTIN_CLZ
// Optional version of count_digits for better performance on 32-bit platforms.
inline int count_digits(uint32_t n) {
  auto t = bsr2log10(FMT_BUILTIN_CLZ(n | 1) ^ 31);
  return t - (n < data::zero_or_powers_of_10_32_new[t]);
}
#endif

template <typename Int> constexpr int digits10() FMT_NOEXCEPT {
  return std::numeric_limits<Int>::digits10;
}
template <> constexpr int digits10<int128_t>() FMT_NOEXCEPT { return 38; }
template <> constexpr int digits10<uint128_t>() FMT_NOEXCEPT { return 38; }

template <typename Char> FMT_API std::string grouping_impl(locale_ref loc);
template <typename Char> inline std::string grouping(locale_ref loc) {
  return grouping_impl<char>(loc);
}
template <> inline std::string grouping<wchar_t>(locale_ref loc) {
  return grouping_impl<wchar_t>(loc);
}

template <typename Char> FMT_API Char thousands_sep_impl(locale_ref loc);
template <typename Char> inline Char thousands_sep(locale_ref loc) {
  return Char(thousands_sep_impl<char>(loc));
}
template <> inline wchar_t thousands_sep(locale_ref loc) {
  return thousands_sep_impl<wchar_t>(loc);
}

template <typename Char> FMT_API Char decimal_point_impl(locale_ref loc);
template <typename Char> inline Char decimal_point(locale_ref loc) {
  return Char(decimal_point_impl<char>(loc));
}
template <> inline wchar_t decimal_point(locale_ref loc) {
  return decimal_point_impl<wchar_t>(loc);
}

// Compares two characters for equality.
template <typename Char> bool equal2(const Char* lhs, const char* rhs) {
  return lhs[0] == rhs[0] && lhs[1] == rhs[1];
}
inline bool equal2(const char* lhs, const char* rhs) {
  return memcmp(lhs, rhs, 2) == 0;
}

// Copies two characters from src to dst.
template <typename Char> void copy2(Char* dst, const char* src) {
  *dst++ = static_cast<Char>(*src++);
  *dst = static_cast<Char>(*src);
}
FMT_INLINE void copy2(char* dst, const char* src) { memcpy(dst, src, 2); }

template <typename Iterator> struct format_decimal_result {
  Iterator begin;
  Iterator end;
};

// Formats a decimal unsigned integer value writing into out pointing to a
// buffer of specified size. The caller must ensure that the buffer is large
// enough.
template <typename Char, typename UInt>
inline format_decimal_result<Char*> format_decimal(Char* out, UInt value,
                                                   int size) {
  FMT_ASSERT(size >= count_digits(value), "invalid digit count");
  out += size;
  Char* end = out;
  while (value >= 100) {
    // Integer division is slow so do it for a group of two digits instead
    // of for every digit. The idea comes from the talk by Alexandrescu
    // "Three Optimization Tips for C++". See speed-test for a comparison.
    out -= 2;
    copy2(out, data::digits[value % 100]);
    value /= 100;
  }
  if (value < 10) {
    *--out = static_cast<Char>('0' + value);
    return {out, end};
  }
  out -= 2;
  copy2(out, data::digits[value]);
  return {out, end};
}

template <typename Char, typename UInt, typename Iterator,
          FMT_ENABLE_IF(!std::is_pointer<remove_cvref_t<Iterator>>::value)>
inline format_decimal_result<Iterator> format_decimal(Iterator out, UInt value,
                                                      int size) {
  // Buffer is large enough to hold all digits (digits10 + 1).
  Char buffer[digits10<UInt>() + 1];
  auto end = format_decimal(buffer, value, size).end;
  return {out, detail::copy_str<Char>(buffer, end, out)};
}

template <unsigned BASE_BITS, typename Char, typename UInt>
inline Char* format_uint(Char* buffer, UInt value, int num_digits,
                         bool upper = false) {
  buffer += num_digits;
  Char* end = buffer;
  do {
    const char* digits = upper ? "0123456789ABCDEF" : data::hex_digits;
    unsigned digit = (value & ((1 << BASE_BITS) - 1));
    *--buffer = static_cast<Char>(BASE_BITS < 4 ? static_cast<char>('0' + digit)
                                                : digits[digit]);
  } while ((value >>= BASE_BITS) != 0);
  return end;
}

template <unsigned BASE_BITS, typename Char>
Char* format_uint(Char* buffer, detail::fallback_uintptr n, int num_digits,
                  bool = false) {
  auto char_digits = std::numeric_limits<unsigned char>::digits / 4;
  int start = (num_digits + char_digits - 1) / char_digits - 1;
  if (int start_digits = num_digits % char_digits) {
    unsigned value = n.value[start--];
    buffer = format_uint<BASE_BITS>(buffer, value, start_digits);
  }
  for (; start >= 0; --start) {
    unsigned value = n.value[start];
    buffer += char_digits;
    auto p = buffer;
    for (int i = 0; i < char_digits; ++i) {
      unsigned digit = (value & ((1 << BASE_BITS) - 1));
      *--p = static_cast<Char>(data::hex_digits[digit]);
      value >>= BASE_BITS;
    }
  }
  return buffer;
}

template <unsigned BASE_BITS, typename Char, typename It, typename UInt>
inline It format_uint(It out, UInt value, int num_digits, bool upper = false) {
  if (auto ptr = to_pointer<Char>(out, to_unsigned(num_digits))) {
    format_uint<BASE_BITS>(ptr, value, num_digits, upper);
    return out;
  }
  // Buffer should be large enough to hold all digits (digits / BASE_BITS + 1).
  char buffer[num_bits<UInt>() / BASE_BITS + 1];
  format_uint<BASE_BITS>(buffer, value, num_digits, upper);
  return detail::copy_str<Char>(buffer, buffer + num_digits, out);
}

// A converter from UTF-8 to UTF-16.
class utf8_to_utf16 {
 private:
  wmemory_buffer buffer_;

 public:
  FMT_API explicit utf8_to_utf16(string_view s);
  operator wstring_view() const { return {&buffer_[0], size()}; }
  size_t size() const { return buffer_.size() - 1; }
  const wchar_t* c_str() const { return &buffer_[0]; }
  std::wstring str() const { return {&buffer_[0], size()}; }
};

template <typename T = void> struct null {};

// Workaround an array initialization issue in gcc 4.8.
template <typename Char> struct fill_t {
 private:
  enum { max_size = 4 };
  Char data_[max_size] = {Char(' '), Char(0), Char(0), Char(0)};
  unsigned char size_ = 1;

 public:
  FMT_CONSTEXPR void operator=(basic_string_view<Char> s) {
    auto size = s.size();
    if (size > max_size) {
      FMT_THROW(format_error("invalid fill"));
      return;
    }
    for (size_t i = 0; i < size; ++i) data_[i] = s[i];
    size_ = static_cast<unsigned char>(size);
  }

  size_t size() const { return size_; }
  const Char* data() const { return data_; }

  FMT_CONSTEXPR Char& operator[](size_t index) { return data_[index]; }
  FMT_CONSTEXPR const Char& operator[](size_t index) const {
    return data_[index];
  }
};
}  // namespace detail

// We cannot use enum classes as bit fields because of a gcc bug
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61414.
namespace align {
enum type { none, left, right, center, numeric };
}
using align_t = align::type;

namespace sign {
enum type { none, minus, plus, space };
}
using sign_t = sign::type;

// Format specifiers for built-in and string types.
template <typename Char> struct basic_format_specs {
  int width;
  int precision;
  char type;
  align_t align : 4;
  sign_t sign : 3;
  bool alt : 1;  // Alternate form ('#').
  detail::fill_t<Char> fill;

  constexpr basic_format_specs()
      : width(0),
        precision(-1),
        type(0),
        align(align::none),
        sign(sign::none),
        alt(false) {}
};

using format_specs = basic_format_specs<char>;

namespace detail {
namespace dragonbox {

// Type-specific information that Dragonbox uses.
template <class T> struct float_info;

template <> struct float_info<float> {
  using carrier_uint = uint32_t;
  static const int significand_bits = 23;
  static const int exponent_bits = 8;
  static const int min_exponent = -126;
  static const int max_exponent = 127;
  static const int exponent_bias = -127;
  static const int decimal_digits = 9;
  static const int kappa = 1;
  static const int big_divisor = 100;
  static const int small_divisor = 10;
  static const int min_k = -31;
  static const int max_k = 46;
  static const int cache_bits = 64;
  static const int divisibility_check_by_5_threshold = 39;
  static const int case_fc_pm_half_lower_threshold = -1;
  static const int case_fc_pm_half_upper_threshold = 6;
  static const int case_fc_lower_threshold = -2;
  static const int case_fc_upper_threshold = 6;
  static const int case_shorter_interval_left_endpoint_lower_threshold = 2;
  static const int case_shorter_interval_left_endpoint_upper_threshold = 3;
  static const int shorter_interval_tie_lower_threshold = -35;
  static const int shorter_interval_tie_upper_threshold = -35;
  static const int max_trailing_zeros = 7;
};

template <> struct float_info<double> {
  using carrier_uint = uint64_t;
  static const int significand_bits = 52;
  static const int exponent_bits = 11;
  static const int min_exponent = -1022;
  static const int max_exponent = 1023;
  static const int exponent_bias = -1023;
  static const int decimal_digits = 17;
  static const int kappa = 2;
  static const int big_divisor = 1000;
  static const int small_divisor = 100;
  static const int min_k = -292;
  static const int max_k = 326;
  static const int cache_bits = 128;
  static const int divisibility_check_by_5_threshold = 86;
  static const int case_fc_pm_half_lower_threshold = -2;
  static const int case_fc_pm_half_upper_threshold = 9;
  static const int case_fc_lower_threshold = -4;
  static const int case_fc_upper_threshold = 9;
  static const int case_shorter_interval_left_endpoint_lower_threshold = 2;
  static const int case_shorter_interval_left_endpoint_upper_threshold = 3;
  static const int shorter_interval_tie_lower_threshold = -77;
  static const int shorter_interval_tie_upper_threshold = -77;
  static const int max_trailing_zeros = 16;
};

template <typename T> struct decimal_fp {
  using significand_type = typename float_info<T>::carrier_uint;
  significand_type significand;
  int exponent;
};

template <typename T> FMT_API decimal_fp<T> to_decimal(T x) FMT_NOEXCEPT;
}  // namespace dragonbox

template <typename T>
constexpr typename dragonbox::float_info<T>::carrier_uint exponent_mask() {
  using uint = typename dragonbox::float_info<T>::carrier_uint;
  return ((uint(1) << dragonbox::float_info<T>::exponent_bits) - 1)
         << dragonbox::float_info<T>::significand_bits;
}

// A floating-point presentation format.
enum class float_format : unsigned char {
  general,  // General: exponent notation or fixed point based on magnitude.
  exp,      // Exponent notation with the default precision of 6, e.g. 1.2e-3.
  fixed,    // Fixed point with the default precision of 6, e.g. 0.0012.
  hex
};

struct float_specs {
  int precision;
  float_format format : 8;
  sign_t sign : 8;
  bool upper : 1;
  bool locale : 1;
  bool binary32 : 1;
  bool use_grisu : 1;
  bool showpoint : 1;
};

// Writes the exponent exp in the form "[+-]d{2,3}" to buffer.
template <typename Char, typename It> It write_exponent(int exp, It it) {
  FMT_ASSERT(-10000 < exp && exp < 10000, "exponent out of range");
  if (exp < 0) {
    *it++ = static_cast<Char>('-');
    exp = -exp;
  } else {
    *it++ = static_cast<Char>('+');
  }
  if (exp >= 100) {
    const char* top = data::digits[exp / 100];
    if (exp >= 1000) *it++ = static_cast<Char>(top[0]);
    *it++ = static_cast<Char>(top[1]);
    exp %= 100;
  }
  const char* d = data::digits[exp];
  *it++ = static_cast<Char>(d[0]);
  *it++ = static_cast<Char>(d[1]);
  return it;
}

template <typename T>
int format_float(T value, int precision, float_specs specs, buffer<char>& buf);

// Formats a floating-point number with snprintf.
template <typename T>
int snprintf_float(T value, int precision, float_specs specs,
                   buffer<char>& buf);

template <typename T> T promote_float(T value) { return value; }
inline double promote_float(float value) { return static_cast<double>(value); }

template <typename Handler>
FMT_CONSTEXPR void handle_int_type_spec(char spec, Handler&& handler) {
  switch (spec) {
  case 0:
  case 'd':
    handler.on_dec();
    break;
  case 'x':
  case 'X':
    handler.on_hex();
    break;
  case 'b':
  case 'B':
    handler.on_bin();
    break;
  case 'o':
    handler.on_oct();
    break;
#ifdef FMT_DEPRECATED_N_SPECIFIER
  case 'n':
#endif
  case 'L':
    handler.on_num();
    break;
  case 'c':
    handler.on_chr();
    break;
  default:
    handler.on_error();
  }
}

template <typename ErrorHandler = error_handler, typename Char>
FMT_CONSTEXPR float_specs parse_float_type_spec(
    const basic_format_specs<Char>& specs, ErrorHandler&& eh = {}) {
  auto result = float_specs();
  result.showpoint = specs.alt;
  switch (specs.type) {
  case 0:
    result.format = float_format::general;
    result.showpoint |= specs.precision > 0;
    break;
  case 'G':
    result.upper = true;
    FMT_FALLTHROUGH;
  case 'g':
    result.format = float_format::general;
    break;
  case 'E':
    result.upper = true;
    FMT_FALLTHROUGH;
  case 'e':
    result.format = float_format::exp;
    result.showpoint |= specs.precision != 0;
    break;
  case 'F':
    result.upper = true;
    FMT_FALLTHROUGH;
  case 'f':
    result.format = float_format::fixed;
    result.showpoint |= specs.precision != 0;
    break;
  case 'A':
    result.upper = true;
    FMT_FALLTHROUGH;
  case 'a':
    result.format = float_format::hex;
    break;
#ifdef FMT_DEPRECATED_N_SPECIFIER
  case 'n':
#endif
  case 'L':
    result.locale = true;
    break;
  default:
    eh.on_error("invalid type specifier");
    break;
  }
  return result;
}

template <typename Char, typename Handler>
FMT_CONSTEXPR void handle_char_specs(const basic_format_specs<Char>* specs,
                                     Handler&& handler) {
  if (!specs) return handler.on_char();
  if (specs->type && specs->type != 'c') return handler.on_int();
  if (specs->align == align::numeric || specs->sign != sign::none || specs->alt)
    handler.on_error("invalid format specifier for char");
  handler.on_char();
}

template <typename Char, typename Handler>
FMT_CONSTEXPR void handle_cstring_type_spec(Char spec, Handler&& handler) {
  if (spec == 0 || spec == 's')
    handler.on_string();
  else if (spec == 'p')
    handler.on_pointer();
  else
    handler.on_error("invalid type specifier");
}

template <typename Char, typename ErrorHandler>
FMT_CONSTEXPR void check_string_type_spec(Char spec, ErrorHandler&& eh) {
  if (spec != 0 && spec != 's') eh.on_error("invalid type specifier");
}

template <typename Char, typename ErrorHandler>
FMT_CONSTEXPR void check_pointer_type_spec(Char spec, ErrorHandler&& eh) {
  if (spec != 0 && spec != 'p') eh.on_error("invalid type specifier");
}

template <typename ErrorHandler> class int_type_checker : private ErrorHandler {
 public:
  FMT_CONSTEXPR explicit int_type_checker(ErrorHandler eh) : ErrorHandler(eh) {}

  FMT_CONSTEXPR void on_dec() {}
  FMT_CONSTEXPR void on_hex() {}
  FMT_CONSTEXPR void on_bin() {}
  FMT_CONSTEXPR void on_oct() {}
  FMT_CONSTEXPR void on_num() {}
  FMT_CONSTEXPR void on_chr() {}

  FMT_CONSTEXPR void on_error() {
    ErrorHandler::on_error("invalid type specifier");
  }
};

template <typename ErrorHandler>
class char_specs_checker : public ErrorHandler {
 private:
  char type_;

 public:
  FMT_CONSTEXPR char_specs_checker(char type, ErrorHandler eh)
      : ErrorHandler(eh), type_(type) {}

  FMT_CONSTEXPR void on_int() {
    handle_int_type_spec(type_, int_type_checker<ErrorHandler>(*this));
  }
  FMT_CONSTEXPR void on_char() {}
};

template <typename ErrorHandler>
class cstring_type_checker : public ErrorHandler {
 public:
  FMT_CONSTEXPR explicit cstring_type_checker(ErrorHandler eh)
      : ErrorHandler(eh) {}

  FMT_CONSTEXPR void on_string() {}
  FMT_CONSTEXPR void on_pointer() {}
};

template <typename OutputIt, typename Char>
FMT_NOINLINE OutputIt fill(OutputIt it, size_t n, const fill_t<Char>& fill) {
  auto fill_size = fill.size();
  if (fill_size == 1) return std::fill_n(it, n, fill[0]);
  for (size_t i = 0; i < n; ++i) it = std::copy_n(fill.data(), fill_size, it);
  return it;
}

// Writes the output of f, padded according to format specifications in specs.
// size: output size in code units.
// width: output display width in (terminal) column positions.
template <align::type align = align::left, typename OutputIt, typename Char,
          typename F>
inline OutputIt write_padded(OutputIt out,
                             const basic_format_specs<Char>& specs, size_t size,
                             size_t width, F&& f) {
  static_assert(align == align::left || align == align::right, "");
  unsigned spec_width = to_unsigned(specs.width);
  size_t padding = spec_width > width ? spec_width - width : 0;
  auto* shifts = align == align::left ? data::left_padding_shifts
                                      : data::right_padding_shifts;
  size_t left_padding = padding >> shifts[specs.align];
  auto it = reserve(out, size + padding * specs.fill.size());
  it = fill(it, left_padding, specs.fill);
  it = f(it);
  it = fill(it, padding - left_padding, specs.fill);
  return base_iterator(out, it);
}

template <align::type align = align::left, typename OutputIt, typename Char,
          typename F>
inline OutputIt write_padded(OutputIt out,
                             const basic_format_specs<Char>& specs, size_t size,
                             F&& f) {
  return write_padded<align>(out, specs, size, size, f);
}

template <typename Char, typename OutputIt>
OutputIt write_bytes(OutputIt out, string_view bytes,
                     const basic_format_specs<Char>& specs) {
  using iterator = remove_reference_t<decltype(reserve(out, 0))>;
  return write_padded(out, specs, bytes.size(), [bytes](iterator it) {
    const char* data = bytes.data();
    return copy_str<Char>(data, data + bytes.size(), it);
  });
}

// Data for write_int that doesn't depend on output iterator type. It is used to
// avoid template code bloat.
template <typename Char> struct write_int_data {
  size_t size;
  size_t padding;

  write_int_data(int num_digits, string_view prefix,
                 const basic_format_specs<Char>& specs)
      : size(prefix.size() + to_unsigned(num_digits)), padding(0) {
    if (specs.align == align::numeric) {
      auto width = to_unsigned(specs.width);
      if (width > size) {
        padding = width - size;
        size = width;
      }
    } else if (specs.precision > num_digits) {
      size = prefix.size() + to_unsigned(specs.precision);
      padding = to_unsigned(specs.precision - num_digits);
    }
  }
};

// Writes an integer in the format
//   <left-padding><prefix><numeric-padding><digits><right-padding>
// where <digits> are written by f(it).
template <typename OutputIt, typename Char, typename F>
OutputIt write_int(OutputIt out, int num_digits, string_view prefix,
                   const basic_format_specs<Char>& specs, F f) {
  auto data = write_int_data<Char>(num_digits, prefix, specs);
  using iterator = remove_reference_t<decltype(reserve(out, 0))>;
  return write_padded<align::right>(out, specs, data.size, [=](iterator it) {
    if (prefix.size() != 0)
      it = copy_str<Char>(prefix.begin(), prefix.end(), it);
    it = std::fill_n(it, data.padding, static_cast<Char>('0'));
    return f(it);
  });
}

template <typename StrChar, typename Char, typename OutputIt>
OutputIt write(OutputIt out, basic_string_view<StrChar> s,
               const basic_format_specs<Char>& specs) {
  auto data = s.data();
  auto size = s.size();
  if (specs.precision >= 0 && to_unsigned(specs.precision) < size)
    size = code_point_index(s, to_unsigned(specs.precision));
  auto width = specs.width != 0
                   ? count_code_points(basic_string_view<StrChar>(data, size))
                   : 0;
  using iterator = remove_reference_t<decltype(reserve(out, 0))>;
  return write_padded(out, specs, size, width, [=](iterator it) {
    return copy_str<Char>(data, data + size, it);
  });
}

// The handle_int_type_spec handler that writes an integer.
template <typename OutputIt, typename Char, typename UInt> struct int_writer {
  OutputIt out;
  locale_ref locale;
  const basic_format_specs<Char>& specs;
  UInt abs_value;
  char prefix[4];
  unsigned prefix_size;

  using iterator =
      remove_reference_t<decltype(reserve(std::declval<OutputIt&>(), 0))>;

  string_view get_prefix() const { return string_view(prefix, prefix_size); }

  template <typename Int>
  int_writer(OutputIt output, locale_ref loc, Int value,
             const basic_format_specs<Char>& s)
      : out(output),
        locale(loc),
        specs(s),
        abs_value(static_cast<UInt>(value)),
        prefix_size(0) {
    static_assert(std::is_same<uint32_or_64_or_128_t<Int>, UInt>::value, "");
    if (is_negative(value)) {
      prefix[0] = '-';
      ++prefix_size;
      abs_value = 0 - abs_value;
    } else if (specs.sign != sign::none && specs.sign != sign::minus) {
      prefix[0] = specs.sign == sign::plus ? '+' : ' ';
      ++prefix_size;
    }
  }

  void on_dec() {
    auto num_digits = count_digits(abs_value);
    out = write_int(
        out, num_digits, get_prefix(), specs, [this, num_digits](iterator it) {
          return format_decimal<Char>(it, abs_value, num_digits).end;
        });
  }

  void on_hex() {
    if (specs.alt) {
      prefix[prefix_size++] = '0';
      prefix[prefix_size++] = specs.type;
    }
    int num_digits = count_digits<4>(abs_value);
    out = write_int(out, num_digits, get_prefix(), specs,
                    [this, num_digits](iterator it) {
                      return format_uint<4, Char>(it, abs_value, num_digits,
                                                  specs.type != 'x');
                    });
  }

  void on_bin() {
    if (specs.alt) {
      prefix[prefix_size++] = '0';
      prefix[prefix_size++] = static_cast<char>(specs.type);
    }
    int num_digits = count_digits<1>(abs_value);
    out = write_int(out, num_digits, get_prefix(), specs,
                    [this, num_digits](iterator it) {
                      return format_uint<1, Char>(it, abs_value, num_digits);
                    });
  }

  void on_oct() {
    int num_digits = count_digits<3>(abs_value);
    if (specs.alt && specs.precision <= num_digits && abs_value != 0) {
      // Octal prefix '0' is counted as a digit, so only add it if precision
      // is not greater than the number of digits.
      prefix[prefix_size++] = '0';
    }
    out = write_int(out, num_digits, get_prefix(), specs,
                    [this, num_digits](iterator it) {
                      return format_uint<3, Char>(it, abs_value, num_digits);
                    });
  }

  enum { sep_size = 1 };

  void on_num() {
    std::string groups = grouping<Char>(locale);
    if (groups.empty()) return on_dec();
    auto sep = thousands_sep<Char>(locale);
    if (!sep) return on_dec();
    int num_digits = count_digits(abs_value);
    int size = num_digits, n = num_digits;
    std::string::const_iterator group = groups.cbegin();
    while (group != groups.cend() && n > *group && *group > 0 &&
           *group != max_value<char>()) {
      size += sep_size;
      n -= *group;
      ++group;
    }
    if (group == groups.cend()) size += sep_size * ((n - 1) / groups.back());
    char digits[40];
    format_decimal(digits, abs_value, num_digits);
    basic_memory_buffer<Char> buffer;
    size += static_cast<int>(prefix_size);
    const auto usize = to_unsigned(size);
    buffer.resize(usize);
    basic_string_view<Char> s(&sep, sep_size);
    // Index of a decimal digit with the least significant digit having index 0.
    int digit_index = 0;
    group = groups.cbegin();
    auto p = buffer.data() + size - 1;
    for (int i = num_digits - 1; i > 0; --i) {
      *p-- = static_cast<Char>(digits[i]);
      if (*group <= 0 || ++digit_index % *group != 0 ||
          *group == max_value<char>())
        continue;
      if (group + 1 != groups.cend()) {
        digit_index = 0;
        ++group;
      }
      std::uninitialized_copy(s.data(), s.data() + s.size(),
                              make_checked(p, s.size()));
      p -= s.size();
    }
    *p-- = static_cast<Char>(*digits);
    if (prefix_size != 0) *p = static_cast<Char>('-');
    auto data = buffer.data();
    out = write_padded<align::right>(
        out, specs, usize, usize,
        [=](iterator it) { return copy_str<Char>(data, data + size, it); });
  }

  void on_chr() { *out++ = static_cast<Char>(abs_value); }

  FMT_NORETURN void on_error() {
    FMT_THROW(format_error("invalid type specifier"));
  }
};

template <typename Char, typename OutputIt>
OutputIt write_nonfinite(OutputIt out, bool isinf,
                         const basic_format_specs<Char>& specs,
                         const float_specs& fspecs) {
  auto str =
      isinf ? (fspecs.upper ? "INF" : "inf") : (fspecs.upper ? "NAN" : "nan");
  constexpr size_t str_size = 3;
  auto sign = fspecs.sign;
  auto size = str_size + (sign ? 1 : 0);
  using iterator = remove_reference_t<decltype(reserve(out, 0))>;
  return write_padded(out, specs, size, [=](iterator it) {
    if (sign) *it++ = static_cast<Char>(data::signs[sign]);
    return copy_str<Char>(str, str + str_size, it);
  });
}

// A decimal floating-point number significand * pow(10, exp).
struct big_decimal_fp {
  const char* significand;
  int significand_size;
  int exponent;
};

inline int get_significand_size(const big_decimal_fp& fp) {
  return fp.significand_size;
}
template <typename T>
inline int get_significand_size(const dragonbox::decimal_fp<T>& fp) {
  return count_digits(fp.significand);
}

template <typename Char, typename OutputIt>
inline OutputIt write_significand(OutputIt out, const char* significand,
                                  int& significand_size) {
  return copy_str<Char>(significand, significand + significand_size, out);
}
template <typename Char, typename OutputIt, typename UInt>
inline OutputIt write_significand(OutputIt out, UInt significand,
                                  int significand_size) {
  return format_decimal<Char>(out, significand, significand_size).end;
}

template <typename Char, typename UInt,
          FMT_ENABLE_IF(std::is_integral<UInt>::value)>
inline Char* write_significand(Char* out, UInt significand,
                               int significand_size, int integral_size,
                               Char decimal_point) {
  if (!decimal_point)
    return format_decimal(out, significand, significand_size).end;
  auto end = format_decimal(out + 1, significand, significand_size).end;
  if (integral_size == 1)
    out[0] = out[1];
  else
    std::copy_n(out + 1, integral_size, out);
  out[integral_size] = decimal_point;
  return end;
}

template <typename OutputIt, typename UInt, typename Char,
          FMT_ENABLE_IF(!std::is_pointer<remove_cvref_t<OutputIt>>::value)>
inline OutputIt write_significand(OutputIt out, UInt significand,
                                  int significand_size, int integral_size,
                                  Char decimal_point) {
  // Buffer is large enough to hold digits (digits10 + 1) and a decimal point.
  Char buffer[digits10<UInt>() + 2];
  auto end = write_significand(buffer, significand, significand_size,
                               integral_size, decimal_point);
  return detail::copy_str<Char>(buffer, end, out);
}

template <typename OutputIt, typename Char>
inline OutputIt write_significand(OutputIt out, const char* significand,
                                  int significand_size, int integral_size,
                                  Char decimal_point) {
  out = detail::copy_str<Char>(significand, significand + integral_size, out);
  if (!decimal_point) return out;
  *out++ = decimal_point;
  return detail::copy_str<Char>(significand + integral_size,
                                significand + significand_size, out);
}

template <typename OutputIt, typename DecimalFP, typename Char>
OutputIt write_float(OutputIt out, const DecimalFP& fp,
                     const basic_format_specs<Char>& specs, float_specs fspecs,
                     Char decimal_point) {
  auto significand = fp.significand;
  int significand_size = get_significand_size(fp);
  static const Char zero = static_cast<Char>('0');
  auto sign = fspecs.sign;
  size_t size = to_unsigned(significand_size) + (sign ? 1 : 0);
  using iterator = remove_reference_t<decltype(reserve(out, 0))>;

  int output_exp = fp.exponent + significand_size - 1;
  auto use_exp_format = [=]() {
    if (fspecs.format == float_format::exp) return true;
    if (fspecs.format != float_format::general) return false;
    // Use the fixed notation if the exponent is in [exp_lower, exp_upper),
    // e.g. 0.0001 instead of 1e-04. Otherwise use the exponent notation.
    const int exp_lower = -4, exp_upper = 16;
    return output_exp < exp_lower ||
           output_exp >= (fspecs.precision > 0 ? fspecs.precision : exp_upper);
  };
  if (use_exp_format()) {
    int num_zeros = 0;
    if (fspecs.showpoint) {
      num_zeros = (std::max)(fspecs.precision - significand_size, 0);
      size += to_unsigned(num_zeros);
    } else if (significand_size == 1) {
      decimal_point = Char();
    }
    auto abs_output_exp = output_exp >= 0 ? output_exp : -output_exp;
    int exp_digits = 2;
    if (abs_output_exp >= 100) exp_digits = abs_output_exp >= 1000 ? 4 : 3;

    size += to_unsigned((decimal_point ? 1 : 0) + 2 + exp_digits);
    char exp_char = fspecs.upper ? 'E' : 'e';
    auto write = [=](iterator it) {
      if (sign) *it++ = static_cast<Char>(data::signs[sign]);
      // Insert a decimal point after the first digit and add an exponent.
      it = write_significand(it, significand, significand_size, 1,
                             decimal_point);
      if (num_zeros > 0) it = std::fill_n(it, num_zeros, zero);
      *it++ = static_cast<Char>(exp_char);
      return write_exponent<Char>(output_exp, it);
    };
    return specs.width > 0 ? write_padded<align::right>(out, specs, size, write)
                           : base_iterator(out, write(reserve(out, size)));
  }

  int exp = fp.exponent + significand_size;
  if (fp.exponent >= 0) {
    // 1234e5 -> 123400000[.0+]
    size += to_unsigned(fp.exponent);
    int num_zeros = fspecs.precision - exp;
#ifdef FMT_FUZZ
    if (num_zeros > 5000)
      throw std::runtime_error("fuzz mode - avoiding excessive cpu use");
#endif
    if (fspecs.showpoint) {
      if (num_zeros <= 0 && fspecs.format != float_format::fixed) num_zeros = 1;
      if (num_zeros > 0) size += to_unsigned(num_zeros);
    }
    return write_padded<align::right>(out, specs, size, [&](iterator it) {
      if (sign) *it++ = static_cast<Char>(data::signs[sign]);
      it = write_significand<Char>(it, significand, significand_size);
      it = std::fill_n(it, fp.exponent, zero);
      if (!fspecs.showpoint) return it;
      *it++ = decimal_point;
      return num_zeros > 0 ? std::fill_n(it, num_zeros, zero) : it;
    });
  } else if (exp > 0) {
    // 1234e-2 -> 12.34[0+]
    int num_zeros = fspecs.showpoint ? fspecs.precision - significand_size : 0;
    size += 1 + to_unsigned(num_zeros > 0 ? num_zeros : 0);
    return write_padded<align::right>(out, specs, size, [&](iterator it) {
      if (sign) *it++ = static_cast<Char>(data::signs[sign]);
      it = write_significand(it, significand, significand_size, exp,
                             decimal_point);
      return num_zeros > 0 ? std::fill_n(it, num_zeros, zero) : it;
    });
  }
  // 1234e-6 -> 0.001234
  int num_zeros = -exp;
  if (significand_size == 0 && fspecs.precision >= 0 &&
      fspecs.precision < num_zeros) {
    num_zeros = fspecs.precision;
  }
  size += 2 + to_unsigned(num_zeros);
  return write_padded<align::right>(out, specs, size, [&](iterator it) {
    if (sign) *it++ = static_cast<Char>(data::signs[sign]);
    *it++ = zero;
    if (num_zeros == 0 && significand_size == 0 && !fspecs.showpoint) return it;
    *it++ = decimal_point;
    it = std::fill_n(it, num_zeros, zero);
    return write_significand<Char>(it, significand, significand_size);
  });
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(std::is_floating_point<T>::value)>
OutputIt write(OutputIt out, T value, basic_format_specs<Char> specs,
               locale_ref loc = {}) {
  if (const_check(!is_supported_floating_point(value))) return out;
  float_specs fspecs = parse_float_type_spec(specs);
  fspecs.sign = specs.sign;
  if (std::signbit(value)) {  // value < 0 is false for NaN so use signbit.
    fspecs.sign = sign::minus;
    value = -value;
  } else if (fspecs.sign == sign::minus) {
    fspecs.sign = sign::none;
  }

  if (!std::isfinite(value))
    return write_nonfinite(out, std::isinf(value), specs, fspecs);

  if (specs.align == align::numeric && fspecs.sign) {
    auto it = reserve(out, 1);
    *it++ = static_cast<Char>(data::signs[fspecs.sign]);
    out = base_iterator(out, it);
    fspecs.sign = sign::none;
    if (specs.width != 0) --specs.width;
  }

  memory_buffer buffer;
  if (fspecs.format == float_format::hex) {
    if (fspecs.sign) buffer.push_back(data::signs[fspecs.sign]);
    snprintf_float(promote_float(value), specs.precision, fspecs, buffer);
    return write_bytes(out, {buffer.data(), buffer.size()}, specs);
  }
  int precision = specs.precision >= 0 || !specs.type ? specs.precision : 6;
  if (fspecs.format == float_format::exp) {
    if (precision == max_value<int>())
      FMT_THROW(format_error("number is too big"));
    else
      ++precision;
  }
  if (const_check(std::is_same<T, float>())) fspecs.binary32 = true;
  fspecs.use_grisu = is_fast_float<T>();
  int exp = format_float(promote_float(value), precision, fspecs, buffer);
  fspecs.precision = precision;
  Char point =
      fspecs.locale ? decimal_point<Char>(loc) : static_cast<Char>('.');
  auto fp = big_decimal_fp{buffer.data(), static_cast<int>(buffer.size()), exp};
  return write_float(out, fp, specs, fspecs, point);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_fast_float<T>::value)>
OutputIt write(OutputIt out, T value) {
  if (const_check(!is_supported_floating_point(value))) return out;

  using floaty = conditional_t<std::is_same<T, long double>::value, double, T>;
  using uint = typename dragonbox::float_info<floaty>::carrier_uint;
  auto bits = bit_cast<uint>(value);

  auto fspecs = float_specs();
  auto sign_bit = bits & (uint(1) << (num_bits<uint>() - 1));
  if (sign_bit != 0) {
    fspecs.sign = sign::minus;
    value = -value;
  }

  static const auto specs = basic_format_specs<Char>();
  uint mask = exponent_mask<floaty>();
  if ((bits & mask) == mask)
    return write_nonfinite(out, std::isinf(value), specs, fspecs);

  auto dec = dragonbox::to_decimal(static_cast<floaty>(value));
  return write_float(out, dec, specs, fspecs, static_cast<Char>('.'));
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(std::is_floating_point<T>::value &&
                        !is_fast_float<T>::value)>
inline OutputIt write(OutputIt out, T value) {
  return write(out, value, basic_format_specs<Char>());
}

template <typename Char, typename OutputIt>
OutputIt write_char(OutputIt out, Char value,
                    const basic_format_specs<Char>& specs) {
  using iterator = remove_reference_t<decltype(reserve(out, 0))>;
  return write_padded(out, specs, 1, [=](iterator it) {
    *it++ = value;
    return it;
  });
}

template <typename Char, typename OutputIt, typename UIntPtr>
OutputIt write_ptr(OutputIt out, UIntPtr value,
                   const basic_format_specs<Char>* specs) {
  int num_digits = count_digits<4>(value);
  auto size = to_unsigned(num_digits) + size_t(2);
  using iterator = remove_reference_t<decltype(reserve(out, 0))>;
  auto write = [=](iterator it) {
    *it++ = static_cast<Char>('0');
    *it++ = static_cast<Char>('x');
    return format_uint<4, Char>(it, value, num_digits);
  };
  return specs ? write_padded<align::right>(out, *specs, size, write)
               : base_iterator(out, write(reserve(out, size)));
}

template <typename T> struct is_integral : std::is_integral<T> {};
template <> struct is_integral<int128_t> : std::true_type {};
template <> struct is_integral<uint128_t> : std::true_type {};

template <typename Char, typename OutputIt>
OutputIt write(OutputIt out, monostate) {
  FMT_ASSERT(false, "");
  return out;
}

template <typename Char, typename OutputIt,
          FMT_ENABLE_IF(!std::is_same<Char, char>::value)>
OutputIt write(OutputIt out, string_view value) {
  auto it = reserve(out, value.size());
  it = copy_str<Char>(value.begin(), value.end(), it);
  return base_iterator(out, it);
}

template <typename Char, typename OutputIt>
OutputIt write(OutputIt out, basic_string_view<Char> value) {
  auto it = reserve(out, value.size());
  it = std::copy(value.begin(), value.end(), it);
  return base_iterator(out, it);
}

template <typename Char>
buffer_appender<Char> write(buffer_appender<Char> out,
                            basic_string_view<Char> value) {
  get_container(out).append(value.begin(), value.end());
  return out;
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_integral<T>::value &&
                        !std::is_same<T, bool>::value &&
                        !std::is_same<T, Char>::value)>
OutputIt write(OutputIt out, T value) {
  auto abs_value = static_cast<uint32_or_64_or_128_t<T>>(value);
  bool negative = is_negative(value);
  // Don't do -abs_value since it trips unsigned-integer-overflow sanitizer.
  if (negative) abs_value = ~abs_value + 1;
  int num_digits = count_digits(abs_value);
  auto size = (negative ? 1 : 0) + static_cast<size_t>(num_digits);
  auto it = reserve(out, size);
  if (auto ptr = to_pointer<Char>(it, size)) {
    if (negative) *ptr++ = static_cast<Char>('-');
    format_decimal<Char>(ptr, abs_value, num_digits);
    return out;
  }
  if (negative) *it++ = static_cast<Char>('-');
  it = format_decimal<Char>(it, abs_value, num_digits).end;
  return base_iterator(out, it);
}

template <typename Char, typename OutputIt>
OutputIt write(OutputIt out, bool value) {
  return write<Char>(out, string_view(value ? "true" : "false"));
}

template <typename Char, typename OutputIt>
OutputIt write(OutputIt out, Char value) {
  auto it = reserve(out, 1);
  *it++ = value;
  return base_iterator(out, it);
}

template <typename Char, typename OutputIt>
OutputIt write(OutputIt out, const Char* value) {
  if (!value) {
    FMT_THROW(format_error("string pointer is null"));
  } else {
    auto length = std::char_traits<Char>::length(value);
    out = write(out, basic_string_view<Char>(value, length));
  }
  return out;
}

template <typename Char, typename OutputIt>
OutputIt write(OutputIt out, const void* value) {
  return write_ptr<Char>(out, to_uintptr(value), nullptr);
}

template <typename Char, typename OutputIt, typename T>
auto write(OutputIt out, const T& value) -> typename std::enable_if<
    mapped_type_constant<T, basic_format_context<OutputIt, Char>>::value ==
        type::custom_type,
    OutputIt>::type {
  using context_type = basic_format_context<OutputIt, Char>;
  using formatter_type =
      conditional_t<has_formatter<T, context_type>::value,
                    typename context_type::template formatter_type<T>,
                    fallback_formatter<T, Char>>;
  context_type ctx(out, {}, {});
  return formatter_type().format(value, ctx);
}

// An argument visitor that formats the argument and writes it via the output
// iterator. It's a class and not a generic lambda for compatibility with C++11.
template <typename OutputIt, typename Char> struct default_arg_formatter {
  using context = basic_format_context<OutputIt, Char>;

  OutputIt out;
  basic_format_args<context> args;
  locale_ref loc;

  template <typename T> OutputIt operator()(T value) {
    return write<Char>(out, value);
  }

  OutputIt operator()(typename basic_format_arg<context>::handle handle) {
    basic_format_parse_context<Char> parse_ctx({});
    basic_format_context<OutputIt, Char> format_ctx(out, args, loc);
    handle.format(parse_ctx, format_ctx);
    return format_ctx.out();
  }
};

template <typename OutputIt, typename Char,
          typename ErrorHandler = error_handler>
class arg_formatter_base {
 public:
  using iterator = OutputIt;
  using char_type = Char;
  using format_specs = basic_format_specs<Char>;

 private:
  iterator out_;
  locale_ref locale_;
  format_specs* specs_;

  // Attempts to reserve space for n extra characters in the output range.
  // Returns a pointer to the reserved range or a reference to out_.
  auto reserve(size_t n) -> decltype(detail::reserve(out_, n)) {
    return detail::reserve(out_, n);
  }

  using reserve_iterator = remove_reference_t<decltype(
      detail::reserve(std::declval<iterator&>(), 0))>;

  template <typename T> void write_int(T value, const format_specs& spec) {
    using uint_type = uint32_or_64_or_128_t<T>;
    int_writer<iterator, Char, uint_type> w(out_, locale_, value, spec);
    handle_int_type_spec(spec.type, w);
    out_ = w.out;
  }

  void write(char value) {
    auto&& it = reserve(1);
    *it++ = value;
  }

  template <typename Ch, FMT_ENABLE_IF(std::is_same<Ch, Char>::value)>
  void write(Ch value) {
    out_ = detail::write<Char>(out_, value);
  }

  void write(string_view value) {
    auto&& it = reserve(value.size());
    it = copy_str<Char>(value.begin(), value.end(), it);
  }
  void write(wstring_view value) {
    static_assert(std::is_same<Char, wchar_t>::value, "");
    auto&& it = reserve(value.size());
    it = std::copy(value.begin(), value.end(), it);
  }

  template <typename Ch>
  void write(const Ch* s, size_t size, const format_specs& specs) {
    auto width = specs.width != 0
                     ? count_code_points(basic_string_view<Ch>(s, size))
                     : 0;
    out_ = write_padded(out_, specs, size, width, [=](reserve_iterator it) {
      return copy_str<Char>(s, s + size, it);
    });
  }

  template <typename Ch>
  void write(basic_string_view<Ch> s, const format_specs& specs = {}) {
    out_ = detail::write(out_, s, specs);
  }

  void write_pointer(const void* p) {
    out_ = write_ptr<char_type>(out_, to_uintptr(p), specs_);
  }

  struct char_spec_handler : ErrorHandler {
    arg_formatter_base& formatter;
    Char value;

    char_spec_handler(arg_formatter_base& f, Char val)
        : formatter(f), value(val) {}

    void on_int() {
      // char is only formatted as int if there are specs.
      formatter.write_int(static_cast<int>(value), *formatter.specs_);
    }
    void on_char() {
      if (formatter.specs_)
        formatter.out_ = write_char(formatter.out_, value, *formatter.specs_);
      else
        formatter.write(value);
    }
  };

  struct cstring_spec_handler : error_handler {
    arg_formatter_base& formatter;
    const Char* value;

    cstring_spec_handler(arg_formatter_base& f, const Char* val)
        : formatter(f), value(val) {}

    void on_string() { formatter.write(value); }
    void on_pointer() { formatter.write_pointer(value); }
  };

 protected:
  iterator out() { return out_; }
  format_specs* specs() { return specs_; }

  void write(bool value) {
    if (specs_)
      write(string_view(value ? "true" : "false"), *specs_);
    else
      out_ = detail::write<Char>(out_, value);
  }

  void write(const Char* value) {
    if (!value) {
      FMT_THROW(format_error("string pointer is null"));
    } else {
      auto length = std::char_traits<char_type>::length(value);
      basic_string_view<char_type> sv(value, length);
      specs_ ? write(sv, *specs_) : write(sv);
    }
  }

 public:
  arg_formatter_base(OutputIt out, format_specs* s, locale_ref loc)
      : out_(out), locale_(loc), specs_(s) {}

  iterator operator()(monostate) {
    FMT_ASSERT(false, "invalid argument type");
    return out_;
  }

  template <typename T, FMT_ENABLE_IF(is_integral<T>::value)>
  FMT_INLINE iterator operator()(T value) {
    if (specs_)
      write_int(value, *specs_);
    else
      out_ = detail::write<Char>(out_, value);
    return out_;
  }

  iterator operator()(Char value) {
    handle_char_specs(specs_,
                      char_spec_handler(*this, static_cast<Char>(value)));
    return out_;
  }

  iterator operator()(bool value) {
    if (specs_ && specs_->type) return (*this)(value ? 1 : 0);
    write(value != 0);
    return out_;
  }

  template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value)>
  iterator operator()(T value) {
    auto specs = specs_ ? *specs_ : format_specs();
    if (const_check(is_supported_floating_point(value)))
      out_ = detail::write(out_, value, specs, locale_);
    else
      FMT_ASSERT(false, "unsupported float argument type");
    return out_;
  }

  iterator operator()(const Char* value) {
    if (!specs_) return write(value), out_;
    handle_cstring_type_spec(specs_->type, cstring_spec_handler(*this, value));
    return out_;
  }

  iterator operator()(basic_string_view<Char> value) {
    if (specs_) {
      check_string_type_spec(specs_->type, error_handler());
      write(value, *specs_);
    } else {
      write(value);
    }
    return out_;
  }

  iterator operator()(const void* value) {
    if (specs_) check_pointer_type_spec(specs_->type, error_handler());
    write_pointer(value);
    return out_;
  }
};

/** The default argument formatter. */
template <typename OutputIt, typename Char>
class arg_formatter : public arg_formatter_base<OutputIt, Char> {
 private:
  using char_type = Char;
  using base = arg_formatter_base<OutputIt, Char>;
  using context_type = basic_format_context<OutputIt, Char>;

  context_type& ctx_;
  basic_format_parse_context<char_type>* parse_ctx_;
  const Char* ptr_;

 public:
  using iterator = typename base::iterator;
  using format_specs = typename base::format_specs;

  /**
    \rst
    Constructs an argument formatter object.
    *ctx* is a reference to the formatting context,
    *specs* contains format specifier information for standard argument types.
    \endrst
   */
  explicit arg_formatter(
      context_type& ctx,
      basic_format_parse_context<char_type>* parse_ctx = nullptr,
      format_specs* specs = nullptr, const Char* ptr = nullptr)
      : base(ctx.out(), specs, ctx.locale()),
        ctx_(ctx),
        parse_ctx_(parse_ctx),
        ptr_(ptr) {}

  using base::operator();

  /** Formats an argument of a user-defined type. */
  iterator operator()(typename basic_format_arg<context_type>::handle handle) {
    if (ptr_) advance_to(*parse_ctx_, ptr_);
    handle.format(*parse_ctx_, ctx_);
    return ctx_.out();
  }
};

template <typename Char> FMT_CONSTEXPR bool is_name_start(Char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || '_' == c;
}

// Parses the range [begin, end) as an unsigned integer. This function assumes
// that the range is non-empty and the first character is a digit.
template <typename Char, typename ErrorHandler>
FMT_CONSTEXPR int parse_nonnegative_int(const Char*& begin, const Char* end,
                                        ErrorHandler&& eh) {
  FMT_ASSERT(begin != end && '0' <= *begin && *begin <= '9', "");
  unsigned value = 0;
  // Convert to unsigned to prevent a warning.
  constexpr unsigned max_int = max_value<int>();
  unsigned big = max_int / 10;
  do {
    // Check for overflow.
    if (value > big) {
      value = max_int + 1;
      break;
    }
    value = value * 10 + unsigned(*begin - '0');
    ++begin;
  } while (begin != end && '0' <= *begin && *begin <= '9');
  if (value > max_int) eh.on_error("number is too big");
  return static_cast<int>(value);
}

template <typename Context> class custom_formatter {
 private:
  using char_type = typename Context::char_type;

  basic_format_parse_context<char_type>& parse_ctx_;
  Context& ctx_;

 public:
  explicit custom_formatter(basic_format_parse_context<char_type>& parse_ctx,
                            Context& ctx)
      : parse_ctx_(parse_ctx), ctx_(ctx) {}

  void operator()(typename basic_format_arg<Context>::handle h) const {
    h.format(parse_ctx_, ctx_);
  }

  template <typename T> void operator()(T) const {}
};

template <typename T>
using is_integer =
    bool_constant<is_integral<T>::value && !std::is_same<T, bool>::value &&
                  !std::is_same<T, char>::value &&
                  !std::is_same<T, wchar_t>::value>;

template <typename ErrorHandler> class width_checker {
 public:
  explicit FMT_CONSTEXPR width_checker(ErrorHandler& eh) : handler_(eh) {}

  template <typename T, FMT_ENABLE_IF(is_integer<T>::value)>
  FMT_CONSTEXPR unsigned long long operator()(T value) {
    if (is_negative(value)) handler_.on_error("negative width");
    return static_cast<unsigned long long>(value);
  }

  template <typename T, FMT_ENABLE_IF(!is_integer<T>::value)>
  FMT_CONSTEXPR unsigned long long operator()(T) {
    handler_.on_error("width is not integer");
    return 0;
  }

 private:
  ErrorHandler& handler_;
};

template <typename ErrorHandler> class precision_checker {
 public:
  explicit FMT_CONSTEXPR precision_checker(ErrorHandler& eh) : handler_(eh) {}

  template <typename T, FMT_ENABLE_IF(is_integer<T>::value)>
  FMT_CONSTEXPR unsigned long long operator()(T value) {
    if (is_negative(value)) handler_.on_error("negative precision");
    return static_cast<unsigned long long>(value);
  }

  template <typename T, FMT_ENABLE_IF(!is_integer<T>::value)>
  FMT_CONSTEXPR unsigned long long operator()(T) {
    handler_.on_error("precision is not integer");
    return 0;
  }

 private:
  ErrorHandler& handler_;
};

// A format specifier handler that sets fields in basic_format_specs.
template <typename Char> class specs_setter {
 public:
  explicit FMT_CONSTEXPR specs_setter(basic_format_specs<Char>& specs)
      : specs_(specs) {}

  FMT_CONSTEXPR specs_setter(const specs_setter& other)
      : specs_(other.specs_) {}

  FMT_CONSTEXPR void on_align(align_t align) { specs_.align = align; }
  FMT_CONSTEXPR void on_fill(basic_string_view<Char> fill) {
    specs_.fill = fill;
  }
  FMT_CONSTEXPR void on_plus() { specs_.sign = sign::plus; }
  FMT_CONSTEXPR void on_minus() { specs_.sign = sign::minus; }
  FMT_CONSTEXPR void on_space() { specs_.sign = sign::space; }
  FMT_CONSTEXPR void on_hash() { specs_.alt = true; }

  FMT_CONSTEXPR void on_zero() {
    specs_.align = align::numeric;
    specs_.fill[0] = Char('0');
  }

  FMT_CONSTEXPR void on_width(int width) { specs_.width = width; }
  FMT_CONSTEXPR void on_precision(int precision) {
    specs_.precision = precision;
  }
  FMT_CONSTEXPR void end_precision() {}

  FMT_CONSTEXPR void on_type(Char type) {
    specs_.type = static_cast<char>(type);
  }

 protected:
  basic_format_specs<Char>& specs_;
};

template <typename ErrorHandler> class numeric_specs_checker {
 public:
  FMT_CONSTEXPR numeric_specs_checker(ErrorHandler& eh, detail::type arg_type)
      : error_handler_(eh), arg_type_(arg_type) {}

  FMT_CONSTEXPR void require_numeric_argument() {
    if (!is_arithmetic_type(arg_type_))
      error_handler_.on_error("format specifier requires numeric argument");
  }

  FMT_CONSTEXPR void check_sign() {
    require_numeric_argument();
    if (is_integral_type(arg_type_) && arg_type_ != type::int_type &&
        arg_type_ != type::long_long_type && arg_type_ != type::char_type) {
      error_handler_.on_error("format specifier requires signed argument");
    }
  }

  FMT_CONSTEXPR void check_precision() {
    if (is_integral_type(arg_type_) || arg_type_ == type::pointer_type)
      error_handler_.on_error("precision not allowed for this argument type");
  }

 private:
  ErrorHandler& error_handler_;
  detail::type arg_type_;
};

// A format specifier handler that checks if specifiers are consistent with the
// argument type.
template <typename Handler> class specs_checker : public Handler {
 private:
  numeric_specs_checker<Handler> checker_;

  // Suppress an MSVC warning about using this in initializer list.
  FMT_CONSTEXPR Handler& error_handler() { return *this; }

 public:
  FMT_CONSTEXPR specs_checker(const Handler& handler, detail::type arg_type)
      : Handler(handler), checker_(error_handler(), arg_type) {}

  FMT_CONSTEXPR specs_checker(const specs_checker& other)
      : Handler(other), checker_(error_handler(), other.arg_type_) {}

  FMT_CONSTEXPR void on_align(align_t align) {
    if (align == align::numeric) checker_.require_numeric_argument();
    Handler::on_align(align);
  }

  FMT_CONSTEXPR void on_plus() {
    checker_.check_sign();
    Handler::on_plus();
  }

  FMT_CONSTEXPR void on_minus() {
    checker_.check_sign();
    Handler::on_minus();
  }

  FMT_CONSTEXPR void on_space() {
    checker_.check_sign();
    Handler::on_space();
  }

  FMT_CONSTEXPR void on_hash() {
    checker_.require_numeric_argument();
    Handler::on_hash();
  }

  FMT_CONSTEXPR void on_zero() {
    checker_.require_numeric_argument();
    Handler::on_zero();
  }

  FMT_CONSTEXPR void end_precision() { checker_.check_precision(); }
};

template <template <typename> class Handler, typename FormatArg,
          typename ErrorHandler>
FMT_CONSTEXPR int get_dynamic_spec(FormatArg arg, ErrorHandler eh) {
  unsigned long long value = visit_format_arg(Handler<ErrorHandler>(eh), arg);
  if (value > to_unsigned(max_value<int>())) eh.on_error("number is too big");
  return static_cast<int>(value);
}

struct auto_id {};

template <typename Context, typename ID>
FMT_CONSTEXPR typename Context::format_arg get_arg(Context& ctx, ID id) {
  auto arg = ctx.arg(id);
  if (!arg) ctx.on_error("argument not found");
  return arg;
}

// The standard format specifier handler with checking.
template <typename ParseContext, typename Context>
class specs_handler : public specs_setter<typename Context::char_type> {
 public:
  using char_type = typename Context::char_type;

  FMT_CONSTEXPR specs_handler(basic_format_specs<char_type>& specs,
                              ParseContext& parse_ctx, Context& ctx)
      : specs_setter<char_type>(specs),
        parse_context_(parse_ctx),
        context_(ctx) {}

  template <typename Id> FMT_CONSTEXPR void on_dynamic_width(Id arg_id) {
    this->specs_.width = get_dynamic_spec<width_checker>(
        get_arg(arg_id), context_.error_handler());
  }

  template <typename Id> FMT_CONSTEXPR void on_dynamic_precision(Id arg_id) {
    this->specs_.precision = get_dynamic_spec<precision_checker>(
        get_arg(arg_id), context_.error_handler());
  }

  void on_error(const char* message) { context_.on_error(message); }

 private:
  // This is only needed for compatibility with gcc 4.4.
  using format_arg = typename Context::format_arg;

  FMT_CONSTEXPR format_arg get_arg(auto_id) {
    return detail::get_arg(context_, parse_context_.next_arg_id());
  }

  FMT_CONSTEXPR format_arg get_arg(int arg_id) {
    parse_context_.check_arg_id(arg_id);
    return detail::get_arg(context_, arg_id);
  }

  FMT_CONSTEXPR format_arg get_arg(basic_string_view<char_type> arg_id) {
    parse_context_.check_arg_id(arg_id);
    return detail::get_arg(context_, arg_id);
  }

  ParseContext& parse_context_;
  Context& context_;
};

enum class arg_id_kind { none, index, name };

// An argument reference.
template <typename Char> struct arg_ref {
  FMT_CONSTEXPR arg_ref() : kind(arg_id_kind::none), val() {}

  FMT_CONSTEXPR explicit arg_ref(int index)
      : kind(arg_id_kind::index), val(index) {}
  FMT_CONSTEXPR explicit arg_ref(basic_string_view<Char> name)
      : kind(arg_id_kind::name), val(name) {}

  FMT_CONSTEXPR arg_ref& operator=(int idx) {
    kind = arg_id_kind::index;
    val.index = idx;
    return *this;
  }

  arg_id_kind kind;
  union value {
    FMT_CONSTEXPR value(int id = 0) : index{id} {}
    FMT_CONSTEXPR value(basic_string_view<Char> n) : name(n) {}

    int index;
    basic_string_view<Char> name;
  } val;
};

// Format specifiers with width and precision resolved at formatting rather
// than parsing time to allow re-using the same parsed specifiers with
// different sets of arguments (precompilation of format strings).
template <typename Char>
struct dynamic_format_specs : basic_format_specs<Char> {
  arg_ref<Char> width_ref;
  arg_ref<Char> precision_ref;
};

// Format spec handler that saves references to arguments representing dynamic
// width and precision to be resolved at formatting time.
template <typename ParseContext>
class dynamic_specs_handler
    : public specs_setter<typename ParseContext::char_type> {
 public:
  using char_type = typename ParseContext::char_type;

  FMT_CONSTEXPR dynamic_specs_handler(dynamic_format_specs<char_type>& specs,
                                      ParseContext& ctx)
      : specs_setter<char_type>(specs), specs_(specs), context_(ctx) {}

  FMT_CONSTEXPR dynamic_specs_handler(const dynamic_specs_handler& other)
      : specs_setter<char_type>(other),
        specs_(other.specs_),
        context_(other.context_) {}

  template <typename Id> FMT_CONSTEXPR void on_dynamic_width(Id arg_id) {
    specs_.width_ref = make_arg_ref(arg_id);
  }

  template <typename Id> FMT_CONSTEXPR void on_dynamic_precision(Id arg_id) {
    specs_.precision_ref = make_arg_ref(arg_id);
  }

  FMT_CONSTEXPR void on_error(const char* message) {
    context_.on_error(message);
  }

 private:
  using arg_ref_type = arg_ref<char_type>;

  FMT_CONSTEXPR arg_ref_type make_arg_ref(int arg_id) {
    context_.check_arg_id(arg_id);
    return arg_ref_type(arg_id);
  }

  FMT_CONSTEXPR arg_ref_type make_arg_ref(auto_id) {
    return arg_ref_type(context_.next_arg_id());
  }

  FMT_CONSTEXPR arg_ref_type make_arg_ref(basic_string_view<char_type> arg_id) {
    context_.check_arg_id(arg_id);
    basic_string_view<char_type> format_str(
        context_.begin(), to_unsigned(context_.end() - context_.begin()));
    return arg_ref_type(arg_id);
  }

  dynamic_format_specs<char_type>& specs_;
  ParseContext& context_;
};

template <typename Char, typename IDHandler>
FMT_CONSTEXPR const Char* parse_arg_id(const Char* begin, const Char* end,
                                       IDHandler&& handler) {
  FMT_ASSERT(begin != end, "");
  Char c = *begin;
  if (c == '}' || c == ':') {
    handler();
    return begin;
  }
  if (c >= '0' && c <= '9') {
    int index = 0;
    if (c != '0')
      index = parse_nonnegative_int(begin, end, handler);
    else
      ++begin;
    if (begin == end || (*begin != '}' && *begin != ':'))
      handler.on_error("invalid format string");
    else
      handler(index);
    return begin;
  }
  if (!is_name_start(c)) {
    handler.on_error("invalid format string");
    return begin;
  }
  auto it = begin;
  do {
    ++it;
  } while (it != end && (is_name_start(c = *it) || ('0' <= c && c <= '9')));
  handler(basic_string_view<Char>(begin, to_unsigned(it - begin)));
  return it;
}

// Adapts SpecHandler to IDHandler API for dynamic width.
template <typename SpecHandler, typename Char> struct width_adapter {
  explicit FMT_CONSTEXPR width_adapter(SpecHandler& h) : handler(h) {}

  FMT_CONSTEXPR void operator()() { handler.on_dynamic_width(auto_id()); }
  FMT_CONSTEXPR void operator()(int id) { handler.on_dynamic_width(id); }
  FMT_CONSTEXPR void operator()(basic_string_view<Char> id) {
    handler.on_dynamic_width(id);
  }

  FMT_CONSTEXPR void on_error(const char* message) {
    handler.on_error(message);
  }

  SpecHandler& handler;
};

// Adapts SpecHandler to IDHandler API for dynamic precision.
template <typename SpecHandler, typename Char> struct precision_adapter {
  explicit FMT_CONSTEXPR precision_adapter(SpecHandler& h) : handler(h) {}

  FMT_CONSTEXPR void operator()() { handler.on_dynamic_precision(auto_id()); }
  FMT_CONSTEXPR void operator()(int id) { handler.on_dynamic_precision(id); }
  FMT_CONSTEXPR void operator()(basic_string_view<Char> id) {
    handler.on_dynamic_precision(id);
  }

  FMT_CONSTEXPR void on_error(const char* message) {
    handler.on_error(message);
  }

  SpecHandler& handler;
};

template <typename Char>
FMT_CONSTEXPR int code_point_length(const Char* begin) {
  if (const_check(sizeof(Char) != 1)) return 1;
  constexpr char lengths[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                              0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0};
  int len = lengths[static_cast<unsigned char>(*begin) >> 3];

  // Compute the pointer to the next character early so that the next
  // iteration can start working on the next character. Neither Clang
  // nor GCC figure out this reordering on their own.
  return len + !len;
}

template <typename Char> constexpr bool is_ascii_letter(Char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Converts a character to ASCII. Returns a number > 127 on conversion failure.
template <typename Char, FMT_ENABLE_IF(std::is_integral<Char>::value)>
constexpr Char to_ascii(Char value) {
  return value;
}
template <typename Char, FMT_ENABLE_IF(std::is_enum<Char>::value)>
constexpr typename std::underlying_type<Char>::type to_ascii(Char value) {
  return value;
}

// Parses fill and alignment.
template <typename Char, typename Handler>
FMT_CONSTEXPR const Char* parse_align(const Char* begin, const Char* end,
                                      Handler&& handler) {
  FMT_ASSERT(begin != end, "");
  auto align = align::none;
  auto p = begin + code_point_length(begin);
  if (p >= end) p = begin;
  for (;;) {
    switch (to_ascii(*p)) {
    case '<':
      align = align::left;
      break;
    case '>':
      align = align::right;
      break;
#if FMT_DEPRECATED_NUMERIC_ALIGN
    case '=':
      align = align::numeric;
      break;
#endif
    case '^':
      align = align::center;
      break;
    }
    if (align != align::none) {
      if (p != begin) {
        auto c = *begin;
        if (c == '{')
          return handler.on_error("invalid fill character '{'"), begin;
        handler.on_fill(basic_string_view<Char>(begin, to_unsigned(p - begin)));
        begin = p + 1;
      } else
        ++begin;
      handler.on_align(align);
      break;
    } else if (p == begin) {
      break;
    }
    p = begin;
  }
  return begin;
}

template <typename Char, typename Handler>
FMT_CONSTEXPR const Char* parse_width(const Char* begin, const Char* end,
                                      Handler&& handler) {
  FMT_ASSERT(begin != end, "");
  if ('0' <= *begin && *begin <= '9') {
    handler.on_width(parse_nonnegative_int(begin, end, handler));
  } else if (*begin == '{') {
    ++begin;
    if (begin != end)
      begin = parse_arg_id(begin, end, width_adapter<Handler, Char>(handler));
    if (begin == end || *begin != '}')
      return handler.on_error("invalid format string"), begin;
    ++begin;
  }
  return begin;
}

template <typename Char, typename Handler>
FMT_CONSTEXPR const Char* parse_precision(const Char* begin, const Char* end,
                                          Handler&& handler) {
  ++begin;
  auto c = begin != end ? *begin : Char();
  if ('0' <= c && c <= '9') {
    handler.on_precision(parse_nonnegative_int(begin, end, handler));
  } else if (c == '{') {
    ++begin;
    if (begin != end) {
      begin =
          parse_arg_id(begin, end, precision_adapter<Handler, Char>(handler));
    }
    if (begin == end || *begin++ != '}')
      return handler.on_error("invalid format string"), begin;
  } else {
    return handler.on_error("missing precision specifier"), begin;
  }
  handler.end_precision();
  return begin;
}

// Parses standard format specifiers and sends notifications about parsed
// components to handler.
template <typename Char, typename SpecHandler>
FMT_CONSTEXPR const Char* parse_format_specs(const Char* begin, const Char* end,
                                             SpecHandler&& handler) {
  if (begin == end) return begin;

  begin = parse_align(begin, end, handler);
  if (begin == end) return begin;

  // Parse sign.
  switch (to_ascii(*begin)) {
  case '+':
    handler.on_plus();
    ++begin;
    break;
  case '-':
    handler.on_minus();
    ++begin;
    break;
  case ' ':
    handler.on_space();
    ++begin;
    break;
  }
  if (begin == end) return begin;

  if (*begin == '#') {
    handler.on_hash();
    if (++begin == end) return begin;
  }

  // Parse zero flag.
  if (*begin == '0') {
    handler.on_zero();
    if (++begin == end) return begin;
  }

  begin = parse_width(begin, end, handler);
  if (begin == end) return begin;

  // Parse precision.
  if (*begin == '.') {
    begin = parse_precision(begin, end, handler);
  }

  // Parse type.
  if (begin != end && *begin != '}') handler.on_type(*begin++);
  return begin;
}

// Return the result via the out param to workaround gcc bug 77539.
template <bool IS_CONSTEXPR, typename T, typename Ptr = const T*>
FMT_CONSTEXPR bool find(Ptr first, Ptr last, T value, Ptr& out) {
  for (out = first; out != last; ++out) {
    if (*out == value) return true;
  }
  return false;
}

template <>
inline bool find<false, char>(const char* first, const char* last, char value,
                              const char*& out) {
  out = static_cast<const char*>(
      std::memchr(first, value, detail::to_unsigned(last - first)));
  return out != nullptr;
}

template <typename Handler, typename Char> struct id_adapter {
  Handler& handler;
  int arg_id;

  FMT_CONSTEXPR void operator()() { arg_id = handler.on_arg_id(); }
  FMT_CONSTEXPR void operator()(int id) { arg_id = handler.on_arg_id(id); }
  FMT_CONSTEXPR void operator()(basic_string_view<Char> id) {
    arg_id = handler.on_arg_id(id);
  }
  FMT_CONSTEXPR void on_error(const char* message) {
    handler.on_error(message);
  }
};

template <typename Char, typename Handler>
FMT_CONSTEXPR const Char* parse_replacement_field(const Char* begin,
                                                  const Char* end,
                                                  Handler&& handler) {
  ++begin;
  if (begin == end) return handler.on_error("invalid format string"), end;
  if (*begin == '}') {
    handler.on_replacement_field(handler.on_arg_id(), begin);
  } else if (*begin == '{') {
    handler.on_text(begin, begin + 1);
  } else {
    auto adapter = id_adapter<Handler, Char>{handler, 0};
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
FMT_CONSTEXPR_DECL FMT_INLINE void parse_format_string(
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
    FMT_CONSTEXPR void operator()(const Char* pbegin, const Char* pend) {
      if (pbegin == pend) return;
      for (;;) {
        const Char* p = nullptr;
        if (!find<IS_CONSTEXPR>(pbegin, pend, '}', p))
          return handler_.on_text(pbegin, pend);
        ++p;
        if (p == pend || *p != '}')
          return handler_.on_error("unmatched '}' in format string");
        handler_.on_text(pbegin, p);
        pbegin = p + 1;
      }
    }
    Handler& handler_;
  } write{handler};
  while (begin != end) {
    // Doing two passes with memchr (one for '{' and another for '}') is up to
    // 2.5x faster than the naive one-pass implementation on big format strings.
    const Char* p = begin;
    if (*begin != '{' && !find<IS_CONSTEXPR>(begin + 1, end, '{', p))
      return write(begin, end);
    write(begin, p);
    begin = parse_replacement_field(p, end, handler);
  }
}

template <typename T, typename ParseContext>
FMT_CONSTEXPR const typename ParseContext::char_type* parse_format_specs(
    ParseContext& ctx) {
  using char_type = typename ParseContext::char_type;
  using context = buffer_context<char_type>;
  using mapped_type =
      conditional_t<detail::mapped_type_constant<T, context>::value !=
                        type::custom_type,
                    decltype(arg_mapper<context>().map(std::declval<T>())), T>;
  auto f = conditional_t<has_formatter<mapped_type, context>::value,
                         formatter<mapped_type, char_type>,
                         detail::fallback_formatter<T, char_type>>();
  return f.parse(ctx);
}

template <typename OutputIt, typename Char, typename Context>
struct format_handler : detail::error_handler {
  basic_format_parse_context<Char> parse_context;
  Context context;

  format_handler(OutputIt out, basic_string_view<Char> str,
                 basic_format_args<Context> format_args, detail::locale_ref loc)
      : parse_context(str), context(out, format_args, loc) {}

  void on_text(const Char* begin, const Char* end) {
    auto size = to_unsigned(end - begin);
    auto out = context.out();
    auto&& it = reserve(out, size);
    it = std::copy_n(begin, size, it);
    context.advance_to(out);
  }

  int on_arg_id() { return parse_context.next_arg_id(); }
  int on_arg_id(int id) { return parse_context.check_arg_id(id), id; }
  int on_arg_id(basic_string_view<Char> id) {
    int arg_id = context.arg_id(id);
    if (arg_id < 0) on_error("argument not found");
    return arg_id;
  }

  FMT_INLINE void on_replacement_field(int id, const Char*) {
    auto arg = get_arg(context, id);
    context.advance_to(visit_format_arg(
        default_arg_formatter<OutputIt, Char>{context.out(), context.args(),
                                              context.locale()},
        arg));
  }

  const Char* on_format_specs(int id, const Char* begin, const Char* end) {
    auto arg = get_arg(context, id);
    if (arg.type() == type::custom_type) {
      advance_to(parse_context, begin);
      visit_format_arg(custom_formatter<Context>(parse_context, context), arg);
      return parse_context.begin();
    }
    auto specs = basic_format_specs<Char>();
    if (begin + 1 < end && begin[1] == '}' && is_ascii_letter(*begin)) {
      specs.type = static_cast<char>(*begin++);
    } else {
      using parse_context_t = basic_format_parse_context<Char>;
      specs_checker<specs_handler<parse_context_t, Context>> handler(
          specs_handler<parse_context_t, Context>(specs, parse_context,
                                                  context),
          arg.type());
      begin = parse_format_specs(begin, end, handler);
      if (begin == end || *begin != '}')
        on_error("missing '}' in format string");
    }
    context.advance_to(visit_format_arg(
        arg_formatter<OutputIt, Char>(context, &parse_context, &specs), arg));
    return begin;
  }
};

// A parse context with extra argument id checks. It is only used at compile
// time because adding checks at runtime would introduce substantial overhead
// and would be redundant since argument ids are checked when arguments are
// retrieved anyway.
template <typename Char, typename ErrorHandler = error_handler>
class compile_parse_context
    : public basic_format_parse_context<Char, ErrorHandler> {
 private:
  int num_args_;
  using base = basic_format_parse_context<Char, ErrorHandler>;

 public:
  explicit FMT_CONSTEXPR compile_parse_context(
      basic_string_view<Char> format_str, int num_args = max_value<int>(),
      ErrorHandler eh = {})
      : base(format_str, eh), num_args_(num_args) {}

  FMT_CONSTEXPR int next_arg_id() {
    int id = base::next_arg_id();
    if (id >= num_args_) this->on_error("argument not found");
    return id;
  }

  FMT_CONSTEXPR void check_arg_id(int id) {
    base::check_arg_id(id);
    if (id >= num_args_) this->on_error("argument not found");
  }
  using base::check_arg_id;
};

template <typename Char, typename ErrorHandler, typename... Args>
class format_string_checker {
 public:
  explicit FMT_CONSTEXPR format_string_checker(
      basic_string_view<Char> format_str, ErrorHandler eh)
      : context_(format_str, num_args, eh),
        parse_funcs_{&parse_format_specs<Args, parse_context_type>...} {}

  FMT_CONSTEXPR void on_text(const Char*, const Char*) {}

  FMT_CONSTEXPR int on_arg_id() { return context_.next_arg_id(); }
  FMT_CONSTEXPR int on_arg_id(int id) { return context_.check_arg_id(id), id; }
  FMT_CONSTEXPR int on_arg_id(basic_string_view<Char>) {
    on_error("compile-time checks don't support named arguments");
    return 0;
  }

  FMT_CONSTEXPR void on_replacement_field(int, const Char*) {}

  FMT_CONSTEXPR const Char* on_format_specs(int id, const Char* begin,
                                            const Char*) {
    advance_to(context_, begin);
    return id < num_args ? parse_funcs_[id](context_) : begin;
  }

  FMT_CONSTEXPR void on_error(const char* message) {
    context_.on_error(message);
  }

 private:
  using parse_context_type = compile_parse_context<Char, ErrorHandler>;
  enum { num_args = sizeof...(Args) };

  // Format specifier parsing function.
  using parse_func = const Char* (*)(parse_context_type&);

  parse_context_type context_;
  parse_func parse_funcs_[num_args > 0 ? num_args : 1];
};

// Converts string literals to basic_string_view.
template <typename Char, size_t N>
FMT_CONSTEXPR basic_string_view<Char> compile_string_to_view(
    const Char (&s)[N]) {
  // Remove trailing null character if needed. Won't be present if this is used
  // with raw character array (i.e. not defined as a string).
  return {s,
          N - ((std::char_traits<Char>::to_int_type(s[N - 1]) == 0) ? 1 : 0)};
}

// Converts string_view to basic_string_view.
template <typename Char>
FMT_CONSTEXPR basic_string_view<Char> compile_string_to_view(
    const std_string_view<Char>& s) {
  return {s.data(), s.size()};
}

#define FMT_STRING_IMPL(s, base)                                  \
  [] {                                                            \
    /* Use a macro-like name to avoid shadowing warnings. */      \
    struct FMT_COMPILE_STRING : base {                            \
      using char_type = fmt::remove_cvref_t<decltype(s[0])>;      \
      FMT_MAYBE_UNUSED FMT_CONSTEXPR                              \
      operator fmt::basic_string_view<char_type>() const {        \
        return fmt::detail::compile_string_to_view<char_type>(s); \
      }                                                           \
    };                                                            \
    return FMT_COMPILE_STRING();                                  \
  }()

/**
  \rst
  Constructs a compile-time format string from a string literal *s*.

  **Example**::

    // A compile-time error because 'd' is an invalid specifier for strings.
    std::string s = fmt::format(FMT_STRING("{:d}"), "foo");
  \endrst
 */
#define FMT_STRING(s) FMT_STRING_IMPL(s, fmt::compile_string)

template <typename... Args, typename S,
          enable_if_t<(is_compile_string<S>::value), int>>
void check_format_string(S format_str) {
  FMT_CONSTEXPR_DECL auto s = to_string_view(format_str);
  using checker = format_string_checker<typename S::char_type, error_handler,
                                        remove_cvref_t<Args>...>;
  FMT_CONSTEXPR_DECL bool invalid_format =
      (parse_format_string<true>(s, checker(s, {})), true);
  (void)invalid_format;
}

template <template <typename> class Handler, typename Context>
void handle_dynamic_spec(int& value, arg_ref<typename Context::char_type> ref,
                         Context& ctx) {
  switch (ref.kind) {
  case arg_id_kind::none:
    break;
  case arg_id_kind::index:
    value = detail::get_dynamic_spec<Handler>(ctx.arg(ref.val.index),
                                              ctx.error_handler());
    break;
  case arg_id_kind::name:
    value = detail::get_dynamic_spec<Handler>(ctx.arg(ref.val.name),
                                              ctx.error_handler());
    break;
  }
}

using format_func = void (*)(detail::buffer<char>&, int, string_view);

FMT_API void format_error_code(buffer<char>& out, int error_code,
                               string_view message) FMT_NOEXCEPT;

FMT_API void report_error(format_func func, int error_code,
                          string_view message) FMT_NOEXCEPT;
}  // namespace detail

template <typename OutputIt, typename Char>
using arg_formatter FMT_DEPRECATED_ALIAS =
    detail::arg_formatter<OutputIt, Char>;

/**
 An error returned by an operating system or a language runtime,
 for example a file opening error.
*/
FMT_CLASS_API
class FMT_API system_error : public std::runtime_error {
 private:
  void init(int err_code, string_view format_str, format_args args);

 protected:
  int error_code_;

  system_error() : std::runtime_error(""), error_code_(0) {}

 public:
  /**
   \rst
   Constructs a :class:`fmt::system_error` object with a description
   formatted with `fmt::format_system_error`. *message* and additional
   arguments passed into the constructor are formatted similarly to
   `fmt::format`.

   **Example**::

     // This throws a system_error with the description
     //   cannot open file 'madeup': No such file or directory
     // or similar (system message may vary).
     const char *filename = "madeup";
     std::FILE *file = std::fopen(filename, "r");
     if (!file)
       throw fmt::system_error(errno, "cannot open file '{}'", filename);
   \endrst
  */
  template <typename... Args>
  system_error(int error_code, string_view message, const Args&... args)
      : std::runtime_error("") {
    init(error_code, message, make_format_args(args...));
  }
  system_error(const system_error&) = default;
  system_error& operator=(const system_error&) = default;
  system_error(system_error&&) = default;
  system_error& operator=(system_error&&) = default;
  ~system_error() FMT_NOEXCEPT FMT_OVERRIDE;

  int error_code() const { return error_code_; }
};

/**
  \rst
  Formats an error returned by an operating system or a language runtime,
  for example a file opening error, and writes it to *out* in the following
  form:

  .. parsed-literal::
     *<message>*: *<system-message>*

  where *<message>* is the passed message and *<system-message>* is
  the system message corresponding to the error code.
  *error_code* is a system error code as given by ``errno``.
  If *error_code* is not a valid error code such as -1, the system message
  may look like "Unknown error -1" and is platform-dependent.
  \endrst
 */
FMT_API void format_system_error(detail::buffer<char>& out, int error_code,
                                 string_view message) FMT_NOEXCEPT;

// Reports a system error without throwing an exception.
// Can be used to report errors from destructors.
FMT_API void report_system_error(int error_code,
                                 string_view message) FMT_NOEXCEPT;

/** Fast integer formatter. */
class format_int {
 private:
  // Buffer should be large enough to hold all digits (digits10 + 1),
  // a sign and a null character.
  enum { buffer_size = std::numeric_limits<unsigned long long>::digits10 + 3 };
  mutable char buffer_[buffer_size];
  char* str_;

  template <typename UInt> char* format_unsigned(UInt value) {
    auto n = static_cast<detail::uint32_or_64_or_128_t<UInt>>(value);
    return detail::format_decimal(buffer_, n, buffer_size - 1).begin;
  }

  template <typename Int> char* format_signed(Int value) {
    auto abs_value = static_cast<detail::uint32_or_64_or_128_t<Int>>(value);
    bool negative = value < 0;
    if (negative) abs_value = 0 - abs_value;
    auto begin = format_unsigned(abs_value);
    if (negative) *--begin = '-';
    return begin;
  }

 public:
  explicit format_int(int value) : str_(format_signed(value)) {}
  explicit format_int(long value) : str_(format_signed(value)) {}
  explicit format_int(long long value) : str_(format_signed(value)) {}
  explicit format_int(unsigned value) : str_(format_unsigned(value)) {}
  explicit format_int(unsigned long value) : str_(format_unsigned(value)) {}
  explicit format_int(unsigned long long value)
      : str_(format_unsigned(value)) {}

  /** Returns the number of characters written to the output buffer. */
  size_t size() const {
    return detail::to_unsigned(buffer_ - str_ + buffer_size - 1);
  }

  /**
    Returns a pointer to the output buffer content. No terminating null
    character is appended.
   */
  const char* data() const { return str_; }

  /**
    Returns a pointer to the output buffer content with terminating null
    character appended.
   */
  const char* c_str() const {
    buffer_[buffer_size - 1] = '\0';
    return str_;
  }

  /**
    \rst
    Returns the content of the output buffer as an ``std::string``.
    \endrst
   */
  std::string str() const { return std::string(str_, size()); }
};

// A formatter specialization for the core types corresponding to detail::type
// constants.
template <typename T, typename Char>
struct formatter<T, Char,
                 enable_if_t<detail::type_constant<T, Char>::value !=
                             detail::type::custom_type>> {
  FMT_CONSTEXPR formatter() = default;

  // Parses format specifiers stopping either at the end of the range or at the
  // terminating '}'.
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    using handler_type = detail::dynamic_specs_handler<ParseContext>;
    auto type = detail::type_constant<T, Char>::value;
    detail::specs_checker<handler_type> handler(handler_type(specs_, ctx),
                                                type);
    auto it = parse_format_specs(ctx.begin(), ctx.end(), handler);
    auto eh = ctx.error_handler();
    switch (type) {
    case detail::type::none_type:
      FMT_ASSERT(false, "invalid argument type");
      break;
    case detail::type::int_type:
    case detail::type::uint_type:
    case detail::type::long_long_type:
    case detail::type::ulong_long_type:
    case detail::type::int128_type:
    case detail::type::uint128_type:
    case detail::type::bool_type:
      handle_int_type_spec(specs_.type,
                           detail::int_type_checker<decltype(eh)>(eh));
      break;
    case detail::type::char_type:
      handle_char_specs(
          &specs_, detail::char_specs_checker<decltype(eh)>(specs_.type, eh));
      break;
    case detail::type::float_type:
      if (detail::const_check(FMT_USE_FLOAT))
        detail::parse_float_type_spec(specs_, eh);
      else
        FMT_ASSERT(false, "float support disabled");
      break;
    case detail::type::double_type:
      if (detail::const_check(FMT_USE_DOUBLE))
        detail::parse_float_type_spec(specs_, eh);
      else
        FMT_ASSERT(false, "double support disabled");
      break;
    case detail::type::long_double_type:
      if (detail::const_check(FMT_USE_LONG_DOUBLE))
        detail::parse_float_type_spec(specs_, eh);
      else
        FMT_ASSERT(false, "long double support disabled");
      break;
    case detail::type::cstring_type:
      detail::handle_cstring_type_spec(
          specs_.type, detail::cstring_type_checker<decltype(eh)>(eh));
      break;
    case detail::type::string_type:
      detail::check_string_type_spec(specs_.type, eh);
      break;
    case detail::type::pointer_type:
      detail::check_pointer_type_spec(specs_.type, eh);
      break;
    case detail::type::custom_type:
      // Custom format specifiers should be checked in parse functions of
      // formatter specializations.
      break;
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const T& val, FormatContext& ctx) -> decltype(ctx.out()) {
    detail::handle_dynamic_spec<detail::width_checker>(specs_.width,
                                                       specs_.width_ref, ctx);
    detail::handle_dynamic_spec<detail::precision_checker>(
        specs_.precision, specs_.precision_ref, ctx);
    using af = detail::arg_formatter<typename FormatContext::iterator,
                                     typename FormatContext::char_type>;
    return visit_format_arg(af(ctx, nullptr, &specs_),
                            detail::make_arg<FormatContext>(val));
  }

 private:
  detail::dynamic_format_specs<Char> specs_;
};

#define FMT_FORMAT_AS(Type, Base)                                             \
  template <typename Char>                                                    \
  struct formatter<Type, Char> : formatter<Base, Char> {                      \
    template <typename FormatContext>                                         \
    auto format(Type const& val, FormatContext& ctx) -> decltype(ctx.out()) { \
      return formatter<Base, Char>::format(val, ctx);                         \
    }                                                                         \
  }

FMT_FORMAT_AS(signed char, int);
FMT_FORMAT_AS(unsigned char, unsigned);
FMT_FORMAT_AS(short, int);
FMT_FORMAT_AS(unsigned short, unsigned);
FMT_FORMAT_AS(long, long long);
FMT_FORMAT_AS(unsigned long, unsigned long long);
FMT_FORMAT_AS(Char*, const Char*);
FMT_FORMAT_AS(std::basic_string<Char>, basic_string_view<Char>);
FMT_FORMAT_AS(std::nullptr_t, const void*);
FMT_FORMAT_AS(detail::std_string_view<Char>, basic_string_view<Char>);

template <typename Char>
struct formatter<void*, Char> : formatter<const void*, Char> {
  template <typename FormatContext>
  auto format(void* val, FormatContext& ctx) -> decltype(ctx.out()) {
    return formatter<const void*, Char>::format(val, ctx);
  }
};

template <typename Char, size_t N>
struct formatter<Char[N], Char> : formatter<basic_string_view<Char>, Char> {
  template <typename FormatContext>
  auto format(const Char* val, FormatContext& ctx) -> decltype(ctx.out()) {
    return formatter<basic_string_view<Char>, Char>::format(val, ctx);
  }
};

// A formatter for types known only at run time such as variant alternatives.
//
// Usage:
//   using variant = std::variant<int, std::string>;
//   template <>
//   struct formatter<variant>: dynamic_formatter<> {
//     auto format(const variant& v, format_context& ctx) {
//       return visit([&](const auto& val) {
//           return dynamic_formatter<>::format(val, ctx);
//       }, v);
//     }
//   };
template <typename Char = char> class dynamic_formatter {
 private:
  struct null_handler : detail::error_handler {
    void on_align(align_t) {}
    void on_plus() {}
    void on_minus() {}
    void on_space() {}
    void on_hash() {}
  };

 public:
  template <typename ParseContext>
  auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    format_str_ = ctx.begin();
    // Checks are deferred to formatting time when the argument type is known.
    detail::dynamic_specs_handler<ParseContext> handler(specs_, ctx);
    return parse_format_specs(ctx.begin(), ctx.end(), handler);
  }

  template <typename T, typename FormatContext>
  auto format(const T& val, FormatContext& ctx) -> decltype(ctx.out()) {
    handle_specs(ctx);
    detail::specs_checker<null_handler> checker(
        null_handler(), detail::mapped_type_constant<T, FormatContext>::value);
    checker.on_align(specs_.align);
    switch (specs_.sign) {
    case sign::none:
      break;
    case sign::plus:
      checker.on_plus();
      break;
    case sign::minus:
      checker.on_minus();
      break;
    case sign::space:
      checker.on_space();
      break;
    }
    if (specs_.alt) checker.on_hash();
    if (specs_.precision >= 0) checker.end_precision();
    using af = detail::arg_formatter<typename FormatContext::iterator,
                                     typename FormatContext::char_type>;
    visit_format_arg(af(ctx, nullptr, &specs_),
                     detail::make_arg<FormatContext>(val));
    return ctx.out();
  }

 private:
  template <typename Context> void handle_specs(Context& ctx) {
    detail::handle_dynamic_spec<detail::width_checker>(specs_.width,
                                                       specs_.width_ref, ctx);
    detail::handle_dynamic_spec<detail::precision_checker>(
        specs_.precision, specs_.precision_ref, ctx);
  }

  detail::dynamic_format_specs<Char> specs_;
  const Char* format_str_;
};

template <typename Char, typename ErrorHandler>
FMT_CONSTEXPR void advance_to(
    basic_format_parse_context<Char, ErrorHandler>& ctx, const Char* p) {
  ctx.advance_to(ctx.begin() + (p - &*ctx.begin()));
}

/**
  \rst
  Converts ``p`` to ``const void*`` for pointer formatting.

  **Example**::

    auto s = fmt::format("{}", fmt::ptr(p));
  \endrst
 */
template <typename T> inline const void* ptr(const T* p) { return p; }
template <typename T> inline const void* ptr(const std::unique_ptr<T>& p) {
  return p.get();
}
template <typename T> inline const void* ptr(const std::shared_ptr<T>& p) {
  return p.get();
}

class bytes {
 private:
  string_view data_;
  friend struct formatter<bytes>;

 public:
  explicit bytes(string_view data) : data_(data) {}
};

template <> struct formatter<bytes> {
 private:
  detail::dynamic_format_specs<char> specs_;

 public:
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    using handler_type = detail::dynamic_specs_handler<ParseContext>;
    detail::specs_checker<handler_type> handler(handler_type(specs_, ctx),
                                                detail::type::string_type);
    auto it = parse_format_specs(ctx.begin(), ctx.end(), handler);
    detail::check_string_type_spec(specs_.type, ctx.error_handler());
    return it;
  }

  template <typename FormatContext>
  auto format(bytes b, FormatContext& ctx) -> decltype(ctx.out()) {
    detail::handle_dynamic_spec<detail::width_checker>(specs_.width,
                                                       specs_.width_ref, ctx);
    detail::handle_dynamic_spec<detail::precision_checker>(
        specs_.precision, specs_.precision_ref, ctx);
    return detail::write_bytes(ctx.out(), b.data_, specs_);
  }
};

template <typename It, typename Sentinel, typename Char>
struct arg_join : detail::view {
  It begin;
  Sentinel end;
  basic_string_view<Char> sep;

  arg_join(It b, Sentinel e, basic_string_view<Char> s)
      : begin(b), end(e), sep(s) {}
};

template <typename It, typename Sentinel, typename Char>
struct formatter<arg_join<It, Sentinel, Char>, Char>
    : formatter<typename std::iterator_traits<It>::value_type, Char> {
  template <typename FormatContext>
  auto format(const arg_join<It, Sentinel, Char>& value, FormatContext& ctx)
      -> decltype(ctx.out()) {
    using base = formatter<typename std::iterator_traits<It>::value_type, Char>;
    auto it = value.begin;
    auto out = ctx.out();
    if (it != value.end) {
      out = base::format(*it++, ctx);
      while (it != value.end) {
        out = std::copy(value.sep.begin(), value.sep.end(), out);
        ctx.advance_to(out);
        out = base::format(*it++, ctx);
      }
    }
    return out;
  }
};

/**
  Returns an object that formats the iterator range `[begin, end)` with elements
  separated by `sep`.
 */
template <typename It, typename Sentinel>
arg_join<It, Sentinel, char> join(It begin, Sentinel end, string_view sep) {
  return {begin, end, sep};
}

template <typename It, typename Sentinel>
arg_join<It, Sentinel, wchar_t> join(It begin, Sentinel end, wstring_view sep) {
  return {begin, end, sep};
}

/**
  \rst
  Returns an object that formats `range` with elements separated by `sep`.

  **Example**::

    std::vector<int> v = {1, 2, 3};
    fmt::print("{}", fmt::join(v, ", "));
    // Output: "1, 2, 3"

  ``fmt::join`` applies passed format specifiers to the range elements::

    fmt::print("{:02}", fmt::join(v, ", "));
    // Output: "01, 02, 03"
  \endrst
 */
template <typename Range>
arg_join<detail::iterator_t<Range>, detail::sentinel_t<Range>, char> join(
    Range&& range, string_view sep) {
  return join(std::begin(range), std::end(range), sep);
}

template <typename Range>
arg_join<detail::iterator_t<Range>, detail::sentinel_t<Range>, wchar_t> join(
    Range&& range, wstring_view sep) {
  return join(std::begin(range), std::end(range), sep);
}

/**
  \rst
  Converts *value* to ``std::string`` using the default format for type *T*.

  **Example**::

    #include <fmt/format.h>

    std::string answer = fmt::to_string(42);
  \endrst
 */
template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
inline std::string to_string(const T& value) {
  std::string result;
  detail::write<char>(std::back_inserter(result), value);
  return result;
}

template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
inline std::string to_string(T value) {
  // The buffer should be large enough to store the number including the sign or
  // "false" for bool.
  constexpr int max_size = detail::digits10<T>() + 2;
  char buffer[max_size > 5 ? static_cast<unsigned>(max_size) : 5];
  char* begin = buffer;
  return std::string(begin, detail::write<char>(begin, value));
}

/**
  Converts *value* to ``std::wstring`` using the default format for type *T*.
 */
template <typename T> inline std::wstring to_wstring(const T& value) {
  return format(L"{}", value);
}

template <typename Char, size_t SIZE>
std::basic_string<Char> to_string(const basic_memory_buffer<Char, SIZE>& buf) {
  auto size = buf.size();
  detail::assume(size < std::basic_string<Char>().max_size());
  return std::basic_string<Char>(buf.data(), size);
}

template <typename Char>
void detail::vformat_to(
    detail::buffer<Char>& buf, basic_string_view<Char> format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args,
    detail::locale_ref loc) {
  using iterator = typename buffer_context<Char>::iterator;
  auto out = buffer_appender<Char>(buf);
  if (format_str.size() == 2 && equal2(format_str.data(), "{}")) {
    auto arg = args.get(0);
    if (!arg) error_handler().on_error("argument not found");
    visit_format_arg(default_arg_formatter<iterator, Char>{out, args, loc},
                     arg);
    return;
  }
  format_handler<iterator, Char, buffer_context<Char>> h(out, format_str, args,
                                                         loc);
  parse_format_string<false>(format_str, h);
}

#ifndef FMT_HEADER_ONLY
extern template void detail::vformat_to(detail::buffer<char>&, string_view,
                                        basic_format_args<format_context>,
                                        detail::locale_ref);
namespace detail {

extern template FMT_API std::string grouping_impl<char>(locale_ref loc);
extern template FMT_API std::string grouping_impl<wchar_t>(locale_ref loc);
extern template FMT_API char thousands_sep_impl<char>(locale_ref loc);
extern template FMT_API wchar_t thousands_sep_impl<wchar_t>(locale_ref loc);
extern template FMT_API char decimal_point_impl(locale_ref loc);
extern template FMT_API wchar_t decimal_point_impl(locale_ref loc);
extern template int format_float<double>(double value, int precision,
                                         float_specs specs, buffer<char>& buf);
extern template int format_float<long double>(long double value, int precision,
                                              float_specs specs,
                                              buffer<char>& buf);
int snprintf_float(float value, int precision, float_specs specs,
                   buffer<char>& buf) = delete;
extern template int snprintf_float<double>(double value, int precision,
                                           float_specs specs,
                                           buffer<char>& buf);
extern template int snprintf_float<long double>(long double value,
                                                int precision,
                                                float_specs specs,
                                                buffer<char>& buf);
}  // namespace detail
#endif

template <typename S, typename Char = char_t<S>,
          FMT_ENABLE_IF(detail::is_string<S>::value)>
inline void vformat_to(
    detail::buffer<Char>& buf, const S& format_str,
    basic_format_args<FMT_BUFFER_CONTEXT(type_identity_t<Char>)> args) {
  return detail::vformat_to(buf, to_string_view(format_str), args);
}

template <typename S, typename... Args, size_t SIZE = inline_buffer_size,
          typename Char = enable_if_t<detail::is_string<S>::value, char_t<S>>>
inline typename buffer_context<Char>::iterator format_to(
    basic_memory_buffer<Char, SIZE>& buf, const S& format_str, Args&&... args) {
  const auto& vargs = fmt::make_args_checked<Args...>(format_str, args...);
  detail::vformat_to(buf, to_string_view(format_str), vargs);
  return detail::buffer_appender<Char>(buf);
}

template <typename OutputIt, typename Char = char>
using format_context_t = basic_format_context<OutputIt, Char>;

template <typename OutputIt, typename Char = char>
using format_args_t = basic_format_args<format_context_t<OutputIt, Char>>;

template <typename OutputIt, typename Char = typename OutputIt::value_type>
using format_to_n_context FMT_DEPRECATED_ALIAS = buffer_context<Char>;

template <typename OutputIt, typename Char = typename OutputIt::value_type>
using format_to_n_args FMT_DEPRECATED_ALIAS =
    basic_format_args<buffer_context<Char>>;

template <typename OutputIt, typename Char, typename... Args>
FMT_DEPRECATED format_arg_store<buffer_context<Char>, Args...>
make_format_to_n_args(const Args&... args) {
  return format_arg_store<buffer_context<Char>, Args...>(args...);
}

template <typename Char, enable_if_t<(!std::is_same<Char, char>::value), int>>
std::basic_string<Char> detail::vformat(
    basic_string_view<Char> format_str,
    basic_format_args<buffer_context<type_identity_t<Char>>> args) {
  basic_memory_buffer<Char> buffer;
  detail::vformat_to(buffer, format_str, args);
  return to_string(buffer);
}

template <typename Char, FMT_ENABLE_IF(std::is_same<Char, wchar_t>::value)>
void vprint(std::FILE* f, basic_string_view<Char> format_str,
            wformat_args args) {
  wmemory_buffer buffer;
  detail::vformat_to(buffer, format_str, args);
  buffer.push_back(L'\0');
  if (std::fputws(buffer.data(), f) == -1)
    FMT_THROW(system_error(errno, "cannot write to file"));
}

template <typename Char, FMT_ENABLE_IF(std::is_same<Char, wchar_t>::value)>
void vprint(basic_string_view<Char> format_str, wformat_args args) {
  vprint(stdout, format_str, args);
}

#if FMT_USE_USER_DEFINED_LITERALS
namespace detail {

#  if FMT_USE_UDL_TEMPLATE
template <typename Char, Char... CHARS> class udl_formatter {
 public:
  template <typename... Args>
  std::basic_string<Char> operator()(Args&&... args) const {
    static FMT_CONSTEXPR_DECL Char s[] = {CHARS..., '\0'};
    return format(FMT_STRING(s), std::forward<Args>(args)...);
  }
};
#  else
template <typename Char> struct udl_formatter {
  basic_string_view<Char> str;

  template <typename... Args>
  std::basic_string<Char> operator()(Args&&... args) const {
    return format(str, std::forward<Args>(args)...);
  }
};
#  endif  // FMT_USE_UDL_TEMPLATE

template <typename Char> struct udl_arg {
  const Char* str;

  template <typename T> named_arg<Char, T> operator=(T&& value) const {
    return {str, std::forward<T>(value)};
  }
};
}  // namespace detail

inline namespace literals {
#  if FMT_USE_UDL_TEMPLATE
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpedantic"
#    if FMT_CLANG_VERSION
#      pragma GCC diagnostic ignored "-Wgnu-string-literal-operator-template"
#    endif
template <typename Char, Char... CHARS>
FMT_CONSTEXPR detail::udl_formatter<Char, CHARS...> operator""_format() {
  return {};
}
#    pragma GCC diagnostic pop
#  else
/**
  \rst
  User-defined literal equivalent of :func:`fmt::format`.

  **Example**::

    using namespace fmt::literals;
    std::string message = "The answer is {}"_format(42);
  \endrst
 */
FMT_CONSTEXPR detail::udl_formatter<char> operator"" _format(const char* s,
                                                             size_t n) {
  return {{s, n}};
}
FMT_CONSTEXPR detail::udl_formatter<wchar_t> operator"" _format(
    const wchar_t* s, size_t n) {
  return {{s, n}};
}
#  endif  // FMT_USE_UDL_TEMPLATE

/**
  \rst
  User-defined literal equivalent of :func:`fmt::arg`.

  **Example**::

    using namespace fmt::literals;
    fmt::print("Elapsed time: {s:.2f} seconds", "s"_a=1.23);
  \endrst
 */
FMT_CONSTEXPR detail::udl_arg<char> operator"" _a(const char* s, size_t) {
  return {s};
}
FMT_CONSTEXPR detail::udl_arg<wchar_t> operator"" _a(const wchar_t* s, size_t) {
  return {s};
}
}  // namespace literals
#endif  // FMT_USE_USER_DEFINED_LITERALS
FMT_END_NAMESPACE

#ifdef FMT_HEADER_ONLY
#  define FMT_FUNC inline
#  include "format-inl.h"
#else
#  define FMT_FUNC
#endif

#endif  // FMT_FORMAT_H_
