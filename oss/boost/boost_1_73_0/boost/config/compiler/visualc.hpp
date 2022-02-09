//  (C) Copyright John Maddock 2001 - 2003.
//  (C) Copyright Darin Adler 2001 - 2002.
//  (C) Copyright Peter Dimov 2001.
//  (C) Copyright Aleksey Gurtovoy 2002.
//  (C) Copyright David Abrahams 2002 - 2003.
//  (C) Copyright Beman Dawes 2002 - 2003.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for most recent version.
//
//  Microsoft Visual C++ compiler setup:
//
//  We need to be careful with the checks in this file, as contrary
//  to popular belief there are versions with _MSC_VER with the final
//  digit non-zero (mainly the MIPS cross compiler).
//
//  So we either test _MSC_VER >= XXXX or else _MSC_VER < XXXX.
//  No other comparisons (==, >, or <=) are safe.
//

#define BOOST_MSVC _MSC_VER

//
// Helper macro BOOST_MSVC_FULL_VER for use in Boost code:
//
#if _MSC_FULL_VER > 100000000
#  define BOOST_MSVC_FULL_VER _MSC_FULL_VER
#else
#  define BOOST_MSVC_FULL_VER (_MSC_FULL_VER * 10)
#endif

// Attempt to suppress VC6 warnings about the length of decorated names (obsolete):
#pragma warning( disable : 4503 ) // warning: decorated name length exceeded

#define BOOST_HAS_PRAGMA_ONCE

//
// versions check:
// we don't support Visual C++ prior to version 7.1:
#if _MSC_VER < 1310
#  error "Compiler not supported or configured - please reconfigure"
#endif

// VS2005 (VC8) docs: __assume has been in Visual C++ for multiple releases
#define BOOST_UNREACHABLE_RETURN(x) __assume(0);

#if _MSC_FULL_VER < 180020827
#  define BOOST_NO_FENV_H
#endif

#if _MSC_VER < 1400
// although a conforming signature for swprint exists in VC7.1
// it appears not to actually work:
#  define BOOST_NO_SWPRINTF
// Our extern template tests also fail for this compiler:
#  define BOOST_NO_CXX11_EXTERN_TEMPLATE
// Variadic macros do not exist for VC7.1 and lower
#  define BOOST_NO_CXX11_VARIADIC_MACROS
#  define BOOST_NO_CXX11_LOCAL_CLASS_TEMPLATE_PARAMETERS
#endif

#if _MSC_VER < 1500  // 140X == VC++ 8.0
#  define BOOST_NO_MEMBER_TEMPLATE_FRIENDS
#endif

#if _MSC_VER < 1600  // 150X == VC++ 9.0
   // A bug in VC9:
#  define BOOST_NO_ADL_BARRIER
#endif


#ifndef _NATIVE_WCHAR_T_DEFINED
#  define BOOST_NO_INTRINSIC_WCHAR_T
#endif

//
// check for exception handling support:
#if !defined(_CPPUNWIND) && !defined(BOOST_NO_EXCEPTIONS)
#  define BOOST_NO_EXCEPTIONS
#endif

//
// __int64 support:
//
#define BOOST_HAS_MS_INT64
#if defined(_MSC_EXTENSIONS) || (_MSC_VER >= 1400)
#   define BOOST_HAS_LONG_LONG
#else
#   define BOOST_NO_LONG_LONG
#endif
#if (_MSC_VER >= 1400) && !defined(_DEBUG)
#   define BOOST_HAS_NRVO
#endif
#if _MSC_VER >= 1600  // 160X == VC++ 10.0
#  define BOOST_HAS_PRAGMA_DETECT_MISMATCH
#endif
//
// disable Win32 API's if compiler extensions are
// turned off:
//
#if !defined(_MSC_EXTENSIONS) && !defined(BOOST_DISABLE_WIN32)
#  define BOOST_DISABLE_WIN32
#endif
#if !defined(_CPPRTTI) && !defined(BOOST_NO_RTTI)
#  define BOOST_NO_RTTI
#endif

