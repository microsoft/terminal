// Formatting library for C++ - implementation
//
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_FORMAT_INL_H_
#define FMT_FORMAT_INL_H_

#include <cassert>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstring>  // for std::memmove
#include <cwchar>

#include "format.h"
#if !defined(FMT_STATIC_THOUSANDS_SEPARATOR)
#  include <locale>
#endif

#ifdef _WIN32
#  include <io.h>
#  include <windows.h>
#endif

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4702)  // unreachable code
#endif

// Dummy implementations of strerror_r and strerror_s called if corresponding
// system functions are not available.
inline fmt::internal::null<> strerror_r(int, char*, ...) { return {}; }
inline fmt::internal::null<> strerror_s(char*, std::size_t, ...) { return {}; }

FMT_BEGIN_NAMESPACE
namespace internal {

FMT_FUNC void assert_fail(const char* file, int line, const char* message) {
  print(stderr, "{}:{}: assertion failed: {}", file, line, message);
  std::abort();
}

#ifndef _MSC_VER
#  define FMT_SNPRINTF snprintf
#else  // _MSC_VER
inline int fmt_snprintf(char* buffer, size_t size, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = vsnprintf_s(buffer, size, _TRUNCATE, format, args);
  va_end(args);
  return result;
}
#  define FMT_SNPRINTF fmt_snprintf
#endif  // _MSC_VER

// A portable thread-safe version of strerror.
// Sets buffer to point to a string describing the error code.
// This can be either a pointer to a string stored in buffer,
// or a pointer to some static immutable string.
// Returns one of the following values:
//   0      - success
//   ERANGE - buffer is not large enough to store the error message
//   other  - failure
// Buffer should be at least of size 1.
FMT_FUNC int safe_strerror(int error_code, char*& buffer,
                           std::size_t buffer_size) FMT_NOEXCEPT {
  FMT_ASSERT(buffer != nullptr && buffer_size != 0, "invalid buffer");

  class dispatcher {
   private:
    int error_code_;
    char*& buffer_;
    std::size_t buffer_size_;

    // A noop assignment operator to avoid bogus warnings.
    void operator=(const dispatcher&) {}

    // Handle the result of XSI-compliant version of strerror_r.
    int handle(int result) {
      // glibc versions before 2.13 return result in errno.
      return result == -1 ? errno : result;
    }

    // Handle the result of GNU-specific version of strerror_r.
    FMT_MAYBE_UNUSED
    int handle(char* message) {
      // If the buffer is full then the message is probably truncated.
      if (message == buffer_ && strlen(buffer_) == buffer_size_ - 1)
        return ERANGE;
      buffer_ = message;
      return 0;
    }

    // Handle the case when strerror_r is not available.
    FMT_MAYBE_UNUSED
    int handle(internal::null<>) {
      return fallback(strerror_s(buffer_, buffer_size_, error_code_));
    }

    // Fallback to strerror_s when strerror_r is not available.
    FMT_MAYBE_UNUSED
    int fallback(int result) {
      // If the buffer is full then the message is probably truncated.
      return result == 0 && strlen(buffer_) == buffer_size_ - 1 ? ERANGE
                                                                : result;
    }

#if !FMT_MSC_VER
    // Fallback to strerror if strerror_r and strerror_s are not available.
    int fallback(internal::null<>) {
      errno = 0;
      buffer_ = strerror(error_code_);
      return errno;
    }
#endif

   public:
    dispatcher(int err_code, char*& buf, std::size_t buf_size)
        : error_code_(err_code), buffer_(buf), buffer_size_(buf_size) {}

    int run() { return handle(strerror_r(error_code_, buffer_, buffer_size_)); }
  };
  return dispatcher(error_code, buffer, buffer_size).run();
}

FMT_FUNC void format_error_code(internal::buffer<char>& out, int error_code,
                                string_view message) FMT_NOEXCEPT {
  // Report error code making sure that the output fits into
  // inline_buffer_size to avoid dynamic memory allocation and potential
  // bad_alloc.
  out.resize(0);
  static const char SEP[] = ": ";
  static const char ERROR_STR[] = "error ";
  // Subtract 2 to account for terminating null characters in SEP and ERROR_STR.
  std::size_t error_code_size = sizeof(SEP) + sizeof(ERROR_STR) - 2;
  auto abs_value = static_cast<uint32_or_64_or_128_t<int>>(error_code);
  if (internal::is_negative(error_code)) {
    abs_value = 0 - abs_value;
    ++error_code_size;
  }
  error_code_size += internal::to_unsigned(internal::count_digits(abs_value));
  internal::writer w(out);
  if (message.size() <= inline_buffer_size - error_code_size) {
    w.write(message);
    w.write(SEP);
  }
  w.write(ERROR_STR);
  w.write(error_code);
  assert(out.size() <= inline_buffer_size);
}

FMT_FUNC void report_error(format_func func, int error_code,
                           string_view message) FMT_NOEXCEPT {
  memory_buffer full_message;
  func(full_message, error_code, message);
  // Don't use fwrite_fully because the latter may throw.
  (void)std::fwrite(full_message.data(), full_message.size(), 1, stderr);
  std::fputc('\n', stderr);
}

// A wrapper around fwrite that throws on error.
FMT_FUNC void fwrite_fully(const void* ptr, size_t size, size_t count,
                           FILE* stream) {
  size_t written = std::fwrite(ptr, size, count, stream);
  if (written < count) FMT_THROW(system_error(errno, "cannot write to file"));
}
}  // namespace internal

#if !defined(FMT_STATIC_THOUSANDS_SEPARATOR)
namespace internal {

template <typename Locale>
locale_ref::locale_ref(const Locale& loc) : locale_(&loc) {
  static_assert(std::is_same<Locale, std::locale>::value, "");
}

template <typename Locale> Locale locale_ref::get() const {
  static_assert(std::is_same<Locale, std::locale>::value, "");
  return locale_ ? *static_cast<const std::locale*>(locale_) : std::locale();
}

template <typename Char> FMT_FUNC std::string grouping_impl(locale_ref loc) {
  return std::use_facet<std::numpunct<Char>>(loc.get<std::locale>()).grouping();
}
template <typename Char> FMT_FUNC Char thousands_sep_impl(locale_ref loc) {
  return std::use_facet<std::numpunct<Char>>(loc.get<std::locale>())
      .thousands_sep();
}
template <typename Char> FMT_FUNC Char decimal_point_impl(locale_ref loc) {
  return std::use_facet<std::numpunct<Char>>(loc.get<std::locale>())
      .decimal_point();
}
}  // namespace internal
#else
template <typename Char>
FMT_FUNC std::string internal::grouping_impl(locale_ref) {
  return "\03";
}
template <typename Char>
FMT_FUNC Char internal::thousands_sep_impl(locale_ref) {
  return FMT_STATIC_THOUSANDS_SEPARATOR;
}
template <typename Char>
FMT_FUNC Char internal::decimal_point_impl(locale_ref) {
  return '.';
}
#endif

