// Formatting library for C++ - experimental range support
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.
//
// Copyright (c) 2018 - present, Remotion (Igor Schulz)
// All Rights Reserved
// {fmt} support for ranges, containers and types tuple interface.

#ifndef FMT_RANGES_H_
#define FMT_RANGES_H_

#include <initializer_list>
#include <tuple>
#include <type_traits>

#include "format.h"

FMT_BEGIN_NAMESPACE

namespace detail {

template <typename RangeT, typename OutputIterator>
OutputIterator copy(const RangeT& range, OutputIterator out) {
  for (auto it = range.begin(), end = range.end(); it != end; ++it)
    *out++ = *it;
  return out;
}

template <typename OutputIterator>
OutputIterator copy(const char* str, OutputIterator out) {
  while (*str) *out++ = *str++;
  return out;
}

template <typename OutputIterator>
OutputIterator copy(char ch, OutputIterator out) {
  *out++ = ch;
  return out;
}

template <typename OutputIterator>
OutputIterator copy(wchar_t ch, OutputIterator out) {
  *out++ = ch;
  return out;
}

// Returns true if T has a std::string-like interface, like std::string_view.
template <typename T> class is_std_string_like {
  template <typename U>
  static auto check(U* p)
      -> decltype((void)p->find('a'), p->length(), (void)p->data(), int());
  template <typename> static void check(...);

 public:
  static constexpr const bool value =
      is_string<T>::value ||
      std::is_convertible<T, std_string_view<char>>::value ||
      !std::is_void<decltype(check<T>(nullptr))>::value;
};

template <typename Char>
struct is_std_string_like<fmt::basic_string_view<Char>> : std::true_type {};

template <typename T> class is_map {
  template <typename U> static auto check(U*) -> typename U::mapped_type;
  template <typename> static void check(...);

 public:
#ifdef FMT_FORMAT_MAP_AS_LIST
  static constexpr const bool value = false;
#else
  static constexpr const bool value =
      !std::is_void<decltype(check<T>(nullptr))>::value;
#endif
};

template <typename T> class is_set {
  template <typename U> static auto check(U*) -> typename U::key_type;
  template <typename> static void check(...);

 public:
#ifdef FMT_FORMAT_SET_AS_LIST
  static constexpr const bool value = false;
#else
  static constexpr const bool value =
      !std::is_void<decltype(check<T>(nullptr))>::value && !is_map<T>::value;
#endif
};

template <typename... Ts> struct conditional_helper {};

template <typename T, typename _ = void> struct is_range_ : std::false_type {};

#if !FMT_MSC_VERSION || FMT_MSC_VERSION > 1800

#  define FMT_DECLTYPE_RETURN(val)  \
    ->decltype(val) { return val; } \
    static_assert(                  \
        true, "")  // This makes it so that a semicolon is required after the
                   // macro, which helps clang-format handle the formatting.

// C array overload
template <typename T, std::size_t N>
auto range_begin(const T (&arr)[N]) -> const T* {
  return arr;
}
template <typename T, std::size_t N>
auto range_end(const T (&arr)[N]) -> const T* {
  return arr + N;
}

template <typename T, typename Enable = void>
struct has_member_fn_begin_end_t : std::false_type {};

template <typename T>
struct has_member_fn_begin_end_t<T, void_t<decltype(std::declval<T>().begin()),
                                           decltype(std::declval<T>().end())>>
    : std::true_type {};

// Member function overload
template <typename T>
auto range_begin(T&& rng) FMT_DECLTYPE_RETURN(static_cast<T&&>(rng).begin());
template <typename T>
auto range_end(T&& rng) FMT_DECLTYPE_RETURN(static_cast<T&&>(rng).end());

// ADL overload. Only participates in overload resolution if member functions
// are not found.
template <typename T>
auto range_begin(T&& rng)
    -> enable_if_t<!has_member_fn_begin_end_t<T&&>::value,
                   decltype(begin(static_cast<T&&>(rng)))> {
  return begin(static_cast<T&&>(rng));
}
template <typename T>
auto range_end(T&& rng) -> enable_if_t<!has_member_fn_begin_end_t<T&&>::value,
                                       decltype(end(static_cast<T&&>(rng)))> {
  return end(static_cast<T&&>(rng));
}

template <typename T, typename Enable = void>
struct has_const_begin_end : std::false_type {};
template <typename T, typename Enable = void>
struct has_mutable_begin_end : std::false_type {};

template <typename T>
struct has_const_begin_end<
    T,
    void_t<
        decltype(detail::range_begin(std::declval<const remove_cvref_t<T>&>())),
        decltype(detail::range_end(std::declval<const remove_cvref_t<T>&>()))>>
    : std::true_type {};

template <typename T>
struct has_mutable_begin_end<
    T, void_t<decltype(detail::range_begin(std::declval<T>())),
              decltype(detail::range_end(std::declval<T>())),
              enable_if_t<std::is_copy_constructible<T>::value>>>
    : std::true_type {};

template <typename T>
struct is_range_<T, void>
    : std::integral_constant<bool, (has_const_begin_end<T>::value ||
                                    has_mutable_begin_end<T>::value)> {};
#  undef FMT_DECLTYPE_RETURN
#endif

// tuple_size and tuple_element check.
template <typename T> class is_tuple_like_ {
  template <typename U>
  static auto check(U* p) -> decltype(std::tuple_size<U>::value, int());
  template <typename> static void check(...);

 public:
  static constexpr const bool value =
      !std::is_void<decltype(check<T>(nullptr))>::value;
};

// Check for integer_sequence
#if defined(__cpp_lib_integer_sequence) || FMT_MSC_VERSION >= 1900
template <typename T, T... N>
using integer_sequence = std::integer_sequence<T, N...>;
template <size_t... N> using index_sequence = std::index_sequence<N...>;
template <size_t N> using make_index_sequence = std::make_index_sequence<N>;
#else
template <typename T, T... N> struct integer_sequence {
  using value_type = T;

  static FMT_CONSTEXPR size_t size() { return sizeof...(N); }
};

template <size_t... N> using index_sequence = integer_sequence<size_t, N...>;

template <typename T, size_t N, T... Ns>
struct make_integer_sequence : make_integer_sequence<T, N - 1, N - 1, Ns...> {};
template <typename T, T... Ns>
struct make_integer_sequence<T, 0, Ns...> : integer_sequence<T, Ns...> {};

template <size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;
#endif

template <typename T>
using tuple_index_sequence = make_index_sequence<std::tuple_size<T>::value>;

template <typename T, typename C, bool = is_tuple_like_<T>::value>
class is_tuple_formattable_ {
 public:
  static constexpr const bool value = false;
};
template <typename T, typename C> class is_tuple_formattable_<T, C, true> {
  template <std::size_t... I>
  static std::true_type check2(index_sequence<I...>,
                               integer_sequence<bool, (I == I)...>);
  static std::false_type check2(...);
  template <std::size_t... I>
  static decltype(check2(
      index_sequence<I...>{},
      integer_sequence<
          bool, (is_formattable<typename std::tuple_element<I, T>::type,
                                C>::value)...>{})) check(index_sequence<I...>);

 public:
  static constexpr const bool value =
      decltype(check(tuple_index_sequence<T>{}))::value;
};

template <class Tuple, class F, size_t... Is>
void for_each(index_sequence<Is...>, Tuple&& tup, F&& f) noexcept {
  using std::get;
  // using free function get<I>(T) now.
  const int _[] = {0, ((void)f(get<Is>(tup)), 0)...};
  (void)_;  // blocks warnings
}

template <class T>
FMT_CONSTEXPR make_index_sequence<std::tuple_size<T>::value> get_indexes(
    T const&) {
  return {};
}

template <class Tuple, class F> void for_each(Tuple&& tup, F&& f) {
  const auto indexes = get_indexes(tup);
  for_each(indexes, std::forward<Tuple>(tup), std::forward<F>(f));
}

#if FMT_MSC_VERSION && FMT_MSC_VERSION < 1920
// Older MSVC doesn't get the reference type correctly for arrays.
template <typename R> struct range_reference_type_impl {
  using type = decltype(*detail::range_begin(std::declval<R&>()));
};

template <typename T, std::size_t N> struct range_reference_type_impl<T[N]> {
  using type = T&;
};

template <typename T>
using range_reference_type = typename range_reference_type_impl<T>::type;
#else
template <typename Range>
using range_reference_type =
    decltype(*detail::range_begin(std::declval<Range&>()));
#endif

// We don't use the Range's value_type for anything, but we do need the Range's
// reference type, with cv-ref stripped.
template <typename Range>
using uncvref_type = remove_cvref_t<range_reference_type<Range>>;

template <typename Range>
using uncvref_first_type =
    remove_cvref_t<decltype(std::declval<range_reference_type<Range>>().first)>;

template <typename Range>
using uncvref_second_type = remove_cvref_t<
    decltype(std::declval<range_reference_type<Range>>().second)>;

template <typename OutputIt> OutputIt write_delimiter(OutputIt out) {
  *out++ = ',';
  *out++ = ' ';
  return out;
}

template <typename Char, typename OutputIt>
auto write_range_entry(OutputIt out, basic_string_view<Char> str) -> OutputIt {
  return write_escaped_string(out, str);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(std::is_convertible<T, std_string_view<char>>::value)>
inline auto write_range_entry(OutputIt out, const T& str) -> OutputIt {
  auto sv = std_string_view<Char>(str);
  return write_range_entry<Char>(out, basic_string_view<Char>(sv));
}

template <typename Char, typename OutputIt, typename Arg,
          FMT_ENABLE_IF(std::is_same<Arg, Char>::value)>
OutputIt write_range_entry(OutputIt out, const Arg v) {
  return write_escaped_char(out, v);
}

template <
    typename Char, typename OutputIt, typename Arg,
    FMT_ENABLE_IF(!is_std_string_like<typename std::decay<Arg>::type>::value &&
                  !std::is_same<Arg, Char>::value)>
OutputIt write_range_entry(OutputIt out, const Arg& v) {
  return write<Char>(out, v);
}

}  // namespace detail

template <typename T> struct is_tuple_like {
  static constexpr const bool value =
      detail::is_tuple_like_<T>::value && !detail::is_range_<T>::value;
};

template <typename T, typename C> struct is_tuple_formattable {
  static constexpr const bool value =
      detail::is_tuple_formattable_<T, C>::value;
};

template <typename TupleT, typename Char>
struct formatter<TupleT, Char,
                 enable_if_t<fmt::is_tuple_like<TupleT>::value &&
                             fmt::is_tuple_formattable<TupleT, Char>::value>> {
 private:
  basic_string_view<Char> separator_ = detail::string_literal<Char, ',', ' '>{};
  basic_string_view<Char> opening_bracket_ =
      detail::string_literal<Char, '('>{};
  basic_string_view<Char> closing_bracket_ =
      detail::string_literal<Char, ')'>{};

  // C++11 generic lambda for format().
  template <typename FormatContext> struct format_each {
    template <typename T> void operator()(const T& v) {
      if (i > 0) out = detail::copy_str<Char>(separator, out);
      out = detail::write_range_entry<Char>(out, v);
      ++i;
    }
    int i;
    typename FormatContext::iterator& out;
    basic_string_view<Char> separator;
  };

 public:
  FMT_CONSTEXPR formatter() {}

  FMT_CONSTEXPR void set_separator(basic_string_view<Char> sep) {
    separator_ = sep;
  }

  FMT_CONSTEXPR void set_brackets(basic_string_view<Char> open,
                                  basic_string_view<Char> close) {
    opening_bracket_ = open;
    closing_bracket_ = close;
  }

  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename FormatContext = format_context>
  auto format(const TupleT& values, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto out = ctx.out();
    out = detail::copy_str<Char>(opening_bracket_, out);
    detail::for_each(values, format_each<FormatContext>{0, out, separator_});
    out = detail::copy_str<Char>(closing_bracket_, out);
    return out;
  }
};

template <typename T, typename Char> struct is_range {
  static constexpr const bool value =
      detail::is_range_<T>::value && !detail::is_std_string_like<T>::value &&
      !std::is_convertible<T, std::basic_string<Char>>::value &&
      !std::is_convertible<T, detail::std_string_view<Char>>::value;
};

namespace detail {
template <typename Context> struct range_mapper {
  using mapper = arg_mapper<Context>;

  template <typename T,
            FMT_ENABLE_IF(has_formatter<remove_cvref_t<T>, Context>::value)>
  static auto map(T&& value) -> T&& {
    return static_cast<T&&>(value);
  }
  template <typename T,
            FMT_ENABLE_IF(!has_formatter<remove_cvref_t<T>, Context>::value)>
  static auto map(T&& value)
      -> decltype(mapper().map(static_cast<T&&>(value))) {
    return mapper().map(static_cast<T&&>(value));
  }
};

template <typename Char, typename Element>
using range_formatter_type = conditional_t<
    is_formattable<Element, Char>::value,
    formatter<remove_cvref_t<decltype(range_mapper<buffer_context<Char>>{}.map(
                  std::declval<Element>()))>,
              Char>,
    fallback_formatter<Element, Char>>;

template <typename R>
using maybe_const_range =
    conditional_t<has_const_begin_end<R>::value, const R, R>;

// Workaround a bug in MSVC 2015 and earlier.
#if !FMT_MSC_VERSION || FMT_MSC_VERSION >= 1910
template <typename R, typename Char>
struct is_formattable_delayed
    : disjunction<
          is_formattable<uncvref_type<maybe_const_range<R>>, Char>,
          has_fallback_formatter<uncvref_type<maybe_const_range<R>>, Char>> {};
#endif

}  // namespace detail

template <typename T, typename Char, typename Enable = void>
struct range_formatter;

template <typename T, typename Char>
struct range_formatter<
    T, Char,
    enable_if_t<conjunction<
        std::is_same<T, remove_cvref_t<T>>,
        disjunction<is_formattable<T, Char>,
                    detail::has_fallback_formatter<T, Char>>>::value>> {
 private:
  detail::range_formatter_type<Char, T> underlying_;
  bool custom_specs_ = false;
  basic_string_view<Char> separator_ = detail::string_literal<Char, ',', ' '>{};
  basic_string_view<Char> opening_bracket_ =
      detail::string_literal<Char, '['>{};
  basic_string_view<Char> closing_bracket_ =
      detail::string_literal<Char, ']'>{};

  template <class U>
  FMT_CONSTEXPR static auto maybe_set_debug_format(U& u, int)
      -> decltype(u.set_debug_format()) {
    u.set_debug_format();
  }

  template <class U>
  FMT_CONSTEXPR static void maybe_set_debug_format(U&, ...) {}

  FMT_CONSTEXPR void maybe_set_debug_format() {
    maybe_set_debug_format(underlying_, 0);
  }

 public:
  FMT_CONSTEXPR range_formatter() {}

  FMT_CONSTEXPR auto underlying() -> detail::range_formatter_type<Char, T>& {
    return underlying_;
  }

  FMT_CONSTEXPR void set_separator(basic_string_view<Char> sep) {
    separator_ = sep;
  }

  FMT_CONSTEXPR void set_brackets(basic_string_view<Char> open,
                                  basic_string_view<Char> close) {
    opening_bracket_ = open;
    closing_bracket_ = close;
  }

  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin();
    auto end = ctx.end();
    if (it == end || *it == '}') {
      maybe_set_debug_format();
      return it;
    }

    if (*it == 'n') {
      set_brackets({}, {});
      ++it;
    }

    if (*it == '}') {
      maybe_set_debug_format();
      return it;
    }

    if (*it != ':')
      FMT_THROW(format_error("no other top-level range formatters supported"));

    custom_specs_ = true;
    ++it;
    ctx.advance_to(it);
    return underlying_.parse(ctx);
  }