//
// TR1 features:
//
#if (_MSC_VER >= 1700) && defined(_HAS_CXX17) && (_HAS_CXX17 > 0)
// # define BOOST_HAS_TR1_HASH          // don't know if this is true yet.
// # define BOOST_HAS_TR1_TYPE_TRAITS   // don't know if this is true yet.
# define BOOST_HAS_TR1_UNORDERED_MAP
# define BOOST_HAS_TR1_UNORDERED_SET
#endif

//
// C++0x features
//
//   See above for BOOST_NO_LONG_LONG

// C++ features supported by VC++ 10 (aka 2010)
//
#if _MSC_VER < 1600
#  define BOOST_NO_CXX11_AUTO_DECLARATIONS
#  define BOOST_NO_CXX11_AUTO_MULTIDECLARATIONS
#  define BOOST_NO_CXX11_LAMBDAS
#  define BOOST_NO_CXX11_RVALUE_REFERENCES
#  define BOOST_NO_CXX11_STATIC_ASSERT
#  define BOOST_NO_CXX11_NULLPTR
#  define BOOST_NO_CXX11_DECLTYPE
#endif // _MSC_VER < 1600

#if _MSC_VER >= 1600
#  define BOOST_HAS_STDINT_H
#endif

// C++11 features supported by VC++ 11 (aka 2012)
//
#if _MSC_VER < 1700
#  define BOOST_NO_CXX11_FINAL
#  define BOOST_NO_CXX11_RANGE_BASED_FOR
#  define BOOST_NO_CXX11_SCOPED_ENUMS
#endif // _MSC_VER < 1700

// C++11 features supported by VC++ 12 (aka 2013).
//
#if _MSC_FULL_VER < 180020827
#  define BOOST_NO_CXX11_DEFAULTED_FUNCTIONS
#  define BOOST_NO_CXX11_DELETED_FUNCTIONS
#  define BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS
#  define BOOST_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_ARGS
#  define BOOST_NO_CXX11_RAW_LITERALS
#  define BOOST_NO_CXX11_TEMPLATE_ALIASES
#  define BOOST_NO_CXX11_TRAILING_RESULT_TYPES
#  define BOOST_NO_CXX11_VARIADIC_TEMPLATES
#  define BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
#  define BOOST_NO_CXX11_DECLTYPE_N3276
#endif

#if _MSC_FULL_VER >= 180020827
#define BOOST_HAS_EXPM1
#define BOOST_HAS_LOG1P
#endif

// C++11 features supported by VC++ 14 (aka 2015)
//
#if (_MSC_FULL_VER < 190023026)
#  define BOOST_NO_CXX11_NOEXCEPT
#  define BOOST_NO_CXX11_DEFAULTED_MOVES
#  define BOOST_NO_CXX11_REF_QUALIFIERS
#  define BOOST_NO_CXX11_USER_DEFINED_LITERALS
#  define BOOST_NO_CXX11_ALIGNAS
#  define BOOST_NO_CXX11_INLINE_NAMESPACES
#  define BOOST_NO_CXX11_CHAR16_T
#  define BOOST_NO_CXX11_CHAR32_T
#  define BOOST_NO_CXX11_UNICODE_LITERALS
#  define BOOST_NO_CXX14_DECLTYPE_AUTO
#  define BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES
#  define BOOST_NO_CXX14_RETURN_TYPE_DEDUCTION
#  define BOOST_NO_CXX14_BINARY_LITERALS
#  define BOOST_NO_CXX14_GENERIC_LAMBDAS
#  define BOOST_NO_CXX14_DIGIT_SEPARATORS
#  define BOOST_NO_CXX11_THREAD_LOCAL
#  define BOOST_NO_CXX11_UNRESTRICTED_UNION
#endif
// C++11 features supported by VC++ 14 update 3 (aka 2015)
//
#if (_MSC_FULL_VER < 190024210)
#  define BOOST_NO_CXX14_VARIABLE_TEMPLATES
#  define BOOST_NO_SFINAE_EXPR
#  define BOOST_NO_CXX11_CONSTEXPR
#endif

