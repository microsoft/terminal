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

#include <cmath>             // std::signbit
#include <cstdint>           // uint32_t
#include <cstring>           // std::memcpy
#include <initializer_list>  // std::initializer_list
#include <limits>            // std::numeric_limits
#include <memory>            // std::uninitialized_copy
#include <stdexcept>         // std::runtime_error
#include <system_error>      // std::system_error

#ifdef __cpp_lib_bit_cast
#  include <bit>  // std::bitcast
#endif

#include "core.h"

#if defined __cpp_inline_variables && __cpp_inline_variables >= 201606L
#  define FMT_INLINE_VARIABLE inline
#else
#  define FMT_INLINE_VARIABLE
#endif

#if FMT_HAS_CPP17_ATTRIBUTE(fallthrough)
#  define FMT_FALLTHROUGH [[fallthrough]]
#elif defined(__clang__)
#  define FMT_FALLTHROUGH [[clang::fallthrough]]
#elif FMT_GCC_VERSION >= 700 && \
    (!defined(__EDG_VERSION__) || __EDG_VERSION__ >= 520)
#  define FMT_FALLTHROUGH [[gnu::fallthrough]]
#else
#  define FMT_FALLTHROUGH
#endif

#ifndef FMT_DEPRECATED
#  if FMT_HAS_CPP14_ATTRIBUTE(deprecated) || FMT_MSC_VERSION >= 1900
#    define FMT_DEPRECATED [[deprecated]]
#  else
#    if (defined(__GNUC__) && !defined(__LCC__)) || defined(__clang__)
#      define FMT_DEPRECATED __attribute__((deprecated))
#    elif FMT_MSC_VERSION
#      define FMT_DEPRECATED __declspec(deprecated)
#    else
#      define FMT_DEPRECATED /* deprecated */
#    endif
#  endif
#endif

#ifndef FMT_NO_UNIQUE_ADDRESS
#  if FMT_CPLUSPLUS >= 202002L
#    if FMT_HAS_CPP_ATTRIBUTE(no_unique_address)
#      define FMT_NO_UNIQUE_ADDRESS [[no_unique_address]]
// VS2019 v16.10 and later except clang-cl (https://reviews.llvm.org/D110485)
#    elif (FMT_MSC_VERSION >= 1929) && !FMT_CLANG_VERSION
#      define FMT_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#    endif
#  endif
#endif
#ifndef FMT_NO_UNIQUE_ADDRESS
#  define FMT_NO_UNIQUE_ADDRESS
#endif

#if FMT_GCC_VERSION || defined(__clang__)
#  define FMT_VISIBILITY(value) __attribute__((visibility(value)))
#else
#  define FMT_VISIBILITY(value)
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

#ifndef FMT_THROW
#  if FMT_EXCEPTIONS
#    if FMT_MSC_VERSION || defined(__NVCC__)
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
#    define FMT_THROW(x) \
      ::fmt::detail::assert_fail(__FILE__, __LINE__, (x).what())
#  endif
#endif

#if FMT_EXCEPTIONS
#  define FMT_TRY try
#  define FMT_CATCH(x) catch (x)
#else
#  define FMT_TRY if (true)
#  define FMT_CATCH(x) if (false)
#endif

#ifndef FMT_MAYBE_UNUSED
#  if FMT_HAS_CPP17_ATTRIBUTE(maybe_unused)
#    define FMT_MAYBE_UNUSED [[maybe_unused]]
#  else
#    define FMT_MAYBE_UNUSED
#  endif
#endif

#ifndef FMT_USE_USER_DEFINED_LITERALS
// EDG based compilers (Intel, NVIDIA, Elbrus, etc), GCC and MSVC support UDLs.
#  if (FMT_HAS_FEATURE(cxx_user_literals) || FMT_GCC_VERSION >= 407 || \
       FMT_MSC_VERSION >= 1900) &&                                     \
      (!defined(__EDG_VERSION__) || __EDG_VERSION__ >= /* UDL feature */ 480)
#    define FMT_USE_USER_DEFINED_LITERALS 1
#  else
#    define FMT_USE_USER_DEFINED_LITERALS 0
#  endif
#endif

// Defining FMT_REDUCE_INT_INSTANTIATIONS to 1, will reduce the number of
// integer formatter template instantiations to just one by only using the
// largest integer type. This results in a reduction in binary size but will
// cause a decrease in integer formatting performance.
#if !defined(FMT_REDUCE_INT_INSTANTIATIONS)
#  define FMT_REDUCE_INT_INSTANTIATIONS 0
#endif

// __builtin_clz is broken in clang with Microsoft CodeGen:
// https://github.com/fmtlib/fmt/issues/519.
#if !FMT_MSC_VERSION
#  if FMT_HAS_BUILTIN(__builtin_clz) || FMT_GCC_VERSION || FMT_ICC_VERSION
#    define FMT_BUILTIN_CLZ(n) __builtin_clz(n)
#  endif
#  if FMT_HAS_BUILTIN(__builtin_clzll) || FMT_GCC_VERSION || FMT_ICC_VERSION
#    define FMT_BUILTIN_CLZLL(n) __builtin_clzll(n)
#  endif
#endif

// __builtin_ctz is broken in Intel Compiler Classic on Windows:
// https://github.com/fmtlib/fmt/issues/2510.
#ifndef __ICL
#  if FMT_HAS_BUILTIN(__builtin_ctz) || FMT_GCC_VERSION || FMT_ICC_VERSION || \
      defined(__NVCOMPILER)
#    define FMT_BUILTIN_CTZ(n) __builtin_ctz(n)
#  endif
#  if FMT_HAS_BUILTIN(__builtin_ctzll) || FMT_GCC_VERSION || \
      FMT_ICC_VERSION || defined(__NVCOMPILER)
#    define FMT_BUILTIN_CTZLL(n) __builtin_ctzll(n)
#  endif
#endif

#if FMT_MSC_VERSION
#  include <intrin.h>  // _BitScanReverse[64], _BitScanForward[64], _umul128
#endif

// Some compilers masquerade as both MSVC and GCC-likes or otherwise support
// __builtin_clz and __builtin_clzll, so only define FMT_BUILTIN_CLZ using the
// MSVC intrinsics if the clz and clzll builtins are not available.
#if FMT_MSC_VERSION && !defined(FMT_BUILTIN_CLZLL) && \
    !defined(FMT_BUILTIN_CTZLL)
