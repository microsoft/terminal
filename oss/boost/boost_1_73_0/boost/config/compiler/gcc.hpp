//  (C) Copyright John Maddock 2001 - 2003.
//  (C) Copyright Darin Adler 2001 - 2002.
//  (C) Copyright Jens Maurer 2001 - 2002.
//  (C) Copyright Beman Dawes 2001 - 2003.
//  (C) Copyright Douglas Gregor 2002.
//  (C) Copyright David Abrahams 2002 - 2003.
//  (C) Copyright Synge Todo 2003.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for most recent version.

//  GNU C++ compiler setup.

//
// Define BOOST_GCC so we know this is "real" GCC and not some pretender:
//
#define BOOST_GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if !defined(__CUDACC__)
#define BOOST_GCC BOOST_GCC_VERSION
#endif

#if defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L)
#  define BOOST_GCC_CXX11
#endif

#if __GNUC__ == 3
#  if defined (__PATHSCALE__)
#     define BOOST_NO_TWO_PHASE_NAME_LOOKUP
#     define BOOST_NO_IS_ABSTRACT
#  endif

#  if __GNUC_MINOR__ < 4
#     define BOOST_NO_IS_ABSTRACT
#  endif
#  define BOOST_NO_CXX11_EXTERN_TEMPLATE
#endif
#if __GNUC__ < 4
//
// All problems to gcc-3.x and earlier here:
//
#define BOOST_NO_TWO_PHASE_NAME_LOOKUP
#  ifdef __OPEN64__
#     define BOOST_NO_IS_ABSTRACT
#  endif
#endif

// GCC prior to 3.4 had #pragma once too but it didn't work well with filesystem links
#if BOOST_GCC_VERSION >= 30400
#define BOOST_HAS_PRAGMA_ONCE
#endif

#if BOOST_GCC_VERSION < 40400
// Previous versions of GCC did not completely implement value-initialization:
// GCC Bug 30111, "Value-initialization of POD base class doesn't initialize
// members", reported by Jonathan Wakely in 2006,
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=30111 (fixed for GCC 4.4)
// GCC Bug 33916, "Default constructor fails to initialize array members",
// reported by Michael Elizabeth Chastain in 2007,
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=33916 (fixed for GCC 4.2.4)
// See also: http://www.boost.org/libs/utility/value_init.htm#compiler_issues
#define BOOST_NO_COMPLETE_VALUE_INITIALIZATION
#endif

#if !defined(__EXCEPTIONS) && !defined(BOOST_NO_EXCEPTIONS)
# define BOOST_NO_EXCEPTIONS
#endif


//
// Threading support: Turn this on unconditionally here (except for
// those platforms where we can know for sure). It will get turned off again
// later if no threading API is detected.
//
#if !defined(__MINGW32__) && !defined(linux) && !defined(__linux) && !defined(__linux__)
# define BOOST_HAS_THREADS
#endif

//
// gcc has "long long"
// Except on Darwin with standard compliance enabled (-pedantic)
// Apple gcc helpfully defines this macro we can query
//
#if !defined(__DARWIN_NO_LONG_LONG)
# define BOOST_HAS_LONG_LONG
#endif

//
// gcc implements the named return value optimization since version 3.1
//
#define BOOST_HAS_NRVO

// Branch prediction hints
#define BOOST_LIKELY(x) __builtin_expect(x, 1)
#define BOOST_UNLIKELY(x) __builtin_expect(x, 0)

//
// Dynamic shared object (DSO) and dynamic-link library (DLL) support
//
#if __GNUC__ >= 4
#  if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__CYGWIN__)
     // All Win32 development environments, including 64-bit Windows and MinGW, define
     // _WIN32 or one of its variant spellings. Note that Cygwin is a POSIX environment,
     // so does not define _WIN32 or its variants, but still supports dllexport/dllimport.
#    define BOOST_HAS_DECLSPEC
#    define BOOST_SYMBOL_EXPORT __attribute__((__dllexport__))
#    define BOOST_SYMBOL_IMPORT __attribute__((__dllimport__))
#  else
#    define BOOST_SYMBOL_EXPORT __attribute__((__visibility__("default")))
#    define BOOST_SYMBOL_IMPORT
#  endif
#  define BOOST_SYMBOL_VISIBLE __attribute__((__visibility__("default")))
#else
// config/platform/win32.hpp will define BOOST_SYMBOL_EXPORT, etc., unless already defined
#  define BOOST_SYMBOL_EXPORT
#endif

//
// RTTI and typeinfo detection is possible post gcc-4.3:
//
#if BOOST_GCC_VERSION > 40300
#  ifndef __GXX_RTTI
#     ifndef BOOST_NO_TYPEID
#        define BOOST_NO_TYPEID
#     endif
#     ifndef BOOST_NO_RTTI
#        define BOOST_NO_RTTI
#     endif
#  endif
#endif