// C++14 features supported by VC++ 14.1 (Visual Studio 2017)
//
#if (_MSC_VER < 1910)
#  define BOOST_NO_CXX14_AGGREGATE_NSDMI
#endif

// C++17 features supported by VC++ 14.1 (Visual Studio 2017) Update 3
//
#if (_MSC_VER < 1911) || (_MSVC_LANG < 201703)
#  define BOOST_NO_CXX17_STRUCTURED_BINDINGS
#  define BOOST_NO_CXX17_IF_CONSTEXPR
// Let the defaults handle these now:
//#  define BOOST_NO_CXX17_HDR_OPTIONAL
//#  define BOOST_NO_CXX17_HDR_STRING_VIEW
#endif

// MSVC including version 14 has not yet completely
// implemented value-initialization, as is reported:
// "VC++ does not value-initialize members of derived classes without
// user-declared constructor", reported in 2009 by Sylvester Hesp:
// https://connect.microsoft.com/VisualStudio/feedback/details/484295
// "Presence of copy constructor breaks member class initialization",
// reported in 2009 by Alex Vakulenko:
// https://connect.microsoft.com/VisualStudio/feedback/details/499606
// "Value-initialization in new-expression", reported in 2005 by
// Pavel Kuznetsov (MetaCommunications Engineering):
// https://connect.microsoft.com/VisualStudio/feedback/details/100744
// Reported again by John Maddock in 2015 for VC14:
// https://connect.microsoft.com/VisualStudio/feedback/details/1582233/c-subobjects-still-not-value-initialized-correctly
// See also: http://www.boost.org/libs/utility/value_init.htm#compiler_issues
// (Niels Dekker, LKEB, May 2010)
// Still present in VC15.5, Dec 2017.
#define BOOST_NO_COMPLETE_VALUE_INITIALIZATION
//
// C++ 11:
//
// This is supported with /permissive- for 15.5 onwards, unfortunately we appear to have no way to tell
// if this is in effect or not, in any case nothing in Boost is currently using this, so we'll just go
// on defining it for now:
//
#  define BOOST_NO_TWO_PHASE_NAME_LOOKUP

#if (_MSC_VER < 1912) || (_MSVC_LANG < 201402)
// Supported from msvc-15.5 onwards:
#define BOOST_NO_CXX11_SFINAE_EXPR
#endif
#if (_MSC_VER < 1915) || (_MSVC_LANG < 201402)
// C++ 14:
// Still gives internal compiler error for msvc-15.5:
#  define BOOST_NO_CXX14_CONSTEXPR
#endif
// C++ 17:
#if (_MSC_VER < 1912) || (_MSVC_LANG < 201703)
#define BOOST_NO_CXX17_INLINE_VARIABLES
#define BOOST_NO_CXX17_FOLD_EXPRESSIONS
#endif

//
// Things that don't work in clr mode:
//
#ifdef _M_CEE
#ifndef BOOST_NO_CXX11_THREAD_LOCAL
#  define BOOST_NO_CXX11_THREAD_LOCAL
#endif
#ifndef BOOST_NO_SFINAE_EXPR
#  define BOOST_NO_SFINAE_EXPR
#endif
#ifndef BOOST_NO_CXX11_REF_QUALIFIERS
#  define BOOST_NO_CXX11_REF_QUALIFIERS
#endif
#endif
#ifdef _M_CEE_PURE
#ifndef BOOST_NO_CXX11_CONSTEXPR
#  define BOOST_NO_CXX11_CONSTEXPR
#endif
#endif

//
// prefix and suffix headers:
//
#ifndef BOOST_ABI_PREFIX
#  define BOOST_ABI_PREFIX "boost/config/abi/msvc_prefix.hpp"
#endif
#ifndef BOOST_ABI_SUFFIX
#  define BOOST_ABI_SUFFIX "boost/config/abi/msvc_suffix.hpp"
#endif

