//  Boost config.hpp configuration header file  ------------------------------//
//  boostinspect:ndprecated_macros -- tell the inspect tool to ignore this file

//  Copyright (c) 2001-2003 John Maddock
//  Copyright (c) 2001 Darin Adler
//  Copyright (c) 2001 Peter Dimov
//  Copyright (c) 2002 Bill Kempf
//  Copyright (c) 2002 Jens Maurer
//  Copyright (c) 2002-2003 David Abrahams
//  Copyright (c) 2003 Gennaro Prota
//  Copyright (c) 2003 Eric Friedman
//  Copyright (c) 2010 Eric Jourdanneau, Joel Falcou
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/ for most recent version.

//  Boost config.hpp policy and rationale documentation has been moved to
//  http://www.boost.org/libs/config/
//
//  This file is intended to be stable, and relatively unchanging.
//  It should contain boilerplate code only - no compiler specific
//  code unless it is unavoidable - no changes unless unavoidable.

#ifndef BOOST_CONFIG_SUFFIX_HPP
#define BOOST_CONFIG_SUFFIX_HPP

#if defined(__GNUC__) && (__GNUC__ >= 4)
//
// Some GCC-4.x versions issue warnings even when __extension__ is used,
// so use this as a workaround:
//
#pragma GCC system_header
#endif

//
// ensure that visibility macros are always defined, thus symplifying use
//
#ifndef BOOST_SYMBOL_EXPORT
# define BOOST_SYMBOL_EXPORT
#endif
#ifndef BOOST_SYMBOL_IMPORT
# define BOOST_SYMBOL_IMPORT
#endif
#ifndef BOOST_SYMBOL_VISIBLE
# define BOOST_SYMBOL_VISIBLE
#endif

//
// look for long long by looking for the appropriate macros in <limits.h>.
// Note that we use limits.h rather than climits for maximal portability,
// remember that since these just declare a bunch of macros, there should be
// no namespace issues from this.
//
#if !defined(BOOST_HAS_LONG_LONG) && !defined(BOOST_NO_LONG_LONG)                                              \
   && !defined(BOOST_MSVC) && !defined(__BORLANDC__)
# include <limits.h>
# if (defined(ULLONG_MAX) || defined(ULONG_LONG_MAX) || defined(ULONGLONG_MAX))
#   define BOOST_HAS_LONG_LONG
# else
#   define BOOST_NO_LONG_LONG
# endif
#endif

// GCC 3.x will clean up all of those nasty macro definitions that
// BOOST_NO_CTYPE_FUNCTIONS is intended to help work around, so undefine
// it under GCC 3.x.
#if defined(__GNUC__) && (__GNUC__ >= 3) && defined(BOOST_NO_CTYPE_FUNCTIONS)
#  undef BOOST_NO_CTYPE_FUNCTIONS
#endif

//
// Assume any extensions are in namespace std:: unless stated otherwise:
//
#  ifndef BOOST_STD_EXTENSION_NAMESPACE
#    define BOOST_STD_EXTENSION_NAMESPACE std
#  endif

//
// If cv-qualified specializations are not allowed, then neither are cv-void ones:
//
#  if defined(BOOST_NO_CV_SPECIALIZATIONS) \
      && !defined(BOOST_NO_CV_VOID_SPECIALIZATIONS)
#     define BOOST_NO_CV_VOID_SPECIALIZATIONS
#  endif

//
// If there is no numeric_limits template, then it can't have any compile time
// constants either!
//
#  if defined(BOOST_NO_LIMITS) \
      && !defined(BOOST_NO_LIMITS_COMPILE_TIME_CONSTANTS)
#     define BOOST_NO_LIMITS_COMPILE_TIME_CONSTANTS
#     define BOOST_NO_MS_INT64_NUMERIC_LIMITS
#     define BOOST_NO_LONG_LONG_NUMERIC_LIMITS
#  endif

//
// if there is no long long then there is no specialisation
// for numeric_limits<long long> either:
//
#if !defined(BOOST_HAS_LONG_LONG) && !defined(BOOST_NO_LONG_LONG_NUMERIC_LIMITS)
#  define BOOST_NO_LONG_LONG_NUMERIC_LIMITS
#endif

//
// if there is no __int64 then there is no specialisation
// for numeric_limits<__int64> either:
//
#if !defined(BOOST_HAS_MS_INT64) && !defined(BOOST_NO_MS_INT64_NUMERIC_LIMITS)
#  define BOOST_NO_MS_INT64_NUMERIC_LIMITS
#endif

//
// if member templates are supported then so is the
// VC6 subset of member templates:
//
#  if !defined(BOOST_NO_MEMBER_TEMPLATES) \
       && !defined(BOOST_MSVC6_MEMBER_TEMPLATES)
#     define BOOST_MSVC6_MEMBER_TEMPLATES
#  endif

//
// Without partial specialization, can't test for partial specialisation bugs:
//
#  if defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION) \
      && !defined(BOOST_BCB_PARTIAL_SPECIALIZATION_BUG)
#     define BOOST_BCB_PARTIAL_SPECIALIZATION_BUG
#  endif

//
// Without partial specialization, we can't have array-type partial specialisations:
//
#  if defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION) \
      && !defined(BOOST_NO_ARRAY_TYPE_SPECIALIZATIONS)
#     define BOOST_NO_ARRAY_TYPE_SPECIALIZATIONS
#  endif

//
// Without partial specialization, std::iterator_traits can't work:
//
#  if defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION) \
      && !defined(BOOST_NO_STD_ITERATOR_TRAITS)
#     define BOOST_NO_STD_ITERATOR_TRAITS
#  endif

//
// Without partial specialization, partial
// specialization with default args won't work either:
//
#  if defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION) \
      && !defined(BOOST_NO_PARTIAL_SPECIALIZATION_IMPLICIT_DEFAULT_ARGS)
#     define BOOST_NO_PARTIAL_SPECIALIZATION_IMPLICIT_DEFAULT_ARGS
#  endif

//
// Without member template support, we can't have template constructors
// in the standard library either:
//
#  if defined(BOOST_NO_MEMBER_TEMPLATES) \
      && !defined(BOOST_MSVC6_MEMBER_TEMPLATES) \
      && !defined(BOOST_NO_TEMPLATED_ITERATOR_CONSTRUCTORS)
#     define BOOST_NO_TEMPLATED_ITERATOR_CONSTRUCTORS
#  endif