FMT_BEGIN_NAMESPACE
namespace detail {
// Avoid Clang with Microsoft CodeGen's -Wunknown-pragmas warning.
#  if !defined(__clang__)
#    pragma intrinsic(_BitScanForward)
#    pragma intrinsic(_BitScanReverse)
#    if defined(_WIN64)
#      pragma intrinsic(_BitScanForward64)
#      pragma intrinsic(_BitScanReverse64)
#    endif
#  endif

inline auto clz(uint32_t x) -> int {
  unsigned long r = 0;
  _BitScanReverse(&r, x);
  FMT_ASSERT(x != 0, "");
  // Static analysis complains about using uninitialized data
  // "r", but the only way that can happen is if "x" is 0,
  // which the callers guarantee to not happen.
  FMT_MSC_WARNING(suppress : 6102)
  return 31 ^ static_cast<int>(r);
}
#  define FMT_BUILTIN_CLZ(n) detail::clz(n)

inline auto clzll(uint64_t x) -> int {
  unsigned long r = 0;
#  ifdef _WIN64
  _BitScanReverse64(&r, x);
#  else
  // Scan the high 32 bits.
  if (_BitScanReverse(&r, static_cast<uint32_t>(x >> 32)))
    return 63 ^ static_cast<int>(r + 32);
  // Scan the low 32 bits.
  _BitScanReverse(&r, static_cast<uint32_t>(x));
#  endif
  FMT_ASSERT(x != 0, "");
  FMT_MSC_WARNING(suppress : 6102)  // Suppress a bogus static analysis warning.
  return 63 ^ static_cast<int>(r);
}
#  define FMT_BUILTIN_CLZLL(n) detail::clzll(n)

inline auto ctz(uint32_t x) -> int {
  unsigned long r = 0;
  _BitScanForward(&r, x);
  FMT_ASSERT(x != 0, "");
  FMT_MSC_WARNING(suppress : 6102)  // Suppress a bogus static analysis warning.
  return static_cast<int>(r);
}
#  define FMT_BUILTIN_CTZ(n) detail::ctz(n)

inline auto ctzll(uint64_t x) -> int {
  unsigned long r = 0;
  FMT_ASSERT(x != 0, "");
  FMT_MSC_WARNING(suppress : 6102)  // Suppress a bogus static analysis warning.
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

FMT_BEGIN_NAMESPACE

template <typename...> struct disjunction : std::false_type {};
template <typename P> struct disjunction<P> : P {};
template <typename P1, typename... Pn>
struct disjunction<P1, Pn...>
    : conditional_t<bool(P1::value), P1, disjunction<Pn...>> {};

template <typename...> struct conjunction : std::true_type {};
template <typename P> struct conjunction<P> : P {};
template <typename P1, typename... Pn>
struct conjunction<P1, Pn...>
    : conditional_t<bool(P1::value), conjunction<Pn...>, P1> {};

namespace detail {

FMT_CONSTEXPR inline void abort_fuzzing_if(bool condition) {
  ignore_unused(condition);
#ifdef FMT_FUZZ
  if (condition) throw std::runtime_error("fuzzing limit reached");
#endif
}

template <typename CharT, CharT... C> struct string_literal {
  static constexpr CharT value[sizeof...(C)] = {C...};
  constexpr operator basic_string_view<CharT>() const {
    return {value, sizeof...(C)};
  }
};

#if FMT_CPLUSPLUS < 201703L
template <typename CharT, CharT... C>
constexpr CharT string_literal<CharT, C...>::value[sizeof...(C)];
#endif

template <typename Streambuf> class formatbuf : public Streambuf {
 private:
  using char_type = typename Streambuf::char_type;
  using streamsize = decltype(std::declval<Streambuf>().sputn(nullptr, 0));
  using int_type = typename Streambuf::int_type;
  using traits_type = typename Streambuf::traits_type;

  buffer<char_type>& buffer_;

 public:
  explicit formatbuf(buffer<char_type>& buf) : buffer_(buf) {}

 protected:
  // The put area is always empty. This makes the implementation simpler and has
  // the advantage that the streambuf and the buffer are always in sync and
  // sputc never writes into uninitialized memory. A disadvantage is that each
  // call to sputc always results in a (virtual) call to overflow. There is no
  // disadvantage here for sputn since this always results in a call to xsputn.

  auto overflow(int_type ch) -> int_type override {
    if (!traits_type::eq_int_type(ch, traits_type::eof()))
      buffer_.push_back(static_cast<char_type>(ch));
    return ch;
  }

  auto xsputn(const char_type* s, streamsize count) -> streamsize override {
    buffer_.append(s, s + count);
    return count;
  }
};

// Implementation of std::bit_cast for pre-C++20.
template <typename To, typename From, FMT_ENABLE_IF(sizeof(To) == sizeof(From))>
FMT_CONSTEXPR20 auto bit_cast(const From& from) -> To {
#ifdef __cpp_lib_bit_cast
  if (is_constant_evaluated()) return std::bit_cast<To>(from);
#endif
  auto to = To();
  // The cast suppresses a bogus -Wclass-memaccess on GCC.
  std::memcpy(static_cast<void*>(&to), &from, sizeof(to));
  return to;
}

inline auto is_big_endian() -> bool {
#ifdef _WIN32
  return false;
#elif defined(__BIG_ENDIAN__)
  return true;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
  return __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__;
#else
  struct bytes {
    char data[sizeof(int)];
  };
  return bit_cast<bytes>(1).data[0] == 0;
#endif
}

class uint128_fallback {
 private:
  uint64_t lo_, hi_;

 public:
  constexpr uint128_fallback(uint64_t hi, uint64_t lo) : lo_(lo), hi_(hi) {}
  constexpr uint128_fallback(uint64_t value = 0) : lo_(value), hi_(0) {}

  constexpr uint64_t high() const noexcept { return hi_; }
  constexpr uint64_t low() const noexcept { return lo_; }

  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  constexpr explicit operator T() const {
    return static_cast<T>(lo_);
  }

  friend constexpr auto operator==(const uint128_fallback& lhs,
                                   const uint128_fallback& rhs) -> bool {
    return lhs.hi_ == rhs.hi_ && lhs.lo_ == rhs.lo_;
  }
  friend constexpr auto operator!=(const uint128_fallback& lhs,
                                   const uint128_fallback& rhs) -> bool {
    return !(lhs == rhs);
  }
  friend constexpr auto operator>(const uint128_fallback& lhs,
                                  const uint128_fallback& rhs) -> bool {
    return lhs.hi_ != rhs.hi_ ? lhs.hi_ > rhs.hi_ : lhs.lo_ > rhs.lo_;
  }
  friend constexpr auto operator|(const uint128_fallback& lhs,
                                  const uint128_fallback& rhs)
      -> uint128_fallback {
    return {lhs.hi_ | rhs.hi_, lhs.lo_ | rhs.lo_};
  }
  friend constexpr auto operator&(const uint128_fallback& lhs,
                                  const uint128_fallback& rhs)
      -> uint128_fallback {
    return {lhs.hi_ & rhs.hi_, lhs.lo_ & rhs.lo_};
  }
  friend constexpr auto operator~(const uint128_fallback& n)
      -> uint128_fallback {
    return {~n.hi_, ~n.lo_};
  }
  friend auto operator+(const uint128_fallback& lhs,
                        const uint128_fallback& rhs) -> uint128_fallback {
    auto result = uint128_fallback(lhs);
    result += rhs;
    return result;
  }
  friend auto operator*(const uint128_fallback& lhs, uint32_t rhs)
      -> uint128_fallback {
    FMT_ASSERT(lhs.hi_ == 0, "");
    uint64_t hi = (lhs.lo_ >> 32) * rhs;
    uint64_t lo = (lhs.lo_ & ~uint32_t()) * rhs;
    uint64_t new_lo = (hi << 32) + lo;
    return {(hi >> 32) + (new_lo < lo ? 1 : 0), new_lo};
  }
  friend auto operator-(const uint128_fallback& lhs, uint64_t rhs)
      -> uint128_fallback {
    return {lhs.hi_ - (lhs.lo_ < rhs ? 1 : 0), lhs.lo_ - rhs};
  }
  FMT_CONSTEXPR auto operator>>(int shift) const -> uint128_fallback {
    if (shift == 64) return {0, hi_};
    if (shift > 64) return uint128_fallback(0, hi_) >> (shift - 64);
    return {hi_ >> shift, (hi_ << (64 - shift)) | (lo_ >> shift)};
  }
  FMT_CONSTEXPR auto operator<<(int shift) const -> uint128_fallback {
    if (shift == 64) return {lo_, 0};
    if (shift > 64) return uint128_fallback(lo_, 0) << (shift - 64);
    return {hi_ << shift | (lo_ >> (64 - shift)), (lo_ << shift)};
  }
  FMT_CONSTEXPR auto operator>>=(int shift) -> uint128_fallback& {
    return *this = *this >> shift;
  }
  FMT_CONSTEXPR void operator+=(uint128_fallback n) {
    uint64_t new_lo = lo_ + n.lo_;
    uint64_t new_hi = hi_ + n.hi_ + (new_lo < lo_ ? 1 : 0);
    FMT_ASSERT(new_hi >= hi_, "");
    lo_ = new_lo;
    hi_ = new_hi;
  }
  FMT_CONSTEXPR void operator&=(uint128_fallback n) {
    lo_ &= n.lo_;
    hi_ &= n.hi_;
  }

  FMT_CONSTEXPR20 uint128_fallback& operator+=(uint64_t n) noexcept {
    if (is_constant_evaluated()) {
      lo_ += n;
      hi_ += (lo_ < n ? 1 : 0);
      return *this;
    }
#if FMT_HAS_BUILTIN(__builtin_addcll) && !defined(__ibmxl__)
    unsigned long long carry;
    lo_ = __builtin_addcll(lo_, n, 0, &carry);
    hi_ += carry;
#elif FMT_HAS_BUILTIN(__builtin_ia32_addcarryx_u64) && !defined(__ibmxl__)
    unsigned long long result;
    auto carry = __builtin_ia32_addcarryx_u64(0, lo_, n, &result);
    lo_ = result;
    hi_ += carry;
#elif defined(_MSC_VER) && defined(_M_X64)
    auto carry = _addcarry_u64(0, lo_, n, &lo_);
    _addcarry_u64(carry, hi_, 0, &hi_);
#else
    lo_ += n;
    hi_ += (lo_ < n ? 1 : 0);
#endif
    return *this;
  }
};

using uint128_t = conditional_t<FMT_USE_INT128, uint128_opt, uint128_fallback>;

#ifdef UINTPTR_MAX
using uintptr_t = ::uintptr_t;
#else
using uintptr_t = uint128_t;
#endif

// Returns the largest possible value for type T. Same as
// std::numeric_limits<T>::max() but shorter and not affected by the max macro.
template <typename T> constexpr auto max_value() -> T {
  return (std::numeric_limits<T>::max)();
}
template <typename T> constexpr auto num_bits() -> int {
  return std::numeric_limits<T>::digits;
}
// std::numeric_limits<T>::digits may return 0 for 128-bit ints.
template <> constexpr auto num_bits<int128_opt>() -> int { return 128; }
template <> constexpr auto num_bits<uint128_t>() -> int { return 128; }

// A heterogeneous bit_cast used for converting 96-bit long double to uint128_t
// and 128-bit pointers to uint128_fallback.
template <typename To, typename From, FMT_ENABLE_IF(sizeof(To) > sizeof(From))>
inline auto bit_cast(const From& from) -> To {
  constexpr auto size = static_cast<int>(sizeof(From) / sizeof(unsigned));
  struct data_t {
    unsigned value[static_cast<unsigned>(size)];
  } data = bit_cast<data_t>(from);
  auto result = To();
  if (const_check(is_big_endian())) {
    for (int i = 0; i < size; ++i)
      result = (result << num_bits<unsigned>()) | data.value[i];
  } else {
    for (int i = size - 1; i >= 0; --i)
      result = (result << num_bits<unsigned>()) | data.value[i];
  }
  return result;
}

template <typename UInt>
FMT_CONSTEXPR20 inline auto countl_zero_fallback(UInt n) -> int {
  int lz = 0;
  constexpr UInt msb_mask = static_cast<UInt>(1) << (num_bits<UInt>() - 1);
  for (; (n & msb_mask) == 0; n <<= 1) lz++;
  return lz;
}

FMT_CONSTEXPR20 inline auto countl_zero(uint32_t n) -> int {
#ifdef FMT_BUILTIN_CLZ
  if (!is_constant_evaluated()) return FMT_BUILTIN_CLZ(n);
#endif
  return countl_zero_fallback(n);
}

FMT_CONSTEXPR20 inline auto countl_zero(uint64_t n) -> int {
#ifdef FMT_BUILTIN_CLZLL
  if (!is_constant_evaluated()) return FMT_BUILTIN_CLZLL(n);
#endif
  return countl_zero_fallback(n);
}

FMT_INLINE void assume(bool condition) {
  (void)condition;
#if FMT_HAS_BUILTIN(__builtin_assume) && !FMT_ICC_VERSION
  __builtin_assume(condition);
#elif FMT_GCC_VERSION
  if (!condition) __builtin_unreachable();
#endif
}

// An approximation of iterator_t for pre-C++20 systems.
template <typename T>
using iterator_t = decltype(std::begin(std::declval<T&>()));
template <typename T> using sentinel_t = decltype(std::end(std::declval<T&>()));

// A workaround for std::string not having mutable data() until C++17.
template <typename Char>
inline auto get_data(std::basic_string<Char>& s) -> Char* {
  return &s[0];
}
template <typename Container>
inline auto get_data(Container& c) -> typename Container::value_type* {
  return c.data();
}

// Attempts to reserve space for n extra characters in the output range.
// Returns a pointer to the reserved range or a reference to it.
template <typename Container, FMT_ENABLE_IF(is_contiguous<Container>::value)>
#if FMT_CLANG_VERSION >= 307 && !FMT_ICC_VERSION
__attribute__((no_sanitize("undefined")))
#endif
inline auto
reserve(std::back_insert_iterator<Container> it, size_t n) ->
    typename Container::value_type* {
  Container& c = get_container(it);
  size_t size = c.size();
  c.resize(size + n);
  return get_data(c) + size;
}

template <typename T>
inline auto reserve(buffer_appender<T> it, size_t n) -> buffer_appender<T> {
  buffer<T>& buf = get_container(it);
  buf.try_reserve(buf.size() + n);
  return it;
}

template <typename Iterator>
constexpr auto reserve(Iterator& it, size_t) -> Iterator& {
  return it;
}

template <typename OutputIt>
using reserve_iterator =
    remove_reference_t<decltype(reserve(std::declval<OutputIt&>(), 0))>;

template <typename T, typename OutputIt>
constexpr auto to_pointer(OutputIt, size_t) -> T* {
  return nullptr;
}
template <typename T> auto to_pointer(buffer_appender<T> it, size_t n) -> T* {
  buffer<T>& buf = get_container(it);
  auto size = buf.size();
  if (buf.capacity() < size + n) return nullptr;
  buf.try_resize(size + n);
  return buf.data() + size;
}

template <typename Container, FMT_ENABLE_IF(is_contiguous<Container>::value)>
inline auto base_iterator(std::back_insert_iterator<Container> it,
                          typename Container::value_type*)
    -> std::back_insert_iterator<Container> {
  return it;
}

template <typename Iterator>
constexpr auto base_iterator(Iterator, Iterator it) -> Iterator {
  return it;
}

// <algorithm> is spectacularly slow to compile in C++20 so use a simple fill_n
// instead (#1998).
template <typename OutputIt, typename Size, typename T>
FMT_CONSTEXPR auto fill_n(OutputIt out, Size count, const T& value)
    -> OutputIt {
  for (Size i = 0; i < count; ++i) *out++ = value;
  return out;
}
template <typename T, typename Size>
FMT_CONSTEXPR20 auto fill_n(T* out, Size count, char value) -> T* {
  if (is_constant_evaluated()) {
    return fill_n<T*, Size, T>(out, count, value);
  }
  std::memset(out, value, to_unsigned(count));
  return out + count;
}

#ifdef __cpp_char8_t
using char8_type = char8_t;
#else
enum char8_type : unsigned char {};
#endif

template <typename OutChar, typename InputIt, typename OutputIt>
FMT_CONSTEXPR FMT_NOINLINE auto copy_str_noinline(InputIt begin, InputIt end,
                                                  OutputIt out) -> OutputIt {
  return copy_str<OutChar>(begin, end, out);
}

// A public domain branchless UTF-8 decoder by Christopher Wellons:
// https://github.com/skeeto/branchless-utf8
/* Decode the next character, c, from s, reporting errors in e.
 *
 * Since this is a branchless decoder, four bytes will be read from the
 * buffer regardless of the actual length of the next character. This
 * means the buffer _must_ have at least three bytes of zero padding
 * following the end of the data stream.
 *
 * Errors are reported in e, which will be non-zero if the parsed
 * character was somehow invalid: invalid byte sequence, non-canonical
 * encoding, or a surrogate half.
 *
 * The function returns a pointer to the next character. When an error
 * occurs, this pointer will be a guess that depends on the particular
 * error, but it will always advance at least one byte.
 */
FMT_CONSTEXPR inline auto utf8_decode(const char* s, uint32_t* c, int* e)
    -> const char* {
  constexpr const int masks[] = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
  constexpr const uint32_t mins[] = {4194304, 0, 128, 2048, 65536};
  constexpr const int shiftc[] = {0, 18, 12, 6, 0};
  constexpr const int shifte[] = {0, 6, 4, 2, 0};

  int len = "\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\0\0\0\0\0\0\0\2\2\2\2\3\3\4"
      [static_cast<unsigned char>(*s) >> 3];
  // Compute the pointer to the next character early so that the next
  // iteration can start working on the next character. Neither Clang
  // nor GCC figure out this reordering on their own.
  const char* next = s + len + !len;

  using uchar = unsigned char;

  // Assume a four-byte character and load four bytes. Unused bits are
  // shifted out.
  *c = uint32_t(uchar(s[0]) & masks[len]) << 18;
  *c |= uint32_t(uchar(s[1]) & 0x3f) << 12;
  *c |= uint32_t(uchar(s[2]) & 0x3f) << 6;
  *c |= uint32_t(uchar(s[3]) & 0x3f) << 0;
  *c >>= shiftc[len];

  // Accumulate the various error conditions.
  *e = (*c < mins[len]) << 6;       // non-canonical encoding
  *e |= ((*c >> 11) == 0x1b) << 7;  // surrogate half?
  *e |= (*c > 0x10FFFF) << 8;       // out of range?
  *e |= (uchar(s[1]) & 0xc0) >> 2;
  *e |= (uchar(s[2]) & 0xc0) >> 4;
  *e |= uchar(s[3]) >> 6;
  *e ^= 0x2a;  // top two bits of each tail byte correct?
  *e >>= shifte[len];

  return next;
}

constexpr FMT_INLINE_VARIABLE uint32_t invalid_code_point = ~uint32_t();

// Invokes f(cp, sv) for every code point cp in s with sv being the string view
// corresponding to the code point. cp is invalid_code_point on error.
template <typename F>
FMT_CONSTEXPR void for_each_codepoint(string_view s, F f) {
  auto decode = [f](const char* buf_ptr, const char* ptr) {
    auto cp = uint32_t();
    auto error = 0;
    auto end = utf8_decode(buf_ptr, &cp, &error);
    bool result = f(error ? invalid_code_point : cp,
                    string_view(ptr, error ? 1 : to_unsigned(end - buf_ptr)));
    return result ? (error ? buf_ptr + 1 : end) : nullptr;
  };
  auto p = s.data();
  const size_t block_size = 4;  // utf8_decode always reads blocks of 4 chars.
  if (s.size() >= block_size) {
    for (auto end = p + s.size() - block_size + 1; p < end;) {
      p = decode(p, p);
      if (!p) return;
    }
  }
  if (auto num_chars_left = s.data() + s.size() - p) {
    char buf[2 * block_size - 1] = {};
    copy_str<char>(p, p + num_chars_left, buf);
    const char* buf_ptr = buf;
    do {
      auto end = decode(buf_ptr, p);
      if (!end) return;
      p += end - buf_ptr;
      buf_ptr = end;
    } while (buf_ptr - buf < num_chars_left);
  }
}

template <typename Char>
inline auto compute_width(basic_string_view<Char> s) -> size_t {
  return s.size();
}

// Computes approximate display width of a UTF-8 string.
FMT_CONSTEXPR inline size_t compute_width(string_view s) {
  size_t num_code_points = 0;
  // It is not a lambda for compatibility with C++14.
  struct count_code_points {
    size_t* count;
    FMT_CONSTEXPR auto operator()(uint32_t cp, string_view) const -> bool {
      *count += detail::to_unsigned(
          1 +
          (cp >= 0x1100 &&
           (cp <= 0x115f ||  // Hangul Jamo init. consonants
            cp == 0x2329 ||  // LEFT-POINTING ANGLE BRACKET
            cp == 0x232a ||  // RIGHT-POINTING ANGLE BRACKET
            // CJK ... Yi except IDEOGRAPHIC HALF FILL SPACE:
            (cp >= 0x2e80 && cp <= 0xa4cf && cp != 0x303f) ||
            (cp >= 0xac00 && cp <= 0xd7a3) ||    // Hangul Syllables
            (cp >= 0xf900 && cp <= 0xfaff) ||    // CJK Compatibility Ideographs
            (cp >= 0xfe10 && cp <= 0xfe19) ||    // Vertical Forms
            (cp >= 0xfe30 && cp <= 0xfe6f) ||    // CJK Compatibility Forms
            (cp >= 0xff00 && cp <= 0xff60) ||    // Fullwidth Forms
            (cp >= 0xffe0 && cp <= 0xffe6) ||    // Fullwidth Forms
            (cp >= 0x20000 && cp <= 0x2fffd) ||  // CJK
            (cp >= 0x30000 && cp <= 0x3fffd) ||
            // Miscellaneous Symbols and Pictographs + Emoticons:
            (cp >= 0x1f300 && cp <= 0x1f64f) ||
            // Supplemental Symbols and Pictographs:
            (cp >= 0x1f900 && cp <= 0x1f9ff))));
      return true;
    }
  };
  // We could avoid branches by using utf8_decode directly.
  for_each_codepoint(s, count_code_points{&num_code_points});
  return num_code_points;
}

inline auto compute_width(basic_string_view<char8_type> s) -> size_t {
  return compute_width(
      string_view(reinterpret_cast<const char*>(s.data()), s.size()));
}

template <typename Char>
inline auto code_point_index(basic_string_view<Char> s, size_t n) -> size_t {
  size_t size = s.size();
  return n < size ? n : size;
}

// Calculates the index of the nth code point in a UTF-8 string.
inline auto code_point_index(string_view s, size_t n) -> size_t {
  const char* data = s.data();
  size_t num_code_points = 0;
  for (size_t i = 0, size = s.size(); i != size; ++i) {
    if ((data[i] & 0xc0) != 0x80 && ++num_code_points > n) return i;
  }
  return s.size();
}

inline auto code_point_index(basic_string_view<char8_type> s, size_t n)
    -> size_t {
  return code_point_index(
      string_view(reinterpret_cast<const char*>(s.data()), s.size()), n);
}

template <typename T> struct is_integral : std::is_integral<T> {};
template <> struct is_integral<int128_opt> : std::true_type {};
template <> struct is_integral<uint128_t> : std::true_type {};

template <typename T>
using is_signed =
    std::integral_constant<bool, std::numeric_limits<T>::is_signed ||
                                     std::is_same<T, int128_opt>::value>;

template <typename T>
using is_integer =
    bool_constant<is_integral<T>::value && !std::is_same<T, bool>::value &&
                  !std::is_same<T, char>::value &&
                  !std::is_same<T, wchar_t>::value>;

#ifndef FMT_USE_FLOAT
#  define FMT_USE_FLOAT 1
#endif
#ifndef FMT_USE_DOUBLE
#  define FMT_USE_DOUBLE 1
#endif
#ifndef FMT_USE_LONG_DOUBLE
#  define FMT_USE_LONG_DOUBLE 1
#endif

#ifndef FMT_USE_FLOAT128
#  ifdef __clang__
// Clang emulates GCC, so it has to appear early.
#    if FMT_HAS_INCLUDE(<quadmath.h>)
#      define FMT_USE_FLOAT128 1
#    endif
#  elif defined(__GNUC__)
// GNU C++:
#    if defined(_GLIBCXX_USE_FLOAT128) && !defined(__STRICT_ANSI__)
#      define FMT_USE_FLOAT128 1
#    endif
#  endif
#  ifndef FMT_USE_FLOAT128
#    define FMT_USE_FLOAT128 0
#  endif
#endif

#if FMT_USE_FLOAT128
using float128 = __float128;
#else
using float128 = void;
#endif
template <typename T> using is_float128 = std::is_same<T, float128>;

template <typename T>
using is_floating_point =
    bool_constant<std::is_floating_point<T>::value || is_float128<T>::value>;

template <typename T, bool = std::is_floating_point<T>::value>
struct is_fast_float : bool_constant<std::numeric_limits<T>::is_iec559 &&
                                     sizeof(T) <= sizeof(double)> {};
template <typename T> struct is_fast_float<T, false> : std::false_type {};

template <typename T>
using is_double_double = bool_constant<std::numeric_limits<T>::digits == 106>;

#ifndef FMT_USE_FULL_CACHE_DRAGONBOX
#  define FMT_USE_FULL_CACHE_DRAGONBOX 0
#endif

template <typename T>
template <typename U>
void buffer<T>::append(const U* begin, const U* end) {
  while (begin != end) {
    auto count = to_unsigned(end - begin);
    try_reserve(size_ + count);
    auto free_cap = capacity_ - size_;
    if (free_cap < count) count = free_cap;
    std::uninitialized_copy_n(begin, count, ptr_ + size_);
    size_ += count;
    begin += count;
  }
}

template <typename T, typename Enable = void>
struct is_locale : std::false_type {};
template <typename T>
struct is_locale<T, void_t<decltype(T::classic())>> : std::true_type {};
}  // namespace detail

FMT_BEGIN_EXPORT

// The number of characters to store in the basic_memory_buffer object itself
// to avoid dynamic memory allocation.
enum { inline_buffer_size = 500 };

/**
  \rst
  A dynamically growing memory buffer for trivially copyable/constructible types
  with the first ``SIZE`` elements stored in the object itself.

  You can use the ``memory_buffer`` type alias for ``char`` instead.

  **Example**::

     auto out = fmt::memory_buffer();
     format_to(std::back_inserter(out), "The answer is {}.", 42);

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

  // Don't inherit from Allocator to avoid generating type_info for it.
  FMT_NO_UNIQUE_ADDRESS Allocator alloc_;

  // Deallocate memory allocated by the buffer.
  FMT_CONSTEXPR20 void deallocate() {
    T* data = this->data();
    if (data != store_) alloc_.deallocate(data, this->capacity());
  }

 protected:
  FMT_CONSTEXPR20 void grow(size_t size) override {
    detail::abort_fuzzing_if(size > 5000);
    const size_t max_size = std::allocator_traits<Allocator>::max_size(alloc_);
    size_t old_capacity = this->capacity();
    size_t new_capacity = old_capacity + old_capacity / 2;
    if (size > new_capacity)
      new_capacity = size;
    else if (new_capacity > max_size)
      new_capacity = size > max_size ? size : max_size;
    T* old_data = this->data();
    T* new_data =
        std::allocator_traits<Allocator>::allocate(alloc_, new_capacity);
    // Suppress a bogus -Wstringop-overflow in gcc 13.1 (#3481).
    detail::assume(this->size() <= new_capacity);
    // The following code doesn't throw, so the raw pointer above doesn't leak.
    std::uninitialized_copy_n(old_data, this->size(), new_data);
    this->set(new_data, new_capacity);
    // deallocate must not throw according to the standard, but even if it does,
    // the buffer already uses the new storage and will deallocate it in
    // destructor.
    if (old_data != store_) alloc_.deallocate(old_data, old_capacity);
  }

 public:
  using value_type = T;
  using const_reference = const T&;

  FMT_CONSTEXPR20 explicit basic_memory_buffer(
      const Allocator& alloc = Allocator())
      : alloc_(alloc) {
    this->set(store_, SIZE);
    if (detail::is_constant_evaluated()) detail::fill_n(store_, SIZE, T());
  }
  FMT_CONSTEXPR20 ~basic_memory_buffer() { deallocate(); }

 private:
  // Move data from other to this buffer.
  FMT_CONSTEXPR20 void move(basic_memory_buffer& other) {
    alloc_ = std::move(other.alloc_);
    T* data = other.data();
    size_t size = other.size(), capacity = other.capacity();
    if (data == other.store_) {
      this->set(store_, capacity);
      detail::copy_str<T>(other.store_, other.store_ + size, store_);
    } else {
      this->set(data, capacity);
      // Set pointer to the inline array so that delete is not called
      // when deallocating.
      other.set(other.store_, 0);
      other.clear();
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
  FMT_CONSTEXPR20 basic_memory_buffer(basic_memory_buffer&& other) noexcept {
    move(other);
  }

  /**
    \rst
    Moves the content of the other ``basic_memory_buffer`` object to this one.
    \endrst
   */
  auto operator=(basic_memory_buffer&& other) noexcept -> basic_memory_buffer& {
    FMT_ASSERT(this != &other, "");
    deallocate();
    move(other);
    return *this;
  }

  // Returns a copy of the allocator associated with this buffer.
  auto get_allocator() const -> Allocator { return alloc_; }

  /**
    Resizes the buffer to contain *count* elements. If T is a POD type new
    elements may not be initialized.
   */
  FMT_CONSTEXPR20 void resize(size_t count) { this->try_resize(count); }

  /** Increases the buffer capacity to *new_capacity*. */
  void reserve(size_t new_capacity) { this->try_reserve(new_capacity); }

  // Directly append data into the buffer
  using detail::buffer<T>::append;
  template <typename ContiguousRange>
  void append(const ContiguousRange& range) {
    append(range.data(), range.data() + range.size());
  }
};

using memory_buffer = basic_memory_buffer<char>;

template <typename T, size_t SIZE, typename Allocator>
struct is_contiguous<basic_memory_buffer<T, SIZE, Allocator>> : std::true_type {
};

FMT_END_EXPORT
namespace detail {
FMT_API bool write_console(std::FILE* f, string_view text);
FMT_API void print(std::FILE*, string_view);
}  // namespace detail

FMT_BEGIN_EXPORT

// Suppress a misleading warning in older versions of clang.
#if FMT_CLANG_VERSION
#  pragma clang diagnostic ignored "-Wweak-vtables"
#endif

/** An error reported from a formatting function. */
class FMT_VISIBILITY("default") format_error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

namespace detail_exported {
#if FMT_USE_NONTYPE_TEMPLATE_ARGS
template <typename Char, size_t N> struct fixed_string {
  constexpr fixed_string(const Char (&str)[N]) {
    detail::copy_str<Char, const Char*, Char*>(static_cast<const Char*>(str),
                                               str + N, data);
  }
  Char data[N] = {};
};
#endif

// Converts a compile-time string to basic_string_view.
template <typename Char, size_t N>
constexpr auto compile_string_to_view(const Char (&s)[N])
    -> basic_string_view<Char> {
  // Remove trailing NUL character if needed. Won't be present if this is used
  // with a raw character array (i.e. not defined as a string).
  return {s, N - (std::char_traits<Char>::to_int_type(s[N - 1]) == 0 ? 1 : 0)};
}
template <typename Char>
constexpr auto compile_string_to_view(detail::std_string_view<Char> s)
    -> basic_string_view<Char> {
  return {s.data(), s.size()};
}
}  // namespace detail_exported

class loc_value {
 private:
  basic_format_arg<format_context> value_;

 public:
  template <typename T, FMT_ENABLE_IF(!detail::is_float128<T>::value)>
  loc_value(T value) : value_(detail::make_arg<format_context>(value)) {}

  template <typename T, FMT_ENABLE_IF(detail::is_float128<T>::value)>
  loc_value(T) {}