  template <typename R, class FormatContext>
  auto format(R&& range, FormatContext& ctx) const -> decltype(ctx.out()) {
    detail::range_mapper<buffer_context<Char>> mapper;
    auto out = ctx.out();
    out = detail::copy_str<Char>(opening_bracket_, out);
    int i = 0;
    auto it = detail::range_begin(range);
    auto end = detail::range_end(range);
    for (; it != end; ++it) {
      if (i > 0) out = detail::copy_str<Char>(separator_, out);
      ;
      ctx.advance_to(out);
      out = underlying_.format(mapper.map(*it), ctx);
      ++i;
    }
    out = detail::copy_str<Char>(closing_bracket_, out);
    return out;
  }
};

enum class range_format { disabled, map, set, sequence, string, debug_string };

namespace detail {
template <typename T> struct range_format_kind_ {
  static constexpr auto value = std::is_same<range_reference_type<T>, T>::value
                                    ? range_format::disabled
                                : is_map<T>::value ? range_format::map
                                : is_set<T>::value ? range_format::set
                                                   : range_format::sequence;
};

template <range_format K, typename R, typename Char, typename Enable = void>
struct range_default_formatter;

template <range_format K>
using range_format_constant = std::integral_constant<range_format, K>;

template <range_format K, typename R, typename Char>
struct range_default_formatter<
    K, R, Char,
    enable_if_t<(K == range_format::sequence || K == range_format::map ||
                 K == range_format::set)>> {
  using range_type = detail::maybe_const_range<R>;
  range_formatter<detail::uncvref_type<range_type>, Char> underlying_;

  FMT_CONSTEXPR range_default_formatter() { init(range_format_constant<K>()); }

  FMT_CONSTEXPR void init(range_format_constant<range_format::set>) {
    underlying_.set_brackets(detail::string_literal<Char, '{'>{},
                             detail::string_literal<Char, '}'>{});
  }

  FMT_CONSTEXPR void init(range_format_constant<range_format::map>) {
    underlying_.set_brackets(detail::string_literal<Char, '{'>{},
                             detail::string_literal<Char, '}'>{});
    underlying_.underlying().set_brackets({}, {});
    underlying_.underlying().set_separator(
        detail::string_literal<Char, ':', ' '>{});
  }

  FMT_CONSTEXPR void init(range_format_constant<range_format::sequence>) {}

  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return underlying_.parse(ctx);
  }