FMT_API FMT_FUNC format_error::~format_error() FMT_NOEXCEPT = default;
FMT_API FMT_FUNC system_error::~system_error() FMT_NOEXCEPT = default;

FMT_FUNC void system_error::init(int err_code, string_view format_str,
                                 format_args args) {
  error_code_ = err_code;
  memory_buffer buffer;
  format_system_error(buffer, err_code, vformat(format_str, args));
  std::runtime_error& base = *this;
  base = std::runtime_error(to_string(buffer));
}

namespace internal {

template <> FMT_FUNC int count_digits<4>(internal::fallback_uintptr n) {
  // fallback_uintptr is always stored in little endian.
  int i = static_cast<int>(sizeof(void*)) - 1;
  while (i > 0 && n.value[i] == 0) --i;
  auto char_digits = std::numeric_limits<unsigned char>::digits / 4;
  return i >= 0 ? i * char_digits + count_digits<4, unsigned>(n.value[i]) : 1;
}

template <typename T>
const char basic_data<T>::digits[] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

template <typename T>
const char basic_data<T>::hex_digits[] = "0123456789abcdef";

#define FMT_POWERS_OF_10(factor)                                             \
  factor * 10, (factor)*100, (factor)*1000, (factor)*10000, (factor)*100000, \
      (factor)*1000000, (factor)*10000000, (factor)*100000000,               \
      (factor)*1000000000

template <typename T>
const uint64_t basic_data<T>::powers_of_10_64[] = {
    1, FMT_POWERS_OF_10(1), FMT_POWERS_OF_10(1000000000ULL),
    10000000000000000000ULL};

template <typename T>
const uint32_t basic_data<T>::zero_or_powers_of_10_32[] = {0,
                                                           FMT_POWERS_OF_10(1)};

template <typename T>
const uint64_t basic_data<T>::zero_or_powers_of_10_64[] = {
    0, FMT_POWERS_OF_10(1), FMT_POWERS_OF_10(1000000000ULL),
    10000000000000000000ULL};

// Normalized 64-bit significands of pow(10, k), for k = -348, -340, ..., 340.
// These are generated by support/compute-powers.py.
template <typename T>
const uint64_t basic_data<T>::pow10_significands[] = {
    0xfa8fd5a0081c0288, 0xbaaee17fa23ebf76, 0x8b16fb203055ac76,
    0xcf42894a5dce35ea, 0x9a6bb0aa55653b2d, 0xe61acf033d1a45df,
    0xab70fe17c79ac6ca, 0xff77b1fcbebcdc4f, 0xbe5691ef416bd60c,
    0x8dd01fad907ffc3c, 0xd3515c2831559a83, 0x9d71ac8fada6c9b5,
    0xea9c227723ee8bcb, 0xaecc49914078536d, 0x823c12795db6ce57,
    0xc21094364dfb5637, 0x9096ea6f3848984f, 0xd77485cb25823ac7,
    0xa086cfcd97bf97f4, 0xef340a98172aace5, 0xb23867fb2a35b28e,
    0x84c8d4dfd2c63f3b, 0xc5dd44271ad3cdba, 0x936b9fcebb25c996,
    0xdbac6c247d62a584, 0xa3ab66580d5fdaf6, 0xf3e2f893dec3f126,
    0xb5b5ada8aaff80b8, 0x87625f056c7c4a8b, 0xc9bcff6034c13053,
    0x964e858c91ba2655, 0xdff9772470297ebd, 0xa6dfbd9fb8e5b88f,
    0xf8a95fcf88747d94, 0xb94470938fa89bcf, 0x8a08f0f8bf0f156b,
    0xcdb02555653131b6, 0x993fe2c6d07b7fac, 0xe45c10c42a2b3b06,
    0xaa242499697392d3, 0xfd87b5f28300ca0e, 0xbce5086492111aeb,
    0x8cbccc096f5088cc, 0xd1b71758e219652c, 0x9c40000000000000,
    0xe8d4a51000000000, 0xad78ebc5ac620000, 0x813f3978f8940984,
    0xc097ce7bc90715b3, 0x8f7e32ce7bea5c70, 0xd5d238a4abe98068,
    0x9f4f2726179a2245, 0xed63a231d4c4fb27, 0xb0de65388cc8ada8,
    0x83c7088e1aab65db, 0xc45d1df942711d9a, 0x924d692ca61be758,
    0xda01ee641a708dea, 0xa26da3999aef774a, 0xf209787bb47d6b85,
    0xb454e4a179dd1877, 0x865b86925b9bc5c2, 0xc83553c5c8965d3d,
    0x952ab45cfa97a0b3, 0xde469fbd99a05fe3, 0xa59bc234db398c25,
    0xf6c69a72a3989f5c, 0xb7dcbf5354e9bece, 0x88fcf317f22241e2,
    0xcc20ce9bd35c78a5, 0x98165af37b2153df, 0xe2a0b5dc971f303a,
    0xa8d9d1535ce3b396, 0xfb9b7cd9a4a7443c, 0xbb764c4ca7a44410,
    0x8bab8eefb6409c1a, 0xd01fef10a657842c, 0x9b10a4e5e9913129,
    0xe7109bfba19c0c9d, 0xac2820d9623bf429, 0x80444b5e7aa7cf85,
    0xbf21e44003acdd2d, 0x8e679c2f5e44ff8f, 0xd433179d9c8cb841,
    0x9e19db92b4e31ba9, 0xeb96bf6ebadf77d9, 0xaf87023b9bf0ee6b,
};

// Binary exponents of pow(10, k), for k = -348, -340, ..., 340, corresponding
// to significands above.
template <typename T>
const int16_t basic_data<T>::pow10_exponents[] = {
    -1220, -1193, -1166, -1140, -1113, -1087, -1060, -1034, -1007, -980, -954,
    -927,  -901,  -874,  -847,  -821,  -794,  -768,  -741,  -715,  -688, -661,
    -635,  -608,  -582,  -555,  -529,  -502,  -475,  -449,  -422,  -396, -369,
    -343,  -316,  -289,  -263,  -236,  -210,  -183,  -157,  -130,  -103, -77,
    -50,   -24,   3,     30,    56,    83,    109,   136,   162,   189,  216,
    242,   269,   295,   322,   348,   375,   402,   428,   455,   481,  508,
    534,   561,   588,   614,   641,   667,   694,   720,   747,   774,  800,
    827,   853,   880,   907,   933,   960,   986,   1013,  1039,  1066};

template <typename T>
const char basic_data<T>::foreground_color[] = "\x1b[38;2;";
template <typename T>
const char basic_data<T>::background_color[] = "\x1b[48;2;";
template <typename T> const char basic_data<T>::reset_color[] = "\x1b[0m";
template <typename T> const wchar_t basic_data<T>::wreset_color[] = L"\x1b[0m";
template <typename T> const char basic_data<T>::signs[] = {0, '-', '+', ' '};

template <typename T> struct bits {
  static FMT_CONSTEXPR_DECL const int value =
      static_cast<int>(sizeof(T) * std::numeric_limits<unsigned char>::digits);
};

class fp;
template <int SHIFT = 0> fp normalize(fp value);

// Lower (upper) boundary is a value half way between a floating-point value
// and its predecessor (successor). Boundaries have the same exponent as the
// value so only significands are stored.
struct boundaries {
  uint64_t lower;
  uint64_t upper;
};

// A handmade floating-point number f * pow(2, e).
class fp {
 private:
  using significand_type = uint64_t;