  template <typename Visitor> auto visit(Visitor&& vis) -> decltype(vis(0)) {
    return visit_format_arg(vis, value_);
  }
};

// A locale facet that formats values in UTF-8.
// It is parameterized on the locale to avoid the heavy <locale> include.
template <typename Locale> class format_facet : public Locale::facet {
 private:
  std::string separator_;
  std::string grouping_;
  std::string decimal_point_;

 protected:
  virtual auto do_put(appender out, loc_value val,
                      const format_specs<>& specs) const -> bool;

 public:
  static FMT_API typename Locale::id id;

  explicit format_facet(Locale& loc);
  explicit format_facet(string_view sep = "",
                        std::initializer_list<unsigned char> g = {3},
                        std::string decimal_point = ".")
      : separator_(sep.data(), sep.size()),
        grouping_(g.begin(), g.end()),
        decimal_point_(decimal_point) {}

  auto put(appender out, loc_value val, const format_specs<>& specs) const
      -> bool {
    return do_put(out, val, specs);
  }
};

namespace detail {

// Returns true if value is negative, false otherwise.
// Same as `value < 0` but doesn't produce warnings if T is an unsigned type.
template <typename T, FMT_ENABLE_IF(is_signed<T>::value)>
constexpr auto is_negative(T value) -> bool {
  return value < 0;
}
template <typename T, FMT_ENABLE_IF(!is_signed<T>::value)>
constexpr auto is_negative(T) -> bool {
  return false;
}

template <typename T>
FMT_CONSTEXPR auto is_supported_floating_point(T) -> bool {
  if (std::is_same<T, float>()) return FMT_USE_FLOAT;
  if (std::is_same<T, double>()) return FMT_USE_DOUBLE;
  if (std::is_same<T, long double>()) return FMT_USE_LONG_DOUBLE;
  return true;
}

// Smallest of uint32_t, uint64_t, uint128_t that is large enough to
// represent all values of an integral type T.
template <typename T>
using uint32_or_64_or_128_t =
    conditional_t<num_bits<T>() <= 32 && !FMT_REDUCE_INT_INSTANTIATIONS,
                  uint32_t,
                  conditional_t<num_bits<T>() <= 64, uint64_t, uint128_t>>;
template <typename T>
using uint64_or_128_t = conditional_t<num_bits<T>() <= 64, uint64_t, uint128_t>;

#define FMT_POWERS_OF_10(factor)                                             \
  factor * 10, (factor)*100, (factor)*1000, (factor)*10000, (factor)*100000, \
      (factor)*1000000, (factor)*10000000, (factor)*100000000,               \
      (factor)*1000000000

// Converts value in the range [0, 100) to a string.
constexpr const char* digits2(size_t value) {
  // GCC generates slightly better code when value is pointer-size.
  return &"0001020304050607080910111213141516171819"
         "2021222324252627282930313233343536373839"
         "4041424344454647484950515253545556575859"
         "6061626364656667686970717273747576777879"
         "8081828384858687888990919293949596979899"[value * 2];
}

// Sign is a template parameter to workaround a bug in gcc 4.8.
template <typename Char, typename Sign> constexpr Char sign(Sign s) {
#if !FMT_GCC_VERSION || FMT_GCC_VERSION >= 604
  static_assert(std::is_same<Sign, sign_t>::value, "");
#endif
  return static_cast<Char>("\0-+ "[s]);
}

template <typename T> FMT_CONSTEXPR auto count_digits_fallback(T n) -> int {
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
#if FMT_USE_INT128
FMT_CONSTEXPR inline auto count_digits(uint128_opt n) -> int {
  return count_digits_fallback(n);
}
#endif

#ifdef FMT_BUILTIN_CLZLL
// It is a separate function rather than a part of count_digits to workaround
// the lack of static constexpr in constexpr functions.
inline auto do_count_digits(uint64_t n) -> int {
  // This has comparable performance to the version by Kendall Willets
  // (https://github.com/fmtlib/format-benchmark/blob/master/digits10)
  // but uses smaller tables.
  // Maps bsr(n) to ceil(log10(pow(2, bsr(n) + 1) - 1)).
  static constexpr uint8_t bsr2log10[] = {
      1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,
      6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 10,
      10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15,
      15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20};
  auto t = bsr2log10[FMT_BUILTIN_CLZLL(n | 1) ^ 63];
  static constexpr const uint64_t zero_or_powers_of_10[] = {
      0, 0, FMT_POWERS_OF_10(1U), FMT_POWERS_OF_10(1000000000ULL),
      10000000000000000000ULL};
  return t - (n < zero_or_powers_of_10[t]);
}
#endif

// Returns the number of decimal digits in n. Leading zeros are not counted
// except for n == 0 in which case count_digits returns 1.
FMT_CONSTEXPR20 inline auto count_digits(uint64_t n) -> int {
#ifdef FMT_BUILTIN_CLZLL
  if (!is_constant_evaluated()) {
    return do_count_digits(n);
  }
#endif
  return count_digits_fallback(n);
}

// Counts the number of digits in n. BITS = log2(radix).
template <int BITS, typename UInt>
FMT_CONSTEXPR auto count_digits(UInt n) -> int {
#ifdef FMT_BUILTIN_CLZ
  if (!is_constant_evaluated() && num_bits<UInt>() == 32)
    return (FMT_BUILTIN_CLZ(static_cast<uint32_t>(n) | 1) ^ 31) / BITS + 1;
#endif
  // Lambda avoids unreachable code warnings from NVHPC.
  return [](UInt m) {
    int num_digits = 0;
    do {
      ++num_digits;
    } while ((m >>= BITS) != 0);
    return num_digits;
  }(n);
}

#ifdef FMT_BUILTIN_CLZ
// It is a separate function rather than a part of count_digits to workaround
// the lack of static constexpr in constexpr functions.
FMT_INLINE auto do_count_digits(uint32_t n) -> int {
// An optimization by Kendall Willets from https://bit.ly/3uOIQrB.
// This increments the upper 32 bits (log10(T) - 1) when >= T is added.
#  define FMT_INC(T) (((sizeof(#T) - 1ull) << 32) - T)
  static constexpr uint64_t table[] = {
      FMT_INC(0),          FMT_INC(0),          FMT_INC(0),           // 8
      FMT_INC(10),         FMT_INC(10),         FMT_INC(10),          // 64
      FMT_INC(100),        FMT_INC(100),        FMT_INC(100),         // 512
      FMT_INC(1000),       FMT_INC(1000),       FMT_INC(1000),        // 4096
      FMT_INC(10000),      FMT_INC(10000),      FMT_INC(10000),       // 32k
      FMT_INC(100000),     FMT_INC(100000),     FMT_INC(100000),      // 256k
      FMT_INC(1000000),    FMT_INC(1000000),    FMT_INC(1000000),     // 2048k
      FMT_INC(10000000),   FMT_INC(10000000),   FMT_INC(10000000),    // 16M
      FMT_INC(100000000),  FMT_INC(100000000),  FMT_INC(100000000),   // 128M
      FMT_INC(1000000000), FMT_INC(1000000000), FMT_INC(1000000000),  // 1024M
      FMT_INC(1000000000), FMT_INC(1000000000)                        // 4B
  };
  auto inc = table[FMT_BUILTIN_CLZ(n | 1) ^ 31];
  return static_cast<int>((n + inc) >> 32);
}
#endif

// Optional version of count_digits for better performance on 32-bit platforms.
FMT_CONSTEXPR20 inline auto count_digits(uint32_t n) -> int {
#ifdef FMT_BUILTIN_CLZ
  if (!is_constant_evaluated()) {
    return do_count_digits(n);
  }
#endif
  return count_digits_fallback(n);
}

template <typename Int> constexpr auto digits10() noexcept -> int {
  return std::numeric_limits<Int>::digits10;
}
template <> constexpr auto digits10<int128_opt>() noexcept -> int { return 38; }
template <> constexpr auto digits10<uint128_t>() noexcept -> int { return 38; }

template <typename Char> struct thousands_sep_result {
  std::string grouping;
  Char thousands_sep;
};

template <typename Char>
FMT_API auto thousands_sep_impl(locale_ref loc) -> thousands_sep_result<Char>;
template <typename Char>
inline auto thousands_sep(locale_ref loc) -> thousands_sep_result<Char> {
  auto result = thousands_sep_impl<char>(loc);
  return {result.grouping, Char(result.thousands_sep)};
}
template <>
inline auto thousands_sep(locale_ref loc) -> thousands_sep_result<wchar_t> {
  return thousands_sep_impl<wchar_t>(loc);
}

template <typename Char>
FMT_API auto decimal_point_impl(locale_ref loc) -> Char;
template <typename Char> inline auto decimal_point(locale_ref loc) -> Char {
  return Char(decimal_point_impl<char>(loc));
}
template <> inline auto decimal_point(locale_ref loc) -> wchar_t {
  return decimal_point_impl<wchar_t>(loc);
}

// Compares two characters for equality.
template <typename Char> auto equal2(const Char* lhs, const char* rhs) -> bool {
  return lhs[0] == Char(rhs[0]) && lhs[1] == Char(rhs[1]);
}
inline auto equal2(const char* lhs, const char* rhs) -> bool {
  return memcmp(lhs, rhs, 2) == 0;
}

// Copies two characters from src to dst.
template <typename Char>
FMT_CONSTEXPR20 FMT_INLINE void copy2(Char* dst, const char* src) {
  if (!is_constant_evaluated() && sizeof(Char) == sizeof(char)) {
    memcpy(dst, src, 2);
    return;
  }
  *dst++ = static_cast<Char>(*src++);
  *dst = static_cast<Char>(*src);
}

template <typename Iterator> struct format_decimal_result {
  Iterator begin;
  Iterator end;
};

// Formats a decimal unsigned integer value writing into out pointing to a
// buffer of specified size. The caller must ensure that the buffer is large
// enough.
template <typename Char, typename UInt>
FMT_CONSTEXPR20 auto format_decimal(Char* out, UInt value, int size)
    -> format_decimal_result<Char*> {
  FMT_ASSERT(size >= count_digits(value), "invalid digit count");
  out += size;
  Char* end = out;
  while (value >= 100) {
    // Integer division is slow so do it for a group of two digits instead
    // of for every digit. The idea comes from the talk by Alexandrescu
    // "Three Optimization Tips for C++". See speed-test for a comparison.
    out -= 2;
    copy2(out, digits2(static_cast<size_t>(value % 100)));
    value /= 100;
  }
  if (value < 10) {
    *--out = static_cast<Char>('0' + value);
    return {out, end};
  }
  out -= 2;
  copy2(out, digits2(static_cast<size_t>(value)));
  return {out, end};
}

template <typename Char, typename UInt, typename Iterator,
          FMT_ENABLE_IF(!std::is_pointer<remove_cvref_t<Iterator>>::value)>
FMT_CONSTEXPR inline auto format_decimal(Iterator out, UInt value, int size)
    -> format_decimal_result<Iterator> {
  // Buffer is large enough to hold all digits (digits10 + 1).
  Char buffer[digits10<UInt>() + 1] = {};
  auto end = format_decimal(buffer, value, size).end;
  return {out, detail::copy_str_noinline<Char>(buffer, end, out)};
}

template <unsigned BASE_BITS, typename Char, typename UInt>
FMT_CONSTEXPR auto format_uint(Char* buffer, UInt value, int num_digits,
                               bool upper = false) -> Char* {
  buffer += num_digits;
  Char* end = buffer;
  do {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    unsigned digit = static_cast<unsigned>(value & ((1 << BASE_BITS) - 1));
    *--buffer = static_cast<Char>(BASE_BITS < 4 ? static_cast<char>('0' + digit)
                                                : digits[digit]);
  } while ((value >>= BASE_BITS) != 0);
  return end;
}

template <unsigned BASE_BITS, typename Char, typename It, typename UInt>
FMT_CONSTEXPR inline auto format_uint(It out, UInt value, int num_digits,
                                      bool upper = false) -> It {
  if (auto ptr = to_pointer<Char>(out, to_unsigned(num_digits))) {
    format_uint<BASE_BITS>(ptr, value, num_digits, upper);
    return out;
  }
  // Buffer should be large enough to hold all digits (digits / BASE_BITS + 1).
  char buffer[num_bits<UInt>() / BASE_BITS + 1];
  format_uint<BASE_BITS>(buffer, value, num_digits, upper);
  return detail::copy_str_noinline<Char>(buffer, buffer + num_digits, out);
}

// A converter from UTF-8 to UTF-16.
class utf8_to_utf16 {
 private:
  basic_memory_buffer<wchar_t> buffer_;

 public:
  FMT_API explicit utf8_to_utf16(string_view s);
  operator basic_string_view<wchar_t>() const { return {&buffer_[0], size()}; }
  auto size() const -> size_t { return buffer_.size() - 1; }
  auto c_str() const -> const wchar_t* { return &buffer_[0]; }
  auto str() const -> std::wstring { return {&buffer_[0], size()}; }
};

enum class to_utf8_error_policy { abort, replace };

// A converter from UTF-16/UTF-32 (host endian) to UTF-8.
template <typename WChar, typename Buffer = memory_buffer> class to_utf8 {
 private:
  Buffer buffer_;

 public:
  to_utf8() {}
  explicit to_utf8(basic_string_view<WChar> s,
                   to_utf8_error_policy policy = to_utf8_error_policy::abort) {
    static_assert(sizeof(WChar) == 2 || sizeof(WChar) == 4,
                  "Expect utf16 or utf32");
    if (!convert(s, policy))
      FMT_THROW(std::runtime_error(sizeof(WChar) == 2 ? "invalid utf16"
                                                      : "invalid utf32"));
  }
  operator string_view() const { return string_view(&buffer_[0], size()); }
  size_t size() const { return buffer_.size() - 1; }
  const char* c_str() const { return &buffer_[0]; }
  std::string str() const { return std::string(&buffer_[0], size()); }

  // Performs conversion returning a bool instead of throwing exception on
  // conversion error. This method may still throw in case of memory allocation
  // error.
  bool convert(basic_string_view<WChar> s,
               to_utf8_error_policy policy = to_utf8_error_policy::abort) {
    if (!convert(buffer_, s, policy)) return false;
    buffer_.push_back(0);
    return true;
  }
  static bool convert(
      Buffer& buf, basic_string_view<WChar> s,
      to_utf8_error_policy policy = to_utf8_error_policy::abort) {
    for (auto p = s.begin(); p != s.end(); ++p) {
      uint32_t c = static_cast<uint32_t>(*p);
      if (sizeof(WChar) == 2 && c >= 0xd800 && c <= 0xdfff) {
        // Handle a surrogate pair.
        ++p;
        if (p == s.end() || (c & 0xfc00) != 0xd800 || (*p & 0xfc00) != 0xdc00) {
          if (policy == to_utf8_error_policy::abort) return false;
          buf.append(string_view("\xEF\xBF\xBD"));
          --p;
        } else {
          c = (c << 10) + static_cast<uint32_t>(*p) - 0x35fdc00;
        }
      } else if (c < 0x80) {
        buf.push_back(static_cast<char>(c));
      } else if (c < 0x800) {
        buf.push_back(static_cast<char>(0xc0 | (c >> 6)));
        buf.push_back(static_cast<char>(0x80 | (c & 0x3f)));
      } else if ((c >= 0x800 && c <= 0xd7ff) || (c >= 0xe000 && c <= 0xffff)) {
        buf.push_back(static_cast<char>(0xe0 | (c >> 12)));
        buf.push_back(static_cast<char>(0x80 | ((c & 0xfff) >> 6)));
        buf.push_back(static_cast<char>(0x80 | (c & 0x3f)));
      } else if (c >= 0x10000 && c <= 0x10ffff) {
        buf.push_back(static_cast<char>(0xf0 | (c >> 18)));
        buf.push_back(static_cast<char>(0x80 | ((c & 0x3ffff) >> 12)));
        buf.push_back(static_cast<char>(0x80 | ((c & 0xfff) >> 6)));
        buf.push_back(static_cast<char>(0x80 | (c & 0x3f)));
      } else {
        return false;
      }
    }
    return true;
  }
};

// Computes 128-bit result of multiplication of two 64-bit unsigned integers.
inline uint128_fallback umul128(uint64_t x, uint64_t y) noexcept {
#if FMT_USE_INT128
  auto p = static_cast<uint128_opt>(x) * static_cast<uint128_opt>(y);
  return {static_cast<uint64_t>(p >> 64), static_cast<uint64_t>(p)};
#elif defined(_MSC_VER) && defined(_M_X64)
  auto hi = uint64_t();
  auto lo = _umul128(x, y, &hi);
  return {hi, lo};
#else
  const uint64_t mask = static_cast<uint64_t>(max_value<uint32_t>());

  uint64_t a = x >> 32;
  uint64_t b = x & mask;
  uint64_t c = y >> 32;
  uint64_t d = y & mask;

  uint64_t ac = a * c;
  uint64_t bc = b * c;
  uint64_t ad = a * d;
  uint64_t bd = b * d;

  uint64_t intermediate = (bd >> 32) + (ad & mask) + (bc & mask);

  return {ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32),
          (intermediate << 32) + (bd & mask)};
#endif
}

namespace dragonbox {
// Computes floor(log10(pow(2, e))) for e in [-2620, 2620] using the method from
// https://fmt.dev/papers/Dragonbox.pdf#page=28, section 6.1.
inline int floor_log10_pow2(int e) noexcept {
  FMT_ASSERT(e <= 2620 && e >= -2620, "too large exponent");
  static_assert((-1 >> 1) == -1, "right shift is not arithmetic");
  return (e * 315653) >> 20;
}

inline int floor_log2_pow10(int e) noexcept {
  FMT_ASSERT(e <= 1233 && e >= -1233, "too large exponent");
  return (e * 1741647) >> 19;
}

// Computes upper 64 bits of multiplication of two 64-bit unsigned integers.
inline uint64_t umul128_upper64(uint64_t x, uint64_t y) noexcept {
#if FMT_USE_INT128
  auto p = static_cast<uint128_opt>(x) * static_cast<uint128_opt>(y);
  return static_cast<uint64_t>(p >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
  return __umulh(x, y);
#else
  return umul128(x, y).high();
#endif
}

// Computes upper 128 bits of multiplication of a 64-bit unsigned integer and a
// 128-bit unsigned integer.
inline uint128_fallback umul192_upper128(uint64_t x,
                                         uint128_fallback y) noexcept {
  uint128_fallback r = umul128(x, y.high());
  r += umul128_upper64(x, y.low());
  return r;
}

FMT_API uint128_fallback get_cached_power(int k) noexcept;

// Type-specific information that Dragonbox uses.
template <typename T, typename Enable = void> struct float_info;

template <> struct float_info<float> {
  using carrier_uint = uint32_t;
  static const int exponent_bits = 8;
  static const int kappa = 1;
  static const int big_divisor = 100;
  static const int small_divisor = 10;
  static const int min_k = -31;
  static const int max_k = 46;
  static const int shorter_interval_tie_lower_threshold = -35;
  static const int shorter_interval_tie_upper_threshold = -35;
};

template <> struct float_info<double> {
  using carrier_uint = uint64_t;
  static const int exponent_bits = 11;
  static const int kappa = 2;
  static const int big_divisor = 1000;
  static const int small_divisor = 100;
  static const int min_k = -292;
  static const int max_k = 341;
  static const int shorter_interval_tie_lower_threshold = -77;
  static const int shorter_interval_tie_upper_threshold = -77;
};

// An 80- or 128-bit floating point number.
template <typename T>
struct float_info<T, enable_if_t<std::numeric_limits<T>::digits == 64 ||
                                 std::numeric_limits<T>::digits == 113 ||
                                 is_float128<T>::value>> {
  using carrier_uint = detail::uint128_t;
  static const int exponent_bits = 15;
};

// A double-double floating point number.
template <typename T>
struct float_info<T, enable_if_t<is_double_double<T>::value>> {
  using carrier_uint = detail::uint128_t;
};

template <typename T> struct decimal_fp {
  using significand_type = typename float_info<T>::carrier_uint;
  significand_type significand;
  int exponent;
};

template <typename T> FMT_API auto to_decimal(T x) noexcept -> decimal_fp<T>;
}  // namespace dragonbox

// Returns true iff Float has the implicit bit which is not stored.
template <typename Float> constexpr bool has_implicit_bit() {
  // An 80-bit FP number has a 64-bit significand an no implicit bit.
  return std::numeric_limits<Float>::digits != 64;
}

// Returns the number of significand bits stored in Float. The implicit bit is
// not counted since it is not stored.
template <typename Float> constexpr int num_significand_bits() {
  // std::numeric_limits may not support __float128.
  return is_float128<Float>() ? 112
                              : (std::numeric_limits<Float>::digits -
                                 (has_implicit_bit<Float>() ? 1 : 0));
}

template <typename Float>
constexpr auto exponent_mask() ->
    typename dragonbox::float_info<Float>::carrier_uint {
  using float_uint = typename dragonbox::float_info<Float>::carrier_uint;
  return ((float_uint(1) << dragonbox::float_info<Float>::exponent_bits) - 1)
         << num_significand_bits<Float>();
}
template <typename Float> constexpr auto exponent_bias() -> int {
  // std::numeric_limits may not support __float128.
  return is_float128<Float>() ? 16383
                              : std::numeric_limits<Float>::max_exponent - 1;
}

// Writes the exponent exp in the form "[+-]d{2,3}" to buffer.
template <typename Char, typename It>
FMT_CONSTEXPR auto write_exponent(int exp, It it) -> It {
  FMT_ASSERT(-10000 < exp && exp < 10000, "exponent out of range");
  if (exp < 0) {
    *it++ = static_cast<Char>('-');
    exp = -exp;
  } else {
    *it++ = static_cast<Char>('+');
  }
  if (exp >= 100) {
    const char* top = digits2(to_unsigned(exp / 100));
    if (exp >= 1000) *it++ = static_cast<Char>(top[0]);
    *it++ = static_cast<Char>(top[1]);
    exp %= 100;
  }
  const char* d = digits2(to_unsigned(exp));
  *it++ = static_cast<Char>(d[0]);
  *it++ = static_cast<Char>(d[1]);
  return it;
}

// A floating-point number f * pow(2, e) where F is an unsigned type.
template <typename F> struct basic_fp {
  F f;
  int e;

  static constexpr const int num_significand_bits =
      static_cast<int>(sizeof(F) * num_bits<unsigned char>());

  constexpr basic_fp() : f(0), e(0) {}
  constexpr basic_fp(uint64_t f_val, int e_val) : f(f_val), e(e_val) {}

  // Constructs fp from an IEEE754 floating-point number.
  template <typename Float> FMT_CONSTEXPR basic_fp(Float n) { assign(n); }

  // Assigns n to this and return true iff predecessor is closer than successor.
  template <typename Float, FMT_ENABLE_IF(!is_double_double<Float>::value)>
  FMT_CONSTEXPR auto assign(Float n) -> bool {
    static_assert(std::numeric_limits<Float>::digits <= 113, "unsupported FP");
    // Assume Float is in the format [sign][exponent][significand].
    using carrier_uint = typename dragonbox::float_info<Float>::carrier_uint;
    const auto num_float_significand_bits =
        detail::num_significand_bits<Float>();
    const auto implicit_bit = carrier_uint(1) << num_float_significand_bits;
    const auto significand_mask = implicit_bit - 1;
    auto u = bit_cast<carrier_uint>(n);
    f = static_cast<F>(u & significand_mask);
    auto biased_e = static_cast<int>((u & exponent_mask<Float>()) >>
                                     num_float_significand_bits);
    // The predecessor is closer if n is a normalized power of 2 (f == 0)
    // other than the smallest normalized number (biased_e > 1).
    auto is_predecessor_closer = f == 0 && biased_e > 1;
    if (biased_e == 0)
      biased_e = 1;  // Subnormals use biased exponent 1 (min exponent).
    else if (has_implicit_bit<Float>())
      f += static_cast<F>(implicit_bit);
    e = biased_e - exponent_bias<Float>() - num_float_significand_bits;
    if (!has_implicit_bit<Float>()) ++e;
    return is_predecessor_closer;
  }

  template <typename Float, FMT_ENABLE_IF(is_double_double<Float>::value)>
  FMT_CONSTEXPR auto assign(Float n) -> bool {
    static_assert(std::numeric_limits<double>::is_iec559, "unsupported FP");
    return assign(static_cast<double>(n));
  }
};

using fp = basic_fp<unsigned long long>;

// Normalizes the value converted from double and multiplied by (1 << SHIFT).
template <int SHIFT = 0, typename F>
FMT_CONSTEXPR basic_fp<F> normalize(basic_fp<F> value) {
  // Handle subnormals.
  const auto implicit_bit = F(1) << num_significand_bits<double>();
  const auto shifted_implicit_bit = implicit_bit << SHIFT;
  while ((value.f & shifted_implicit_bit) == 0) {
    value.f <<= 1;
    --value.e;
  }
  // Subtract 1 to account for hidden bit.
  const auto offset = basic_fp<F>::num_significand_bits -
                      num_significand_bits<double>() - SHIFT - 1;
  value.f <<= offset;
  value.e -= offset;
  return value;
}

// Computes lhs * rhs / pow(2, 64) rounded to nearest with half-up tie breaking.
FMT_CONSTEXPR inline uint64_t multiply(uint64_t lhs, uint64_t rhs) {
#if FMT_USE_INT128
  auto product = static_cast<__uint128_t>(lhs) * rhs;
  auto f = static_cast<uint64_t>(product >> 64);
  return (static_cast<uint64_t>(product) & (1ULL << 63)) != 0 ? f + 1 : f;
#else
  // Multiply 32-bit parts of significands.
  uint64_t mask = (1ULL << 32) - 1;
  uint64_t a = lhs >> 32, b = lhs & mask;
  uint64_t c = rhs >> 32, d = rhs & mask;
  uint64_t ac = a * c, bc = b * c, ad = a * d, bd = b * d;
  // Compute mid 64-bit of result and round.
  uint64_t mid = (bd >> 32) + (ad & mask) + (bc & mask) + (1U << 31);
  return ac + (ad >> 32) + (bc >> 32) + (mid >> 32);
#endif
}

FMT_CONSTEXPR inline fp operator*(fp x, fp y) {
  return {multiply(x.f, y.f), x.e + y.e + 64};
}

template <typename T = void> struct basic_data {
  // For checking rounding thresholds.
  // The kth entry is chosen to be the smallest integer such that the
  // upper 32-bits of 10^(k+1) times it is strictly bigger than 5 * 10^k.
  static constexpr uint32_t fractional_part_rounding_thresholds[8] = {
      2576980378U,  // ceil(2^31 + 2^32/10^1)
      2190433321U,  // ceil(2^31 + 2^32/10^2)
      2151778616U,  // ceil(2^31 + 2^32/10^3)
      2147913145U,  // ceil(2^31 + 2^32/10^4)
      2147526598U,  // ceil(2^31 + 2^32/10^5)
      2147487943U,  // ceil(2^31 + 2^32/10^6)
      2147484078U,  // ceil(2^31 + 2^32/10^7)
      2147483691U   // ceil(2^31 + 2^32/10^8)
  };
};
// This is a struct rather than an alias to avoid shadowing warnings in gcc.
struct data : basic_data<> {};

#if FMT_CPLUSPLUS < 201703L
template <typename T>
constexpr uint32_t basic_data<T>::fractional_part_rounding_thresholds[];
#endif

template <typename T, bool doublish = num_bits<T>() == num_bits<double>()>
using convert_float_result =
    conditional_t<std::is_same<T, float>::value || doublish, double, T>;

template <typename T>
constexpr auto convert_float(T value) -> convert_float_result<T> {
  return static_cast<convert_float_result<T>>(value);
}

template <typename OutputIt, typename Char>
FMT_NOINLINE FMT_CONSTEXPR auto fill(OutputIt it, size_t n,
                                     const fill_t<Char>& fill) -> OutputIt {
  auto fill_size = fill.size();
  if (fill_size == 1) return detail::fill_n(it, n, fill[0]);
  auto data = fill.data();
  for (size_t i = 0; i < n; ++i)
    it = copy_str<Char>(data, data + fill_size, it);
  return it;
}

// Writes the output of f, padded according to format specifications in specs.
// size: output size in code units.
// width: output display width in (terminal) column positions.
template <align::type align = align::left, typename OutputIt, typename Char,
          typename F>
FMT_CONSTEXPR auto write_padded(OutputIt out, const format_specs<Char>& specs,
                                size_t size, size_t width, F&& f) -> OutputIt {
  static_assert(align == align::left || align == align::right, "");
  unsigned spec_width = to_unsigned(specs.width);
  size_t padding = spec_width > width ? spec_width - width : 0;
  // Shifts are encoded as string literals because static constexpr is not
  // supported in constexpr functions.
  auto* shifts = align == align::left ? "\x1f\x1f\x00\x01" : "\x00\x1f\x00\x01";
  size_t left_padding = padding >> shifts[specs.align];
  size_t right_padding = padding - left_padding;
  auto it = reserve(out, size + padding * specs.fill.size());
  if (left_padding != 0) it = fill(it, left_padding, specs.fill);
  it = f(it);
  if (right_padding != 0) it = fill(it, right_padding, specs.fill);
  return base_iterator(out, it);
}

template <align::type align = align::left, typename OutputIt, typename Char,
          typename F>
constexpr auto write_padded(OutputIt out, const format_specs<Char>& specs,
                            size_t size, F&& f) -> OutputIt {
  return write_padded<align>(out, specs, size, size, f);
}

template <align::type align = align::left, typename Char, typename OutputIt>
FMT_CONSTEXPR auto write_bytes(OutputIt out, string_view bytes,
                               const format_specs<Char>& specs) -> OutputIt {
  return write_padded<align>(
      out, specs, bytes.size(), [bytes](reserve_iterator<OutputIt> it) {
        const char* data = bytes.data();
        return copy_str<Char>(data, data + bytes.size(), it);
      });
}

template <typename Char, typename OutputIt, typename UIntPtr>
auto write_ptr(OutputIt out, UIntPtr value, const format_specs<Char>* specs)
    -> OutputIt {
  int num_digits = count_digits<4>(value);
  auto size = to_unsigned(num_digits) + size_t(2);
  auto write = [=](reserve_iterator<OutputIt> it) {
    *it++ = static_cast<Char>('0');
    *it++ = static_cast<Char>('x');
    return format_uint<4, Char>(it, value, num_digits);
  };
  return specs ? write_padded<align::right>(out, *specs, size, write)
               : base_iterator(out, write(reserve(out, size)));
}

// Returns true iff the code point cp is printable.
FMT_API auto is_printable(uint32_t cp) -> bool;

inline auto needs_escape(uint32_t cp) -> bool {
  return cp < 0x20 || cp == 0x7f || cp == '"' || cp == '\\' ||
         !is_printable(cp);
}

template <typename Char> struct find_escape_result {
  const Char* begin;
  const Char* end;
  uint32_t cp;
};

template <typename Char>
using make_unsigned_char =
    typename conditional_t<std::is_integral<Char>::value,
                           std::make_unsigned<Char>,
                           type_identity<uint32_t>>::type;

template <typename Char>
auto find_escape(const Char* begin, const Char* end)
    -> find_escape_result<Char> {
  for (; begin != end; ++begin) {
    uint32_t cp = static_cast<make_unsigned_char<Char>>(*begin);
    if (const_check(sizeof(Char) == 1) && cp >= 0x80) continue;
    if (needs_escape(cp)) return {begin, begin + 1, cp};
  }
  return {begin, nullptr, 0};
}

inline auto find_escape(const char* begin, const char* end)
    -> find_escape_result<char> {
  if (!is_utf8()) return find_escape<char>(begin, end);
  auto result = find_escape_result<char>{end, nullptr, 0};
  for_each_codepoint(string_view(begin, to_unsigned(end - begin)),
                     [&](uint32_t cp, string_view sv) {
                       if (needs_escape(cp)) {
                         result = {sv.begin(), sv.end(), cp};
                         return false;
                       }
                       return true;
                     });
  return result;
}

#define FMT_STRING_IMPL(s, base, explicit)                                    \
  [] {                                                                        \
    /* Use the hidden visibility as a workaround for a GCC bug (#1973). */    \
    /* Use a macro-like name to avoid shadowing warnings. */                  \
    struct FMT_VISIBILITY("hidden") FMT_COMPILE_STRING : base {               \
      using char_type FMT_MAYBE_UNUSED = fmt::remove_cvref_t<decltype(s[0])>; \
      FMT_MAYBE_UNUSED FMT_CONSTEXPR explicit                                 \
      operator fmt::basic_string_view<char_type>() const {                    \
        return fmt::detail_exported::compile_string_to_view<char_type>(s);    \
      }                                                                       \
    };                                                                        \
    return FMT_COMPILE_STRING();                                              \
  }()

/**
  \rst
  Constructs a compile-time format string from a string literal *s*.

  **Example**::

    // A compile-time error because 'd' is an invalid specifier for strings.
    std::string s = fmt::format(FMT_STRING("{:d}"), "foo");
  \endrst
 */
#define FMT_STRING(s) FMT_STRING_IMPL(s, fmt::detail::compile_string, )

template <size_t width, typename Char, typename OutputIt>
auto write_codepoint(OutputIt out, char prefix, uint32_t cp) -> OutputIt {
  *out++ = static_cast<Char>('\\');
  *out++ = static_cast<Char>(prefix);
  Char buf[width];
  fill_n(buf, width, static_cast<Char>('0'));
  format_uint<4>(buf, cp, width);
  return copy_str<Char>(buf, buf + width, out);
}

template <typename OutputIt, typename Char>
auto write_escaped_cp(OutputIt out, const find_escape_result<Char>& escape)
    -> OutputIt {
  auto c = static_cast<Char>(escape.cp);
  switch (escape.cp) {
  case '\n':
    *out++ = static_cast<Char>('\\');
    c = static_cast<Char>('n');
    break;
  case '\r':
    *out++ = static_cast<Char>('\\');
    c = static_cast<Char>('r');
    break;
  case '\t':
    *out++ = static_cast<Char>('\\');
    c = static_cast<Char>('t');
    break;
  case '"':
    FMT_FALLTHROUGH;
  case '\'':
    FMT_FALLTHROUGH;
  case '\\':
    *out++ = static_cast<Char>('\\');
    break;
  default:
    if (escape.cp < 0x100) {
      return write_codepoint<2, Char>(out, 'x', escape.cp);
    }
    if (escape.cp < 0x10000) {
      return write_codepoint<4, Char>(out, 'u', escape.cp);
    }
    if (escape.cp < 0x110000) {
      return write_codepoint<8, Char>(out, 'U', escape.cp);
    }
    for (Char escape_char : basic_string_view<Char>(
             escape.begin, to_unsigned(escape.end - escape.begin))) {
      out = write_codepoint<2, Char>(out, 'x',
                                     static_cast<uint32_t>(escape_char) & 0xFF);
    }
    return out;
  }
  *out++ = c;
  return out;
}

template <typename Char, typename OutputIt>
auto write_escaped_string(OutputIt out, basic_string_view<Char> str)
    -> OutputIt {
  *out++ = static_cast<Char>('"');
  auto begin = str.begin(), end = str.end();
  do {
    auto escape = find_escape(begin, end);
    out = copy_str<Char>(begin, escape.begin, out);
    begin = escape.end;
    if (!begin) break;
    out = write_escaped_cp<OutputIt, Char>(out, escape);
  } while (begin != end);
  *out++ = static_cast<Char>('"');
  return out;
}

template <typename Char, typename OutputIt>
auto write_escaped_char(OutputIt out, Char v) -> OutputIt {
  *out++ = static_cast<Char>('\'');
  if ((needs_escape(static_cast<uint32_t>(v)) && v != static_cast<Char>('"')) ||
      v == static_cast<Char>('\'')) {
    out = write_escaped_cp(
        out, find_escape_result<Char>{&v, &v + 1, static_cast<uint32_t>(v)});
  } else {
    *out++ = v;
  }
  *out++ = static_cast<Char>('\'');
  return out;
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write_char(OutputIt out, Char value,
                              const format_specs<Char>& specs) -> OutputIt {
  bool is_debug = specs.type == presentation_type::debug;
  return write_padded(out, specs, 1, [=](reserve_iterator<OutputIt> it) {
    if (is_debug) return write_escaped_char(it, value);
    *it++ = value;
    return it;
  });
}
template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, Char value,
                         const format_specs<Char>& specs, locale_ref loc = {})
    -> OutputIt {
  // char is formatted as unsigned char for consistency across platforms.
  using unsigned_type =
      conditional_t<std::is_same<Char, char>::value, unsigned char, unsigned>;
  return check_char_specs(specs)
             ? write_char(out, value, specs)
             : write(out, static_cast<unsigned_type>(value), specs, loc);
}

// Data for write_int that doesn't depend on output iterator type. It is used to
// avoid template code bloat.
template <typename Char> struct write_int_data {
  size_t size;
  size_t padding;

  FMT_CONSTEXPR write_int_data(int num_digits, unsigned prefix,
                               const format_specs<Char>& specs)
      : size((prefix >> 24) + to_unsigned(num_digits)), padding(0) {
    if (specs.align == align::numeric) {
      auto width = to_unsigned(specs.width);
      if (width > size) {
        padding = width - size;
        size = width;
      }
    } else if (specs.precision > num_digits) {
      size = (prefix >> 24) + to_unsigned(specs.precision);
      padding = to_unsigned(specs.precision - num_digits);
    }
  }
};

// Writes an integer in the format
//   <left-padding><prefix><numeric-padding><digits><right-padding>
// where <digits> are written by write_digits(it).
// prefix contains chars in three lower bytes and the size in the fourth byte.
template <typename OutputIt, typename Char, typename W>
FMT_CONSTEXPR FMT_INLINE auto write_int(OutputIt out, int num_digits,
                                        unsigned prefix,
                                        const format_specs<Char>& specs,
                                        W write_digits) -> OutputIt {
  // Slightly faster check for specs.width == 0 && specs.precision == -1.
  if ((specs.width | (specs.precision + 1)) == 0) {
    auto it = reserve(out, to_unsigned(num_digits) + (prefix >> 24));
    if (prefix != 0) {
      for (unsigned p = prefix & 0xffffff; p != 0; p >>= 8)
        *it++ = static_cast<Char>(p & 0xff);
    }
    return base_iterator(out, write_digits(it));
  }
  auto data = write_int_data<Char>(num_digits, prefix, specs);
  return write_padded<align::right>(
      out, specs, data.size, [=](reserve_iterator<OutputIt> it) {
        for (unsigned p = prefix & 0xffffff; p != 0; p >>= 8)
          *it++ = static_cast<Char>(p & 0xff);
        it = detail::fill_n(it, data.padding, static_cast<Char>('0'));
        return write_digits(it);
      });
}

template <typename Char> class digit_grouping {
 private:
  std::string grouping_;
  std::basic_string<Char> thousands_sep_;

  struct next_state {
    std::string::const_iterator group;
    int pos;
  };
  next_state initial_state() const { return {grouping_.begin(), 0}; }

  // Returns the next digit group separator position.
  int next(next_state& state) const {
    if (thousands_sep_.empty()) return max_value<int>();
    if (state.group == grouping_.end()) return state.pos += grouping_.back();
    if (*state.group <= 0 || *state.group == max_value<char>())
      return max_value<int>();
    state.pos += *state.group++;
    return state.pos;
  }

 public:
  explicit digit_grouping(locale_ref loc, bool localized = true) {
    if (!localized) return;
    auto sep = thousands_sep<Char>(loc);
    grouping_ = sep.grouping;
    if (sep.thousands_sep) thousands_sep_.assign(1, sep.thousands_sep);
  }
  digit_grouping(std::string grouping, std::basic_string<Char> sep)
      : grouping_(std::move(grouping)), thousands_sep_(std::move(sep)) {}

  bool has_separator() const { return !thousands_sep_.empty(); }

  int count_separators(int num_digits) const {
    int count = 0;
    auto state = initial_state();
    while (num_digits > next(state)) ++count;
    return count;
  }

  // Applies grouping to digits and write the output to out.
  template <typename Out, typename C>
  Out apply(Out out, basic_string_view<C> digits) const {
    auto num_digits = static_cast<int>(digits.size());
    auto separators = basic_memory_buffer<int>();
    separators.push_back(0);
    auto state = initial_state();
    while (int i = next(state)) {
      if (i >= num_digits) break;
      separators.push_back(i);
    }
    for (int i = 0, sep_index = static_cast<int>(separators.size() - 1);
         i < num_digits; ++i) {
      if (num_digits - i == separators[sep_index]) {
        out =
            copy_str<Char>(thousands_sep_.data(),
                           thousands_sep_.data() + thousands_sep_.size(), out);
        --sep_index;
      }
      *out++ = static_cast<Char>(digits[to_unsigned(i)]);
    }
    return out;
  }
};

// Writes a decimal integer with digit grouping.
template <typename OutputIt, typename UInt, typename Char>
auto write_int(OutputIt out, UInt value, unsigned prefix,
               const format_specs<Char>& specs,
               const digit_grouping<Char>& grouping) -> OutputIt {
  static_assert(std::is_same<uint64_or_128_t<UInt>, UInt>::value, "");
  int num_digits = count_digits(value);
  char digits[40];
  format_decimal(digits, value, num_digits);
  unsigned size = to_unsigned((prefix != 0 ? 1 : 0) + num_digits +
                              grouping.count_separators(num_digits));
  return write_padded<align::right>(
      out, specs, size, size, [&](reserve_iterator<OutputIt> it) {
        if (prefix != 0) {
          char sign = static_cast<char>(prefix);
          *it++ = static_cast<Char>(sign);
        }
        return grouping.apply(it, string_view(digits, to_unsigned(num_digits)));
      });
}

// Writes a localized value.
FMT_API auto write_loc(appender out, loc_value value,
                       const format_specs<>& specs, locale_ref loc) -> bool;
template <typename OutputIt, typename Char>
inline auto write_loc(OutputIt, loc_value, const format_specs<Char>&,
                      locale_ref) -> bool {
  return false;
}

FMT_CONSTEXPR inline void prefix_append(unsigned& prefix, unsigned value) {
  prefix |= prefix != 0 ? value << 8 : value;
  prefix += (1u + (value > 0xff ? 1 : 0)) << 24;
}

template <typename UInt> struct write_int_arg {
  UInt abs_value;
  unsigned prefix;
};

template <typename T>
FMT_CONSTEXPR auto make_write_int_arg(T value, sign_t sign)
    -> write_int_arg<uint32_or_64_or_128_t<T>> {
  auto prefix = 0u;
  auto abs_value = static_cast<uint32_or_64_or_128_t<T>>(value);
  if (is_negative(value)) {
    prefix = 0x01000000 | '-';
    abs_value = 0 - abs_value;
  } else {
    constexpr const unsigned prefixes[4] = {0, 0, 0x1000000u | '+',
                                            0x1000000u | ' '};
    prefix = prefixes[sign];
  }
  return {abs_value, prefix};
}

template <typename Char = char> struct loc_writer {
  buffer_appender<Char> out;
  const format_specs<Char>& specs;
  std::basic_string<Char> sep;
  std::string grouping;
  std::basic_string<Char> decimal_point;

  template <typename T, FMT_ENABLE_IF(is_integer<T>::value)>
  auto operator()(T value) -> bool {
    auto arg = make_write_int_arg(value, specs.sign);
    write_int(out, static_cast<uint64_or_128_t<T>>(arg.abs_value), arg.prefix,
              specs, digit_grouping<Char>(grouping, sep));
    return true;
  }

  template <typename T, FMT_ENABLE_IF(!is_integer<T>::value)>
  auto operator()(T) -> bool {
    return false;
  }
};

template <typename Char, typename OutputIt, typename T>
FMT_CONSTEXPR FMT_INLINE auto write_int(OutputIt out, write_int_arg<T> arg,
                                        const format_specs<Char>& specs,
                                        locale_ref) -> OutputIt {
  static_assert(std::is_same<T, uint32_or_64_or_128_t<T>>::value, "");
  auto abs_value = arg.abs_value;
  auto prefix = arg.prefix;
  switch (specs.type) {
  case presentation_type::none:
  case presentation_type::dec: {
    auto num_digits = count_digits(abs_value);
    return write_int(
        out, num_digits, prefix, specs, [=](reserve_iterator<OutputIt> it) {
          return format_decimal<Char>(it, abs_value, num_digits).end;
        });
  }
  case presentation_type::hex_lower:
  case presentation_type::hex_upper: {
    bool upper = specs.type == presentation_type::hex_upper;
    if (specs.alt)
      prefix_append(prefix, unsigned(upper ? 'X' : 'x') << 8 | '0');
    int num_digits = count_digits<4>(abs_value);
    return write_int(
        out, num_digits, prefix, specs, [=](reserve_iterator<OutputIt> it) {
          return format_uint<4, Char>(it, abs_value, num_digits, upper);
        });
  }
  case presentation_type::bin_lower:
  case presentation_type::bin_upper: {
    bool upper = specs.type == presentation_type::bin_upper;
    if (specs.alt)
      prefix_append(prefix, unsigned(upper ? 'B' : 'b') << 8 | '0');
    int num_digits = count_digits<1>(abs_value);
    return write_int(out, num_digits, prefix, specs,
                     [=](reserve_iterator<OutputIt> it) {
                       return format_uint<1, Char>(it, abs_value, num_digits);
                     });
  }
  case presentation_type::oct: {
    int num_digits = count_digits<3>(abs_value);
    // Octal prefix '0' is counted as a digit, so only add it if precision
    // is not greater than the number of digits.
    if (specs.alt && specs.precision <= num_digits && abs_value != 0)
      prefix_append(prefix, '0');
    return write_int(out, num_digits, prefix, specs,
                     [=](reserve_iterator<OutputIt> it) {
                       return format_uint<3, Char>(it, abs_value, num_digits);
                     });
  }
  case presentation_type::chr:
    return write_char(out, static_cast<Char>(abs_value), specs);
  default:
    throw_format_error("invalid format specifier");
  }
  return out;
}
template <typename Char, typename OutputIt, typename T>
FMT_CONSTEXPR FMT_NOINLINE auto write_int_noinline(
    OutputIt out, write_int_arg<T> arg, const format_specs<Char>& specs,
    locale_ref loc) -> OutputIt {
  return write_int(out, arg, specs, loc);
}
template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_integral<T>::value &&
                        !std::is_same<T, bool>::value &&
                        std::is_same<OutputIt, buffer_appender<Char>>::value)>
FMT_CONSTEXPR FMT_INLINE auto write(OutputIt out, T value,
                                    const format_specs<Char>& specs,
                                    locale_ref loc) -> OutputIt {
  if (specs.localized && write_loc(out, value, specs, loc)) return out;
  return write_int_noinline(out, make_write_int_arg(value, specs.sign), specs,
                            loc);
}
// An inlined version of write used in format string compilation.
template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_integral<T>::value &&
                        !std::is_same<T, bool>::value &&
                        !std::is_same<OutputIt, buffer_appender<Char>>::value)>
FMT_CONSTEXPR FMT_INLINE auto write(OutputIt out, T value,
                                    const format_specs<Char>& specs,
                                    locale_ref loc) -> OutputIt {
  if (specs.localized && write_loc(out, value, specs, loc)) return out;
  return write_int(out, make_write_int_arg(value, specs.sign), specs, loc);
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
  FMT_UNCHECKED_ITERATOR(counting_iterator);

  struct value_type {
    template <typename T> FMT_CONSTEXPR void operator=(const T&) {}
  };

  FMT_CONSTEXPR counting_iterator() : count_(0) {}

  FMT_CONSTEXPR size_t count() const { return count_; }

  FMT_CONSTEXPR counting_iterator& operator++() {
    ++count_;
    return *this;
  }
  FMT_CONSTEXPR counting_iterator operator++(int) {
    auto it = *this;
    ++*this;
    return it;
  }

  FMT_CONSTEXPR friend counting_iterator operator+(counting_iterator it,
                                                   difference_type n) {
    it.count_ += static_cast<size_t>(n);
    return it;
  }

  FMT_CONSTEXPR value_type operator*() const { return {}; }
};

template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, basic_string_view<Char> s,
                         const format_specs<Char>& specs) -> OutputIt {
  auto data = s.data();
  auto size = s.size();
  if (specs.precision >= 0 && to_unsigned(specs.precision) < size)
    size = code_point_index(s, to_unsigned(specs.precision));
  bool is_debug = specs.type == presentation_type::debug;
  size_t width = 0;
  if (specs.width != 0) {
    if (is_debug)
      width = write_escaped_string(counting_iterator{}, s).count();
    else
      width = compute_width(basic_string_view<Char>(data, size));
  }
  return write_padded(out, specs, size, width,
                      [=](reserve_iterator<OutputIt> it) {
                        if (is_debug) return write_escaped_string(it, s);
                        return copy_str<Char>(data, data + size, it);
                      });
}
template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out,
                         basic_string_view<type_identity_t<Char>> s,
                         const format_specs<Char>& specs, locale_ref)
    -> OutputIt {
  return write(out, s, specs);
}
template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, const Char* s,
                         const format_specs<Char>& specs, locale_ref)
    -> OutputIt {
  return specs.type != presentation_type::pointer
             ? write(out, basic_string_view<Char>(s), specs, {})
             : write_ptr<Char>(out, bit_cast<uintptr_t>(s), &specs);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_integral<T>::value &&
                        !std::is_same<T, bool>::value &&
                        !std::is_same<T, Char>::value)>