  template <typename FormatContext>
  auto format(range_type& range, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return underlying_.format(range, ctx);
  }
};
}  // namespace detail

template <typename T, typename Char, typename Enable = void>
struct range_format_kind
    : conditional_t<
          is_range<T, Char>::value, detail::range_format_kind_<T>,
          std::integral_constant<range_format, range_format::disabled>> {};

template <typename R, typename Char>
struct formatter<
    R, Char,
    enable_if_t<conjunction<bool_constant<range_format_kind<R, Char>::value !=
                                          range_format::disabled>
// Workaround a bug in MSVC 2015 and earlier.
#if !FMT_MSC_VERSION || FMT_MSC_VERSION >= 1910
                            ,
                            detail::is_formattable_delayed<R, Char>
#endif
                            >::value>>
    : detail::range_default_formatter<range_format_kind<R, Char>::value, R,
                                      Char> {
};

template <typename Char, typename... T> struct tuple_join_view : detail::view {
  const std::tuple<T...>& tuple;
  basic_string_view<Char> sep;

  tuple_join_view(const std::tuple<T...>& t, basic_string_view<Char> s)
      : tuple(t), sep{s} {}
};

template <typename Char, typename... T>
using tuple_arg_join = tuple_join_view<Char, T...>;

// Define FMT_TUPLE_JOIN_SPECIFIERS to enable experimental format specifiers
// support in tuple_join. It is disabled by default because of issues with
// the dynamic width and precision.
#ifndef FMT_TUPLE_JOIN_SPECIFIERS
#  define FMT_TUPLE_JOIN_SPECIFIERS 0
#endif

