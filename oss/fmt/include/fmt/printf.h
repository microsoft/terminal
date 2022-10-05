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
FMT_MODULE_EXPORT_BEGIN

template <typename T> struct printf_formatter { printf_formatter() = delete; };

template <typename Char>
class basic_printf_parse_context : public basic_format_parse_context<Char> {
  using basic_format_parse_context<Char>::basic_format_parse_context;
};

template <typename OutputIt, typename Char> class basic_printf_context {
 private:
  OutputIt out_;
  basic_format_args<basic_printf_context> args_;

 public:
  using char_type = Char;
  using format_arg = basic_format_arg<basic_printf_context>;
  using parse_context_type = basic_printf_parse_context<Char>;
  template <typename T> using formatter_type = printf_formatter<T>;

  /**
    \rst
    Constructs a ``printf_context`` object. References to the arguments are
    stored in the context object so make sure they have appropriate lifetimes.
    \endrst
   */
  basic_printf_context(OutputIt out,
                       basic_format_args<basic_printf_context> args)
      : out_(out), args_(args) {}

  OutputIt out() { return out_; }
  void advance_to(OutputIt it) { out_ = it; }

  detail::locale_ref locale() { return {}; }

  format_arg arg(int id) const { return args_.get(id); }

  FMT_CONSTEXPR void on_error(const char* message) {
    detail::error_handler().on_error(message);
  }
};

FMT_BEGIN_DETAIL_NAMESPACE

// Checks if a value fits in int - used to avoid warnings about comparing
// signed and unsigned integers.
template <bool IsSigned> struct int_checker {
  template <typename T> static bool fits_in_int(T value) {
    unsigned max = max_value<int>();
    return value <= max;
  }
  static bool fits_in_int(bool) { return true; }
};

template <> struct int_checker<true> {
  template <typename T> static bool fits_in_int(T value) {
    return value >= (std::numeric_limits<int>::min)() &&
           value <= max_value<int>();
  }
  static bool fits_in_int(int) { return true; }
};