FMT_CONSTEXPR auto write(OutputIt out, T value) -> OutputIt {
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

// DEPRECATED!
template <typename Char>
FMT_CONSTEXPR auto parse_align(const Char* begin, const Char* end,
                               format_specs<Char>& specs) -> const Char* {
  FMT_ASSERT(begin != end, "");
  auto align = align::none;
  auto p = begin + code_point_length(begin);
  if (end - p <= 0) p = begin;
  for (;;) {
    switch (to_ascii(*p)) {
    case '<':
      align = align::left;
      break;
    case '>':
      align = align::right;
      break;
    case '^':
      align = align::center;
      break;
    }
    if (align != align::none) {
      if (p != begin) {
        auto c = *begin;
        if (c == '}') return begin;
        if (c == '{') {
          throw_format_error("invalid fill character '{'");
          return begin;
        }
        specs.fill = {begin, to_unsigned(p - begin)};
        begin = p + 1;
      } else {
        ++begin;
      }
      break;
    } else if (p == begin) {
      break;
    }
    p = begin;
  }
  specs.align = align;
  return begin;
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
  bool showpoint : 1;
};

template <typename ErrorHandler = error_handler, typename Char>
FMT_CONSTEXPR auto parse_float_type_spec(const format_specs<Char>& specs,
                                         ErrorHandler&& eh = {})
    -> float_specs {
  auto result = float_specs();
  result.showpoint = specs.alt;
  result.locale = specs.localized;
  switch (specs.type) {
  case presentation_type::none:
    result.format = float_format::general;
    break;
  case presentation_type::general_upper:
    result.upper = true;
    FMT_FALLTHROUGH;
  case presentation_type::general_lower:
    result.format = float_format::general;
    break;
  case presentation_type::exp_upper:
    result.upper = true;
    FMT_FALLTHROUGH;
  case presentation_type::exp_lower:
    result.format = float_format::exp;
    result.showpoint |= specs.precision != 0;
    break;
  case presentation_type::fixed_upper:
    result.upper = true;
    FMT_FALLTHROUGH;
  case presentation_type::fixed_lower:
    result.format = float_format::fixed;
    result.showpoint |= specs.precision != 0;
    break;
  case presentation_type::hexfloat_upper:
    result.upper = true;
    FMT_FALLTHROUGH;
  case presentation_type::hexfloat_lower:
    result.format = float_format::hex;
    break;
  default:
    eh.on_error("invalid format specifier");
    break;
  }
  return result;
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR20 auto write_nonfinite(OutputIt out, bool isnan,
                                     format_specs<Char> specs,
                                     const float_specs& fspecs) -> OutputIt {
  auto str =
      isnan ? (fspecs.upper ? "NAN" : "nan") : (fspecs.upper ? "INF" : "inf");
  constexpr size_t str_size = 3;
  auto sign = fspecs.sign;
  auto size = str_size + (sign ? 1 : 0);
  // Replace '0'-padding with space for non-finite values.
  const bool is_zero_fill =
      specs.fill.size() == 1 && *specs.fill.data() == static_cast<Char>('0');
  if (is_zero_fill) specs.fill[0] = static_cast<Char>(' ');
  return write_padded(out, specs, size, [=](reserve_iterator<OutputIt> it) {
    if (sign) *it++ = detail::sign<Char>(sign);
    return copy_str<Char>(str, str + str_size, it);
  });
}

// A decimal floating-point number significand * pow(10, exp).
struct big_decimal_fp {
  const char* significand;
  int significand_size;
  int exponent;
};

constexpr auto get_significand_size(const big_decimal_fp& f) -> int {
  return f.significand_size;
}
template <typename T>
inline auto get_significand_size(const dragonbox::decimal_fp<T>& f) -> int {
  return count_digits(f.significand);
}

template <typename Char, typename OutputIt>
constexpr auto write_significand(OutputIt out, const char* significand,
                                 int significand_size) -> OutputIt {
  return copy_str<Char>(significand, significand + significand_size, out);
}
template <typename Char, typename OutputIt, typename UInt>
inline auto write_significand(OutputIt out, UInt significand,
                              int significand_size) -> OutputIt {
  return format_decimal<Char>(out, significand, significand_size).end;
}
template <typename Char, typename OutputIt, typename T, typename Grouping>
FMT_CONSTEXPR20 auto write_significand(OutputIt out, T significand,
                                       int significand_size, int exponent,
                                       const Grouping& grouping) -> OutputIt {
  if (!grouping.has_separator()) {
    out = write_significand<Char>(out, significand, significand_size);
    return detail::fill_n(out, exponent, static_cast<Char>('0'));
  }
  auto buffer = memory_buffer();
  write_significand<char>(appender(buffer), significand, significand_size);
  detail::fill_n(appender(buffer), exponent, '0');
  return grouping.apply(out, string_view(buffer.data(), buffer.size()));
}

template <typename Char, typename UInt,
          FMT_ENABLE_IF(std::is_integral<UInt>::value)>
inline auto write_significand(Char* out, UInt significand, int significand_size,
                              int integral_size, Char decimal_point) -> Char* {
  if (!decimal_point)
    return format_decimal(out, significand, significand_size).end;
  out += significand_size + 1;
  Char* end = out;
  int floating_size = significand_size - integral_size;
  for (int i = floating_size / 2; i > 0; --i) {
    out -= 2;
    copy2(out, digits2(static_cast<std::size_t>(significand % 100)));
    significand /= 100;
  }
  if (floating_size % 2 != 0) {
    *--out = static_cast<Char>('0' + significand % 10);
    significand /= 10;
  }
  *--out = decimal_point;
  format_decimal(out - integral_size, significand, integral_size);
  return end;
}

template <typename OutputIt, typename UInt, typename Char,
          FMT_ENABLE_IF(!std::is_pointer<remove_cvref_t<OutputIt>>::value)>
inline auto write_significand(OutputIt out, UInt significand,
                              int significand_size, int integral_size,
                              Char decimal_point) -> OutputIt {
  // Buffer is large enough to hold digits (digits10 + 1) and a decimal point.
  Char buffer[digits10<UInt>() + 2];
  auto end = write_significand(buffer, significand, significand_size,
                               integral_size, decimal_point);
  return detail::copy_str_noinline<Char>(buffer, end, out);
}

template <typename OutputIt, typename Char>
FMT_CONSTEXPR auto write_significand(OutputIt out, const char* significand,
                                     int significand_size, int integral_size,
                                     Char decimal_point) -> OutputIt {
  out = detail::copy_str_noinline<Char>(significand,
                                        significand + integral_size, out);
  if (!decimal_point) return out;
  *out++ = decimal_point;
  return detail::copy_str_noinline<Char>(significand + integral_size,
                                         significand + significand_size, out);
}

template <typename OutputIt, typename Char, typename T, typename Grouping>
FMT_CONSTEXPR20 auto write_significand(OutputIt out, T significand,
                                       int significand_size, int integral_size,
                                       Char decimal_point,
                                       const Grouping& grouping) -> OutputIt {
  if (!grouping.has_separator()) {
    return write_significand(out, significand, significand_size, integral_size,
                             decimal_point);
  }
  auto buffer = basic_memory_buffer<Char>();
  write_significand(buffer_appender<Char>(buffer), significand,
                    significand_size, integral_size, decimal_point);
  grouping.apply(
      out, basic_string_view<Char>(buffer.data(), to_unsigned(integral_size)));
  return detail::copy_str_noinline<Char>(buffer.data() + integral_size,
                                         buffer.end(), out);
}

template <typename OutputIt, typename DecimalFP, typename Char,
          typename Grouping = digit_grouping<Char>>
FMT_CONSTEXPR20 auto do_write_float(OutputIt out, const DecimalFP& f,
                                    const format_specs<Char>& specs,
                                    float_specs fspecs, locale_ref loc)
    -> OutputIt {
  auto significand = f.significand;
  int significand_size = get_significand_size(f);
  const Char zero = static_cast<Char>('0');
  auto sign = fspecs.sign;
  size_t size = to_unsigned(significand_size) + (sign ? 1 : 0);
  using iterator = reserve_iterator<OutputIt>;

  Char decimal_point =
      fspecs.locale ? detail::decimal_point<Char>(loc) : static_cast<Char>('.');

  int output_exp = f.exponent + significand_size - 1;
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
      num_zeros = fspecs.precision - significand_size;
      if (num_zeros < 0) num_zeros = 0;
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
      if (sign) *it++ = detail::sign<Char>(sign);
      // Insert a decimal point after the first digit and add an exponent.
      it = write_significand(it, significand, significand_size, 1,
                             decimal_point);
      if (num_zeros > 0) it = detail::fill_n(it, num_zeros, zero);
      *it++ = static_cast<Char>(exp_char);
      return write_exponent<Char>(output_exp, it);
    };
    return specs.width > 0 ? write_padded<align::right>(out, specs, size, write)
                           : base_iterator(out, write(reserve(out, size)));
  }

  int exp = f.exponent + significand_size;
  if (f.exponent >= 0) {
    // 1234e5 -> 123400000[.0+]
    size += to_unsigned(f.exponent);
    int num_zeros = fspecs.precision - exp;
    abort_fuzzing_if(num_zeros > 5000);
    if (fspecs.showpoint) {
      ++size;
      if (num_zeros <= 0 && fspecs.format != float_format::fixed) num_zeros = 0;
      if (num_zeros > 0) size += to_unsigned(num_zeros);
    }
    auto grouping = Grouping(loc, fspecs.locale);
    size += to_unsigned(grouping.count_separators(exp));
    return write_padded<align::right>(out, specs, size, [&](iterator it) {
      if (sign) *it++ = detail::sign<Char>(sign);
      it = write_significand<Char>(it, significand, significand_size,
                                   f.exponent, grouping);
      if (!fspecs.showpoint) return it;
      *it++ = decimal_point;
      return num_zeros > 0 ? detail::fill_n(it, num_zeros, zero) : it;
    });
  } else if (exp > 0) {
    // 1234e-2 -> 12.34[0+]
    int num_zeros = fspecs.showpoint ? fspecs.precision - significand_size : 0;
    size += 1 + to_unsigned(num_zeros > 0 ? num_zeros : 0);
    auto grouping = Grouping(loc, fspecs.locale);
    size += to_unsigned(grouping.count_separators(exp));
    return write_padded<align::right>(out, specs, size, [&](iterator it) {
      if (sign) *it++ = detail::sign<Char>(sign);
      it = write_significand(it, significand, significand_size, exp,
                             decimal_point, grouping);
      return num_zeros > 0 ? detail::fill_n(it, num_zeros, zero) : it;
    });
  }
  // 1234e-6 -> 0.001234
  int num_zeros = -exp;
  if (significand_size == 0 && fspecs.precision >= 0 &&
      fspecs.precision < num_zeros) {
    num_zeros = fspecs.precision;
  }
  bool pointy = num_zeros != 0 || significand_size != 0 || fspecs.showpoint;
  size += 1 + (pointy ? 1 : 0) + to_unsigned(num_zeros);
  return write_padded<align::right>(out, specs, size, [&](iterator it) {
    if (sign) *it++ = detail::sign<Char>(sign);
    *it++ = zero;
    if (!pointy) return it;
    *it++ = decimal_point;
    it = detail::fill_n(it, num_zeros, zero);
    return write_significand<Char>(it, significand, significand_size);
  });
}

