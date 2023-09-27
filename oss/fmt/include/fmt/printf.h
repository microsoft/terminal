// Formatting library for C++ - legacy printf implementation
//
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_PRINTF_H_
#define FMT_PRINTF_H_

#include <algorithm>  // std::max
#include <limits>     // std::numeric_limits

#include "format.h"

FMT_BEGIN_NAMESPACE
FMT_BEGIN_EXPORT

template <typename T> struct printf_formatter { printf_formatter() = delete; };

template <typename Char> class basic_printf_context {
 private:
  detail::buffer_appender<Char> out_;
  basic_format_args<basic_printf_context> args_;

 public:
  using char_type = Char;
  using parse_context_type = basic_format_parse_context<Char>;
  template <typename T> using formatter_type = printf_formatter<T>;

  /**
    \rst
    Constructs a ``printf_context`` object. References to the arguments are
    stored in the context object so make sure they have appropriate lifetimes.
    \endrst
   */
  basic_printf_context(detail::buffer_appender<Char> out,
                       basic_format_args<basic_printf_context> args)
      : out_(out), args_(args) {}

  auto out() -> detail::buffer_appender<Char> { return out_; }
  void advance_to(detail::buffer_appender<Char>) {}

  auto locale() -> detail::locale_ref { return {}; }

  auto arg(int id) const -> basic_format_arg<basic_printf_context> {
    return args_.get(id);
  }

  FMT_CONSTEXPR void on_error(const char* message) {
    detail::error_handler().on_error(message);
  }
};