//
// Recent GCC versions have __int128 when in 64-bit mode.
//
// We disable this if the compiler is really nvcc with C++03 as it
// doesn't actually support __int128 as of CUDA_VERSION=7500
// even though it defines __SIZEOF_INT128__.
// See https://svn.boost.org/trac/boost/ticket/8048
//     https://svn.boost.org/trac/boost/ticket/11852
// Only re-enable this for nvcc if you're absolutely sure
// of the circumstances under which it's supported:
//
#if defined(__CUDACC__)
#  if defined(BOOST_GCC_CXX11)
#    define BOOST_NVCC_CXX11
#  else
#    define BOOST_NVCC_CXX03
#  endif
#endif

#if defined(__SIZEOF_INT128__) && !defined(BOOST_NVCC_CXX03)
#  define BOOST_HAS_INT128
#endif
//
// Recent GCC versions have a __float128 native type, we need to
// include a std lib header to detect this - not ideal, but we'll
// be including <cstddef> later anyway when we select the std lib.
//
// Nevertheless, as of CUDA 7.5, using __float128 with the host
// compiler in pre-C++11 mode is still not supported.
// See https://svn.boost.org/trac/boost/ticket/11852
//
#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif
#if defined(_GLIBCXX_USE_FLOAT128) && !defined(__STRICT_ANSI__) && !defined(BOOST_NVCC_CXX03)
# define BOOST_HAS_FLOAT128
#endif

// C++0x features in 4.3.n and later
//
#if (BOOST_GCC_VERSION >= 40300) && defined(BOOST_GCC_CXX11)
// C++0x features are only enabled when -std=c++0x or -std=gnu++0x are
// passed on the command line, which in turn defines
// __GXX_EXPERIMENTAL_CXX0X__.
#  define BOOST_HAS_DECLTYPE
#  define BOOST_HAS_RVALUE_REFS
#  define BOOST_HAS_STATIC_ASSERT
#  define BOOST_HAS_VARIADIC_TMPL
#else
#  define BOOST_NO_CXX11_DECLTYPE
#  define BOOST_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_ARGS
#  define BOOST_NO_CXX11_RVALUE_REFERENCES
#  define BOOST_NO_CXX11_STATIC_ASSERT
#endif

// C++0x features in 4.4.n and later
//
#if (BOOST_GCC_VERSION < 40400) || !defined(BOOST_GCC_CXX11)
#  define BOOST_NO_CXX11_AUTO_DECLARATIONS
#  define BOOST_NO_CXX11_AUTO_MULTIDECLARATIONS
#  define BOOST_NO_CXX11_CHAR16_T
#  define BOOST_NO_CXX11_CHAR32_T
#  define BOOST_NO_CXX11_HDR_INITIALIZER_LIST
#  define BOOST_NO_CXX11_DEFAULTED_FUNCTIONS
#  define BOOST_NO_CXX11_DELETED_FUNCTIONS
#  define BOOST_NO_CXX11_TRAILING_RESULT_TYPES
#  define BOOST_NO_CXX11_INLINE_NAMESPACES
#  define BOOST_NO_CXX11_VARIADIC_TEMPLATES
#endif

#if BOOST_GCC_VERSION < 40500
#  define BOOST_NO_SFINAE_EXPR
#endif

// GCC 4.5 forbids declaration of defaulted functions in private or protected sections
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ == 5) || !defined(BOOST_GCC_CXX11)
#  define BOOST_NO_CXX11_NON_PUBLIC_DEFAULTED_FUNCTIONS
#endif

// C++0x features in 4.5.0 and later
//
#if (BOOST_GCC_VERSION < 40500) || !defined(BOOST_GCC_CXX11)
#  define BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS
#  define BOOST_NO_CXX11_LAMBDAS
#  define BOOST_NO_CXX11_LOCAL_CLASS_TEMPLATE_PARAMETERS
#  define BOOST_NO_CXX11_RAW_LITERALS
#  define BOOST_NO_CXX11_UNICODE_LITERALS
#endif

// C++0x features in 4.5.1 and later
//
#if (BOOST_GCC_VERSION < 40501) || !defined(BOOST_GCC_CXX11)
// scoped enums have a serious bug in 4.4.0, so define BOOST_NO_CXX11_SCOPED_ENUMS before 4.5.1
// See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=38064
#  define BOOST_NO_CXX11_SCOPED_ENUMS
#endif

// C++0x features in 4.6.n and later
//
#if (BOOST_GCC_VERSION < 40600) || !defined(BOOST_GCC_CXX11)
#define BOOST_NO_CXX11_DEFAULTED_MOVES
#define BOOST_NO_CXX11_NOEXCEPT
#define BOOST_NO_CXX11_NULLPTR
#define BOOST_NO_CXX11_RANGE_BASED_FOR
#define BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
#endif

// C++0x features in 4.7.n and later
//
#if (BOOST_GCC_VERSION < 40700) || !defined(BOOST_GCC_CXX11)
// Note that while constexpr is partly supported in gcc-4.6 it's a 
// pre-std version with several bugs:
#  define BOOST_NO_CXX11_CONSTEXPR
#  define BOOST_NO_CXX11_FINAL
#  define BOOST_NO_CXX11_TEMPLATE_ALIASES
#  define BOOST_NO_CXX11_USER_DEFINED_LITERALS
#  define BOOST_NO_CXX11_FIXED_LENGTH_VARIADIC_TEMPLATE_EXPANSION_PACKS
#endif