//
// Without member template support, we can't have a conforming
// std::allocator template either:
//
#  if defined(BOOST_NO_MEMBER_TEMPLATES) \
      && !defined(BOOST_MSVC6_MEMBER_TEMPLATES) \
      && !defined(BOOST_NO_STD_ALLOCATOR)
#     define BOOST_NO_STD_ALLOCATOR
#  endif

//
// without ADL support then using declarations will break ADL as well:
//
#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP) && !defined(BOOST_FUNCTION_SCOPE_USING_DECLARATION_BREAKS_ADL)
#  define BOOST_FUNCTION_SCOPE_USING_DECLARATION_BREAKS_ADL
#endif

//
// Without typeid support we have no dynamic RTTI either:
//
#if defined(BOOST_NO_TYPEID) && !defined(BOOST_NO_RTTI)
#  define BOOST_NO_RTTI
#endif

//
// If we have a standard allocator, then we have a partial one as well:
//
#if !defined(BOOST_NO_STD_ALLOCATOR)
#  define BOOST_HAS_PARTIAL_STD_ALLOCATOR
#endif

//
// We can't have a working std::use_facet if there is no std::locale:
//
#  if defined(BOOST_NO_STD_LOCALE) && !defined(BOOST_NO_STD_USE_FACET)
#     define BOOST_NO_STD_USE_FACET
#  endif

//
// We can't have a std::messages facet if there is no std::locale:
//
#  if defined(BOOST_NO_STD_LOCALE) && !defined(BOOST_NO_STD_MESSAGES)
#     define BOOST_NO_STD_MESSAGES
#  endif

//
// We can't have a working std::wstreambuf if there is no std::locale:
//
#  if defined(BOOST_NO_STD_LOCALE) && !defined(BOOST_NO_STD_WSTREAMBUF)
#     define BOOST_NO_STD_WSTREAMBUF
#  endif

//
// We can't have a <cwctype> if there is no <cwchar>:
//
#  if defined(BOOST_NO_CWCHAR) && !defined(BOOST_NO_CWCTYPE)
#     define BOOST_NO_CWCTYPE
#  endif

//
// We can't have a swprintf if there is no <cwchar>:
//
#  if defined(BOOST_NO_CWCHAR) && !defined(BOOST_NO_SWPRINTF)
#     define BOOST_NO_SWPRINTF
#  endif

//
// If Win32 support is turned off, then we must turn off
// threading support also, unless there is some other
// thread API enabled:
//
#if defined(BOOST_DISABLE_WIN32) && defined(_WIN32) \
   && !defined(BOOST_DISABLE_THREADS) && !defined(BOOST_HAS_PTHREADS)
#  define BOOST_DISABLE_THREADS
#endif

//
// Turn on threading support if the compiler thinks that it's in
// multithreaded mode.  We put this here because there are only a
// limited number of macros that identify this (if there's any missing
// from here then add to the appropriate compiler section):
//
#if (defined(__MT__) || defined(_MT) || defined(_REENTRANT) \
    || defined(_PTHREADS) || defined(__APPLE__) || defined(__DragonFly__)) \
    && !defined(BOOST_HAS_THREADS)
#  define BOOST_HAS_THREADS
#endif

//
// Turn threading support off if BOOST_DISABLE_THREADS is defined:
//
#if defined(BOOST_DISABLE_THREADS) && defined(BOOST_HAS_THREADS)
#  undef BOOST_HAS_THREADS
#endif

//
// Turn threading support off if we don't recognise the threading API:
//
#if defined(BOOST_HAS_THREADS) && !defined(BOOST_HAS_PTHREADS)\
      && !defined(BOOST_HAS_WINTHREADS) && !defined(BOOST_HAS_BETHREADS)\
      && !defined(BOOST_HAS_MPTASKS)
#  undef BOOST_HAS_THREADS
#endif

//
// Turn threading detail macros off if we don't (want to) use threading
//
#ifndef BOOST_HAS_THREADS
#  undef BOOST_HAS_PTHREADS
#  undef BOOST_HAS_PTHREAD_MUTEXATTR_SETTYPE
#  undef BOOST_HAS_PTHREAD_YIELD
#  undef BOOST_HAS_PTHREAD_DELAY_NP
#  undef BOOST_HAS_WINTHREADS
#  undef BOOST_HAS_BETHREADS
#  undef BOOST_HAS_MPTASKS
#endif

//
// If the compiler claims to be C99 conformant, then it had better
// have a <stdint.h>:
//
#  if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#     define BOOST_HAS_STDINT_H
#     ifndef BOOST_HAS_LOG1P
#        define BOOST_HAS_LOG1P
#     endif
#     ifndef BOOST_HAS_EXPM1
#        define BOOST_HAS_EXPM1
#     endif
#  endif

//
// Define BOOST_NO_SLIST and BOOST_NO_HASH if required.
// Note that this is for backwards compatibility only.
//
#  if !defined(BOOST_HAS_SLIST) && !defined(BOOST_NO_SLIST)
#     define BOOST_NO_SLIST
#  endif

#  if !defined(BOOST_HAS_HASH) && !defined(BOOST_NO_HASH)
#     define BOOST_NO_HASH
#  endif

//
// Set BOOST_SLIST_HEADER if not set already:
//
#if defined(BOOST_HAS_SLIST) && !defined(BOOST_SLIST_HEADER)
#  define BOOST_SLIST_HEADER <slist>
#endif

//
// Set BOOST_HASH_SET_HEADER if not set already:
//
#if defined(BOOST_HAS_HASH) && !defined(BOOST_HASH_SET_HEADER)
#  define BOOST_HASH_SET_HEADER <hash_set>
#endif

//
// Set BOOST_HASH_MAP_HEADER if not set already:
//
#if defined(BOOST_HAS_HASH) && !defined(BOOST_HASH_MAP_HEADER)
#  define BOOST_HASH_MAP_HEADER <hash_map>
#endif

//  BOOST_HAS_ABI_HEADERS
//  This macro gets set if we have headers that fix the ABI,
//  and prevent ODR violations when linking to external libraries:
#if defined(BOOST_ABI_PREFIX) && defined(BOOST_ABI_SUFFIX) && !defined(BOOST_HAS_ABI_HEADERS)
#  define BOOST_HAS_ABI_HEADERS
#endif

#if defined(BOOST_HAS_ABI_HEADERS) && defined(BOOST_DISABLE_ABI_HEADERS)
#  undef BOOST_HAS_ABI_HEADERS
#endif