class printf_precision_handler {
 public:
  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  int operator()(T value) {
    if (!int_checker<std::numeric_limits<T>::is_signed>::fits_in_int(value))
      FMT_THROW(format_error("number is too big"));
    return (std::max)(static_cast<int>(value), 0);
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  int operator()(T) {
    FMT_THROW(format_error("precision is not integer"));
    return 0;
  }
};

// An argument visitor that returns true iff arg is a zero integer.
class is_zero_int {
 public:
  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  bool operator()(T value) {
    return value == 0;
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  bool operator()(T) {
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
        arg_ = detail::make_arg<Context>(
            static_cast<int>(static_cast<target_type>(value)));
      } else {
        using unsigned_type = typename make_unsigned_or_bool<target_type>::type;
        arg_ = detail::make_arg<Context>(
            static_cast<unsigned>(static_cast<unsigned_type>(value)));
      }
    } else {
      if (is_signed) {
        // glibc's printf doesn't sign extend arguments of smaller types:
        //   std::printf("%lld", -42);  // prints "4294967254"
        // but we don't have to do the same because it's a UB.
        arg_ = detail::make_arg<Context>(static_cast<long long>(value));
      } else {
        arg_ = detail::make_arg<Context>(
            static_cast<typename make_unsigned_or_bool<U>::type>(value));
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
    arg_ = detail::make_arg<Context>(
        static_cast<typename Context::char_type>(value));
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  void operator()(T) {}  // No conversion needed for non-integral types.
};

// An argument visitor that return a pointer to a C string if argument is a
// string or null otherwise.
template <typename Char> struct get_cstring {
  template <typename T> const Char* operator()(T) { return nullptr; }
  const Char* operator()(const Char* s) { return s; }
};

// Checks if an argument is a valid printf width specifier and sets
// left alignment if it is negative.
template <typename Char> class printf_width_handler {
 private:
  using format_specs = basic_format_specs<Char>;

  format_specs& specs_;

 public:
  explicit printf_width_handler(format_specs& specs) : specs_(specs) {}

  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  unsigned operator()(T value) {
    auto width = static_cast<uint32_or_64_or_128_t<T>>(value);
    if (detail::is_negative(value)) {
      specs_.align = align::left;
      width = 0 - width;
    }
    unsigned int_max = max_value<int>();
    if (width > int_max) FMT_THROW(format_error("number is too big"));
    return static_cast<unsigned>(width);
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  unsigned operator()(T) {
    FMT_THROW(format_error("width is not integer"));
    return 0;
  }
};

// The ``printf`` argument formatter.
template <typename OutputIt, typename Char>
class printf_arg_formatter : public arg_formatter<Char> {
 private:
  using base = arg_formatter<Char>;
  using context_type = basic_printf_context<OutputIt, Char>;
  using format_specs = basic_format_specs<Char>;

  context_type& context_;

  OutputIt write_null_pointer(bool is_string = false) {
    auto s = this->specs;
    s.type = presentation_type::none;
    return write_bytes(this->out, is_string ? "(null)" : "(nil)", s);
  }

 public:
  printf_arg_formatter(OutputIt iter, format_specs& s, context_type& ctx)
      : base{iter, s, locale_ref()}, context_(ctx) {}

  OutputIt operator()(monostate value) { return base::operator()(value); }

  template <typename T, FMT_ENABLE_IF(detail::is_integral<T>::value)>
  OutputIt operator()(T value) {
    // MSVC2013 fails to compile separate overloads for bool and Char so use
    // std::is_same instead.
    if (std::is_same<T, Char>::value) {
      format_specs fmt_specs = this->specs;
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
      return write<Char>(this->out, static_cast<Char>(value), fmt_specs);
    }
    return base::operator()(value);
  }

  template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value)>
  OutputIt operator()(T value) {
    return base::operator()(value);
  }

  /** Formats a null-terminated C string. */
  OutputIt operator()(const char* value) {
    if (value) return base::operator()(value);
    return write_null_pointer(this->specs.type != presentation_type::pointer);
  }

  /** Formats a null-terminated wide C string. */
  OutputIt operator()(const wchar_t* value) {
    if (value) return base::operator()(value);
    return write_null_pointer(this->specs.type != presentation_type::pointer);
  }

  OutputIt operator()(basic_string_view<Char> value) {
    return base::operator()(value);
  }

  /** Formats a pointer. */
  OutputIt operator()(const void* value) {
    return value ? base::operator()(value) : write_null_pointer();
  }

  /** Formats an argument of a custom (user-defined) type. */
  OutputIt operator()(typename basic_format_arg<context_type>::handle handle) {
    auto parse_ctx =
        basic_printf_parse_context<Char>(basic_string_view<Char>());
    handle.format(parse_ctx, context_);
    return this->out;
  }
};

template <typename Char>
void parse_flags(basic_format_specs<Char>& specs, const Char*& it,
                 const Char* end) {
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
      if (specs.sign != sign::plus) {
        specs.sign = sign::space;
      }
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
int parse_header(const Char*& it, const Char* end,
                 basic_format_specs<Char>& specs, GetArg get_arg) {
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
        if (value == -1) FMT_THROW(format_error("number is too big"));
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
      if (specs.width == -1) FMT_THROW(format_error("number is too big"));
    } else if (*it == '*') {
      ++it;
      specs.width = static_cast<int>(visit_format_arg(
          detail::printf_width_handler<Char>(specs), get_arg(-1)));
    }
  }
  return arg_index;
}

template <typename Char, typename Context>
void vprintf(buffer<Char>& buf, basic_string_view<Char> format,
             basic_format_args<Context> args) {
  using OutputIt = buffer_appender<Char>;
  auto out = OutputIt(buf);
  auto context = basic_printf_context<OutputIt, Char>(out, args);
  auto parse_ctx = basic_printf_parse_context<Char>(format);

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
    if (!detail::find<false, Char>(it, end, '%', it)) {
      it = end;  // detail::find leaves it == nullptr if it doesn't find '%'
      break;
    }
    Char c = *it++;
    if (it != end && *it == c) {
      out = detail::write(
          out, basic_string_view<Char>(start, detail::to_unsigned(it - start)));
      start = ++it;
      continue;
    }
    out = detail::write(out, basic_string_view<Char>(
                                 start, detail::to_unsigned(it - 1 - start)));

    basic_format_specs<Char> specs;
    specs.align = align::right;

    // Parse argument index, flags and width.
    int arg_index = parse_header(it, end, specs, get_arg);
    if (arg_index == 0) parse_ctx.on_error("argument not found");

    // Parse precision.
    if (it != end && *it == '.') {
      ++it;
      c = it != end ? *it : 0;
      if ('0' <= c && c <= '9') {
        specs.precision = parse_nonnegative_int(it, end, 0);
      } else if (c == '*') {
        ++it;
        specs.precision = static_cast<int>(
            visit_format_arg(detail::printf_precision_handler(), get_arg(-1)));
      } else {
        specs.precision = 0;
      }
    }

    auto arg = get_arg(arg_index);
    // For d, i, o, u, x, and X conversion specifiers, if a precision is
    // specified, the '0' flag is ignored
    if (specs.precision >= 0 && arg.is_integral())
      specs.fill[0] =
          ' ';  // Ignore '0' flag for non-numeric types or if '-' present.
    if (specs.precision >= 0 && arg.type() == detail::type::cstring_type) {
      auto str = visit_format_arg(detail::get_cstring<Char>(), arg);
      auto str_end = str + specs.precision;
      auto nul = std::find(str, str_end, Char());
      arg = detail::make_arg<basic_printf_context<OutputIt, Char>>(
          basic_string_view<Char>(
              str, detail::to_unsigned(nul != str_end ? nul - str
                                                      : specs.precision)));
    }
    if (specs.alt && visit_format_arg(detail::is_zero_int(), arg))
      specs.alt = false;
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
    using detail::convert_arg;
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
    if (it == end) FMT_THROW(format_error("invalid format string"));
    char type = static_cast<char>(*it++);
    if (arg.is_integral()) {
      // Normalize type.
      switch (type) {
      case 'i':
      case 'u':
        type = 'd';
        break;
      case 'c':
        visit_format_arg(
            detail::char_converter<basic_printf_context<OutputIt, Char>>(arg),
            arg);
        break;
      }
    }
    specs.type = parse_presentation_type(type);
    if (specs.type == presentation_type::none)
      parse_ctx.on_error("invalid type specifier");

    start = it;

    // Format argument.
    out = visit_format_arg(
        detail::printf_arg_formatter<OutputIt, Char>(out, specs, context), arg);
  }
  detail::write(out, basic_string_view<Char>(start, to_unsigned(it - start)));
}
FMT_END_DETAIL_NAMESPACE

template <typename Char>
using basic_printf_context_t =
    basic_printf_context<detail::buffer_appender<Char>, Char>;

using printf_context = basic_printf_context_t<char>;
using wprintf_context = basic_printf_context_t<wchar_t>;

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

/**
  \rst
  Constructs an `~fmt::format_arg_store` object that contains references to
  arguments and can be implicitly converted to `~fmt::wprintf_args`.
  \endrst
 */
template <typename... T>
inline auto make_wprintf_args(const T&... args)
    -> format_arg_store<wprintf_context, T...> {
  return {args...};
}

template <typename S, typename Char = char_t<S>>
inline auto vsprintf(
    const S& fmt,
    basic_format_args<basic_printf_context_t<type_identity_t<Char>>> args)
    -> std::basic_string<Char> {
  basic_memory_buffer<Char> buffer;
  vprintf(buffer, detail::to_string_view(fmt), args);
  return to_string(buffer);
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
  using context = basic_printf_context_t<Char>;
  return vsprintf(detail::to_string_view(fmt),
                  fmt::make_format_args<context>(args...));
}

template <typename S, typename Char = char_t<S>>
inline auto vfprintf(
    std::FILE* f, const S& fmt,
    basic_format_args<basic_printf_context_t<type_identity_t<Char>>> args)
    -> int {
  basic_memory_buffer<Char> buffer;
  vprintf(buffer, detail::to_string_view(fmt), args);
  size_t size = buffer.size();
  return std::fwrite(buffer.data(), sizeof(Char), size, f) < size
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
  using context = basic_printf_context_t<Char>;
  return vfprintf(f, detail::to_string_view(fmt),
                  fmt::make_format_args<context>(args...));
}

template <typename S, typename Char = char_t<S>>
inline auto vprintf(
    const S& fmt,
    basic_format_args<basic_printf_context_t<type_identity_t<Char>>> args)
    -> int {
  return vfprintf(stdout, detail::to_string_view(fmt), args);
}

/**
  \rst
  Prints formatted data to ``stdout``.

  **Example**::

    fmt::printf("Elapsed time: %.2f seconds", 1.23);
  \endrst
 */
template <typename S, typename... T, FMT_ENABLE_IF(detail::is_string<S>::value)>
inline auto printf(const S& fmt, const T&... args) -> int {
  return vprintf(
      detail::to_string_view(fmt),
      fmt::make_format_args<basic_printf_context_t<char_t<S>>>(args...));
}

FMT_MODULE_EXPORT_END
FMT_END_NAMESPACE

#endif  // FMT_PRINTF_H_