template <typename Char> class fallback_digit_grouping {
 public:
  constexpr fallback_digit_grouping(locale_ref, bool) {}

  constexpr bool has_separator() const { return false; }

  constexpr int count_separators(int) const { return 0; }

  template <typename Out, typename C>
  constexpr Out apply(Out out, basic_string_view<C>) const {
    return out;
  }
};

template <typename OutputIt, typename DecimalFP, typename Char>
FMT_CONSTEXPR20 auto write_float(OutputIt out, const DecimalFP& f,
                                 const format_specs<Char>& specs,
                                 float_specs fspecs, locale_ref loc)
    -> OutputIt {
  if (is_constant_evaluated()) {
    return do_write_float<OutputIt, DecimalFP, Char,
                          fallback_digit_grouping<Char>>(out, f, specs, fspecs,
                                                         loc);
  } else {
    return do_write_float(out, f, specs, fspecs, loc);
  }
}

template <typename T> constexpr bool isnan(T value) {
  return !(value >= value);  // std::isnan doesn't support __float128.
}

template <typename T, typename Enable = void>
struct has_isfinite : std::false_type {};

template <typename T>
struct has_isfinite<T, enable_if_t<sizeof(std::isfinite(T())) != 0>>
    : std::true_type {};

template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value&&
                                        has_isfinite<T>::value)>
FMT_CONSTEXPR20 bool isfinite(T value) {
  constexpr T inf = T(std::numeric_limits<double>::infinity());
  if (is_constant_evaluated())
    return !detail::isnan(value) && value < inf && value > -inf;
  return std::isfinite(value);
}
template <typename T, FMT_ENABLE_IF(!has_isfinite<T>::value)>
FMT_CONSTEXPR bool isfinite(T value) {
  T inf = T(std::numeric_limits<double>::infinity());
  // std::isfinite doesn't support __float128.
  return !detail::isnan(value) && value < inf && value > -inf;
}

template <typename T, FMT_ENABLE_IF(is_floating_point<T>::value)>
FMT_INLINE FMT_CONSTEXPR bool signbit(T value) {
  if (is_constant_evaluated()) {
#ifdef __cpp_if_constexpr
    if constexpr (std::numeric_limits<double>::is_iec559) {
      auto bits = detail::bit_cast<uint64_t>(static_cast<double>(value));
      return (bits >> (num_bits<uint64_t>() - 1)) != 0;
    }
#endif
  }
  return std::signbit(static_cast<double>(value));
}

inline FMT_CONSTEXPR20 void adjust_precision(int& precision, int exp10) {
  // Adjust fixed precision by exponent because it is relative to decimal
  // point.
  if (exp10 > 0 && precision > max_value<int>() - exp10)
    FMT_THROW(format_error("number is too big"));
  precision += exp10;
}