// C++0x features in 4.8.n and later
//
#if (BOOST_GCC_VERSION < 40800) || !defined(BOOST_GCC_CXX11)
#  define BOOST_NO_CXX11_ALIGNAS
#  define BOOST_NO_CXX11_THREAD_LOCAL
#  define BOOST_NO_CXX11_SFINAE_EXPR
#endif

// C++0x features in 4.8.1 and later
//
#if (BOOST_GCC_VERSION < 40801) || !defined(BOOST_GCC_CXX11)
#  define BOOST_NO_CXX11_DECLTYPE_N3276
#  define BOOST_NO_CXX11_REF_QUALIFIERS
#  define BOOST_NO_CXX14_BINARY_LITERALS
#endif

// C++0x features in 5.1 and later
//
#if (BOOST_GCC_VERSION < 50100) || !defined(BOOST_GCC_CXX11)
#  define BOOST_NO_CXX11_UNRESTRICTED_UNION
#endif

// C++14 features in 4.9.0 and later
//
#if (BOOST_GCC_VERSION < 40900) || (__cplusplus < 201300)
#  define BOOST_NO_CXX14_RETURN_TYPE_DEDUCTION
#  define BOOST_NO_CXX14_GENERIC_LAMBDAS
#  define BOOST_NO_CXX14_DIGIT_SEPARATORS
#  define BOOST_NO_CXX14_DECLTYPE_AUTO
#  if !((BOOST_GCC_VERSION >= 40801) && (BOOST_GCC_VERSION < 40900) && defined(BOOST_GCC_CXX11))
#     define BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES
#  endif
#endif


// C++ 14:
#if !defined(__cpp_aggregate_nsdmi) || (__cpp_aggregate_nsdmi < 201304)
#  define BOOST_NO_CXX14_AGGREGATE_NSDMI
#endif
#if !defined(__cpp_constexpr) || (__cpp_constexpr < 201304)
#  define BOOST_NO_CXX14_CONSTEXPR
#endif
#if (BOOST_GCC_VERSION < 50200) || !defined(__cpp_variable_templates) || (__cpp_variable_templates < 201304)
#  define BOOST_NO_CXX14_VARIABLE_TEMPLATES
#endif

// C++17
#if !defined(__cpp_structured_bindings) || (__cpp_structured_bindings < 201606)
#  define BOOST_NO_CXX17_STRUCTURED_BINDINGS
#endif
#if !defined(__cpp_inline_variables) || (__cpp_inline_variables < 201606)
#  define BOOST_NO_CXX17_INLINE_VARIABLES
#endif
#if !defined(__cpp_fold_expressions) || (__cpp_fold_expressions < 201603)
#  define BOOST_NO_CXX17_FOLD_EXPRESSIONS
#endif
#if !defined(__cpp_if_constexpr) || (__cpp_if_constexpr < 201606)
#  define BOOST_NO_CXX17_IF_CONSTEXPR
#endif

#if __GNUC__ >= 7
#  define BOOST_FALLTHROUGH __attribute__((fallthrough))
#endif

#if defined(__MINGW32__) && !defined(__MINGW64__)
// Currently (March 2019) thread_local is broken on mingw for all current 32bit compiler releases, see
// https://sourceforge.net/p/mingw-w64/bugs/527/
// Not setting this causes program termination on thread exit.
#define BOOST_NO_CXX11_THREAD_LOCAL
#endif

//
// Unused attribute:
#if __GNUC__ >= 4
#  define BOOST_ATTRIBUTE_UNUSED __attribute__((__unused__))
#endif

// Type aliasing hint. Supported since gcc 3.3.
#define BOOST_MAY_ALIAS __attribute__((__may_alias__))

//
// __builtin_unreachable:
#if BOOST_GCC_VERSION >= 40500
#define BOOST_UNREACHABLE_RETURN(x) __builtin_unreachable();
#endif

#ifndef BOOST_COMPILER
#  define BOOST_COMPILER "GNU C++ version " __VERSION__
#endif

// ConceptGCC compiler:
//   http://www.generic-programming.org/software/ConceptGCC/
#ifdef __GXX_CONCEPTS__
#  define BOOST_HAS_CONCEPTS
#  define BOOST_COMPILER "ConceptGCC version " __VERSION__
#endif

// versions check:
// we don't know gcc prior to version 3.30:
#if (BOOST_GCC_VERSION< 30300)
#  error "Compiler not configured - please reconfigure"
#endif
//
// last known and checked version is 8.1:
#if (BOOST_GCC_VERSION > 80100)
#  if defined(BOOST_ASSERT_CONFIG)
#     error "Boost.Config is older than your compiler - please check for an updated Boost release."
#  else
// we don't emit warnings here anymore since there are no defect macros defined for
// gcc post 3.4, so any failures are gcc regressions...
//#     warning "boost: Unknown compiler version - please run the configure tests and report the results"
#  endif
#endif