//  BOOST_NO_STDC_NAMESPACE workaround  --------------------------------------//
//  Because std::size_t usage is so common, even in boost headers which do not
//  otherwise use the C library, the <cstddef> workaround is included here so
//  that ugly workaround code need not appear in many other boost headers.
//  NOTE WELL: This is a workaround for non-conforming compilers; <cstddef>
//  must still be #included in the usual places so that <cstddef> inclusion
//  works as expected with standard conforming compilers.  The resulting
//  double inclusion of <cstddef> is harmless.

# if defined(BOOST_NO_STDC_NAMESPACE) && defined(__cplusplus)
#   include <cstddef>
    namespace std { using ::ptrdiff_t; using ::size_t; }
# endif

//  Workaround for the unfortunate min/max macros defined by some platform headers

#define BOOST_PREVENT_MACRO_SUBSTITUTION

#ifndef BOOST_USING_STD_MIN
#  define BOOST_USING_STD_MIN() using std::min
#endif

#ifndef BOOST_USING_STD_MAX
#  define BOOST_USING_STD_MAX() using std::max
#endif

//  BOOST_NO_STD_MIN_MAX workaround  -----------------------------------------//

#  if defined(BOOST_NO_STD_MIN_MAX) && defined(__cplusplus)

namespace std {
  template <class _Tp>
  inline const _Tp& min BOOST_PREVENT_MACRO_SUBSTITUTION (const _Tp& __a, const _Tp& __b) {
    return __b < __a ? __b : __a;
  }
  template <class _Tp>
  inline const _Tp& max BOOST_PREVENT_MACRO_SUBSTITUTION (const _Tp& __a, const _Tp& __b) {
    return  __a < __b ? __b : __a;
  }
}

#  endif

// BOOST_STATIC_CONSTANT workaround --------------------------------------- //
// On compilers which don't allow in-class initialization of static integral
// constant members, we must use enums as a workaround if we want the constants
// to be available at compile-time. This macro gives us a convenient way to
// declare such constants.

#  ifdef BOOST_NO_INCLASS_MEMBER_INITIALIZATION
#       define BOOST_STATIC_CONSTANT(type, assignment) enum { assignment }
#  else
#     define BOOST_STATIC_CONSTANT(type, assignment) static const type assignment
#  endif

// BOOST_USE_FACET / HAS_FACET workaround ----------------------------------//
// When the standard library does not have a conforming std::use_facet there
// are various workarounds available, but they differ from library to library.
// The same problem occurs with has_facet.
// These macros provide a consistent way to access a locale's facets.
// Usage:
//    replace
//       std::use_facet<Type>(loc);
//    with
//       BOOST_USE_FACET(Type, loc);
//    Note do not add a std:: prefix to the front of BOOST_USE_FACET!
//  Use for BOOST_HAS_FACET is analogous.

#if defined(BOOST_NO_STD_USE_FACET)
#  ifdef BOOST_HAS_TWO_ARG_USE_FACET
#     define BOOST_USE_FACET(Type, loc) std::use_facet(loc, static_cast<Type*>(0))
#     define BOOST_HAS_FACET(Type, loc) std::has_facet(loc, static_cast<Type*>(0))
#  elif defined(BOOST_HAS_MACRO_USE_FACET)
#     define BOOST_USE_FACET(Type, loc) std::_USE(loc, Type)
#     define BOOST_HAS_FACET(Type, loc) std::_HAS(loc, Type)
#  elif defined(BOOST_HAS_STLP_USE_FACET)
#     define BOOST_USE_FACET(Type, loc) (*std::_Use_facet<Type >(loc))
#     define BOOST_HAS_FACET(Type, loc) std::has_facet< Type >(loc)
#  endif
#else
#  define BOOST_USE_FACET(Type, loc) std::use_facet< Type >(loc)
#  define BOOST_HAS_FACET(Type, loc) std::has_facet< Type >(loc)
#endif

// BOOST_NESTED_TEMPLATE workaround ------------------------------------------//
// Member templates are supported by some compilers even though they can't use
// the A::template member<U> syntax, as a workaround replace:
//
// typedef typename A::template rebind<U> binder;
//
// with:
//
// typedef typename A::BOOST_NESTED_TEMPLATE rebind<U> binder;

#ifndef BOOST_NO_MEMBER_TEMPLATE_KEYWORD
#  define BOOST_NESTED_TEMPLATE template
#else
#  define BOOST_NESTED_TEMPLATE
#endif

// BOOST_UNREACHABLE_RETURN(x) workaround -------------------------------------//
// Normally evaluates to nothing, unless BOOST_NO_UNREACHABLE_RETURN_DETECTION
// is defined, in which case it evaluates to return x; Use when you have a return
// statement that can never be reached.

#ifndef BOOST_UNREACHABLE_RETURN
#  ifdef BOOST_NO_UNREACHABLE_RETURN_DETECTION
#     define BOOST_UNREACHABLE_RETURN(x) return x;
#  else
#     define BOOST_UNREACHABLE_RETURN(x)
#  endif
#endif

// BOOST_DEDUCED_TYPENAME workaround ------------------------------------------//
//
// Some compilers don't support the use of `typename' for dependent
// types in deduced contexts, e.g.
//
//     template <class T> void f(T, typename T::type);
//                                  ^^^^^^^^
// Replace these declarations with:
//
//     template <class T> void f(T, BOOST_DEDUCED_TYPENAME T::type);

#ifndef BOOST_NO_DEDUCED_TYPENAME
#  define BOOST_DEDUCED_TYPENAME typename
#else
#  define BOOST_DEDUCED_TYPENAME
#endif

#ifndef BOOST_NO_TYPENAME_WITH_CTOR
#  define BOOST_CTOR_TYPENAME typename
#else
#  define BOOST_CTOR_TYPENAME
#endif

// long long workaround ------------------------------------------//
// On gcc (and maybe other compilers?) long long is alway supported
// but it's use may generate either warnings (with -ansi), or errors
// (with -pedantic -ansi) unless it's use is prefixed by __extension__
//
#if defined(BOOST_HAS_LONG_LONG) && defined(__cplusplus)
namespace boost{
#  ifdef __GNUC__
   __extension__ typedef long long long_long_type;
   __extension__ typedef unsigned long long ulong_long_type;
#  else
   typedef long long long_long_type;
   typedef unsigned long long ulong_long_type;
#  endif
}
#endif
// same again for __int128:
#if defined(BOOST_HAS_INT128) && defined(__cplusplus)
namespace boost{
#  ifdef __GNUC__
   __extension__ typedef __int128 int128_type;
   __extension__ typedef unsigned __int128 uint128_type;
#  else
   typedef __int128 int128_type;
   typedef unsigned __int128 uint128_type;
#  endif
}
#endif
// same again for __float128:
#if defined(BOOST_HAS_FLOAT128) && defined(__cplusplus)
namespace boost {
#  ifdef __GNUC__
   __extension__ typedef __float128 float128_type;
#  else
   typedef __float128 float128_type;
#  endif
}
#endif