template <typename Char, typename... T>
struct formatter<tuple_join_view<Char, T...>, Char> {
  template <typename ParseContext>
  FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return do_parse(ctx, std::integral_constant<size_t, sizeof...(T)>());
  }

  template <typename FormatContext>
  auto format(const tuple_join_view<Char, T...>& value,
              FormatContext& ctx) const -> typename FormatContext::iterator {
    return do_format(value, ctx,
                     std::integral_constant<size_t, sizeof...(T)>());
  }

 private:
  std::tuple<formatter<typename std::decay<T>::type, Char>...> formatters_;

  template <typename ParseContext>
  FMT_CONSTEXPR auto do_parse(ParseContext& ctx,
                              std::integral_constant<size_t, 0>)
      -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename ParseContext, size_t N>
  FMT_CONSTEXPR auto do_parse(ParseContext& ctx,
                              std::integral_constant<size_t, N>)
      -> decltype(ctx.begin()) {
    auto end = ctx.begin();
#if FMT_TUPLE_JOIN_SPECIFIERS
    end = std::get<sizeof...(T) - N>(formatters_).parse(ctx);
    if (N > 1) {
      auto end1 = do_parse(ctx, std::integral_constant<size_t, N - 1>());
      if (end != end1)
        FMT_THROW(format_error("incompatible format specs for tuple elements"));
    }
#endif
    return end;
  }

  template <typename FormatContext>
  auto do_format(const tuple_join_view<Char, T...>&, FormatContext& ctx,
                 std::integral_constant<size_t, 0>) const ->
      typename FormatContext::iterator {
    return ctx.out();
  }

  template <typename FormatContext, size_t N>
  auto do_format(const tuple_join_view<Char, T...>& value, FormatContext& ctx,
                 std::integral_constant<size_t, N>) const ->
      typename FormatContext::iterator {
    auto out = std::get<sizeof...(T) - N>(formatters_)
                   .format(std::get<sizeof...(T) - N>(value.tuple), ctx);
    if (N > 1) {
      out = std::copy(value.sep.begin(), value.sep.end(), out);
      ctx.advance_to(out);
      return do_format(value, ctx, std::integral_constant<size_t, N - 1>());
    }
    return out;
  }
};