class bigint {
 private:
  // A bigint is stored as an array of bigits (big digits), with bigit at index
  // 0 being the least significant one.
  using bigit = uint32_t;
  using double_bigit = uint64_t;
  enum { bigits_capacity = 32 };
  basic_memory_buffer<bigit, bigits_capacity> bigits_;
  int exp_;

  FMT_CONSTEXPR20 bigit operator[](int index) const {
    return bigits_[to_unsigned(index)];
  }
  FMT_CONSTEXPR20 bigit& operator[](int index) {
    return bigits_[to_unsigned(index)];
  }

  static constexpr const int bigit_bits = num_bits<bigit>();

  friend struct formatter<bigint>;

  FMT_CONSTEXPR20 void subtract_bigits(int index, bigit other, bigit& borrow) {
    auto result = static_cast<double_bigit>((*this)[index]) - other - borrow;
    (*this)[index] = static_cast<bigit>(result);
    borrow = static_cast<bigit>(result >> (bigit_bits * 2 - 1));
  }

  FMT_CONSTEXPR20 void remove_leading_zeros() {
    int num_bigits = static_cast<int>(bigits_.size()) - 1;
    while (num_bigits > 0 && (*this)[num_bigits] == 0) --num_bigits;
    bigits_.resize(to_unsigned(num_bigits + 1));
  }

  // Computes *this -= other assuming aligned bigints and *this >= other.
  FMT_CONSTEXPR20 void subtract_aligned(const bigint& other) {
    FMT_ASSERT(other.exp_ >= exp_, "unaligned bigints");
    FMT_ASSERT(compare(*this, other) >= 0, "");
    bigit borrow = 0;
    int i = other.exp_ - exp_;
    for (size_t j = 0, n = other.bigits_.size(); j != n; ++i, ++j)
      subtract_bigits(i, other.bigits_[j], borrow);
    while (borrow > 0) subtract_bigits(i, 0, borrow);
    remove_leading_zeros();
  }

  FMT_CONSTEXPR20 void multiply(uint32_t value) {
    const double_bigit wide_value = value;
    bigit carry = 0;
    for (size_t i = 0, n = bigits_.size(); i < n; ++i) {
      double_bigit result = bigits_[i] * wide_value + carry;
      bigits_[i] = static_cast<bigit>(result);
      carry = static_cast<bigit>(result >> bigit_bits);
    }
    if (carry != 0) bigits_.push_back(carry);
  }

  template <typename UInt, FMT_ENABLE_IF(std::is_same<UInt, uint64_t>::value ||
                                         std::is_same<UInt, uint128_t>::value)>
  FMT_CONSTEXPR20 void multiply(UInt value) {
    using half_uint =
        conditional_t<std::is_same<UInt, uint128_t>::value, uint64_t, uint32_t>;
    const int shift = num_bits<half_uint>() - bigit_bits;
    const UInt lower = static_cast<half_uint>(value);
    const UInt upper = value >> num_bits<half_uint>();
    UInt carry = 0;
    for (size_t i = 0, n = bigits_.size(); i < n; ++i) {
      UInt result = lower * bigits_[i] + static_cast<bigit>(carry);
      carry = (upper * bigits_[i] << shift) + (result >> bigit_bits) +
              (carry >> bigit_bits);
      bigits_[i] = static_cast<bigit>(result);
    }
    while (carry != 0) {
      bigits_.push_back(static_cast<bigit>(carry));
      carry >>= bigit_bits;
    }
  }

  template <typename UInt, FMT_ENABLE_IF(std::is_same<UInt, uint64_t>::value ||
                                         std::is_same<UInt, uint128_t>::value)>
  FMT_CONSTEXPR20 void assign(UInt n) {
    size_t num_bigits = 0;
    do {
      bigits_[num_bigits++] = static_cast<bigit>(n);
      n >>= bigit_bits;
    } while (n != 0);
    bigits_.resize(num_bigits);
    exp_ = 0;
  }

 public:
  FMT_CONSTEXPR20 bigint() : exp_(0) {}
  explicit bigint(uint64_t n) { assign(n); }

  bigint(const bigint&) = delete;
  void operator=(const bigint&) = delete;

  FMT_CONSTEXPR20 void assign(const bigint& other) {
    auto size = other.bigits_.size();
    bigits_.resize(size);
    auto data = other.bigits_.data();
    copy_str<bigit>(data, data + size, bigits_.data());
    exp_ = other.exp_;
  }

  template <typename Int> FMT_CONSTEXPR20 void operator=(Int n) {
    FMT_ASSERT(n > 0, "");
    assign(uint64_or_128_t<Int>(n));
  }

  FMT_CONSTEXPR20 int num_bigits() const {
    return static_cast<int>(bigits_.size()) + exp_;
  }

  FMT_NOINLINE FMT_CONSTEXPR20 bigint& operator<<=(int shift) {
    FMT_ASSERT(shift >= 0, "");
    exp_ += shift / bigit_bits;
    shift %= bigit_bits;
    if (shift == 0) return *this;
    bigit carry = 0;
    for (size_t i = 0, n = bigits_.size(); i < n; ++i) {
      bigit c = bigits_[i] >> (bigit_bits - shift);
      bigits_[i] = (bigits_[i] << shift) + carry;
      carry = c;
    }
    if (carry != 0) bigits_.push_back(carry);
    return *this;
  }

  template <typename Int> FMT_CONSTEXPR20 bigint& operator*=(Int value) {
    FMT_ASSERT(value > 0, "");
    multiply(uint32_or_64_or_128_t<Int>(value));
    return *this;
  }

  friend FMT_CONSTEXPR20 int compare(const bigint& lhs, const bigint& rhs) {
    int num_lhs_bigits = lhs.num_bigits(), num_rhs_bigits = rhs.num_bigits();
    if (num_lhs_bigits != num_rhs_bigits)
      return num_lhs_bigits > num_rhs_bigits ? 1 : -1;
    int i = static_cast<int>(lhs.bigits_.size()) - 1;
    int j = static_cast<int>(rhs.bigits_.size()) - 1;
    int end = i - j;
    if (end < 0) end = 0;
    for (; i >= end; --i, --j) {
      bigit lhs_bigit = lhs[i], rhs_bigit = rhs[j];
      if (lhs_bigit != rhs_bigit) return lhs_bigit > rhs_bigit ? 1 : -1;
    }
    if (i != j) return i > j ? 1 : -1;
    return 0;
  }

  // Returns compare(lhs1 + lhs2, rhs).
  friend FMT_CONSTEXPR20 int add_compare(const bigint& lhs1, const bigint& lhs2,
                                         const bigint& rhs) {
    auto minimum = [](int a, int b) { return a < b ? a : b; };
    auto maximum = [](int a, int b) { return a > b ? a : b; };
    int max_lhs_bigits = maximum(lhs1.num_bigits(), lhs2.num_bigits());
    int num_rhs_bigits = rhs.num_bigits();
    if (max_lhs_bigits + 1 < num_rhs_bigits) return -1;
    if (max_lhs_bigits > num_rhs_bigits) return 1;
    auto get_bigit = [](const bigint& n, int i) -> bigit {
      return i >= n.exp_ && i < n.num_bigits() ? n[i - n.exp_] : 0;
    };
    double_bigit borrow = 0;
    int min_exp = minimum(minimum(lhs1.exp_, lhs2.exp_), rhs.exp_);
    for (int i = num_rhs_bigits - 1; i >= min_exp; --i) {
      double_bigit sum =
          static_cast<double_bigit>(get_bigit(lhs1, i)) + get_bigit(lhs2, i);
      bigit rhs_bigit = get_bigit(rhs, i);
      if (sum > rhs_bigit + borrow) return 1;
      borrow = rhs_bigit + borrow - sum;
      if (borrow > 1) return -1;
      borrow <<= bigit_bits;
    }
    return borrow != 0 ? -1 : 0;
  }

  // Assigns pow(10, exp) to this bigint.
  FMT_CONSTEXPR20 void assign_pow10(int exp) {
    FMT_ASSERT(exp >= 0, "");
    if (exp == 0) return *this = 1;
    // Find the top bit.
    int bitmask = 1;
    while (exp >= bitmask) bitmask <<= 1;
    bitmask >>= 1;
    // pow(10, exp) = pow(5, exp) * pow(2, exp). First compute pow(5, exp) by
    // repeated squaring and multiplication.
    *this = 5;
    bitmask >>= 1;
    while (bitmask != 0) {
      square();
      if ((exp & bitmask) != 0) *this *= 5;
      bitmask >>= 1;
    }
    *this <<= exp;  // Multiply by pow(2, exp) by shifting.
  }

  FMT_CONSTEXPR20 void square() {
    int num_bigits = static_cast<int>(bigits_.size());
    int num_result_bigits = 2 * num_bigits;
    basic_memory_buffer<bigit, bigits_capacity> n(std::move(bigits_));
    bigits_.resize(to_unsigned(num_result_bigits));
    auto sum = uint128_t();
    for (int bigit_index = 0; bigit_index < num_bigits; ++bigit_index) {
      // Compute bigit at position bigit_index of the result by adding
      // cross-product terms n[i] * n[j] such that i + j == bigit_index.
      for (int i = 0, j = bigit_index; j >= 0; ++i, --j) {
        // Most terms are multiplied twice which can be optimized in the future.
        sum += static_cast<double_bigit>(n[i]) * n[j];
      }
      (*this)[bigit_index] = static_cast<bigit>(sum);
      sum >>= num_bits<bigit>();  // Compute the carry.
    }
    // Do the same for the top half.
    for (int bigit_index = num_bigits; bigit_index < num_result_bigits;
         ++bigit_index) {
      for (int j = num_bigits - 1, i = bigit_index - j; i < num_bigits;)
        sum += static_cast<double_bigit>(n[i++]) * n[j--];
      (*this)[bigit_index] = static_cast<bigit>(sum);
      sum >>= num_bits<bigit>();
    }
    remove_leading_zeros();
    exp_ *= 2;
  }

  // If this bigint has a bigger exponent than other, adds trailing zero to make
  // exponents equal. This simplifies some operations such as subtraction.
  FMT_CONSTEXPR20 void align(const bigint& other) {
    int exp_difference = exp_ - other.exp_;
    if (exp_difference <= 0) return;
    int num_bigits = static_cast<int>(bigits_.size());
    bigits_.resize(to_unsigned(num_bigits + exp_difference));
    for (int i = num_bigits - 1, j = i + exp_difference; i >= 0; --i, --j)
      bigits_[j] = bigits_[i];
    std::uninitialized_fill_n(bigits_.data(), exp_difference, 0);
    exp_ -= exp_difference;
  }

  // Divides this bignum by divisor, assigning the remainder to this and
  // returning the quotient.
  FMT_CONSTEXPR20 int divmod_assign(const bigint& divisor) {
    FMT_ASSERT(this != &divisor, "");
    if (compare(*this, divisor) < 0) return 0;
    FMT_ASSERT(divisor.bigits_[divisor.bigits_.size() - 1u] != 0, "");
    align(divisor);
    int quotient = 0;
    do {
      subtract_aligned(divisor);
      ++quotient;
    } while (compare(*this, divisor) >= 0);
    return quotient;
  }
};

// format_dragon flags.
enum dragon {
  predecessor_closer = 1,
  fixup = 2,  // Run fixup to correct exp10 which can be off by one.
  fixed = 4,
};

// Formats a floating-point number using a variation of the Fixed-Precision
// Positive Floating-Point Printout ((FPP)^2) algorithm by Steele & White:
// https://fmt.dev/papers/p372-steele.pdf.
FMT_CONSTEXPR20 inline void format_dragon(basic_fp<uint128_t> value,
                                          unsigned flags, int num_digits,
                                          buffer<char>& buf, int& exp10) {
  bigint numerator;    // 2 * R in (FPP)^2.
  bigint denominator;  // 2 * S in (FPP)^2.
  // lower and upper are differences between value and corresponding boundaries.
  bigint lower;             // (M^- in (FPP)^2).
  bigint upper_store;       // upper's value if different from lower.
  bigint* upper = nullptr;  // (M^+ in (FPP)^2).
  // Shift numerator and denominator by an extra bit or two (if lower boundary
  // is closer) to make lower and upper integers. This eliminates multiplication
  // by 2 during later computations.
  bool is_predecessor_closer = (flags & dragon::predecessor_closer) != 0;
  int shift = is_predecessor_closer ? 2 : 1;
  if (value.e >= 0) {
    numerator = value.f;
    numerator <<= value.e + shift;
    lower = 1;
    lower <<= value.e;
    if (is_predecessor_closer) {
      upper_store = 1;
      upper_store <<= value.e + 1;
      upper = &upper_store;
    }
    denominator.assign_pow10(exp10);
    denominator <<= shift;
  } else if (exp10 < 0) {
    numerator.assign_pow10(-exp10);
    lower.assign(numerator);
    if (is_predecessor_closer) {
      upper_store.assign(numerator);
      upper_store <<= 1;
      upper = &upper_store;
    }
    numerator *= value.f;
    numerator <<= shift;
    denominator = 1;
    denominator <<= shift - value.e;
  } else {
    numerator = value.f;
    numerator <<= shift;
    denominator.assign_pow10(exp10);
    denominator <<= shift - value.e;
    lower = 1;
    if (is_predecessor_closer) {
      upper_store = 1ULL << 1;
      upper = &upper_store;
    }
  }
  int even = static_cast<int>((value.f & 1) == 0);
  if (!upper) upper = &lower;
  bool shortest = num_digits < 0;
  if ((flags & dragon::fixup) != 0) {
    if (add_compare(numerator, *upper, denominator) + even <= 0) {
      --exp10;
      numerator *= 10;
      if (num_digits < 0) {
        lower *= 10;
        if (upper != &lower) *upper *= 10;
      }
    }
    if ((flags & dragon::fixed) != 0) adjust_precision(num_digits, exp10 + 1);
  }
  // Invariant: value == (numerator / denominator) * pow(10, exp10).
  if (shortest) {
    // Generate the shortest representation.
    num_digits = 0;
    char* data = buf.data();
    for (;;) {
      int digit = numerator.divmod_assign(denominator);
      bool low = compare(numerator, lower) - even < 0;  // numerator <[=] lower.
      // numerator + upper >[=] pow10:
      bool high = add_compare(numerator, *upper, denominator) + even > 0;
      data[num_digits++] = static_cast<char>('0' + digit);
      if (low || high) {
        if (!low) {
          ++data[num_digits - 1];
        } else if (high) {
          int result = add_compare(numerator, numerator, denominator);
          // Round half to even.
          if (result > 0 || (result == 0 && (digit % 2) != 0))
            ++data[num_digits - 1];
        }
        buf.try_resize(to_unsigned(num_digits));
        exp10 -= num_digits - 1;
        return;
      }
      numerator *= 10;
      lower *= 10;
      if (upper != &lower) *upper *= 10;
    }
  }
  // Generate the given number of digits.
  exp10 -= num_digits - 1;
  if (num_digits <= 0) {
    denominator *= 10;
    auto digit = add_compare(numerator, numerator, denominator) > 0 ? '1' : '0';
    buf.push_back(digit);
    return;
  }
  buf.try_resize(to_unsigned(num_digits));
  for (int i = 0; i < num_digits - 1; ++i) {
    int digit = numerator.divmod_assign(denominator);
    buf[i] = static_cast<char>('0' + digit);
    numerator *= 10;
  }
  int digit = numerator.divmod_assign(denominator);
  auto result = add_compare(numerator, numerator, denominator);
  if (result > 0 || (result == 0 && (digit % 2) != 0)) {
    if (digit == 9) {
      const auto overflow = '0' + 10;
      buf[num_digits - 1] = overflow;
      // Propagate the carry.
      for (int i = num_digits - 1; i > 0 && buf[i] == overflow; --i) {
        buf[i] = '0';
        ++buf[i - 1];
      }
      if (buf[0] == overflow) {
        buf[0] = '1';
        if ((flags & dragon::fixed) != 0) buf.push_back('0');
        else ++exp10;
      }
      return;
    }
    ++digit;
  }
  buf[num_digits - 1] = static_cast<char>('0' + digit);
}

// Formats a floating-point number using the hexfloat format.
template <typename Float, FMT_ENABLE_IF(!is_double_double<Float>::value)>
FMT_CONSTEXPR20 void format_hexfloat(Float value, int precision,
                                     float_specs specs, buffer<char>& buf) {
  // float is passed as double to reduce the number of instantiations and to
  // simplify implementation.
  static_assert(!std::is_same<Float, float>::value, "");

  using info = dragonbox::float_info<Float>;

  // Assume Float is in the format [sign][exponent][significand].
  using carrier_uint = typename info::carrier_uint;

  constexpr auto num_float_significand_bits =
      detail::num_significand_bits<Float>();

  basic_fp<carrier_uint> f(value);
  f.e += num_float_significand_bits;
  if (!has_implicit_bit<Float>()) --f.e;

  constexpr auto num_fraction_bits =
      num_float_significand_bits + (has_implicit_bit<Float>() ? 1 : 0);
  constexpr auto num_xdigits = (num_fraction_bits + 3) / 4;

  constexpr auto leading_shift = ((num_xdigits - 1) * 4);
  const auto leading_mask = carrier_uint(0xF) << leading_shift;
  const auto leading_xdigit =
      static_cast<uint32_t>((f.f & leading_mask) >> leading_shift);
  if (leading_xdigit > 1) f.e -= (32 - countl_zero(leading_xdigit) - 1);

  int print_xdigits = num_xdigits - 1;
  if (precision >= 0 && print_xdigits > precision) {
    const int shift = ((print_xdigits - precision - 1) * 4);
    const auto mask = carrier_uint(0xF) << shift;
    const auto v = static_cast<uint32_t>((f.f & mask) >> shift);

    if (v >= 8) {
      const auto inc = carrier_uint(1) << (shift + 4);
      f.f += inc;
      f.f &= ~(inc - 1);
    }

    // Check long double overflow
    if (!has_implicit_bit<Float>()) {
      const auto implicit_bit = carrier_uint(1) << num_float_significand_bits;
      if ((f.f & implicit_bit) == implicit_bit) {
        f.f >>= 4;
        f.e += 4;
      }
    }

    print_xdigits = precision;
  }

  char xdigits[num_bits<carrier_uint>() / 4];
  detail::fill_n(xdigits, sizeof(xdigits), '0');
  format_uint<4>(xdigits, f.f, num_xdigits, specs.upper);

  // Remove zero tail
  while (print_xdigits > 0 && xdigits[print_xdigits] == '0') --print_xdigits;

  buf.push_back('0');
  buf.push_back(specs.upper ? 'X' : 'x');
  buf.push_back(xdigits[0]);
  if (specs.showpoint || print_xdigits > 0 || print_xdigits < precision)
    buf.push_back('.');
  buf.append(xdigits + 1, xdigits + 1 + print_xdigits);
  for (; print_xdigits < precision; ++print_xdigits) buf.push_back('0');

  buf.push_back(specs.upper ? 'P' : 'p');

  uint32_t abs_e;
  if (f.e < 0) {
    buf.push_back('-');
    abs_e = static_cast<uint32_t>(-f.e);
  } else {
    buf.push_back('+');
    abs_e = static_cast<uint32_t>(f.e);
  }
  format_decimal<char>(appender(buf), abs_e, detail::count_digits(abs_e));
}

template <typename Float, FMT_ENABLE_IF(is_double_double<Float>::value)>
FMT_CONSTEXPR20 void format_hexfloat(Float value, int precision,
                                     float_specs specs, buffer<char>& buf) {
  format_hexfloat(static_cast<double>(value), precision, specs, buf);
}