namespace detail {

// Checks if a value fits in int - used to avoid warnings about comparing
// signed and unsigned integers.
template <bool IsSigned> struct int_checker {
  template <typename T> static auto fits_in_int(T value) -> bool {
    unsigned max = max_value<int>();
    return value <= max;
  }
  static auto fits_in_int(bool) -> bool { return true; }
};

template <> struct int_checker<true> {
  template <typename T> static auto fits_in_int(T value) -> bool {
    return value >= (std::numeric_limits<int>::min)() &&
           value <= max_value<int>();
  }
  static auto fits_in_int(int) -> bool { return true; }
};

struct printf_precision_handler {
  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  auto operator()(T value) -> int {
    if (!int_checker<std::numeric_limits<T>::is_signed>::fits_in_int(value))
      throw_format_error("number is too big");
    return (std::max)(static_cast<int>(value), 0);
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  auto operator()(T) -> int {
    throw_format_error("precision is not integer");
    return 0;
  }
};

// An argument visitor that returns true iff arg is a zero integer.
struct is_zero_int {
  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  auto operator()(T value) -> bool {
    return value == 0;
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  auto operator()(T) -> bool {
    return false;
  }
};

template <typename T> struct make_unsigned_or_bool : std::make_unsigned<T> {};

template <> struct make_unsigned_or_bool<bool> { using type = bool; };

template <typename T, typename Context> class arg_converter {
 private:
  using char_type = typename Context::char_type;

  basic_format_arg<Context>& arg_;
  char_type type_;

 public:
  arg_converter(basic_format_arg<Context>& arg, char_type type)
      : arg_(arg), type_(type) {}

  void operator()(bool value) {
    if (type_ != 's') operator()<bool>(value);
  }

  template <typename U, FMT_ENABLE_IF(std::is_integral<U>::value)>
  void operator()(U value) {
    bool is_signed = type_ == 'd' || type_ == 'i';
    using target_type = conditional_t<std::is_same<T, void>::value, U, T>;
    if (const_check(sizeof(target_type) <= sizeof(int))) {
      // Extra casts are used to silence warnings.
      if (is_signed) {
        auto n = static_cast<int>(static_cast<target_type>(value));
        arg_ = detail::make_arg<Context>(n);
      } else {
        using unsigned_type = typename make_unsigned_or_bool<target_type>::type;
        auto n = static_cast<unsigned>(static_cast<unsigned_type>(value));
        arg_ = detail::make_arg<Context>(n);
      }
    } else {
      if (is_signed) {
        // glibc's printf doesn't sign extend arguments of smaller types:
        //   std::printf("%lld", -42);  // prints "4294967254"
        // but we don't have to do the same because it's a UB.
        auto n = static_cast<long long>(value);
        arg_ = detail::make_arg<Context>(n);
      } else {
        auto n = static_cast<typename make_unsigned_or_bool<U>::type>(value);
        arg_ = detail::make_arg<Context>(n);
      }
    }
  }

  template <typename U, FMT_ENABLE_IF(!std::is_integral<U>::value)>
  void operator()(U) {}  // No conversion needed for non-integral types.
};

// Converts an integer argument to T for printf, if T is an integral type.
// If T is void, the argument is converted to corresponding signed or unsigned
// type depending on the type specifier: 'd' and 'i' - signed, other -
// unsigned).
template <typename T, typename Context, typename Char>
void convert_arg(basic_format_arg<Context>& arg, Char type) {
  visit_format_arg(arg_converter<T, Context>(arg, type), arg);
}

// Converts an integer argument to char for printf.
template <typename Context> class char_converter {
 private:
  basic_format_arg<Context>& arg_;

 public:
  explicit char_converter(basic_format_arg<Context>& arg) : arg_(arg) {}

  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  void operator()(T value) {
    auto c = static_cast<typename Context::char_type>(value);
    arg_ = detail::make_arg<Context>(c);
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  void operator()(T) {}  // No conversion needed for non-integral types.
};

// An argument visitor that return a pointer to a C string if argument is a
// string or null otherwise.
template <typename Char> struct get_cstring {
  template <typename T> auto operator()(T) -> const Char* { return nullptr; }
  auto operator()(const Char* s) -> const Char* { return s; }
};

// Checks if an argument is a valid printf width specifier and sets
// left alignment if it is negative.
template <typename Char> class printf_width_handler {
 private:
  format_specs<Char>& specs_;

 public:
  explicit printf_width_handler(format_specs<Char>& specs) : specs_(specs) {}

  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  auto operator()(T value) -> unsigned {
    auto width = static_cast<uint32_or_64_or_128_t<T>>(value);
    if (detail::is_negative(value)) {
      specs_.align = align::left;
      width = 0 - width;
    }
    unsigned int_max = max_value<int>();
    if (width > int_max) throw_format_error("number is too big");
    return static_cast<unsigned>(width);
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  auto operator()(T) -> unsigned {
    throw_format_error("width is not integer");
    return 0;
  }
};

// Workaround for a bug with the XL compiler when initializing
// printf_arg_formatter's base class.
template <typename Char>
auto make_arg_formatter(buffer_appender<Char> iter, format_specs<Char>& s)
    -> arg_formatter<Char> {
  return {iter, s, locale_ref()};
}

// The ``printf`` argument formatter.
template <typename Char>
class printf_arg_formatter : public arg_formatter<Char> {
 private:
  using base = arg_formatter<Char>;
  using context_type = basic_printf_context<Char>;

  context_type& context_;

  void write_null_pointer(bool is_string = false) {
    auto s = this->specs;
    s.type = presentation_type::none;
    write_bytes(this->out, is_string ? "(null)" : "(nil)", s);
  }

 public:
  printf_arg_formatter(buffer_appender<Char> iter, format_specs<Char>& s,
                       context_type& ctx)
      : base(make_arg_formatter(iter, s)), context_(ctx) {}

  void operator()(monostate value) { base::operator()(value); }

  template <typename T, FMT_ENABLE_IF(detail::is_integral<T>::value)>
  void operator()(T value) {
    // MSVC2013 fails to compile separate overloads for bool and Char so use
    // std::is_same instead.
    if (!std::is_same<T, Char>::value) {
      base::operator()(value);
      return;
    }
    format_specs<Char> fmt_specs = this->specs;
    if (fmt_specs.type != presentation_type::none &&
        fmt_specs.type != presentation_type::chr) {
      return (*this)(static_cast<int>(value));
    }
    fmt_specs.sign = sign::none;
    fmt_specs.alt = false;
    fmt_specs.fill[0] = ' ';  // Ignore '0' flag for char types.
    // align::numeric needs to be overwritten here since the '0' flag is
    // ignored for non-numeric types
    if (fmt_specs.align == align::none || fmt_specs.align == align::numeric)
      fmt_specs.align = align::right;
    write<Char>(this->out, static_cast<Char>(value), fmt_specs);
  }

  template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value)>
  void operator()(T value) {
    base::operator()(value);
  }

  /** Formats a null-terminated C string. */
  void operator()(const char* value) {
    if (value)
      base::operator()(value);
    else
      write_null_pointer(this->specs.type != presentation_type::pointer);
  }

  /** Formats a null-terminated wide C string. */
  void operator()(const wchar_t* value) {
    if (value)
      base::operator()(value);
    else
      write_null_pointer(this->specs.type != presentation_type::pointer);
  }

  void operator()(basic_string_view<Char> value) { base::operator()(value); }

  /** Formats a pointer. */
  void operator()(const void* value) {
    if (value)
      base::operator()(value);
    else
      write_null_pointer();
  }

  /** Formats an argument of a custom (user-defined) type. */
  void operator()(typename basic_format_arg<context_type>::handle handle) {
    auto parse_ctx = basic_format_parse_context<Char>({});
    handle.format(parse_ctx, context_);
  }
};

template <typename Char>
void parse_flags(format_specs<Char>& specs, const Char*& it, const Char* end) {
  for (; it != end; ++it) {
    switch (*it) {
    case '-':
      specs.align = align::left;
      break;
    case '+':
      specs.sign = sign::plus;
      break;
    case '0':
      specs.fill[0] = '0';
      break;
    case ' ':
      if (specs.sign != sign::plus) specs.sign = sign::space;
      break;
    case '#':
      specs.alt = true;
      break;
    default:
      return;
    }
  }
}

template <typename Char, typename GetArg>
auto parse_header(const Char*& it, const Char* end, format_specs<Char>& specs,
                  GetArg get_arg) -> int {
  int arg_index = -1;
  Char c = *it;
  if (c >= '0' && c <= '9') {
    // Parse an argument index (if followed by '$') or a width possibly
    // preceded with '0' flag(s).
    int value = parse_nonnegative_int(it, end, -1);
    if (it != end && *it == '$') {  // value is an argument index
      ++it;
      arg_index = value != -1 ? value : max_value<int>();
    } else {
      if (c == '0') specs.fill[0] = '0';
      if (value != 0) {
        // Nonzero value means that we parsed width and don't need to
        // parse it or flags again, so return now.
        if (value == -1) throw_format_error("number is too big");
        specs.width = value;
        return arg_index;
      }
    }
  }
  parse_flags(specs, it, end);
  // Parse width.
  if (it != end) {
    if (*it >= '0' && *it <= '9') {
      specs.width = parse_nonnegative_int(it, end, -1);
      if (specs.width == -1) throw_format_error("number is too big");
    } else if (*it == '*') {
      ++it;
      specs.width = static_cast<int>(visit_format_arg(
          detail::printf_width_handler<Char>(specs), get_arg(-1)));
    }
  }
  return arg_index;
}

inline auto parse_printf_presentation_type(char c, type t)
    -> presentation_type {
  using pt = presentation_type;
  constexpr auto integral_set = sint_set | uint_set | bool_set | char_set;
  switch (c) {
  case 'd':
    return in(t, integral_set) ? pt::dec : pt::none;
  case 'o':
    return in(t, integral_set) ? pt::oct : pt::none;
  case 'x':
    return in(t, integral_set) ? pt::hex_lower : pt::none;
  case 'X':
    return in(t, integral_set) ? pt::hex_upper : pt::none;
  case 'a':
    return in(t, float_set) ? pt::hexfloat_lower : pt::none;
  case 'A':
    return in(t, float_set) ? pt::hexfloat_upper : pt::none;
  case 'e':
    return in(t, float_set) ? pt::exp_lower : pt::none;
  case 'E':
    return in(t, float_set) ? pt::exp_upper : pt::none;
  case 'f':
    return in(t, float_set) ? pt::fixed_lower : pt::none;
  case 'F':
    return in(t, float_set) ? pt::fixed_upper : pt::none;
  case 'g':
    return in(t, float_set) ? pt::general_lower : pt::none;
  case 'G':
    return in(t, float_set) ? pt::general_upper : pt::none;
  case 'c':
    return in(t, integral_set) ? pt::chr : pt::none;
  case 's':
    return in(t, string_set | cstring_set) ? pt::string : pt::none;
  case 'p':
    return in(t, pointer_set | cstring_set) ? pt::pointer : pt::none;
  default:
    return pt::none;
  }
}

template <typename Char, typename Context>
void vprintf(buffer<Char>& buf, basic_string_view<Char> format,
             basic_format_args<Context> args) {
  using iterator = buffer_appender<Char>;
  auto out = iterator(buf);
  auto context = basic_printf_context<Char>(out, args);
  auto parse_ctx = basic_format_parse_context<Char>(format);

  // Returns the argument with specified index or, if arg_index is -1, the next
  // argument.
  auto get_arg = [&](int arg_index) {
    if (arg_index < 0)
      arg_index = parse_ctx.next_arg_id();
    else
      parse_ctx.check_arg_id(--arg_index);
    return detail::get_arg(context, arg_index);
  };

  const Char* start = parse_ctx.begin();
  const Char* end = parse_ctx.end();
  auto it = start;
  while (it != end) {
    if (!find<false, Char>(it, end, '%', it)) {
      it = end;  // find leaves it == nullptr if it doesn't find '%'.
      break;
    }
    Char c = *it++;
    if (it != end && *it == c) {
      write(out, basic_string_view<Char>(start, to_unsigned(it - start)));
      start = ++it;
      continue;
    }
    write(out, basic_string_view<Char>(start, to_unsigned(it - 1 - start)));

    auto specs = format_specs<Char>();
    specs.align = align::right;

    // Parse argument index, flags and width.
    int arg_index = parse_header(it, end, specs, get_arg);
    if (arg_index == 0) throw_format_error("argument not found");

    // Parse precision.
    if (it != end && *it == '.') {
      ++it;
      c = it != end ? *it : 0;
      if ('0' <= c && c <= '9') {
        specs.precision = parse_nonnegative_int(it, end, 0);
      } else if (c == '*') {
        ++it;
        specs.precision = static_cast<int>(
            visit_format_arg(printf_precision_handler(), get_arg(-1)));
      } else {
        specs.precision = 0;
      }
    }

    auto arg = get_arg(arg_index);
    // For d, i, o, u, x, and X conversion specifiers, if a precision is
    // specified, the '0' flag is ignored
    if (specs.precision >= 0 && arg.is_integral()) {
      // Ignore '0' for non-numeric types or if '-' present.
      specs.fill[0] = ' ';
    }
    if (specs.precision >= 0 && arg.type() == type::cstring_type) {
      auto str = visit_format_arg(get_cstring<Char>(), arg);
      auto str_end = str + specs.precision;
      auto nul = std::find(str, str_end, Char());
      auto sv = basic_string_view<Char>(
          str, to_unsigned(nul != str_end ? nul - str : specs.precision));
      arg = make_arg<basic_printf_context<Char>>(sv);
    }
    if (specs.alt && visit_format_arg(is_zero_int(), arg)) specs.alt = false;
    if (specs.fill[0] == '0') {
      if (arg.is_arithmetic() && specs.align != align::left)
        specs.align = align::numeric;
      else
        specs.fill[0] = ' ';  // Ignore '0' flag for non-numeric types or if '-'
                              // flag is also present.
    }

    // Parse length and convert the argument to the required type.
    c = it != end ? *it++ : 0;
    Char t = it != end ? *it : 0;
    switch (c) {
    case 'h':
      if (t == 'h') {
        ++it;
        t = it != end ? *it : 0;
        convert_arg<signed char>(arg, t);
      } else {
        convert_arg<short>(arg, t);
      }
      break;
    case 'l':
      if (t == 'l') {
        ++it;
        t = it != end ? *it : 0;
        convert_arg<long long>(arg, t);
      } else {
        convert_arg<long>(arg, t);
      }
      break;
    case 'j':
      convert_arg<intmax_t>(arg, t);
      break;
    case 'z':
      convert_arg<size_t>(arg, t);
      break;
    case 't':
      convert_arg<std::ptrdiff_t>(arg, t);
      break;
    case 'L':
      // printf produces garbage when 'L' is omitted for long double, no
      // need to do the same.
      break;
    default:
      --it;
      convert_arg<void>(arg, c);
    }

    // Parse type.
    if (it == end) throw_format_error("invalid format string");
    char type = static_cast<char>(*it++);
    if (arg.is_integral()) {
      // Normalize type.
      switch (type) {
      case 'i':
      case 'u':
        type = 'd';
        break;
      case 'c':
        visit_format_arg(char_converter<basic_printf_context<Char>>(arg), arg);
        break;
      }
    }
    specs.type = parse_printf_presentation_type(type, arg.type());
    if (specs.type == presentation_type::none)
      throw_format_error("invalid format specifier");

    start = it;

    // Format argument.
    visit_format_arg(printf_arg_formatter<Char>(out, specs, context), arg);
  }
  write(out, basic_string_view<Char>(start, to_unsigned(it - start)));
}
}  // namespace detail

using printf_context = basic_printf_context<char>;
using wprintf_context = basic_printf_context<wchar_t>;

using printf_args = basic_format_args<printf_context>;
using wprintf_args = basic_format_args<wprintf_context>;

/**
  \rst
  Constructs an `~fmt::format_arg_store` object that contains references to
  arguments and can be implicitly converted to `~fmt::printf_args`.
  \endrst
 */
template <typename... T>
inline auto make_printf_args(const T&... args)
    -> format_arg_store<printf_context, T...> {
  return {args...};
}

// DEPRECATED!
template <typename... T>
inline auto make_wprintf_args(const T&... args)
    -> format_arg_store<wprintf_context, T...> {
  return {args...};
}

template <typename Char>
inline auto vsprintf(
    basic_string_view<Char> fmt,
    basic_format_args<basic_printf_context<type_identity_t<Char>>> args)
    -> std::basic_string<Char> {
  auto buf = basic_memory_buffer<Char>();
  detail::vprintf(buf, fmt, args);
  return to_string(buf);
}

/**
  \rst
  Formats arguments and returns the result as a string.

  **Example**::

    std::string message = fmt::sprintf("The answer is %d", 42);
  \endrst
*/
template <typename S, typename... T,
          typename Char = enable_if_t<detail::is_string<S>::value, char_t<S>>>
inline auto sprintf(const S& fmt, const T&... args) -> std::basic_string<Char> {
  return vsprintf(detail::to_string_view(fmt),
                  fmt::make_format_args<basic_printf_context<Char>>(args...));
}

template <typename Char>
inline auto vfprintf(
    std::FILE* f, basic_string_view<Char> fmt,
    basic_format_args<basic_printf_context<type_identity_t<Char>>> args)
    -> int {
  auto buf = basic_memory_buffer<Char>();
  detail::vprintf(buf, fmt, args);
  size_t size = buf.size();
  return std::fwrite(buf.data(), sizeof(Char), size, f) < size
             ? -1
             : static_cast<int>(size);
}

/**
  \rst
  Prints formatted data to the file *f*.

  **Example**::

    fmt::fprintf(stderr, "Don't %s!", "panic");
  \endrst
 */
template <typename S, typename... T, typename Char = char_t<S>>
inline auto fprintf(std::FILE* f, const S& fmt, const T&... args) -> int {
  return vfprintf(f, detail::to_string_view(fmt),
                  fmt::make_format_args<basic_printf_context<Char>>(args...));
}

template <typename Char>
FMT_DEPRECATED inline auto vprintf(
    basic_string_view<Char> fmt,
    basic_format_args<basic_printf_context<type_identity_t<Char>>> args)
    -> int {
  return vfprintf(stdout, fmt, args);
}

/**
  \rst
  Prints formatted data to ``stdout``.

  **Example**::

    fmt::printf("Elapsed time: %.2f seconds", 1.23);
  \endrst
 */
template <typename... T>
inline auto printf(string_view fmt, const T&... args) -> int {
  return vfprintf(stdout, fmt, make_printf_args(args...));
}
template <typename... T>
FMT_DEPRECATED inline auto printf(basic_string_view<wchar_t> fmt,
                                  const T&... args) -> int {
  return vfprintf(stdout, fmt, make_wprintf_args(args...));
}

FMT_END_EXPORT
FMT_END_NAMESPACE

#endif  // FMT_PRINTF_H_