FMT_MODULE_EXPORT_BEGIN

/**
  \rst
  Returns an object that formats `tuple` with elements separated by `sep`.

  **Example**::

    std::tuple<int, char> t = {1, 'a'};
    fmt::print("{}", fmt::join(t, ", "));
    // Output: "1, a"
  \endrst
 */
template <typename... T>
FMT_CONSTEXPR auto join(const std::tuple<T...>& tuple, string_view sep)
    -> tuple_join_view<char, T...> {
  return {tuple, sep};
}

template <typename... T>
FMT_CONSTEXPR auto join(const std::tuple<T...>& tuple,
                        basic_string_view<wchar_t> sep)
    -> tuple_join_view<wchar_t, T...> {
  return {tuple, sep};
}

/**
  \rst
  Returns an object that formats `initializer_list` with elements separated by
  `sep`.

  **Example**::

    fmt::print("{}", fmt::join({1, 2, 3}, ", "));
    // Output: "1, 2, 3"
  \endrst
 */
template <typename T>
auto join(std::initializer_list<T> list, string_view sep)
    -> join_view<const T*, const T*> {
  return join(std::begin(list), std::end(list), sep);
}

FMT_MODULE_EXPORT_END
FMT_END_NAMESPACE

#endif  // FMT_RANGES_H_