// BOOST_[APPEND_]EXPLICIT_TEMPLATE_[NON_]TYPE macros --------------------------//

// These macros are obsolete. Port away and remove.

#  define BOOST_EXPLICIT_TEMPLATE_TYPE(t)
#  define BOOST_EXPLICIT_TEMPLATE_TYPE_SPEC(t)
#  define BOOST_EXPLICIT_TEMPLATE_NON_TYPE(t, v)
#  define BOOST_EXPLICIT_TEMPLATE_NON_TYPE_SPEC(t, v)

#  define BOOST_APPEND_EXPLICIT_TEMPLATE_TYPE(t)
#  define BOOST_APPEND_EXPLICIT_TEMPLATE_TYPE_SPEC(t)
#  define BOOST_APPEND_EXPLICIT_TEMPLATE_NON_TYPE(t, v)
#  define BOOST_APPEND_EXPLICIT_TEMPLATE_NON_TYPE_SPEC(t, v)

// When BOOST_NO_STD_TYPEINFO is defined, we can just import
// the global definition into std namespace:
#if defined(BOOST_NO_STD_TYPEINFO) && defined(__cplusplus)
#include <typeinfo>
namespace std{ using ::type_info; }
#endif

// ---------------------------------------------------------------------------//

// Helper macro BOOST_STRINGIZE:
// Helper macro BOOST_JOIN:

#include <boost/config/helper_macros.hpp>

//
// Set some default values for compiler/library/platform names.
// These are for debugging config setup only:
//
#  ifndef BOOST_COMPILER
#     define BOOST_COMPILER "Unknown ISO C++ Compiler"
#  endif
#  ifndef BOOST_STDLIB
#     define BOOST_STDLIB "Unknown ISO standard library"
#  endif
#  ifndef BOOST_PLATFORM
#     if defined(unix) || defined(__unix) || defined(_XOPEN_SOURCE) \
         || defined(_POSIX_SOURCE)
#        define BOOST_PLATFORM "Generic Unix"
#     else
#        define BOOST_PLATFORM "Unknown"
#     endif
#  endif

//
// Set some default values GPU support
//
#  ifndef BOOST_GPU_ENABLED
#  define BOOST_GPU_ENABLED
#  endif

// BOOST_RESTRICT ---------------------------------------------//
// Macro to use in place of 'restrict' keyword variants
#if !defined(BOOST_RESTRICT)
#  if defined(_MSC_VER)
#    define BOOST_RESTRICT __restrict
#    if !defined(BOOST_NO_RESTRICT_REFERENCES) && (_MSC_FULL_VER < 190023026)
#      define BOOST_NO_RESTRICT_REFERENCES
#    endif
#  elif defined(__GNUC__) && __GNUC__ > 3
     // Clang also defines __GNUC__ (as 4)
#    define BOOST_RESTRICT __restrict__
#  else
#    define BOOST_RESTRICT
#    if !defined(BOOST_NO_RESTRICT_REFERENCES)
#      define BOOST_NO_RESTRICT_REFERENCES
#    endif
#  endif
#endif

// BOOST_MAY_ALIAS -----------------------------------------------//
// The macro expands to an attribute to mark a type that is allowed to alias other types.
// The macro is defined in the compiler-specific headers.
#if !defined(BOOST_MAY_ALIAS)
#  define BOOST_NO_MAY_ALIAS
#  define BOOST_MAY_ALIAS
#endif

// BOOST_FORCEINLINE ---------------------------------------------//
// Macro to use in place of 'inline' to force a function to be inline
#if !defined(BOOST_FORCEINLINE)
#  if defined(_MSC_VER)
#    define BOOST_FORCEINLINE __forceinline
#  elif defined(__GNUC__) && __GNUC__ > 3
     // Clang also defines __GNUC__ (as 4)
#    define BOOST_FORCEINLINE inline __attribute__ ((__always_inline__))
#  else
#    define BOOST_FORCEINLINE inline
#  endif
#endif

// BOOST_NOINLINE ---------------------------------------------//
// Macro to use in place of 'inline' to prevent a function to be inlined
#if !defined(BOOST_NOINLINE)
#  if defined(_MSC_VER)
#    define BOOST_NOINLINE __declspec(noinline)
#  elif defined(__GNUC__) && __GNUC__ > 3
     // Clang also defines __GNUC__ (as 4)
#    if defined(__CUDACC__)
       // nvcc doesn't always parse __noinline__,
       // see: https://svn.boost.org/trac/boost/ticket/9392
#      define BOOST_NOINLINE __attribute__ ((noinline))
#    else
#      define BOOST_NOINLINE __attribute__ ((__noinline__))
#    endif
#  else
#    define BOOST_NOINLINE
#  endif
#endif

// BOOST_NORETURN ---------------------------------------------//
// Macro to use before a function declaration/definition to designate
// the function as not returning normally (i.e. with a return statement
// or by leaving the function scope, if the function return type is void).
#if !defined(BOOST_NORETURN)
#  if defined(_MSC_VER)
#    define BOOST_NORETURN __declspec(noreturn)
#  elif defined(__GNUC__)
#    define BOOST_NORETURN __attribute__ ((__noreturn__))
#  elif defined(__has_attribute) && defined(__SUNPRO_CC) && (__SUNPRO_CC > 0x5130)
#    if __has_attribute(noreturn)
#      define BOOST_NORETURN [[noreturn]]
#    endif
#  elif defined(__has_cpp_attribute) 
#    if __has_cpp_attribute(noreturn)
#      define BOOST_NORETURN [[noreturn]]
#    endif
#  endif
#endif

#if !defined(BOOST_NORETURN)
#  define BOOST_NO_NORETURN
#  define BOOST_NORETURN
#endif

// Branch prediction hints
// These macros are intended to wrap conditional expressions that yield true or false
//
//  if (BOOST_LIKELY(var == 10))
//  {
//     // the most probable code here
//  }
//
#if !defined(BOOST_LIKELY)
#  define BOOST_LIKELY(x) x
#endif
#if !defined(BOOST_UNLIKELY)
#  define BOOST_UNLIKELY(x) x
#endif