 public:
  significand_type f;
  int e;

  // All sizes are in bits.
  // Subtract 1 to account for an implicit most significant bit in the
  // normalized form.
  static FMT_CONSTEXPR_DECL const int double_significand_size =
      std::numeric_limits<double>::digits - 1;
  static FMT_CONSTEXPR_DECL const uint64_t implicit_bit =
      1ULL << double_significand_size;
  static FMT_CONSTEXPR_DECL const int significand_size =
      bits<significand_type>::value;

  fp() : f(0), e(0) {}
  fp(uint64_t f_val, int e_val) : f(f_val), e(e_val) {}

  // Constructs fp from an IEEE754 double. It is a template to prevent compile
  // errors on platforms where double is not IEEE754.
  template <typename Double> explicit fp(Double d) { assign(d); }

  // Assigns d to this and return true iff predecessor is closer than successor.
  template <typename Double, FMT_ENABLE_IF(sizeof(Double) == sizeof(uint64_t))>
  bool assign(Double d) {
    // Assume double is in the format [sign][exponent][significand].
    using limits = std::numeric_limits<Double>;
    const int exponent_size =
        bits<Double>::value - double_significand_size - 1;  // -1 for sign
    const uint64_t significand_mask = implicit_bit - 1;
    const uint64_t exponent_mask = (~0ULL >> 1) & ~significand_mask;
    const int exponent_bias = (1 << exponent_size) - limits::max_exponent - 1;
    auto u = bit_cast<uint64_t>(d);
    f = u & significand_mask;
    int biased_e =
        static_cast<int>((u & exponent_mask) >> double_significand_size);
    // Predecessor is closer if d is a normalized power of 2 (f == 0) other than
    // the smallest normalized number (biased_e > 1).
    bool is_predecessor_closer = f == 0 && biased_e > 1;
    if (biased_e != 0)
      f += implicit_bit;
    else
      biased_e = 1;  // Subnormals use biased exponent 1 (min exponent).
    e = biased_e - exponent_bias - double_significand_size;
    return is_predecessor_closer;
  }

  template <typename Double, FMT_ENABLE_IF(sizeof(Double) != sizeof(uint64_t))>
  bool assign(Double) {
    *this = fp();
    return false;
  }

  // Assigns d to this together with computing lower and upper boundaries,
  // where a boundary is a value half way between the number and its predecessor
  // (lower) or successor (upper). The upper boundary is normalized and lower
  // has the same exponent but may be not normalized.
  template <typename Double> boundaries assign_with_boundaries(Double d) {
    bool is_lower_closer = assign(d);
    fp lower =
        is_lower_closer ? fp((f << 2) - 1, e - 2) : fp((f << 1) - 1, e - 1);
    // 1 in normalize accounts for the exponent shift above.
    fp upper = normalize<1>(fp((f << 1) + 1, e - 1));
    lower.f <<= lower.e - upper.e;
    return boundaries{lower.f, upper.f};
  }