#ifndef BOOST_COMPILER
// TODO:
// these things are mostly bogus. 1200 means version 12.0 of the compiler. The
// artificial versions assigned to them only refer to the versions of some IDE
// these compilers have been shipped with, and even that is not all of it. Some
// were shipped with freely downloadable SDKs, others as crosscompilers in eVC.
// IOW, you can't use these 'versions' in any sensible way. Sorry.
# if defined(UNDER_CE)
#   if _MSC_VER < 1400
      // Note: I'm not aware of any CE compiler with version 13xx
#      if defined(BOOST_ASSERT_CONFIG)
#         error "boost: Unknown EVC++ compiler version - please run the configure tests and report the results"
#      else
#         pragma message("boost: Unknown EVC++ compiler version - please run the configure tests and report the results")
#      endif
#   elif _MSC_VER < 1500
#     define BOOST_COMPILER_VERSION evc8
#   elif _MSC_VER < 1600
#     define BOOST_COMPILER_VERSION evc9
#   elif _MSC_VER < 1700
#     define BOOST_COMPILER_VERSION evc10
#   elif _MSC_VER < 1800 
#     define BOOST_COMPILER_VERSION evc11 
#   elif _MSC_VER < 1900 
#     define BOOST_COMPILER_VERSION evc12
#   elif _MSC_VER < 2000  
#     define BOOST_COMPILER_VERSION evc14
#   else
#      if defined(BOOST_ASSERT_CONFIG)
#         error "boost: Unknown EVC++ compiler version - please run the configure tests and report the results"
#      else
#         pragma message("boost: Unknown EVC++ compiler version - please run the configure tests and report the results")
#      endif
#   endif
# else
#   if _MSC_VER < 1200
      // Note: Versions up to 10.0 aren't supported.
#     define BOOST_COMPILER_VERSION 5.0
#   elif _MSC_VER < 1300
#     define BOOST_COMPILER_VERSION 6.0
#   elif _MSC_VER < 1310
#     define BOOST_COMPILER_VERSION 7.0
#   elif _MSC_VER < 1400
#     define BOOST_COMPILER_VERSION 7.1
#   elif _MSC_VER < 1500
#     define BOOST_COMPILER_VERSION 8.0
#   elif _MSC_VER < 1600
#     define BOOST_COMPILER_VERSION 9.0
#   elif _MSC_VER < 1700
#     define BOOST_COMPILER_VERSION 10.0
#   elif _MSC_VER < 1800 
#     define BOOST_COMPILER_VERSION 11.0
#   elif _MSC_VER < 1900
#     define BOOST_COMPILER_VERSION 12.0
#   elif _MSC_VER < 1910
#     define BOOST_COMPILER_VERSION 14.0
#   elif _MSC_VER < 1920
#     define BOOST_COMPILER_VERSION 14.1
#   elif _MSC_VER < 1930
#     define BOOST_COMPILER_VERSION 14.2
#   else
#     define BOOST_COMPILER_VERSION _MSC_VER
#   endif
# endif

#  define BOOST_COMPILER "Microsoft Visual C++ version " BOOST_STRINGIZE(BOOST_COMPILER_VERSION)
#endif

#include <boost/config/pragma_message.hpp>

//
// last known and checked version is 19.20.27508 (VC++ 2019 RC3):
#if (_MSC_VER > 1920)
#  if defined(BOOST_ASSERT_CONFIG)
#     error "Boost.Config is older than your current compiler version."
#  elif !defined(BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE)
      //
      // Disabled as of March 2018 - the pace of VS releases is hard to keep up with
      // and in any case, we have relatively few defect macros defined now.
      // BOOST_PRAGMA_MESSAGE("Info: Boost.Config is older than your compiler version - probably nothing bad will happen - but you may wish to look for an updated Boost version. Define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE to suppress this message.")
#  endif
#endif