// Type and data alignment specification
//
#if !defined(BOOST_ALIGNMENT)
#  if !defined(BOOST_NO_CXX11_ALIGNAS)
#    define BOOST_ALIGNMENT(x) alignas(x)
#  elif defined(_MSC_VER)
#    define BOOST_ALIGNMENT(x) __declspec(align(x))
#  elif defined(__GNUC__)
#    define BOOST_ALIGNMENT(x) __attribute__ ((__aligned__(x)))
#  else
#    define BOOST_NO_ALIGNMENT
#    define BOOST_ALIGNMENT(x)
#  endif
#endif

// Lack of non-public defaulted functions is implied by the lack of any defaulted functions
#if !defined(BOOST_NO_CXX11_NON_PUBLIC_DEFAULTED_FUNCTIONS) && defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS)
#  define BOOST_NO_CXX11_NON_PUBLIC_DEFAULTED_FUNCTIONS
#endif

// Lack of defaulted moves is implied by the lack of either rvalue references or any defaulted functions
#if !defined(BOOST_NO_CXX11_DEFAULTED_MOVES) && (defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS) || defined(BOOST_NO_CXX11_RVALUE_REFERENCES))
#  define BOOST_NO_CXX11_DEFAULTED_MOVES
#endif

// Defaulted and deleted function declaration helpers
// These macros are intended to be inside a class definition.
// BOOST_DEFAULTED_FUNCTION accepts the function declaration and its
// body, which will be used if the compiler doesn't support defaulted functions.
// BOOST_DELETED_FUNCTION only accepts the function declaration. It
// will expand to a private function declaration, if the compiler doesn't support
// deleted functions. Because of this it is recommended to use BOOST_DELETED_FUNCTION
// in the end of the class definition.
//
//  class my_class
//  {
//  public:
//      // Default-constructible
//      BOOST_DEFAULTED_FUNCTION(my_class(), {})
//      // Copying prohibited
//      BOOST_DELETED_FUNCTION(my_class(my_class const&))
//      BOOST_DELETED_FUNCTION(my_class& operator= (my_class const&))
//  };
//
#if !(defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS) || defined(BOOST_NO_CXX11_NON_PUBLIC_DEFAULTED_FUNCTIONS))
#   define BOOST_DEFAULTED_FUNCTION(fun, body) fun = default;
#else
#   define BOOST_DEFAULTED_FUNCTION(fun, body) fun body
#endif

#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
#   define BOOST_DELETED_FUNCTION(fun) fun = delete;
#else
#   define BOOST_DELETED_FUNCTION(fun) private: fun;
#endif

//
// Set BOOST_NO_DECLTYPE_N3276 when BOOST_NO_DECLTYPE is defined
//
#if defined(BOOST_NO_CXX11_DECLTYPE) && !defined(BOOST_NO_CXX11_DECLTYPE_N3276)
#define BOOST_NO_CXX11_DECLTYPE_N3276 BOOST_NO_CXX11_DECLTYPE
#endif

//  -------------------- Deprecated macros for 1.50 ---------------------------
//  These will go away in a future release

//  Use BOOST_NO_CXX11_HDR_UNORDERED_SET or BOOST_NO_CXX11_HDR_UNORDERED_MAP
//           instead of BOOST_NO_STD_UNORDERED
#if defined(BOOST_NO_CXX11_HDR_UNORDERED_MAP) || defined (BOOST_NO_CXX11_HDR_UNORDERED_SET)
# ifndef BOOST_NO_CXX11_STD_UNORDERED
#  define BOOST_NO_CXX11_STD_UNORDERED
# endif
#endif

//  Use BOOST_NO_CXX11_HDR_INITIALIZER_LIST instead of BOOST_NO_INITIALIZER_LISTS
#if defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST) && !defined(BOOST_NO_INITIALIZER_LISTS)
#  define BOOST_NO_INITIALIZER_LISTS
#endif

//  Use BOOST_NO_CXX11_HDR_ARRAY instead of BOOST_NO_0X_HDR_ARRAY
#if defined(BOOST_NO_CXX11_HDR_ARRAY) && !defined(BOOST_NO_0X_HDR_ARRAY)
#  define BOOST_NO_0X_HDR_ARRAY
#endif
//  Use BOOST_NO_CXX11_HDR_CHRONO instead of BOOST_NO_0X_HDR_CHRONO
#if defined(BOOST_NO_CXX11_HDR_CHRONO) && !defined(BOOST_NO_0X_HDR_CHRONO)
#  define BOOST_NO_0X_HDR_CHRONO
#endif
//  Use BOOST_NO_CXX11_HDR_CODECVT instead of BOOST_NO_0X_HDR_CODECVT
#if defined(BOOST_NO_CXX11_HDR_CODECVT) && !defined(BOOST_NO_0X_HDR_CODECVT)
#  define BOOST_NO_0X_HDR_CODECVT
#endif
//  Use BOOST_NO_CXX11_HDR_CONDITION_VARIABLE instead of BOOST_NO_0X_HDR_CONDITION_VARIABLE
#if defined(BOOST_NO_CXX11_HDR_CONDITION_VARIABLE) && !defined(BOOST_NO_0X_HDR_CONDITION_VARIABLE)
#  define BOOST_NO_0X_HDR_CONDITION_VARIABLE
#endif
//  Use BOOST_NO_CXX11_HDR_FORWARD_LIST instead of BOOST_NO_0X_HDR_FORWARD_LIST
#if defined(BOOST_NO_CXX11_HDR_FORWARD_LIST) && !defined(BOOST_NO_0X_HDR_FORWARD_LIST)
#  define BOOST_NO_0X_HDR_FORWARD_LIST
#endif
//  Use BOOST_NO_CXX11_HDR_FUTURE instead of BOOST_NO_0X_HDR_FUTURE
#if defined(BOOST_NO_CXX11_HDR_FUTURE) && !defined(BOOST_NO_0X_HDR_FUTURE)
#  define BOOST_NO_0X_HDR_FUTURE
#endif

//  Use BOOST_NO_CXX11_HDR_INITIALIZER_LIST
//  instead of BOOST_NO_0X_HDR_INITIALIZER_LIST or BOOST_NO_INITIALIZER_LISTS
#ifdef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
# ifndef BOOST_NO_0X_HDR_INITIALIZER_LIST
#  define BOOST_NO_0X_HDR_INITIALIZER_LIST
# endif
# ifndef BOOST_NO_INITIALIZER_LISTS
#  define BOOST_NO_INITIALIZER_LISTS
# endif
#endif