  template <typename Double> boundaries assign_float_with_boundaries(Double d) {
    assign(d);
    constexpr int min_normal_e = std::numeric_limits<float>::min_exponent -
                                 std::numeric_limits<double>::digits;
    significand_type half_ulp = 1 << (std::numeric_limits<double>::digits -
                                      std::numeric_limits<float>::digits - 1);
    if (min_normal_e > e) half_ulp <<= min_normal_e - e;
    fp upper = normalize<0>(fp(f + half_ulp, e));
    fp lower = fp(
        f - (half_ulp >> ((f == implicit_bit && e > min_normal_e) ? 1 : 0)), e);
    lower.f <<= lower.e - upper.e;
    return boundaries{lower.f, upper.f};
  }
};

// Normalizes the value converted from double and multiplied by (1 << SHIFT).
template <int SHIFT> fp normalize(fp value) {
  // Handle subnormals.
  const auto shifted_implicit_bit = fp::implicit_bit << SHIFT;
  while ((value.f & shifted_implicit_bit) == 0) {
    value.f <<= 1;
    --value.e;
  }
  // Subtract 1 to account for hidden bit.
  const auto offset =
      fp::significand_size - fp::double_significand_size - SHIFT - 1;
  value.f <<= offset;
  value.e -= offset;
  return value;
}

inline bool operator==(fp x, fp y) { return x.f == y.f && x.e == y.e; }

// Computes lhs * rhs / pow(2, 64) rounded to nearest with half-up tie breaking.
inline uint64_t multiply(uint64_t lhs, uint64_t rhs) {
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

inline fp operator*(fp x, fp y) { return {multiply(x.f, y.f), x.e + y.e + 64}; }

// Returns a cached power of 10 `c_k = c_k.f * pow(2, c_k.e)` such that its
// (binary) exponent satisfies `min_exponent <= c_k.e <= min_exponent + 28`.
inline fp get_cached_power(int min_exponent, int& pow10_exponent) {
  const int64_t one_over_log2_10 = 0x4d104d42;  // round(pow(2, 32) / log2(10))
  int index = static_cast<int>(
      ((min_exponent + fp::significand_size - 1) * one_over_log2_10 +
       ((int64_t(1) << 32) - 1))  // ceil
      >> 32                       // arithmetic shift
  );
  // Decimal exponent of the first (smallest) cached power of 10.
  const int first_dec_exp = -348;
  // Difference between 2 consecutive decimal exponents in cached powers of 10.
  const int dec_exp_step = 8;
  index = (index - first_dec_exp - 1) / dec_exp_step + 1;
  pow10_exponent = first_dec_exp + index * dec_exp_step;
  return {data::pow10_significands[index], data::pow10_exponents[index]};
}

// A simple accumulator to hold the sums of terms in bigint::square if uint128_t
// is not available.
struct accumulator {
  uint64_t lower;
  uint64_t upper;

  accumulator() : lower(0), upper(0) {}
  explicit operator uint32_t() const { return static_cast<uint32_t>(lower); }

  void operator+=(uint64_t n) {
    lower += n;
    if (lower < n) ++upper;
  }
  void operator>>=(int shift) {
    assert(shift == 32);
    (void)shift;
    lower = (upper << 32) | (lower >> 32);
    upper >>= 32;
  }
};

class bigint {
 private:
  // A bigint is stored as an array of bigits (big digits), with bigit at index
  // 0 being the least significant one.
  using bigit = uint32_t;
  using double_bigit = uint64_t;
  enum { bigits_capacity = 32 };
  basic_memory_buffer<bigit, bigits_capacity> bigits_;
  int exp_;

  bigit operator[](int index) const { return bigits_[to_unsigned(index)]; }
  bigit& operator[](int index) { return bigits_[to_unsigned(index)]; }

  static FMT_CONSTEXPR_DECL const int bigit_bits = bits<bigit>::value;

  friend struct formatter<bigint>;

  void subtract_bigits(int index, bigit other, bigit& borrow) {
    auto result = static_cast<double_bigit>((*this)[index]) - other - borrow;
    (*this)[index] = static_cast<bigit>(result);
    borrow = static_cast<bigit>(result >> (bigit_bits * 2 - 1));
  }

  void remove_leading_zeros() {
    int num_bigits = static_cast<int>(bigits_.size()) - 1;
    while (num_bigits > 0 && (*this)[num_bigits] == 0) --num_bigits;
    bigits_.resize(to_unsigned(num_bigits + 1));
  }

  // Computes *this -= other assuming aligned bigints and *this >= other.
  void subtract_aligned(const bigint& other) {
    FMT_ASSERT(other.exp_ >= exp_, "unaligned bigints");
    FMT_ASSERT(compare(*this, other) >= 0, "");
    bigit borrow = 0;
    int i = other.exp_ - exp_;
    for (size_t j = 0, n = other.bigits_.size(); j != n; ++i, ++j) {
      subtract_bigits(i, other.bigits_[j], borrow);
    }
    while (borrow > 0) subtract_bigits(i, 0, borrow);
    remove_leading_zeros();
  }

  void multiply(uint32_t value) {
    const double_bigit wide_value = value;
    bigit carry = 0;
    for (size_t i = 0, n = bigits_.size(); i < n; ++i) {
      double_bigit result = bigits_[i] * wide_value + carry;
      bigits_[i] = static_cast<bigit>(result);
      carry = static_cast<bigit>(result >> bigit_bits);
    }
    if (carry != 0) bigits_.push_back(carry);
  }

  void multiply(uint64_t value) {
    const bigit mask = ~bigit(0);
    const double_bigit lower = value & mask;
    const double_bigit upper = value >> bigit_bits;
    double_bigit carry = 0;
    for (size_t i = 0, n = bigits_.size(); i < n; ++i) {
      double_bigit result = bigits_[i] * lower + (carry & mask);
      carry =
          bigits_[i] * upper + (result >> bigit_bits) + (carry >> bigit_bits);
      bigits_[i] = static_cast<bigit>(result);
    }
    while (carry != 0) {
      bigits_.push_back(carry & mask);
      carry >>= bigit_bits;
    }
  }

 public:
  bigint() : exp_(0) {}
  explicit bigint(uint64_t n) { assign(n); }
  ~bigint() { assert(bigits_.capacity() <= bigits_capacity); }

  bigint(const bigint&) = delete;
  void operator=(const bigint&) = delete;

  void assign(const bigint& other) {
    bigits_.resize(other.bigits_.size());
    auto data = other.bigits_.data();
    std::copy(data, data + other.bigits_.size(), bigits_.data());
    exp_ = other.exp_;
  }

  void assign(uint64_t n) {
    size_t num_bigits = 0;
    do {
      bigits_[num_bigits++] = n & ~bigit(0);
      n >>= bigit_bits;
    } while (n != 0);
    bigits_.resize(num_bigits);
    exp_ = 0;
  }

  int num_bigits() const { return static_cast<int>(bigits_.size()) + exp_; }

  bigint& operator<<=(int shift) {
    assert(shift >= 0);
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

  template <typename Int> bigint& operator*=(Int value) {
    FMT_ASSERT(value > 0, "");
    multiply(uint32_or_64_or_128_t<Int>(value));
    return *this;
  }

  friend int compare(const bigint& lhs, const bigint& rhs) {
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
  friend int add_compare(const bigint& lhs1, const bigint& lhs2,
                         const bigint& rhs) {
    int max_lhs_bigits = (std::max)(lhs1.num_bigits(), lhs2.num_bigits());
    int num_rhs_bigits = rhs.num_bigits();
    if (max_lhs_bigits + 1 < num_rhs_bigits) return -1;
    if (max_lhs_bigits > num_rhs_bigits) return 1;
    auto get_bigit = [](const bigint& n, int i) -> bigit {
      return i >= n.exp_ && i < n.num_bigits() ? n[i - n.exp_] : 0;
    };
    double_bigit borrow = 0;
    int min_exp = (std::min)((std::min)(lhs1.exp_, lhs2.exp_), rhs.exp_);
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
  void assign_pow10(int exp) {
    assert(exp >= 0);
    if (exp == 0) return assign(1);
    // Find the top bit.
    int bitmask = 1;
    while (exp >= bitmask) bitmask <<= 1;
    bitmask >>= 1;
    // pow(10, exp) = pow(5, exp) * pow(2, exp). First compute pow(5, exp) by
    // repeated squaring and multiplication.
    assign(5);
    bitmask >>= 1;
    while (bitmask != 0) {
      square();
      if ((exp & bitmask) != 0) *this *= 5;
      bitmask >>= 1;
    }
    *this <<= exp;  // Multiply by pow(2, exp) by shifting.
  }

  void square() {
    basic_memory_buffer<bigit, bigits_capacity> n(std::move(bigits_));
    int num_bigits = static_cast<int>(bigits_.size());
    int num_result_bigits = 2 * num_bigits;
    bigits_.resize(to_unsigned(num_result_bigits));
    using accumulator_t = conditional_t<FMT_USE_INT128, uint128_t, accumulator>;
    auto sum = accumulator_t();
    for (int bigit_index = 0; bigit_index < num_bigits; ++bigit_index) {
      // Compute bigit at position bigit_index of the result by adding
      // cross-product terms n[i] * n[j] such that i + j == bigit_index.
      for (int i = 0, j = bigit_index; j >= 0; ++i, --j) {
        // Most terms are multiplied twice which can be optimized in the future.
        sum += static_cast<double_bigit>(n[i]) * n[j];
      }
      (*this)[bigit_index] = static_cast<bigit>(sum);
      sum >>= bits<bigit>::value;  // Compute the carry.
    }
    // Do the same for the top half.
    for (int bigit_index = num_bigits; bigit_index < num_result_bigits;
         ++bigit_index) {
      for (int j = num_bigits - 1, i = bigit_index - j; i < num_bigits;)
        sum += static_cast<double_bigit>(n[i++]) * n[j--];
      (*this)[bigit_index] = static_cast<bigit>(sum);
      sum >>= bits<bigit>::value;
    }
    --num_result_bigits;
    remove_leading_zeros();
    exp_ *= 2;
  }

  // Divides this bignum by divisor, assigning the remainder to this and
  // returning the quotient.
  int divmod_assign(const bigint& divisor) {
    FMT_ASSERT(this != &divisor, "");
    if (compare(*this, divisor) < 0) return 0;
    int num_bigits = static_cast<int>(bigits_.size());
    FMT_ASSERT(divisor.bigits_[divisor.bigits_.size() - 1u] != 0, "");
    int exp_difference = exp_ - divisor.exp_;
    if (exp_difference > 0) {
      // Align bigints by adding trailing zeros to simplify subtraction.
      bigits_.resize(to_unsigned(num_bigits + exp_difference));
      for (int i = num_bigits - 1, j = i + exp_difference; i >= 0; --i, --j)
        bigits_[j] = bigits_[i];
      std::uninitialized_fill_n(bigits_.data(), exp_difference, 0);
      exp_ -= exp_difference;
    }
    int quotient = 0;
    do {
      subtract_aligned(divisor);
      ++quotient;
    } while (compare(*this, divisor) >= 0);
    return quotient;
  }
};

enum class round_direction { unknown, up, down };

// Given the divisor (normally a power of 10), the remainder = v % divisor for
// some number v and the error, returns whether v should be rounded up, down, or
// whether the rounding direction can't be determined due to error.
// error should be less than divisor / 2.
inline round_direction get_round_direction(uint64_t divisor, uint64_t remainder,
                                           uint64_t error) {
  FMT_ASSERT(remainder < divisor, "");  // divisor - remainder won't overflow.
  FMT_ASSERT(error < divisor, "");      // divisor - error won't overflow.
  FMT_ASSERT(error < divisor - error, "");  // error * 2 won't overflow.
  // Round down if (remainder + error) * 2 <= divisor.
  if (remainder <= divisor - remainder && error * 2 <= divisor - remainder * 2)
    return round_direction::down;
  // Round up if (remainder - error) * 2 >= divisor.
  if (remainder >= error &&
      remainder - error >= divisor - (remainder - error)) {
    return round_direction::up;
  }
  return round_direction::unknown;
}

namespace digits {
enum result {
  more,  // Generate more digits.
  done,  // Done generating digits.
  error  // Digit generation cancelled due to an error.
};
}

// A version of count_digits optimized for grisu_gen_digits.
inline int grisu_count_digits(uint32_t n) {
  if (n < 10) return 1;
  if (n < 100) return 2;
  if (n < 1000) return 3;
  if (n < 10000) return 4;
  if (n < 100000) return 5;
  if (n < 1000000) return 6;
  if (n < 10000000) return 7;
  if (n < 100000000) return 8;
  if (n < 1000000000) return 9;
  return 10;
}

// Generates output using the Grisu digit-gen algorithm.
// error: the size of the region (lower, upper) outside of which numbers
// definitely do not round to value (Delta in Grisu3).
template <typename Handler>
FMT_ALWAYS_INLINE digits::result grisu_gen_digits(fp value, uint64_t error,
                                                  int& exp, Handler& handler) {
  const fp one(1ULL << -value.e, value.e);
  // The integral part of scaled value (p1 in Grisu) = value / one. It cannot be
  // zero because it contains a product of two 64-bit numbers with MSB set (due
  // to normalization) - 1, shifted right by at most 60 bits.
  auto integral = static_cast<uint32_t>(value.f >> -one.e);
  FMT_ASSERT(integral != 0, "");
  FMT_ASSERT(integral == value.f >> -one.e, "");
  // The fractional part of scaled value (p2 in Grisu) c = value % one.
  uint64_t fractional = value.f & (one.f - 1);
  exp = grisu_count_digits(integral);  // kappa in Grisu.
  // Divide by 10 to prevent overflow.
  auto result = handler.on_start(data::powers_of_10_64[exp - 1] << -one.e,
                                 value.f / 10, error * 10, exp);
  if (result != digits::more) return result;
  // Generate digits for the integral part. This can produce up to 10 digits.
  do {
    uint32_t digit = 0;
    auto divmod_integral = [&](uint32_t divisor) {
      digit = integral / divisor;
      integral %= divisor;
    };
    // This optimization by Milo Yip reduces the number of integer divisions by
    // one per iteration.
    switch (exp) {
    case 10:
      divmod_integral(1000000000);
      break;
    case 9:
      divmod_integral(100000000);
      break;
    case 8:
      divmod_integral(10000000);
      break;
    case 7:
      divmod_integral(1000000);
      break;
    case 6:
      divmod_integral(100000);
      break;
    case 5:
      divmod_integral(10000);
      break;
    case 4:
      divmod_integral(1000);
      break;
    case 3:
      divmod_integral(100);
      break;
    case 2:
      divmod_integral(10);
      break;
    case 1:
      digit = integral;
      integral = 0;
      break;
    default:
      FMT_ASSERT(false, "invalid number of digits");
    }
    --exp;
    uint64_t remainder =
        (static_cast<uint64_t>(integral) << -one.e) + fractional;
    result = handler.on_digit(static_cast<char>('0' + digit),
                              data::powers_of_10_64[exp] << -one.e, remainder,
                              error, exp, true);
    if (result != digits::more) return result;
  } while (exp > 0);
  // Generate digits for the fractional part.
  for (;;) {
    fractional *= 10;
    error *= 10;
    char digit =
        static_cast<char>('0' + static_cast<char>(fractional >> -one.e));
    fractional &= one.f - 1;
    --exp;
    result = handler.on_digit(digit, one.f, fractional, error, exp, false);
    if (result != digits::more) return result;
  }
}

// The fixed precision digit handler.
struct fixed_handler {
  char* buf;
  int size;
  int precision;
  int exp10;
  bool fixed;

  digits::result on_start(uint64_t divisor, uint64_t remainder, uint64_t error,
                          int& exp) {
    // Non-fixed formats require at least one digit and no precision adjustment.
    if (!fixed) return digits::more;
    // Adjust fixed precision by exponent because it is relative to decimal
    // point.
    precision += exp + exp10;
    // Check if precision is satisfied just by leading zeros, e.g.
    // format("{:.2f}", 0.001) gives "0.00" without generating any digits.
    if (precision > 0) return digits::more;
    if (precision < 0) return digits::done;
    auto dir = get_round_direction(divisor, remainder, error);
    if (dir == round_direction::unknown) return digits::error;
    buf[size++] = dir == round_direction::up ? '1' : '0';
    return digits::done;
  }

  digits::result on_digit(char digit, uint64_t divisor, uint64_t remainder,
                          uint64_t error, int, bool integral) {
    FMT_ASSERT(remainder < divisor, "");
    buf[size++] = digit;
    if (size < precision) return digits::more;
    if (!integral) {
      // Check if error * 2 < divisor with overflow prevention.
      // The check is not needed for the integral part because error = 1
      // and divisor > (1 << 32) there.
      if (error >= divisor || error >= divisor - error) return digits::error;
    } else {
      FMT_ASSERT(error == 1 && divisor > 2, "");
    }
    auto dir = get_round_direction(divisor, remainder, error);
    if (dir != round_direction::up)
      return dir == round_direction::down ? digits::done : digits::error;
    ++buf[size - 1];
    for (int i = size - 1; i > 0 && buf[i] > '9'; --i) {
      buf[i] = '0';
      ++buf[i - 1];
    }
    if (buf[0] > '9') {
      buf[0] = '1';
      buf[size++] = '0';
    }
    return digits::done;
  }
};

// The shortest representation digit handler.
struct grisu_shortest_handler {
  char* buf;
  int size;
  // Distance between scaled value and upper bound (wp_W in Grisu3).
  uint64_t diff;

  digits::result on_start(uint64_t, uint64_t, uint64_t, int&) {
    return digits::more;
  }

  // Decrement the generated number approaching value from above.
  void round(uint64_t d, uint64_t divisor, uint64_t& remainder,
             uint64_t error) {
    while (
        remainder < d && error - remainder >= divisor &&
        (remainder + divisor < d || d - remainder >= remainder + divisor - d)) {
      --buf[size - 1];
      remainder += divisor;
    }
  }

  // Implements Grisu's round_weed.
  digits::result on_digit(char digit, uint64_t divisor, uint64_t remainder,
                          uint64_t error, int exp, bool integral) {
    buf[size++] = digit;
    if (remainder >= error) return digits::more;
    uint64_t unit = integral ? 1 : data::powers_of_10_64[-exp];
    uint64_t up = (diff - 1) * unit;  // wp_Wup
    round(up, divisor, remainder, error);
    uint64_t down = (diff + 1) * unit;  // wp_Wdown
    if (remainder < down && error - remainder >= divisor &&
        (remainder + divisor < down ||
         down - remainder > remainder + divisor - down)) {
      return digits::error;
    }
    return 2 * unit <= remainder && remainder <= error - 4 * unit
               ? digits::done
               : digits::error;
  }
};

// Formats value using a variation of the Fixed-Precision Positive
// Floating-Point Printout ((FPP)^2) algorithm by Steele & White:
// https://fmt.dev/p372-steele.pdf.
template <typename Double>
void fallback_format(Double d, buffer<char>& buf, int& exp10) {
  bigint numerator;    // 2 * R in (FPP)^2.
  bigint denominator;  // 2 * S in (FPP)^2.
  // lower and upper are differences between value and corresponding boundaries.
  bigint lower;             // (M^- in (FPP)^2).
  bigint upper_store;       // upper's value if different from lower.
  bigint* upper = nullptr;  // (M^+ in (FPP)^2).
  fp value;
  // Shift numerator and denominator by an extra bit or two (if lower boundary
  // is closer) to make lower and upper integers. This eliminates multiplication
  // by 2 during later computations.
  // TODO: handle float
  int shift = value.assign(d) ? 2 : 1;
  uint64_t significand = value.f << shift;
  if (value.e >= 0) {
    numerator.assign(significand);
    numerator <<= value.e;
    lower.assign(1);
    lower <<= value.e;
    if (shift != 1) {
      upper_store.assign(1);
      upper_store <<= value.e + 1;
      upper = &upper_store;
    }
    denominator.assign_pow10(exp10);
    denominator <<= 1;
  } else if (exp10 < 0) {
    numerator.assign_pow10(-exp10);
    lower.assign(numerator);
    if (shift != 1) {
      upper_store.assign(numerator);
      upper_store <<= 1;
      upper = &upper_store;
    }
    numerator *= significand;
    denominator.assign(1);
    denominator <<= shift - value.e;
  } else {
    numerator.assign(significand);
    denominator.assign_pow10(exp10);
    denominator <<= shift - value.e;
    lower.assign(1);
    if (shift != 1) {
      upper_store.assign(1ULL << 1);
      upper = &upper_store;
    }
  }
  if (!upper) upper = &lower;
  // Invariant: value == (numerator / denominator) * pow(10, exp10).
  bool even = (value.f & 1) == 0;
  int num_digits = 0;
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
      buf.resize(to_unsigned(num_digits));
      exp10 -= num_digits - 1;
      return;
    }
    numerator *= 10;
    lower *= 10;
    if (upper != &lower) *upper *= 10;
  }
}

// Formats value using the Grisu algorithm
// (https://www.cs.tufts.edu/~nr/cs257/archive/florian-loitsch/printf.pdf)
// if T is a IEEE754 binary32 or binary64 and snprintf otherwise.
template <typename T>
int format_float(T value, int precision, float_specs specs, buffer<char>& buf) {
  static_assert(!std::is_same<T, float>::value, "");
  FMT_ASSERT(value >= 0, "value is negative");

  const bool fixed = specs.format == float_format::fixed;
  if (value <= 0) {  // <= instead of == to silence a warning.
    if (precision <= 0 || !fixed) {
      buf.push_back('0');
      return 0;
    }
    buf.resize(to_unsigned(precision));
    std::uninitialized_fill_n(buf.data(), precision, '0');
    return -precision;
  }

  if (!specs.use_grisu) return snprintf_float(value, precision, specs, buf);

  int exp = 0;
  const int min_exp = -60;  // alpha in Grisu.
  int cached_exp10 = 0;     // K in Grisu.
  if (precision < 0) {
    fp fp_value;
    auto boundaries = specs.binary32
                          ? fp_value.assign_float_with_boundaries(value)
                          : fp_value.assign_with_boundaries(value);
    fp_value = normalize(fp_value);
    // Find a cached power of 10 such that multiplying value by it will bring
    // the exponent in the range [min_exp, -32].
    const fp cached_pow = get_cached_power(
        min_exp - (fp_value.e + fp::significand_size), cached_exp10);
    // Multiply value and boundaries by the cached power of 10.
    fp_value = fp_value * cached_pow;
    boundaries.lower = multiply(boundaries.lower, cached_pow.f);
    boundaries.upper = multiply(boundaries.upper, cached_pow.f);
    assert(min_exp <= fp_value.e && fp_value.e <= -32);
    --boundaries.lower;  // \tilde{M}^- - 1 ulp -> M^-_{\downarrow}.
    ++boundaries.upper;  // \tilde{M}^+ + 1 ulp -> M^+_{\uparrow}.
    // Numbers outside of (lower, upper) definitely do not round to value.
    grisu_shortest_handler handler{buf.data(), 0,
                                   boundaries.upper - fp_value.f};
    auto result =
        grisu_gen_digits(fp(boundaries.upper, fp_value.e),
                         boundaries.upper - boundaries.lower, exp, handler);
    if (result == digits::error) {
      exp += handler.size - cached_exp10 - 1;
      fallback_format(value, buf, exp);
      return exp;
    }
    buf.resize(to_unsigned(handler.size));
  } else {
    if (precision > 17) return snprintf_float(value, precision, specs, buf);
    fp normalized = normalize(fp(value));
    const auto cached_pow = get_cached_power(
        min_exp - (normalized.e + fp::significand_size), cached_exp10);
    normalized = normalized * cached_pow;
    fixed_handler handler{buf.data(), 0, precision, -cached_exp10, fixed};
    if (grisu_gen_digits(normalized, 1, exp, handler) == digits::error)
      return snprintf_float(value, precision, specs, buf);
    int num_digits = handler.size;
    if (!fixed) {
      // Remove trailing zeros.
      while (num_digits > 0 && buf[num_digits - 1] == '0') {
        --num_digits;
        ++exp;
      }
    }
    buf.resize(to_unsigned(num_digits));
  }
  return exp - cached_exp10;
}

template <typename T>
int snprintf_float(T value, int precision, float_specs specs,
                   buffer<char>& buf) {
  // Buffer capacity must be non-zero, otherwise MSVC's vsnprintf_s will fail.
  FMT_ASSERT(buf.capacity() > buf.size(), "empty buffer");
  static_assert(!std::is_same<T, float>::value, "");

  // Subtract 1 to account for the difference in precision since we use %e for
  // both general and exponent format.
  if (specs.format == float_format::general ||
      specs.format == float_format::exp)
    precision = (precision >= 0 ? precision : 6) - 1;

  // Build the format string.
  enum { max_format_size = 7 };  // Ths longest format is "%#.*Le".
  char format[max_format_size];
  char* format_ptr = format;
  *format_ptr++ = '%';
  if (specs.showpoint && specs.format == float_format::hex) *format_ptr++ = '#';
  if (precision >= 0) {
    *format_ptr++ = '.';
    *format_ptr++ = '*';
  }
  if (std::is_same<T, long double>()) *format_ptr++ = 'L';
  *format_ptr++ = specs.format != float_format::hex
                      ? (specs.format == float_format::fixed ? 'f' : 'e')
                      : (specs.upper ? 'A' : 'a');
  *format_ptr = '\0';

  // Format using snprintf.
  auto offset = buf.size();
  for (;;) {
    auto begin = buf.data() + offset;
    auto capacity = buf.capacity() - offset;
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (precision > 100000)
      throw std::runtime_error(
          "fuzz mode - avoid large allocation inside snprintf");
#endif
    // Suppress the warning about a nonliteral format string.
    // Cannot use auto becase of a bug in MinGW (#1532).
    int (*snprintf_ptr)(char*, size_t, const char*, ...) = FMT_SNPRINTF;
    int result = precision >= 0
                     ? snprintf_ptr(begin, capacity, format, precision, value)
                     : snprintf_ptr(begin, capacity, format, value);
    if (result < 0) {
      buf.reserve(buf.capacity() + 1);  // The buffer will grow exponentially.
      continue;
    }
    auto size = to_unsigned(result);
    // Size equal to capacity means that the last character was truncated.
    if (size >= capacity) {
      buf.reserve(size + offset + 1);  // Add 1 for the terminating '\0'.
      continue;
    }
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    if (specs.format == float_format::fixed) {
      if (precision == 0) {
        buf.resize(size);
        return 0;
      }
      // Find and remove the decimal point.
      auto end = begin + size, p = end;
      do {
        --p;
      } while (is_digit(*p));
      int fraction_size = static_cast<int>(end - p - 1);
      std::memmove(p, p + 1, to_unsigned(fraction_size));
      buf.resize(size - 1);
      return -fraction_size;
    }
    if (specs.format == float_format::hex) {
      buf.resize(size + offset);
      return 0;
    }
    // Find and parse the exponent.
    auto end = begin + size, exp_pos = end;
    do {
      --exp_pos;
    } while (*exp_pos != 'e');
    char sign = exp_pos[1];
    assert(sign == '+' || sign == '-');
    int exp = 0;
    auto p = exp_pos + 2;  // Skip 'e' and sign.
    do {
      assert(is_digit(*p));
      exp = exp * 10 + (*p++ - '0');
    } while (p != end);
    if (sign == '-') exp = -exp;
    int fraction_size = 0;
    if (exp_pos != begin + 1) {
      // Remove trailing zeros.
      auto fraction_end = exp_pos - 1;
      while (*fraction_end == '0') --fraction_end;
      // Move the fractional part left to get rid of the decimal point.
      fraction_size = static_cast<int>(fraction_end - begin - 1);
      std::memmove(begin + 1, begin + 2, to_unsigned(fraction_size));
    }
    buf.resize(to_unsigned(fraction_size) + offset + 1);
    return exp - fraction_size;
  }
}

// A public domain branchless UTF-8 decoder by Christopher Wellons:
// https://github.com/skeeto/branchless-utf8
/* Decode the next character, c, from buf, reporting errors in e.
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
FMT_FUNC const char* utf8_decode(const char* buf, uint32_t* c, int* e) {
  static const char lengths[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
                                 0, 0, 2, 2, 2, 2, 3, 3, 4, 0};
  static const int masks[] = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
  static const uint32_t mins[] = {4194304, 0, 128, 2048, 65536};
  static const int shiftc[] = {0, 18, 12, 6, 0};
  static const int shifte[] = {0, 6, 4, 2, 0};

  auto s = reinterpret_cast<const unsigned char*>(buf);
  int len = lengths[s[0] >> 3];

  // Compute the pointer to the next character early so that the next
  // iteration can start working on the next character. Neither Clang
  // nor GCC figure out this reordering on their own.
  const char* next = buf + len + !len;

  // Assume a four-byte character and load four bytes. Unused bits are
  // shifted out.
  *c = uint32_t(s[0] & masks[len]) << 18;
  *c |= uint32_t(s[1] & 0x3f) << 12;
  *c |= uint32_t(s[2] & 0x3f) << 6;
  *c |= uint32_t(s[3] & 0x3f) << 0;
  *c >>= shiftc[len];

  // Accumulate the various error conditions.
  *e = (*c < mins[len]) << 6;       // non-canonical encoding
  *e |= ((*c >> 11) == 0x1b) << 7;  // surrogate half?
  *e |= (*c > 0x10FFFF) << 8;       // out of range?
  *e |= (s[1] & 0xc0) >> 2;
  *e |= (s[2] & 0xc0) >> 4;
  *e |= (s[3]) >> 6;
  *e ^= 0x2a;  // top two bits of each tail byte correct?
  *e >>= shifte[len];

  return next;
}
}  // namespace internal

template <> struct formatter<internal::bigint> {
  format_parse_context::iterator parse(format_parse_context& ctx) {
    return ctx.begin();
  }

  format_context::iterator format(const internal::bigint& n,
                                  format_context& ctx) {
    auto out = ctx.out();
    bool first = true;
    for (auto i = n.bigits_.size(); i > 0; --i) {
      auto value = n.bigits_[i - 1u];
      if (first) {
        out = format_to(out, "{:x}", value);
        first = false;
        continue;
      }
      out = format_to(out, "{:08x}", value);
    }
    if (n.exp_ > 0)
      out = format_to(out, "p{}", n.exp_ * internal::bigint::bigit_bits);
    return out;
  }
};

FMT_FUNC internal::utf8_to_utf16::utf8_to_utf16(string_view s) {
  auto transcode = [this](const char* p) {
    auto cp = uint32_t();
    auto error = 0;
    p = utf8_decode(p, &cp, &error);
    if (error != 0) FMT_THROW(std::runtime_error("invalid utf8"));
    if (cp <= 0xFFFF) {
      buffer_.push_back(static_cast<wchar_t>(cp));
    } else {
      cp -= 0x10000;
      buffer_.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
      buffer_.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
    }
    return p;
  };
  auto p = s.data();
  const size_t block_size = 4;  // utf8_decode always reads blocks of 4 chars.
  if (s.size() >= block_size) {
    for (auto end = p + s.size() - block_size + 1; p < end;) p = transcode(p);
  }
  if (auto num_chars_left = s.data() + s.size() - p) {
    char buf[2 * block_size - 1] = {};
    memcpy(buf, p, to_unsigned(num_chars_left));
    p = buf;
    do {
      p = transcode(p);
    } while (p - buf < num_chars_left);
  }
  buffer_.push_back(0);
}

FMT_FUNC void format_system_error(internal::buffer<char>& out, int error_code,
                                  string_view message) FMT_NOEXCEPT {
  FMT_TRY {
    memory_buffer buf;
    buf.resize(inline_buffer_size);
    for (;;) {
      char* system_message = &buf[0];
      int result =
          internal::safe_strerror(error_code, system_message, buf.size());
      if (result == 0) {
        internal::writer w(out);
        w.write(message);
        w.write(": ");
        w.write(system_message);
        return;
      }
      if (result != ERANGE)
        break;  // Can't get error message, report error code instead.
      buf.resize(buf.size() * 2);
    }
  }
  FMT_CATCH(...) {}
  format_error_code(out, error_code, message);
}

FMT_FUNC void internal::error_handler::on_error(const char* message) {
  FMT_THROW(format_error(message));
}

FMT_FUNC void report_system_error(int error_code,
                                  fmt::string_view message) FMT_NOEXCEPT {
  report_error(format_system_error, error_code, message);
}

FMT_FUNC void vprint(std::FILE* f, string_view format_str, format_args args) {
  memory_buffer buffer;
  internal::vformat_to(buffer, format_str,
                       basic_format_args<buffer_context<char>>(args));
#ifdef _WIN32
  auto fd = _fileno(f);
  if (_isatty(fd)) {
    internal::utf8_to_utf16 u16(string_view(buffer.data(), buffer.size()));
    auto written = DWORD();
    if (!WriteConsoleW(reinterpret_cast<HANDLE>(_get_osfhandle(fd)),
                       u16.c_str(), static_cast<DWORD>(u16.size()), &written,
                       nullptr)) {
      FMT_THROW(format_error("failed to write to console"));
    }
    return;
  }
#endif
  internal::fwrite_fully(buffer.data(), 1, buffer.size(), f);
}

#ifdef _WIN32
// Print assuming legacy (non-Unicode) encoding.
FMT_FUNC void internal::vprint_mojibake(std::FILE* f, string_view format_str,
                                        format_args args) {
  memory_buffer buffer;
  internal::vformat_to(buffer, format_str,
                       basic_format_args<buffer_context<char>>(args));
  fwrite_fully(buffer.data(), 1, buffer.size(), f);
}
#endif

FMT_FUNC void vprint(string_view format_str, format_args args) {
  vprint(stdout, format_str, args);
}

FMT_END_NAMESPACE

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#endif  // FMT_FORMAT_INL_H_
