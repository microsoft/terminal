// Formatting library for C++ - std::ostream support
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_OSTREAM_H_
#define FMT_OSTREAM_H_

#include <ostream>

#include "format.h"

FMT_BEGIN_NAMESPACE

template <typename Char> class basic_printf_parse_context;
template <typename OutputIt, typename Char> class basic_printf_context;

namespace detail {

template <class Char> class formatbuf : public std::basic_streambuf<Char> {
 private:
  using int_type = typename std::basic_streambuf<Char>::int_type;
  using traits_type = typename std::basic_streambuf<Char>::traits_type;

  buffer<Char>& buffer_;

 public:
  formatbuf(buffer<Char>& buf) : buffer_(buf) {}

 protected:
  // The put-area is actually always empty. This makes the implementation
  // simpler and has the advantage that the streambuf and the buffer are always
  // in sync and sputc never writes into uninitialized memory. The obvious
  // disadvantage is that each call to sputc always results in a (virtual) call
  // to overflow. There is no disadvantage here for sputn since this always
  // results in a call to xsputn.

  int_type overflow(int_type ch = traits_type::eof()) FMT_OVERRIDE {
    if (!traits_type::eq_int_type(ch, traits_type::eof()))
      buffer_.push_back(static_cast<Char>(ch));
    return ch;
  }

  std::streamsize xsputn(const Char* s, std::streamsize count) FMT_OVERRIDE {
    buffer_.append(s, s + count);
    return count;
  }
};

struct converter {
  template <typename T, FMT_ENABLE_IF(is_integral<T>::value)> converter(T);
};

template <typename Char> struct test_stream : std::basic_ostream<Char> {
 private:
  void_t<> operator<<(converter);
};

// Hide insertion operators for built-in types.
template <typename Char, typename Traits>
void_t<> operator<<(std::basic_ostream<Char, Traits>&, Char);
template <typename Char, typename Traits>
void_t<> operator<<(std::basic_ostream<Char, Traits>&, char);
template <typename Traits>
void_t<> operator<<(std::basic_ostream<char, Traits>&, char);
template <typename Traits>
void_t<> operator<<(std::basic_ostream<char, Traits>&, signed char);
template <typename Traits>
void_t<> operator<<(std::basic_ostream<char, Traits>&, unsigned char);

// Checks if T has a user-defined operator<< (e.g. not a member of
// std::ostream).
template <typename T, typename Char> class is_streamable {
 private:
  template <typename U>
  static bool_constant<!std::is_same<decltype(std::declval<test_stream<Char>&>()
                                              << std::declval<U>()),
                                     void_t<>>::value>
  test(int);

  template <typename> static std::false_type test(...);

  using result = decltype(test<T>(0));

 public:
  static const bool value = result::value;
};

// Write the content of buf to os.
template <typename Char>
void write_buffer(std::basic_ostream<Char>& os, buffer<Char>& buf) {
  const Char* buf_data = buf.data();
  using unsigned_streamsize = std::make_unsigned<std::streamsize>::type;
  unsigned_streamsize size = buf.size();
  unsigned_streamsize max_size = to_unsigned(max_value<std::streamsize>());
  do {
    unsigned_streamsize n = size <= max_size ? size : max_size;
    os.write(buf_data, static_cast<std::streamsize>(n));
    buf_data += n;
    size -= n;
  } while (size != 0);
}

template <typename Char, typename T>
void format_value(buffer<Char>& buf, const T& value,
                  locale_ref loc = locale_ref()) {
  formatbuf<Char> format_buf(buf);
  std::basic_ostream<Char> output(&format_buf);
#if !defined(FMT_STATIC_THOUSANDS_SEPARATOR)
  if (loc) output.imbue(loc.get<std::locale>());
#endif
  output << value;
  output.exceptions(std::ios_base::failbit | std::ios_base::badbit);
  buf.try_resize(buf.size());
}

// Formats an object of type T that has an overloaded ostream operator<<.
template <typename T, typename Char>
struct fallback_formatter<T, Char, enable_if_t<is_streamable<T, Char>::value>>
    : private formatter<basic_string_view<Char>, Char> {
  FMT_CONSTEXPR auto parse(basic_format_parse_context<Char>& ctx)
      -> decltype(ctx.begin()) {
    return formatter<basic_string_view<Char>, Char>::parse(ctx);
  }
  template <typename ParseCtx,
            FMT_ENABLE_IF(std::is_same<
                          ParseCtx, basic_printf_parse_context<Char>>::value)>
  auto parse(ParseCtx& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename OutputIt>
  auto format(const T& value, basic_format_context<OutputIt, Char>& ctx)
      -> OutputIt {
    basic_memory_buffer<Char> buffer;
    format_value(buffer, value, ctx.locale());
    basic_string_view<Char> str(buffer.data(), buffer.size());
    return formatter<basic_string_view<Char>, Char>::format(str, ctx);
  }
  template <typename OutputIt>
  auto format(const T& value, basic_printf_context<OutputIt, Char>& ctx)
      -> OutputIt {
    basic_memory_buffer<Char> buffer;
    format_value(buffer, value, ctx.locale());
    return std::copy(buffer.begin(), buffer.end(), ctx.out());
  }
};
}  // namespace detail

template <typename Char>
void vprint(std::basic_ostream<Char>& os, basic_string_view<Char> format_str,
            basic_format_args<buffer_context<type_identity_t<Char>>> args) {
  basic_memory_buffer<Char> buffer;
  detail::vformat_to(buffer, format_str, args);
  detail::write_buffer(os, buffer);
}

/**
  \rst
  Prints formatted data to the stream *os*.

  **Example**::

    fmt::print(cerr, "Don't {}!", "panic");
  \endrst
 */
template <typename S, typename... Args,
          typename Char = enable_if_t<detail::is_string<S>::value, char_t<S>>>
void print(std::basic_ostream<Char>& os, const S& format_str, Args&&... args) {
  vprint(os, to_string_view(format_str),
         fmt::make_args_checked<Args...>(format_str, args...));
}
FMT_END_NAMESPACE

#endif  // FMT_OSTREAM_H_