//  Use BOOST_NO_CXX11_HDR_MUTEX instead of BOOST_NO_0X_HDR_MUTEX
#if defined(BOOST_NO_CXX11_HDR_MUTEX) && !defined(BOOST_NO_0X_HDR_MUTEX)
#  define BOOST_NO_0X_HDR_MUTEX
#endif
//  Use BOOST_NO_CXX11_HDR_RANDOM instead of BOOST_NO_0X_HDR_RANDOM
#if defined(BOOST_NO_CXX11_HDR_RANDOM) && !defined(BOOST_NO_0X_HDR_RANDOM)
#  define BOOST_NO_0X_HDR_RANDOM
#endif
//  Use BOOST_NO_CXX11_HDR_RATIO instead of BOOST_NO_0X_HDR_RATIO
#if defined(BOOST_NO_CXX11_HDR_RATIO) && !defined(BOOST_NO_0X_HDR_RATIO)
#  define BOOST_NO_0X_HDR_RATIO
#endif
//  Use BOOST_NO_CXX11_HDR_REGEX instead of BOOST_NO_0X_HDR_REGEX
#if defined(BOOST_NO_CXX11_HDR_REGEX) && !defined(BOOST_NO_0X_HDR_REGEX)
#  define BOOST_NO_0X_HDR_REGEX
#endif
//  Use BOOST_NO_CXX11_HDR_SYSTEM_ERROR instead of BOOST_NO_0X_HDR_SYSTEM_ERROR
#if defined(BOOST_NO_CXX11_HDR_SYSTEM_ERROR) && !defined(BOOST_NO_0X_HDR_SYSTEM_ERROR)
#  define BOOST_NO_0X_HDR_SYSTEM_ERROR
#endif
//  Use BOOST_NO_CXX11_HDR_THREAD instead of BOOST_NO_0X_HDR_THREAD
#if defined(BOOST_NO_CXX11_HDR_THREAD) && !defined(BOOST_NO_0X_HDR_THREAD)
#  define BOOST_NO_0X_HDR_THREAD
#endif
//  Use BOOST_NO_CXX11_HDR_TUPLE instead of BOOST_NO_0X_HDR_TUPLE
#if defined(BOOST_NO_CXX11_HDR_TUPLE) && !defined(BOOST_NO_0X_HDR_TUPLE)
#  define BOOST_NO_0X_HDR_TUPLE
#endif
//  Use BOOST_NO_CXX11_HDR_TYPE_TRAITS instead of BOOST_NO_0X_HDR_TYPE_TRAITS
#if defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS) && !defined(BOOST_NO_0X_HDR_TYPE_TRAITS)
#  define BOOST_NO_0X_HDR_TYPE_TRAITS
#endif
//  Use BOOST_NO_CXX11_HDR_TYPEINDEX instead of BOOST_NO_0X_HDR_TYPEINDEX
#if defined(BOOST_NO_CXX11_HDR_TYPEINDEX) && !defined(BOOST_NO_0X_HDR_TYPEINDEX)
#  define BOOST_NO_0X_HDR_TYPEINDEX
#endif
//  Use BOOST_NO_CXX11_HDR_UNORDERED_MAP instead of BOOST_NO_0X_HDR_UNORDERED_MAP
#if defined(BOOST_NO_CXX11_HDR_UNORDERED_MAP) && !defined(BOOST_NO_0X_HDR_UNORDERED_MAP)
#  define BOOST_NO_0X_HDR_UNORDERED_MAP
#endif
//  Use BOOST_NO_CXX11_HDR_UNORDERED_SET instead of BOOST_NO_0X_HDR_UNORDERED_SET
#if defined(BOOST_NO_CXX11_HDR_UNORDERED_SET) && !defined(BOOST_NO_0X_HDR_UNORDERED_SET)
#  define BOOST_NO_0X_HDR_UNORDERED_SET
#endif

//  ------------------ End of deprecated macros for 1.50 ---------------------------

//  -------------------- Deprecated macros for 1.51 ---------------------------
//  These will go away in a future release