template <typename Float>
FMT_CONSTEXPR20 auto format_float(Float value, int precision, float_specs specs,
                                  buffer<char>& buf) -> int {
  // float is passed as double to reduce the number of instantiations.
  static_assert(!std::is_same<Float, float>::value, "");
  FMT_ASSERT(value >= 0, "value is negative");
  auto converted_value = convert_float(value);

  const bool fixed = specs.format == float_format::fixed;
  if (value <= 0) {  // <= instead of == to silence a warning.
    if (precision <= 0 || !fixed) {
      buf.push_back('0');
      return 0;
    }
    buf.try_resize(to_unsigned(precision));
    fill_n(buf.data(), precision, '0');
    return -precision;
  }

  int exp = 0;
  bool use_dragon = true;
  unsigned dragon_flags = 0;
  if (!is_fast_float<Float>() || is_constant_evaluated()) {
    const auto inv_log2_10 = 0.3010299956639812;  // 1 / log2(10)
    using info = dragonbox::float_info<decltype(converted_value)>;
    const auto f = basic_fp<typename info::carrier_uint>(converted_value);
    // Compute exp, an approximate power of 10, such that
    //   10^(exp - 1) <= value < 10^exp or 10^exp <= value < 10^(exp + 1).
    // This is based on log10(value) == log2(value) / log2(10) and approximation
    // of log2(value) by e + num_fraction_bits idea from double-conversion.
    auto e = (f.e + count_digits<1>(f.f) - 1) * inv_log2_10 - 1e-10;
    exp = static_cast<int>(e);
    if (e > exp) ++exp;  // Compute ceil.
    dragon_flags = dragon::fixup;
  } else if (precision < 0) {
    // Use Dragonbox for the shortest format.
    if (specs.binary32) {
      auto dec = dragonbox::to_decimal(static_cast<float>(value));
      write<char>(buffer_appender<char>(buf), dec.significand);
      return dec.exponent;
    }
    auto dec = dragonbox::to_decimal(static_cast<double>(value));
    write<char>(buffer_appender<char>(buf), dec.significand);
    return dec.exponent;
  } else {
    // Extract significand bits and exponent bits.
    using info = dragonbox::float_info<double>;
    auto br = bit_cast<uint64_t>(static_cast<double>(value));

    const uint64_t significand_mask =
        (static_cast<uint64_t>(1) << num_significand_bits<double>()) - 1;
    uint64_t significand = (br & significand_mask);
    int exponent = static_cast<int>((br & exponent_mask<double>()) >>
                                    num_significand_bits<double>());

    if (exponent != 0) {  // Check if normal.
      exponent -= exponent_bias<double>() + num_significand_bits<double>();
      significand |=
          (static_cast<uint64_t>(1) << num_significand_bits<double>());
      significand <<= 1;
    } else {
      // Normalize subnormal inputs.
      FMT_ASSERT(significand != 0, "zeros should not appear here");
      int shift = countl_zero(significand);
      FMT_ASSERT(shift >= num_bits<uint64_t>() - num_significand_bits<double>(),
                 "");
      shift -= (num_bits<uint64_t>() - num_significand_bits<double>() - 2);
      exponent = (std::numeric_limits<double>::min_exponent -
                  num_significand_bits<double>()) -
                 shift;
      significand <<= shift;
    }

    // Compute the first several nonzero decimal significand digits.
    // We call the number we get the first segment.
    const int k = info::kappa - dragonbox::floor_log10_pow2(exponent);
    exp = -k;
    const int beta = exponent + dragonbox::floor_log2_pow10(k);
    uint64_t first_segment;
    bool has_more_segments;
    int digits_in_the_first_segment;
    {
      const auto r = dragonbox::umul192_upper128(
          significand << beta, dragonbox::get_cached_power(k));
      first_segment = r.high();
      has_more_segments = r.low() != 0;

      // The first segment can have 18 ~ 19 digits.
      if (first_segment >= 1000000000000000000ULL) {
        digits_in_the_first_segment = 19;
      } else {
        // When it is of 18-digits, we align it to 19-digits by adding a bogus
        // zero at the end.
        digits_in_the_first_segment = 18;
        first_segment *= 10;
      }
    }

    // Compute the actual number of decimal digits to print.
    if (fixed) adjust_precision(precision, exp + digits_in_the_first_segment);

    // Use Dragon4 only when there might be not enough digits in the first
    // segment.
    if (digits_in_the_first_segment > precision) {
      use_dragon = false;

      if (precision <= 0) {
        exp += digits_in_the_first_segment;

        if (precision < 0) {
          // Nothing to do, since all we have are just leading zeros.
          buf.try_resize(0);
        } else {
          // We may need to round-up.
          buf.try_resize(1);
          if ((first_segment | static_cast<uint64_t>(has_more_segments)) >
              5000000000000000000ULL) {
            buf[0] = '1';
          } else {
            buf[0] = '0';
          }
        }
      }  // precision <= 0
      else {
        exp += digits_in_the_first_segment - precision;

        // When precision > 0, we divide the first segment into three
        // subsegments, each with 9, 9, and 0 ~ 1 digits so that each fits
        // in 32-bits which usually allows faster calculation than in
        // 64-bits. Since some compiler (e.g. MSVC) doesn't know how to optimize
        // division-by-constant for large 64-bit divisors, we do it here
        // manually. The magic number 7922816251426433760 below is equal to
        // ceil(2^(64+32) / 10^10).
        const uint32_t first_subsegment = static_cast<uint32_t>(
            dragonbox::umul128_upper64(first_segment, 7922816251426433760ULL) >>
            32);
        const uint64_t second_third_subsegments =
            first_segment - first_subsegment * 10000000000ULL;

        uint64_t prod;
        uint32_t digits;
        bool should_round_up;
        int number_of_digits_to_print = precision > 9 ? 9 : precision;

        // Print a 9-digits subsegment, either the first or the second.
        auto print_subsegment = [&](uint32_t subsegment, char* buffer) {
          int number_of_digits_printed = 0;

          // If we want to print an odd number of digits from the subsegment,
          if ((number_of_digits_to_print & 1) != 0) {
            // Convert to 64-bit fixed-point fractional form with 1-digit
            // integer part. The magic number 720575941 is a good enough
            // approximation of 2^(32 + 24) / 10^8; see
            // https://jk-jeon.github.io/posts/2022/12/fixed-precision-formatting/#fixed-length-case
            // for details.
            prod = ((subsegment * static_cast<uint64_t>(720575941)) >> 24) + 1;
            digits = static_cast<uint32_t>(prod >> 32);
            *buffer = static_cast<char>('0' + digits);
            number_of_digits_printed++;
          }
          // If we want to print an even number of digits from the
          // first_subsegment,
          else {
            // Convert to 64-bit fixed-point fractional form with 2-digits
            // integer part. The magic number 450359963 is a good enough
            // approximation of 2^(32 + 20) / 10^7; see
            // https://jk-jeon.github.io/posts/2022/12/fixed-precision-formatting/#fixed-length-case
            // for details.
            prod = ((subsegment * static_cast<uint64_t>(450359963)) >> 20) + 1;
            digits = static_cast<uint32_t>(prod >> 32);
            copy2(buffer, digits2(digits));
            number_of_digits_printed += 2;
          }

          // Print all digit pairs.
          while (number_of_digits_printed < number_of_digits_to_print) {
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            digits = static_cast<uint32_t>(prod >> 32);
            copy2(buffer + number_of_digits_printed, digits2(digits));
            number_of_digits_printed += 2;
          }
        };

        // Print first subsegment.
        print_subsegment(first_subsegment, buf.data());

        // Perform rounding if the first subsegment is the last subsegment to
        // print.
        if (precision <= 9) {
          // Rounding inside the subsegment.
          // We round-up if:
          //  - either the fractional part is strictly larger than 1/2, or
          //  - the fractional part is exactly 1/2 and the last digit is odd.
          // We rely on the following observations:
          //  - If fractional_part >= threshold, then the fractional part is
          //    strictly larger than 1/2.
          //  - If the MSB of fractional_part is set, then the fractional part
          //    must be at least 1/2.
          //  - When the MSB of fractional_part is set, either
          //    second_third_subsegments being nonzero or has_more_segments
          //    being true means there are further digits not printed, so the
          //    fractional part is strictly larger than 1/2.
          if (precision < 9) {
            uint32_t fractional_part = static_cast<uint32_t>(prod);
            should_round_up = fractional_part >=
                                  data::fractional_part_rounding_thresholds
                                      [8 - number_of_digits_to_print] ||
                              ((fractional_part >> 31) &
                               ((digits & 1) | (second_third_subsegments != 0) |
                                has_more_segments)) != 0;
          }
          // Rounding at the subsegment boundary.
          // In this case, the fractional part is at least 1/2 if and only if
          // second_third_subsegments >= 5000000000ULL, and is strictly larger
          // than 1/2 if we further have either second_third_subsegments >
          // 5000000000ULL or has_more_segments == true.
          else {
            should_round_up = second_third_subsegments > 5000000000ULL ||
                              (second_third_subsegments == 5000000000ULL &&
                               ((digits & 1) != 0 || has_more_segments));
          }
        }
        // Otherwise, print the second subsegment.
        else {
          // Compilers are not aware of how to leverage the maximum value of
          // second_third_subsegments to find out a better magic number which
          // allows us to eliminate an additional shift. 1844674407370955162 =
          // ceil(2^64/10) < ceil(2^64*(10^9/(10^10 - 1))).
          const uint32_t second_subsegment =
              static_cast<uint32_t>(dragonbox::umul128_upper64(
                  second_third_subsegments, 1844674407370955162ULL));
          const uint32_t third_subsegment =
              static_cast<uint32_t>(second_third_subsegments) -
              second_subsegment * 10;

          number_of_digits_to_print = precision - 9;
          print_subsegment(second_subsegment, buf.data() + 9);

          // Rounding inside the subsegment.
          if (precision < 18) {
            // The condition third_subsegment != 0 implies that the segment was
            // of 19 digits, so in this case the third segment should be
            // consisting of a genuine digit from the input.
            uint32_t fractional_part = static_cast<uint32_t>(prod);
            should_round_up = fractional_part >=
                                  data::fractional_part_rounding_thresholds
                                      [8 - number_of_digits_to_print] ||
                              ((fractional_part >> 31) &
                               ((digits & 1) | (third_subsegment != 0) |
                                has_more_segments)) != 0;
          }
          // Rounding at the subsegment boundary.
          else {
            // In this case, the segment must be of 19 digits, thus
            // the third subsegment should be consisting of a genuine digit from
            // the input.
            should_round_up = third_subsegment > 5 ||
                              (third_subsegment == 5 &&
                               ((digits & 1) != 0 || has_more_segments));
          }
        }

        // Round-up if necessary.
        if (should_round_up) {
          ++buf[precision - 1];
          for (int i = precision - 1; i > 0 && buf[i] > '9'; --i) {
            buf[i] = '0';
            ++buf[i - 1];
          }
          if (buf[0] > '9') {
            buf[0] = '1';
            if (fixed)
              buf[precision++] = '0';
            else
              ++exp;
          }
        }
        buf.try_resize(to_unsigned(precision));
      }
    }  // if (digits_in_the_first_segment > precision)
    else {
      // Adjust the exponent for its use in Dragon4.
      exp += digits_in_the_first_segment - 1;
    }
  }
  if (use_dragon) {
    auto f = basic_fp<uint128_t>();
    bool is_predecessor_closer = specs.binary32
                                     ? f.assign(static_cast<float>(value))
                                     : f.assign(converted_value);
    if (is_predecessor_closer) dragon_flags |= dragon::predecessor_closer;
    if (fixed) dragon_flags |= dragon::fixed;
    // Limit precision to the maximum possible number of significant digits in
    // an IEEE754 double because we don't need to generate zeros.
    const int max_double_digits = 767;
    if (precision > max_double_digits) precision = max_double_digits;
    format_dragon(f, dragon_flags, precision, buf, exp);
  }
  if (!fixed && !specs.showpoint) {
    // Remove trailing zeros.
    auto num_digits = buf.size();
    while (num_digits > 0 && buf[num_digits - 1] == '0') {
      --num_digits;
      ++exp;
    }
    buf.try_resize(num_digits);
  }
  return exp;
}
template <typename Char, typename OutputIt, typename T>
FMT_CONSTEXPR20 auto write_float(OutputIt out, T value,
                                 format_specs<Char> specs, locale_ref loc)
    -> OutputIt {
  float_specs fspecs = parse_float_type_spec(specs);
  fspecs.sign = specs.sign;
  if (detail::signbit(value)) {  // value < 0 is false for NaN so use signbit.
    fspecs.sign = sign::minus;
    value = -value;
  } else if (fspecs.sign == sign::minus) {
    fspecs.sign = sign::none;
  }

  if (!detail::isfinite(value))
    return write_nonfinite(out, detail::isnan(value), specs, fspecs);

  if (specs.align == align::numeric && fspecs.sign) {
    auto it = reserve(out, 1);
    *it++ = detail::sign<Char>(fspecs.sign);
    out = base_iterator(out, it);
    fspecs.sign = sign::none;
    if (specs.width != 0) --specs.width;
  }

  memory_buffer buffer;
  if (fspecs.format == float_format::hex) {
    if (fspecs.sign) buffer.push_back(detail::sign<char>(fspecs.sign));
    format_hexfloat(convert_float(value), specs.precision, fspecs, buffer);
    return write_bytes<align::right>(out, {buffer.data(), buffer.size()},
                                     specs);
  }
  int precision = specs.precision >= 0 || specs.type == presentation_type::none
                      ? specs.precision
                      : 6;
  if (fspecs.format == float_format::exp) {
    if (precision == max_value<int>())
      throw_format_error("number is too big");
    else
      ++precision;
  } else if (fspecs.format != float_format::fixed && precision == 0) {
    precision = 1;
  }
  if (const_check(std::is_same<T, float>())) fspecs.binary32 = true;
  int exp = format_float(convert_float(value), precision, fspecs, buffer);
  fspecs.precision = precision;
  auto f = big_decimal_fp{buffer.data(), static_cast<int>(buffer.size()), exp};
  return write_float(out, f, specs, fspecs, loc);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_floating_point<T>::value)>
FMT_CONSTEXPR20 auto write(OutputIt out, T value, format_specs<Char> specs,
                           locale_ref loc = {}) -> OutputIt {
  if (const_check(!is_supported_floating_point(value))) return out;
  return specs.localized && write_loc(out, value, specs, loc)
             ? out
             : write_float(out, value, specs, loc);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_fast_float<T>::value)>
FMT_CONSTEXPR20 auto write(OutputIt out, T value) -> OutputIt {
  if (is_constant_evaluated()) return write(out, value, format_specs<Char>());
  if (const_check(!is_supported_floating_point(value))) return out;

  auto fspecs = float_specs();
  if (detail::signbit(value)) {
    fspecs.sign = sign::minus;
    value = -value;
  }

  constexpr auto specs = format_specs<Char>();
  using floaty = conditional_t<std::is_same<T, long double>::value, double, T>;
  using floaty_uint = typename dragonbox::float_info<floaty>::carrier_uint;
  floaty_uint mask = exponent_mask<floaty>();
  if ((bit_cast<floaty_uint>(value) & mask) == mask)
    return write_nonfinite(out, std::isnan(value), specs, fspecs);

  auto dec = dragonbox::to_decimal(static_cast<floaty>(value));
  return write_float(out, dec, specs, fspecs, {});
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_floating_point<T>::value &&
                        !is_fast_float<T>::value)>
inline auto write(OutputIt out, T value) -> OutputIt {
  return write(out, value, format_specs<Char>());
}

template <typename Char, typename OutputIt>
auto write(OutputIt out, monostate, format_specs<Char> = {}, locale_ref = {})
    -> OutputIt {
  FMT_ASSERT(false, "");
  return out;
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, basic_string_view<Char> value)
    -> OutputIt {
  auto it = reserve(out, value.size());
  it = copy_str_noinline<Char>(value.begin(), value.end(), it);
  return base_iterator(out, it);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_string<T>::value)>
constexpr auto write(OutputIt out, const T& value) -> OutputIt {
  return write<Char>(out, to_string_view(value));
}

// FMT_ENABLE_IF() condition separated to workaround an MSVC bug.
template <
    typename Char, typename OutputIt, typename T,
    bool check =
        std::is_enum<T>::value && !std::is_same<T, Char>::value &&
        mapped_type_constant<T, basic_format_context<OutputIt, Char>>::value !=
            type::custom_type,
    FMT_ENABLE_IF(check)>