//  Use     BOOST_NO_CXX11_AUTO_DECLARATIONS instead of   BOOST_NO_AUTO_DECLARATIONS
#if defined(BOOST_NO_CXX11_AUTO_DECLARATIONS) && !defined(BOOST_NO_AUTO_DECLARATIONS)
#  define BOOST_NO_AUTO_DECLARATIONS
#endif
//  Use     BOOST_NO_CXX11_AUTO_MULTIDECLARATIONS instead of   BOOST_NO_AUTO_MULTIDECLARATIONS
#if defined(BOOST_NO_CXX11_AUTO_MULTIDECLARATIONS) && !defined(BOOST_NO_AUTO_MULTIDECLARATIONS)
#  define BOOST_NO_AUTO_MULTIDECLARATIONS
#endif
//  Use     BOOST_NO_CXX11_CHAR16_T instead of   BOOST_NO_CHAR16_T
#if defined(BOOST_NO_CXX11_CHAR16_T) && !defined(BOOST_NO_CHAR16_T)
#  define BOOST_NO_CHAR16_T
#endif
//  Use     BOOST_NO_CXX11_CHAR32_T instead of   BOOST_NO_CHAR32_T
#if defined(BOOST_NO_CXX11_CHAR32_T) && !defined(BOOST_NO_CHAR32_T)
#  define BOOST_NO_CHAR32_T
#endif
//  Use     BOOST_NO_CXX11_TEMPLATE_ALIASES instead of   BOOST_NO_TEMPLATE_ALIASES
#if defined(BOOST_NO_CXX11_TEMPLATE_ALIASES) && !defined(BOOST_NO_TEMPLATE_ALIASES)
#  define BOOST_NO_TEMPLATE_ALIASES
#endif
//  Use     BOOST_NO_CXX11_CONSTEXPR instead of   BOOST_NO_CONSTEXPR
#if defined(BOOST_NO_CXX11_CONSTEXPR) && !defined(BOOST_NO_CONSTEXPR)
#  define BOOST_NO_CONSTEXPR
#endif
//  Use     BOOST_NO_CXX11_DECLTYPE_N3276 instead of   BOOST_NO_DECLTYPE_N3276
#if defined(BOOST_NO_CXX11_DECLTYPE_N3276) && !defined(BOOST_NO_DECLTYPE_N3276)
#  define BOOST_NO_DECLTYPE_N3276
#endif
//  Use     BOOST_NO_CXX11_DECLTYPE instead of   BOOST_NO_DECLTYPE
#if defined(BOOST_NO_CXX11_DECLTYPE) && !defined(BOOST_NO_DECLTYPE)
#  define BOOST_NO_DECLTYPE
#endif
//  Use     BOOST_NO_CXX11_DEFAULTED_FUNCTIONS instead of   BOOST_NO_DEFAULTED_FUNCTIONS
#if defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS) && !defined(BOOST_NO_DEFAULTED_FUNCTIONS)
#  define BOOST_NO_DEFAULTED_FUNCTIONS
#endif
//  Use     BOOST_NO_CXX11_DELETED_FUNCTIONS instead of   BOOST_NO_DELETED_FUNCTIONS
#if defined(BOOST_NO_CXX11_DELETED_FUNCTIONS) && !defined(BOOST_NO_DELETED_FUNCTIONS)
#  define BOOST_NO_DELETED_FUNCTIONS
#endif
//  Use     BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS instead of   BOOST_NO_EXPLICIT_CONVERSION_OPERATORS
#if defined(BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS) && !defined(BOOST_NO_EXPLICIT_CONVERSION_OPERATORS)
#  define BOOST_NO_EXPLICIT_CONVERSION_OPERATORS
#endif
//  Use     BOOST_NO_CXX11_EXTERN_TEMPLATE instead of   BOOST_NO_EXTERN_TEMPLATE
#if defined(BOOST_NO_CXX11_EXTERN_TEMPLATE) && !defined(BOOST_NO_EXTERN_TEMPLATE)
#  define BOOST_NO_EXTERN_TEMPLATE
#endif
//  Use     BOOST_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_ARGS instead of   BOOST_NO_FUNCTION_TEMPLATE_DEFAULT_ARGS
#if defined(BOOST_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_ARGS) && !defined(BOOST_NO_FUNCTION_TEMPLATE_DEFAULT_ARGS)
#  define BOOST_NO_FUNCTION_TEMPLATE_DEFAULT_ARGS
#endif
//  Use     BOOST_NO_CXX11_LAMBDAS instead of   BOOST_NO_LAMBDAS
#if defined(BOOST_NO_CXX11_LAMBDAS) && !defined(BOOST_NO_LAMBDAS)
#  define BOOST_NO_LAMBDAS
#endif
//  Use     BOOST_NO_CXX11_LOCAL_CLASS_TEMPLATE_PARAMETERS instead of   BOOST_NO_LOCAL_CLASS_TEMPLATE_PARAMETERS
#if defined(BOOST_NO_CXX11_LOCAL_CLASS_TEMPLATE_PARAMETERS) && !defined(BOOST_NO_LOCAL_CLASS_TEMPLATE_PARAMETERS)
#  define BOOST_NO_LOCAL_CLASS_TEMPLATE_PARAMETERS
#endif
//  Use     BOOST_NO_CXX11_NOEXCEPT instead of   BOOST_NO_NOEXCEPT
#if defined(BOOST_NO_CXX11_NOEXCEPT) && !defined(BOOST_NO_NOEXCEPT)
#  define BOOST_NO_NOEXCEPT
#endif
//  Use     BOOST_NO_CXX11_NULLPTR instead of   BOOST_NO_NULLPTR
#if defined(BOOST_NO_CXX11_NULLPTR) && !defined(BOOST_NO_NULLPTR)
#  define BOOST_NO_NULLPTR
#endif
//  Use     BOOST_NO_CXX11_RAW_LITERALS instead of   BOOST_NO_RAW_LITERALS
#if defined(BOOST_NO_CXX11_RAW_LITERALS) && !defined(BOOST_NO_RAW_LITERALS)
#  define BOOST_NO_RAW_LITERALS
#endif
//  Use     BOOST_NO_CXX11_RVALUE_REFERENCES instead of   BOOST_NO_RVALUE_REFERENCES
#if defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_NO_RVALUE_REFERENCES)
#  define BOOST_NO_RVALUE_REFERENCES
#endif
//  Use     BOOST_NO_CXX11_SCOPED_ENUMS instead of   BOOST_NO_SCOPED_ENUMS
#if defined(BOOST_NO_CXX11_SCOPED_ENUMS) && !defined(BOOST_NO_SCOPED_ENUMS)
#  define BOOST_NO_SCOPED_ENUMS
#endif
//  Use     BOOST_NO_CXX11_STATIC_ASSERT instead of   BOOST_NO_STATIC_ASSERT
#if defined(BOOST_NO_CXX11_STATIC_ASSERT) && !defined(BOOST_NO_STATIC_ASSERT)
#  define BOOST_NO_STATIC_ASSERT
#endif
//  Use     BOOST_NO_CXX11_STD_UNORDERED instead of   BOOST_NO_STD_UNORDERED
#if defined(BOOST_NO_CXX11_STD_UNORDERED) && !defined(BOOST_NO_STD_UNORDERED)
#  define BOOST_NO_STD_UNORDERED
#endif
//  Use     BOOST_NO_CXX11_UNICODE_LITERALS instead of   BOOST_NO_UNICODE_LITERALS
#if defined(BOOST_NO_CXX11_UNICODE_LITERALS) && !defined(BOOST_NO_UNICODE_LITERALS)
#  define BOOST_NO_UNICODE_LITERALS
#endif
//  Use     BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX instead of   BOOST_NO_UNIFIED_INITIALIZATION_SYNTAX
#if defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) && !defined(BOOST_NO_UNIFIED_INITIALIZATION_SYNTAX)
#  define BOOST_NO_UNIFIED_INITIALIZATION_SYNTAX
#endif
//  Use     BOOST_NO_CXX11_VARIADIC_TEMPLATES instead of   BOOST_NO_VARIADIC_TEMPLATES
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && !defined(BOOST_NO_VARIADIC_TEMPLATES)
#  define BOOST_NO_VARIADIC_TEMPLATES
#endif
//  Use     BOOST_NO_CXX11_VARIADIC_MACROS instead of   BOOST_NO_VARIADIC_MACROS
#if defined(BOOST_NO_CXX11_VARIADIC_MACROS) && !defined(BOOST_NO_VARIADIC_MACROS)
#  define BOOST_NO_VARIADIC_MACROS
#endif
//  Use     BOOST_NO_CXX11_NUMERIC_LIMITS instead of   BOOST_NO_NUMERIC_LIMITS_LOWEST
#if defined(BOOST_NO_CXX11_NUMERIC_LIMITS) && !defined(BOOST_NO_NUMERIC_LIMITS_LOWEST)
#  define BOOST_NO_NUMERIC_LIMITS_LOWEST
#endif
//  ------------------ End of deprecated macros for 1.51 ---------------------------


//
// Helper macro for marking types and methods final
//
#if !defined(BOOST_NO_CXX11_FINAL)
#  define BOOST_FINAL final
#else
#  define BOOST_FINAL
#endif

//
// Helper macros BOOST_NOEXCEPT, BOOST_NOEXCEPT_IF, BOOST_NOEXCEPT_EXPR
// These aid the transition to C++11 while still supporting C++03 compilers
//
#ifdef BOOST_NO_CXX11_NOEXCEPT
#  define BOOST_NOEXCEPT
#  define BOOST_NOEXCEPT_OR_NOTHROW throw()
#  define BOOST_NOEXCEPT_IF(Predicate)
#  define BOOST_NOEXCEPT_EXPR(Expression) false
#else
#  define BOOST_NOEXCEPT noexcept
#  define BOOST_NOEXCEPT_OR_NOTHROW noexcept
#  define BOOST_NOEXCEPT_IF(Predicate) noexcept((Predicate))
#  define BOOST_NOEXCEPT_EXPR(Expression) noexcept((Expression))
#endif
//
// Helper macro BOOST_FALLTHROUGH
// Fallback definition of BOOST_FALLTHROUGH macro used to mark intended
// fall-through between case labels in a switch statement. We use a definition
// that requires a semicolon after it to avoid at least one type of misuse even
// on unsupported compilers.
//
#ifndef BOOST_FALLTHROUGH
#  define BOOST_FALLTHROUGH ((void)0)
#endif

//
// constexpr workarounds
//
#if defined(BOOST_NO_CXX11_CONSTEXPR)
#define BOOST_CONSTEXPR
#define BOOST_CONSTEXPR_OR_CONST const
#else
#define BOOST_CONSTEXPR constexpr
#define BOOST_CONSTEXPR_OR_CONST constexpr
#endif
#if defined(BOOST_NO_CXX14_CONSTEXPR)
#define BOOST_CXX14_CONSTEXPR
#else
#define BOOST_CXX14_CONSTEXPR constexpr
#endif

//
// C++17 inline variables
//
#if !defined(BOOST_NO_CXX17_INLINE_VARIABLES)
#define BOOST_INLINE_VARIABLE inline
#else
#define BOOST_INLINE_VARIABLE
#endif
//
// C++17 if constexpr
//
#if !defined(BOOST_NO_CXX17_IF_CONSTEXPR)
#  define BOOST_IF_CONSTEXPR if constexpr
#else
#  define BOOST_IF_CONSTEXPR if
#endif

#define BOOST_INLINE_CONSTEXPR  BOOST_INLINE_VARIABLE BOOST_CONSTEXPR_OR_CONST

//
// Unused variable/typedef workarounds:
//
#ifndef BOOST_ATTRIBUTE_UNUSED
#  define BOOST_ATTRIBUTE_UNUSED
#endif
//
// [[nodiscard]]:
//
#if defined(__has_attribute) && defined(__SUNPRO_CC) && (__SUNPRO_CC > 0x5130)
#if __has_attribute(nodiscard)
# define BOOST_ATTRIBUTE_NODISCARD [[nodiscard]]
#endif
#if __has_attribute(no_unique_address)
# define BOOST_ATTRIBUTE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
#elif defined(__has_cpp_attribute)
// clang-6 accepts [[nodiscard]] with -std=c++14, but warns about it -pedantic
#if __has_cpp_attribute(nodiscard) && !(defined(__clang__) && (__cplusplus < 201703L))
# define BOOST_ATTRIBUTE_NODISCARD [[nodiscard]]
#endif
#if __has_cpp_attribute(no_unique_address) && !(defined(__GNUC__) && (__cplusplus < 201100))
# define BOOST_ATTRIBUTE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
#endif
#ifndef BOOST_ATTRIBUTE_NODISCARD
# define BOOST_ATTRIBUTE_NODISCARD
#endif
#ifndef BOOST_ATTRIBUTE_NO_UNIQUE_ADDRESS
# define BOOST_ATTRIBUTE_NO_UNIQUE_ADDRESS
#endif

#define BOOST_STATIC_CONSTEXPR  static BOOST_CONSTEXPR_OR_CONST

//
// Set BOOST_HAS_STATIC_ASSERT when BOOST_NO_CXX11_STATIC_ASSERT is not defined
//
#if !defined(BOOST_NO_CXX11_STATIC_ASSERT) && !defined(BOOST_HAS_STATIC_ASSERT)
#  define BOOST_HAS_STATIC_ASSERT
#endif

//
// Set BOOST_HAS_RVALUE_REFS when BOOST_NO_CXX11_RVALUE_REFERENCES is not defined
//
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_HAS_RVALUE_REFS)
#define BOOST_HAS_RVALUE_REFS
#endif

//
// Set BOOST_HAS_VARIADIC_TMPL when BOOST_NO_CXX11_VARIADIC_TEMPLATES is not defined
//
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && !defined(BOOST_HAS_VARIADIC_TMPL)
#define BOOST_HAS_VARIADIC_TMPL
#endif
//
// Set BOOST_NO_CXX11_FIXED_LENGTH_VARIADIC_TEMPLATE_EXPANSION_PACKS when
// BOOST_NO_CXX11_VARIADIC_TEMPLATES is set:
//
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_FIXED_LENGTH_VARIADIC_TEMPLATE_EXPANSION_PACKS)
#  define BOOST_NO_CXX11_FIXED_LENGTH_VARIADIC_TEMPLATE_EXPANSION_PACKS
#endif

// This is a catch all case for obsolete compilers / std libs:
#if !defined(_YVALS) && !defined(_CPPLIB_VER)  // msvc std lib already configured
#if (!defined(__has_include) || (__cplusplus < 201700))
#  define BOOST_NO_CXX17_HDR_OPTIONAL
#  define BOOST_NO_CXX17_HDR_STRING_VIEW
#  define BOOST_NO_CXX17_HDR_VARIANT
#else
#if !__has_include(<optional>)
#  define BOOST_NO_CXX17_HDR_OPTIONAL
#endif
#if !__has_include(<string_view>)
#  define BOOST_NO_CXX17_HDR_STRING_VIEW
#endif
#if !__has_include(<variant>)
#  define BOOST_NO_CXX17_HDR_VARIANT
#endif
#endif
#endif

//
// Finish off with checks for macros that are depricated / no longer supported,
// if any of these are set then it's very likely that much of Boost will no
// longer work.  So stop with a #error for now, but give the user a chance
// to continue at their own risk if they really want to:
//
#if defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION) && !defined(BOOST_CONFIG_ALLOW_DEPRECATED)
#  error "You are using a compiler which lacks features which are now a minimum requirement in order to use Boost, define BOOST_CONFIG_ALLOW_DEPRECATED if you want to continue at your own risk!!!"
#endif

#endif