FMT_CONSTEXPR auto write(OutputIt out, T value) -> OutputIt {
  return write<Char>(out, static_cast<underlying_t<T>>(value));
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(std::is_same<T, bool>::value)>
FMT_CONSTEXPR auto write(OutputIt out, T value,
                         const format_specs<Char>& specs = {}, locale_ref = {})
    -> OutputIt {
  return specs.type != presentation_type::none &&
                 specs.type != presentation_type::string
             ? write(out, value ? 1 : 0, specs, {})
             : write_bytes(out, value ? "true" : "false", specs);
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, Char value) -> OutputIt {
  auto it = reserve(out, 1);
  *it++ = value;
  return base_iterator(out, it);
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR_CHAR_TRAITS auto write(OutputIt out, const Char* value)
    -> OutputIt {
  if (value) return write(out, basic_string_view<Char>(value));
  throw_format_error("string pointer is null");
  return out;
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(std::is_same<T, void>::value)>
auto write(OutputIt out, const T* value, const format_specs<Char>& specs = {},
           locale_ref = {}) -> OutputIt {
  return write_ptr<Char>(out, bit_cast<uintptr_t>(value), &specs);
}

// A write overload that handles implicit conversions.
template <typename Char, typename OutputIt, typename T,
          typename Context = basic_format_context<OutputIt, Char>>
FMT_CONSTEXPR auto write(OutputIt out, const T& value) -> enable_if_t<
    std::is_class<T>::value && !is_string<T>::value &&
        !is_floating_point<T>::value && !std::is_same<T, Char>::value &&
        !std::is_same<T, remove_cvref_t<decltype(arg_mapper<Context>().map(
                             value))>>::value,
    OutputIt> {
  return write<Char>(out, arg_mapper<Context>().map(value));
}

template <typename Char, typename OutputIt, typename T,
          typename Context = basic_format_context<OutputIt, Char>>
FMT_CONSTEXPR auto write(OutputIt out, const T& value)
    -> enable_if_t<mapped_type_constant<T, Context>::value == type::custom_type,
                   OutputIt> {
  auto ctx = Context(out, {}, {});
  return typename Context::template formatter_type<T>().format(value, ctx);
}

// An argument visitor that formats the argument and writes it via the output
// iterator. It's a class and not a generic lambda for compatibility with C++11.
template <typename Char> struct default_arg_formatter {
  using iterator = buffer_appender<Char>;
  using context = buffer_context<Char>;

  iterator out;
  basic_format_args<context> args;
  locale_ref loc;

  template <typename T> auto operator()(T value) -> iterator {
    return write<Char>(out, value);
  }
  auto operator()(typename basic_format_arg<context>::handle h) -> iterator {
    basic_format_parse_context<Char> parse_ctx({});
    context format_ctx(out, args, loc);
    h.format(parse_ctx, format_ctx);
    return format_ctx.out();
  }
};

template <typename Char> struct arg_formatter {
  using iterator = buffer_appender<Char>;
  using context = buffer_context<Char>;

  iterator out;
  const format_specs<Char>& specs;
  locale_ref locale;

  template <typename T>
  FMT_CONSTEXPR FMT_INLINE auto operator()(T value) -> iterator {
    return detail::write(out, value, specs, locale);
  }
  auto operator()(typename basic_format_arg<context>::handle) -> iterator {
    // User-defined types are handled separately because they require access
    // to the parse context.
    return out;
  }
};

template <typename Char> struct custom_formatter {
  basic_format_parse_context<Char>& parse_ctx;
  buffer_context<Char>& ctx;

  void operator()(
      typename basic_format_arg<buffer_context<Char>>::handle h) const {
    h.format(parse_ctx, ctx);
  }
  template <typename T> void operator()(T) const {}
};

template <typename ErrorHandler> class width_checker {
 public:
  explicit FMT_CONSTEXPR width_checker(ErrorHandler& eh) : handler_(eh) {}

  template <typename T, FMT_ENABLE_IF(is_integer<T>::value)>
  FMT_CONSTEXPR auto operator()(T value) -> unsigned long long {
    if (is_negative(value)) handler_.on_error("negative width");
    return static_cast<unsigned long long>(value);
  }

  template <typename T, FMT_ENABLE_IF(!is_integer<T>::value)>
  FMT_CONSTEXPR auto operator()(T) -> unsigned long long {
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
  FMT_CONSTEXPR auto operator()(T value) -> unsigned long long {
    if (is_negative(value)) handler_.on_error("negative precision");
    return static_cast<unsigned long long>(value);
  }

  template <typename T, FMT_ENABLE_IF(!is_integer<T>::value)>
  FMT_CONSTEXPR auto operator()(T) -> unsigned long long {
    handler_.on_error("precision is not integer");
    return 0;
  }

 private:
  ErrorHandler& handler_;
};

template <template <typename> class Handler, typename FormatArg,
          typename ErrorHandler>
FMT_CONSTEXPR auto get_dynamic_spec(FormatArg arg, ErrorHandler eh) -> int {
  unsigned long long value = visit_format_arg(Handler<ErrorHandler>(eh), arg);
  if (value > to_unsigned(max_value<int>())) eh.on_error("number is too big");
  return static_cast<int>(value);
}

template <typename Context, typename ID>
FMT_CONSTEXPR auto get_arg(Context& ctx, ID id) -> decltype(ctx.arg(id)) {
  auto arg = ctx.arg(id);
  if (!arg) ctx.on_error("argument not found");
  return arg;
}

template <template <typename> class Handler, typename Context>
FMT_CONSTEXPR void handle_dynamic_spec(int& value,
                                       arg_ref<typename Context::char_type> ref,
                                       Context& ctx) {
  switch (ref.kind) {
  case arg_id_kind::none:
    break;
  case arg_id_kind::index:
    value = detail::get_dynamic_spec<Handler>(get_arg(ctx, ref.val.index),
                                              ctx.error_handler());
    break;
  case arg_id_kind::name:
    value = detail::get_dynamic_spec<Handler>(get_arg(ctx, ref.val.name),
                                              ctx.error_handler());
    break;
  }
}

#if FMT_USE_USER_DEFINED_LITERALS
#  if FMT_USE_NONTYPE_TEMPLATE_ARGS
template <typename T, typename Char, size_t N,
          fmt::detail_exported::fixed_string<Char, N> Str>
struct statically_named_arg : view {
  static constexpr auto name = Str.data;

  const T& value;
  statically_named_arg(const T& v) : value(v) {}
};

template <typename T, typename Char, size_t N,
          fmt::detail_exported::fixed_string<Char, N> Str>
struct is_named_arg<statically_named_arg<T, Char, N, Str>> : std::true_type {};

template <typename T, typename Char, size_t N,
          fmt::detail_exported::fixed_string<Char, N> Str>
struct is_statically_named_arg<statically_named_arg<T, Char, N, Str>>
    : std::true_type {};

template <typename Char, size_t N,
          fmt::detail_exported::fixed_string<Char, N> Str>
struct udl_arg {
  template <typename T> auto operator=(T&& value) const {
    return statically_named_arg<T, Char, N, Str>(std::forward<T>(value));
  }
};
#  else
template <typename Char> struct udl_arg {
  const Char* str;

  template <typename T> auto operator=(T&& value) const -> named_arg<Char, T> {
    return {str, std::forward<T>(value)};
  }
};
#  endif
#endif  // FMT_USE_USER_DEFINED_LITERALS

template <typename Locale, typename Char>
auto vformat(const Locale& loc, basic_string_view<Char> fmt,
             basic_format_args<buffer_context<type_identity_t<Char>>> args)
    -> std::basic_string<Char> {
  auto buf = basic_memory_buffer<Char>();
  detail::vformat_to(buf, fmt, args, detail::locale_ref(loc));
  return {buf.data(), buf.size()};
}

using format_func = void (*)(detail::buffer<char>&, int, const char*);

FMT_API void format_error_code(buffer<char>& out, int error_code,
                               string_view message) noexcept;

FMT_API void report_error(format_func func, int error_code,
                          const char* message) noexcept;
}  // namespace detail

FMT_API auto vsystem_error(int error_code, string_view format_str,
                           format_args args) -> std::system_error;

/**
  \rst
  Constructs :class:`std::system_error` with a message formatted with
  ``fmt::format(fmt, args...)``.
  *error_code* is a system error code as given by ``errno``.

  **Example**::

    // This throws std::system_error with the description
    //   cannot open file 'madeup': No such file or directory
    // or similar (system message may vary).
    const char* filename = "madeup";
    std::FILE* file = std::fopen(filename, "r");
    if (!file)
      throw fmt::system_error(errno, "cannot open file '{}'", filename);
  \endrst
 */
template <typename... T>
auto system_error(int error_code, format_string<T...> fmt, T&&... args)
    -> std::system_error {
  return vsystem_error(error_code, fmt, fmt::make_format_args(args...));
}

/**
  \rst
  Formats an error message for an error returned by an operating system or a
  language runtime, for example a file opening error, and writes it to *out*.
  The format is the same as the one used by ``std::system_error(ec, message)``
  where ``ec`` is ``std::error_code(error_code, std::generic_category()})``.
  It is implementation-defined but normally looks like:

  .. parsed-literal::
     *<message>*: *<system-message>*

  where *<message>* is the passed message and *<system-message>* is the system
  message corresponding to the error code.
  *error_code* is a system error code as given by ``errno``.
  \endrst
 */
FMT_API void format_system_error(detail::buffer<char>& out, int error_code,
                                 const char* message) noexcept;

// Reports a system error without throwing an exception.
// Can be used to report errors from destructors.
FMT_API void report_system_error(int error_code, const char* message) noexcept;

/** Fast integer formatter. */
class format_int {
 private:
  // Buffer should be large enough to hold all digits (digits10 + 1),
  // a sign and a null character.
  enum { buffer_size = std::numeric_limits<unsigned long long>::digits10 + 3 };
  mutable char buffer_[buffer_size];
  char* str_;

  template <typename UInt> auto format_unsigned(UInt value) -> char* {
    auto n = static_cast<detail::uint32_or_64_or_128_t<UInt>>(value);
    return detail::format_decimal(buffer_, n, buffer_size - 1).begin;
  }

  template <typename Int> auto format_signed(Int value) -> char* {
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
  auto size() const -> size_t {
    return detail::to_unsigned(buffer_ - str_ + buffer_size - 1);
  }

  /**
    Returns a pointer to the output buffer content. No terminating null
    character is appended.
   */
  auto data() const -> const char* { return str_; }

  /**
    Returns a pointer to the output buffer content with terminating null
    character appended.
   */
  auto c_str() const -> const char* {
    buffer_[buffer_size - 1] = '\0';
    return str_;
  }

  /**
    \rst
    Returns the content of the output buffer as an ``std::string``.
    \endrst
   */
  auto str() const -> std::string { return std::string(str_, size()); }
};

template <typename T, typename Char>
struct formatter<T, Char, enable_if_t<detail::has_format_as<T>::value>>
    : private formatter<detail::format_as_t<T>, Char> {
  using base = formatter<detail::format_as_t<T>, Char>;
  using base::parse;

  template <typename FormatContext>
  auto format(const T& value, FormatContext& ctx) const -> decltype(ctx.out()) {
    return base::format(format_as(value), ctx);
  }
};

#define FMT_FORMAT_AS(Type, Base) \
  template <typename Char>        \
  struct formatter<Type, Char> : formatter<Base, Char> {}

FMT_FORMAT_AS(signed char, int);
FMT_FORMAT_AS(unsigned char, unsigned);
FMT_FORMAT_AS(short, int);
FMT_FORMAT_AS(unsigned short, unsigned);
FMT_FORMAT_AS(long, detail::long_type);
FMT_FORMAT_AS(unsigned long, detail::ulong_type);
FMT_FORMAT_AS(Char*, const Char*);
FMT_FORMAT_AS(std::basic_string<Char>, basic_string_view<Char>);
FMT_FORMAT_AS(std::nullptr_t, const void*);
FMT_FORMAT_AS(detail::std_string_view<Char>, basic_string_view<Char>);
FMT_FORMAT_AS(void*, const void*);

template <typename Char, size_t N>
struct formatter<Char[N], Char> : formatter<basic_string_view<Char>, Char> {};

/**
  \rst
  Converts ``p`` to ``const void*`` for pointer formatting.

  **Example**::

    auto s = fmt::format("{}", fmt::ptr(p));
  \endrst
 */
template <typename T> auto ptr(T p) -> const void* {
  static_assert(std::is_pointer<T>::value, "");
  return detail::bit_cast<const void*>(p);
}
template <typename T, typename Deleter>
auto ptr(const std::unique_ptr<T, Deleter>& p) -> const void* {
  return p.get();
}
template <typename T> auto ptr(const std::shared_ptr<T>& p) -> const void* {
  return p.get();
}

/**
  \rst
  Converts ``e`` to the underlying type.

  **Example**::

    enum class color { red, green, blue };
    auto s = fmt::format("{}", fmt::underlying(color::red));
  \endrst
 */
template <typename Enum>
constexpr auto underlying(Enum e) noexcept -> underlying_t<Enum> {
  return static_cast<underlying_t<Enum>>(e);
}

namespace enums {
template <typename Enum, FMT_ENABLE_IF(std::is_enum<Enum>::value)>
constexpr auto format_as(Enum e) noexcept -> underlying_t<Enum> {
  return static_cast<underlying_t<Enum>>(e);
}
}  // namespace enums

class bytes {
 private:
  string_view data_;
  friend struct formatter<bytes>;

 public:
  explicit bytes(string_view data) : data_(data) {}
};

template <> struct formatter<bytes> {
 private:
  detail::dynamic_format_specs<> specs_;

 public:
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> const char* {
    return parse_format_specs(ctx.begin(), ctx.end(), specs_, ctx,
                              detail::type::string_type);
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

// group_digits_view is not derived from view because it copies the argument.
template <typename T> struct group_digits_view {
  T value;
};

/**
  \rst
  Returns a view that formats an integer value using ',' as a locale-independent
  thousands separator.

  **Example**::

    fmt::print("{}", fmt::group_digits(12345));
    // Output: "12,345"
  \endrst
 */
template <typename T> auto group_digits(T value) -> group_digits_view<T> {
  return {value};
}

template <typename T> struct formatter<group_digits_view<T>> : formatter<T> {
 private:
  detail::dynamic_format_specs<> specs_;

 public:
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> const char* {
    return parse_format_specs(ctx.begin(), ctx.end(), specs_, ctx,
                              detail::type::int_type);
  }

  template <typename FormatContext>
  auto format(group_digits_view<T> t, FormatContext& ctx)
      -> decltype(ctx.out()) {
    detail::handle_dynamic_spec<detail::width_checker>(specs_.width,
                                                       specs_.width_ref, ctx);
    detail::handle_dynamic_spec<detail::precision_checker>(
        specs_.precision, specs_.precision_ref, ctx);
    return detail::write_int(
        ctx.out(), static_cast<detail::uint64_or_128_t<T>>(t.value), 0, specs_,
        detail::digit_grouping<char>("\3", ","));
  }
};

// DEPRECATED! join_view will be moved to ranges.h.
template <typename It, typename Sentinel, typename Char = char>
struct join_view : detail::view {
  It begin;
  Sentinel end;
  basic_string_view<Char> sep;

  join_view(It b, Sentinel e, basic_string_view<Char> s)
      : begin(b), end(e), sep(s) {}
};

template <typename It, typename Sentinel, typename Char>
struct formatter<join_view<It, Sentinel, Char>, Char> {
 private:
  using value_type =
#ifdef __cpp_lib_ranges
      std::iter_value_t<It>;
#else
      typename std::iterator_traits<It>::value_type;
#endif
  formatter<remove_cvref_t<value_type>, Char> value_formatter_;

 public:
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> const Char* {
    return value_formatter_.parse(ctx);
  }

  template <typename FormatContext>
  auto format(const join_view<It, Sentinel, Char>& value,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    auto it = value.begin;
    auto out = ctx.out();
    if (it != value.end) {
      out = value_formatter_.format(*it, ctx);
      ++it;
      while (it != value.end) {
        out = detail::copy_str<Char>(value.sep.begin(), value.sep.end(), out);
        ctx.advance_to(out);
        out = value_formatter_.format(*it, ctx);
        ++it;
      }
    }
    return out;
  }
};

/**
  Returns a view that formats the iterator range `[begin, end)` with elements
  separated by `sep`.
 */
template <typename It, typename Sentinel>
auto join(It begin, Sentinel end, string_view sep) -> join_view<It, Sentinel> {
  return {begin, end, sep};
}

/**
  \rst
  Returns a view that formats `range` with elements separated by `sep`.

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
auto join(Range&& range, string_view sep)
    -> join_view<detail::iterator_t<Range>, detail::sentinel_t<Range>> {
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
template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value &&
                                    !detail::has_format_as<T>::value)>
inline auto to_string(const T& value) -> std::string {
  auto buffer = memory_buffer();
  detail::write<char>(appender(buffer), value);
  return {buffer.data(), buffer.size()};
}

template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
FMT_NODISCARD inline auto to_string(T value) -> std::string {
  // The buffer should be large enough to store the number including the sign
  // or "false" for bool.
  constexpr int max_size = detail::digits10<T>() + 2;
  char buffer[max_size > 5 ? static_cast<unsigned>(max_size) : 5];
  char* begin = buffer;
  return std::string(begin, detail::write<char>(begin, value));
}

template <typename Char, size_t SIZE>
FMT_NODISCARD auto to_string(const basic_memory_buffer<Char, SIZE>& buf)
    -> std::basic_string<Char> {
  auto size = buf.size();
  detail::assume(size < std::basic_string<Char>().max_size());
  return std::basic_string<Char>(buf.data(), size);
}

template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value &&
                                    detail::has_format_as<T>::value)>
inline auto to_string(const T& value) -> std::string {
  return to_string(format_as(value));
}

FMT_END_EXPORT

namespace detail {

template <typename Char>
void vformat_to(buffer<Char>& buf, basic_string_view<Char> fmt,
                typename vformat_args<Char>::type args, locale_ref loc) {
  auto out = buffer_appender<Char>(buf);
  if (fmt.size() == 2 && equal2(fmt.data(), "{}")) {
    auto arg = args.get(0);
    if (!arg) error_handler().on_error("argument not found");
    visit_format_arg(default_arg_formatter<Char>{out, args, loc}, arg);
    return;
  }

  struct format_handler : error_handler {
    basic_format_parse_context<Char> parse_context;
    buffer_context<Char> context;

    format_handler(buffer_appender<Char> p_out, basic_string_view<Char> str,
                   basic_format_args<buffer_context<Char>> p_args,
                   locale_ref p_loc)
        : parse_context(str), context(p_out, p_args, p_loc) {}

    void on_text(const Char* begin, const Char* end) {
      auto text = basic_string_view<Char>(begin, to_unsigned(end - begin));
      context.advance_to(write<Char>(context.out(), text));
    }

    FMT_CONSTEXPR auto on_arg_id() -> int {
      return parse_context.next_arg_id();
    }
    FMT_CONSTEXPR auto on_arg_id(int id) -> int {
      return parse_context.check_arg_id(id), id;
    }
    FMT_CONSTEXPR auto on_arg_id(basic_string_view<Char> id) -> int {
      int arg_id = context.arg_id(id);
      if (arg_id < 0) on_error("argument not found");
      return arg_id;
    }

    FMT_INLINE void on_replacement_field(int id, const Char*) {
      auto arg = get_arg(context, id);
      context.advance_to(visit_format_arg(
          default_arg_formatter<Char>{context.out(), context.args(),
                                      context.locale()},
          arg));
    }

    auto on_format_specs(int id, const Char* begin, const Char* end)
        -> const Char* {
      auto arg = get_arg(context, id);
      if (arg.type() == type::custom_type) {
        parse_context.advance_to(begin);
        visit_format_arg(custom_formatter<Char>{parse_context, context}, arg);
        return parse_context.begin();
      }
      auto specs = detail::dynamic_format_specs<Char>();
      begin = parse_format_specs(begin, end, specs, parse_context, arg.type());
      detail::handle_dynamic_spec<detail::width_checker>(
          specs.width, specs.width_ref, context);
      detail::handle_dynamic_spec<detail::precision_checker>(
          specs.precision, specs.precision_ref, context);
      if (begin == end || *begin != '}')
        on_error("missing '}' in format string");
      auto f = arg_formatter<Char>{context.out(), specs, context.locale()};
      context.advance_to(visit_format_arg(f, arg));
      return begin;
    }
  };
  detail::parse_format_string<false>(fmt, format_handler(out, fmt, args, loc));
}

FMT_BEGIN_EXPORT

#ifndef FMT_HEADER_ONLY
extern template FMT_API void vformat_to(buffer<char>&, string_view,
                                        typename vformat_args<>::type,
                                        locale_ref);
extern template FMT_API auto thousands_sep_impl<char>(locale_ref)
    -> thousands_sep_result<char>;
extern template FMT_API auto thousands_sep_impl<wchar_t>(locale_ref)
    -> thousands_sep_result<wchar_t>;
extern template FMT_API auto decimal_point_impl(locale_ref) -> char;
extern template FMT_API auto decimal_point_impl(locale_ref) -> wchar_t;
#endif  // FMT_HEADER_ONLY

}  // namespace detail

#if FMT_USE_USER_DEFINED_LITERALS
inline namespace literals {
/**
  \rst
  User-defined literal equivalent of :func:`fmt::arg`.

  **Example**::

    using namespace fmt::literals;
    fmt::print("Elapsed time: {s:.2f} seconds", "s"_a=1.23);
  \endrst
 */
#  if FMT_USE_NONTYPE_TEMPLATE_ARGS
template <detail_exported::fixed_string Str> constexpr auto operator""_a() {
  using char_t = remove_cvref_t<decltype(Str.data[0])>;
  return detail::udl_arg<char_t, sizeof(Str.data) / sizeof(char_t), Str>();
}
#  else
constexpr auto operator"" _a(const char* s, size_t) -> detail::udl_arg<char> {
  return {s};
}
#  endif
}  // namespace literals
#endif  // FMT_USE_USER_DEFINED_LITERALS

template <typename Locale, FMT_ENABLE_IF(detail::is_locale<Locale>::value)>
inline auto vformat(const Locale& loc, string_view fmt, format_args args)
    -> std::string {
  return detail::vformat(loc, fmt, args);
}

template <typename Locale, typename... T,
          FMT_ENABLE_IF(detail::is_locale<Locale>::value)>
inline auto format(const Locale& loc, format_string<T...> fmt, T&&... args)
    -> std::string {
  return fmt::vformat(loc, string_view(fmt), fmt::make_format_args(args...));
}

template <typename OutputIt, typename Locale,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value&&
                            detail::is_locale<Locale>::value)>
auto vformat_to(OutputIt out, const Locale& loc, string_view fmt,
                format_args args) -> OutputIt {
  using detail::get_buffer;
  auto&& buf = get_buffer<char>(out);
  detail::vformat_to(buf, fmt, args, detail::locale_ref(loc));
  return detail::get_iterator(buf, out);
}

template <typename OutputIt, typename Locale, typename... T,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value&&
                            detail::is_locale<Locale>::value)>
FMT_INLINE auto format_to(OutputIt out, const Locale& loc,
                          format_string<T...> fmt, T&&... args) -> OutputIt {
  return vformat_to(out, loc, fmt, fmt::make_format_args(args...));
}

template <typename Locale, typename... T,
          FMT_ENABLE_IF(detail::is_locale<Locale>::value)>
FMT_NODISCARD FMT_INLINE auto formatted_size(const Locale& loc,
                                             format_string<T...> fmt,
                                             T&&... args) -> size_t {
  auto buf = detail::counting_buffer<>();
  detail::vformat_to<char>(buf, fmt, fmt::make_format_args(args...),
                           detail::locale_ref(loc));
  return buf.count();
}

FMT_END_EXPORT

template <typename T, typename Char>
template <typename FormatContext>
FMT_CONSTEXPR FMT_INLINE auto
formatter<T, Char,
          enable_if_t<detail::type_constant<T, Char>::value !=
                      detail::type::custom_type>>::format(const T& val,
                                                          FormatContext& ctx)
    const -> decltype(ctx.out()) {
  if (specs_.width_ref.kind != detail::arg_id_kind::none ||
      specs_.precision_ref.kind != detail::arg_id_kind::none) {
    auto specs = specs_;
    detail::handle_dynamic_spec<detail::width_checker>(specs.width,
                                                       specs.width_ref, ctx);
    detail::handle_dynamic_spec<detail::precision_checker>(
        specs.precision, specs.precision_ref, ctx);
    return detail::write<Char>(ctx.out(), val, specs, ctx.locale());
  }
  return detail::write<Char>(ctx.out(), val, specs_, ctx.locale());
}

FMT_END_NAMESPACE

#ifdef FMT_HEADER_ONLY
#  define FMT_FUNC inline
#  include "format-inl.h"
#else
#  define FMT_FUNC
#endif

#endif  // FMT_FORMAT_H_
