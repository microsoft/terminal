//  Copyright (c) 2017 Dynatrace
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt

//  See http://www.boost.org for most recent version.

//  Compiler setup for IBM z/OS XL C/C++ compiler.

// Oldest compiler version currently supported is 2.1 (V2R1)
#if !defined(__IBMCPP__) || !defined(__COMPILER_VER__) || __COMPILER_VER__ < 0x42010000
#  error "Compiler not supported or configured - please reconfigure"
#endif

#if __COMPILER_VER__ > 0x42010000
#  if defined(BOOST_ASSERT_CONFIG)
#     error "Unknown compiler version - please run the configure tests and report the results"
#  endif
#endif

#define BOOST_COMPILER "IBM z/OS XL C/C++ version " BOOST_STRINGIZE(__COMPILER_VER__)
#define BOOST_XLCPP_ZOS __COMPILER_VER__

// -------------------------------------

#include <features.h> // For __UU, __C99, __TR1, ...

#if !defined(__IBMCPP_DEFAULTED_AND_DELETED_FUNCTIONS)
#  define BOOST_NO_CXX11_DELETED_FUNCTIONS
#  define BOOST_NO_CXX11_DEFAULTED_FUNCTIONS
#  define BOOST_NO_CXX11_NON_PUBLIC_DEFAULTED_FUNCTIONS
#endif

// -------------------------------------

#if defined(__UU) || defined(__C99) || defined(__TR1)
#  define BOOST_HAS_LOG1P
#  define BOOST_HAS_EXPM1
#endif

#if defined(__C99) || defined(__TR1)
#  define BOOST_HAS_STDINT_H
#else
#  define BOOST_NO_FENV_H
#endif

// -------------------------------------

#define BOOST_HAS_NRVO

#if !defined(__RTTI_ALL__)
#  define BOOST_NO_RTTI
#endif

#if !defined(_CPPUNWIND) && !defined(__EXCEPTIONS)
#  define BOOST_NO_EXCEPTIONS
#endif

#if defined(_LONG_LONG) || defined(__IBMCPP_C99_LONG_LONG) || defined(__LL)
#  define BOOST_HAS_LONG_LONG
#else
#  define BOOST_NO_LONG_LONG
#endif

#if defined(_LONG_LONG) || defined(__IBMCPP_C99_LONG_LONG) || defined(__LL) || defined(_LP64)
#  define BOOST_HAS_MS_INT64
#endif

#define BOOST_NO_SFINAE_EXPR
#define BOOST_NO_CXX11_SFINAE_EXPR

#if defined(__IBMCPP_VARIADIC_TEMPLATES)
#  define BOOST_HAS_VARIADIC_TMPL
#else
#  define BOOST_NO_CXX11_VARIADIC_TEMPLATES
#  define BOOST_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_ARGS
#endif

#if defined(__IBMCPP_STATIC_ASSERT)
#  define BOOST_HAS_STATIC_ASSERT
#else
#  define BOOST_NO_CXX11_STATIC_ASSERT
#endif

#if defined(__IBMCPP_RVALUE_REFERENCES)
#  define BOOST_HAS_RVALUE_REFS
#else
#  define BOOST_NO_CXX11_RVALUE_REFERENCES
#endif

#if !defined(__IBMCPP_SCOPED_ENUM)
#  define BOOST_NO_CXX11_SCOPED_ENUMS
#endif

#define BOOST_NO_CXX11_FIXED_LENGTH_VARIADIC_TEMPLATE_EXPANSION_PACKS
#define BOOST_NO_CXX11_TEMPLATE_ALIASES
#define BOOST_NO_CXX11_LOCAL_CLASS_TEMPLATE_PARAMETERS

#if !defined(__IBMCPP_EXPLICIT_CONVERSION_OPERATORS)
#  define BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS
#endif

#if !defined(__IBMCPP_DECLTYPE)
#  define BOOST_NO_CXX11_DECLTYPE
#else
#  define BOOST_HAS_DECLTYPE
#endif
#define BOOST_NO_CXX11_DECLTYPE_N3276

#if !defined(__IBMCPP_INLINE_NAMESPACE)
#  define BOOST_NO_CXX11_INLINE_NAMESPACES
#endif

#if !defined(__IBMCPP_AUTO_TYPEDEDUCTION)
#  define BOOST_NO_CXX11_AUTO_MULTIDECLARATIONS
#  define BOOST_NO_CXX11_AUTO_DECLARATIONS
#  define BOOST_NO_CXX11_TRAILING_RESULT_TYPES
#endif

#if !defined(__IBM_CHAR32_T__)
#  define BOOST_NO_CXX11_CHAR32_T
#endif
#if !defined(__IBM_CHAR16_T__)
#  define BOOST_NO_CXX11_CHAR16_T
#endif

#if !defined(__IBMCPP_CONSTEXPR)
#  define BOOST_NO_CXX11_CONSTEXPR
#endif

#define BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
#define BOOST_NO_CXX11_UNICODE_LITERALS
#define BOOST_NO_CXX11_RAW_LITERALS
#define BOOST_NO_CXX11_RANGE_BASED_FOR
#define BOOST_NO_CXX11_NULLPTR
#define BOOST_NO_CXX11_NOEXCEPT
#define BOOST_NO_CXX11_LAMBDAS
#define BOOST_NO_CXX11_USER_DEFINED_LITERALS
#define BOOST_NO_CXX11_THREAD_LOCAL
#define BOOST_NO_CXX11_REF_QUALIFIERS
#define BOOST_NO_CXX11_FINAL
#define BOOST_NO_CXX11_ALIGNAS
#define BOOST_NO_CXX11_UNRESTRICTED_UNION
#define BOOST_NO_CXX14_VARIABLE_TEMPLATES
#define BOOST_NO_CXX14_RETURN_TYPE_DEDUCTION
#define BOOST_NO_CXX14_AGGREGATE_NSDMI
#define BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES
#define BOOST_NO_CXX14_GENERIC_LAMBDAS
#define BOOST_NO_CXX14_DIGIT_SEPARATORS
#define BOOST_NO_CXX14_DECLTYPE_AUTO
#define BOOST_NO_CXX14_CONSTEXPR
#define BOOST_NO_CXX14_BINARY_LITERALS
#define BOOST_NO_CXX17_STRUCTURED_BINDINGS
#define BOOST_NO_CXX17_INLINE_VARIABLES
#define BOOST_NO_CXX17_FOLD_EXPRESSIONS
#define BOOST_NO_CXX17_IF_CONSTEXPR

// -------------------------------------

#if defined(__IBM_ATTRIBUTES)
#  define BOOST_FORCEINLINE inline __attribute__ ((__always_inline__))
#  define BOOST_NOINLINE __attribute__ ((__noinline__))
#  define BOOST_MAY_ALIAS __attribute__((__may_alias__))
// No BOOST_ALIGNMENT - explicit alignment support is broken (V2R1).
#endif

extern "builtin" long __builtin_expect(long, long);

#define BOOST_LIKELY(x) __builtin_expect((x) && true, 1)
#define BOOST_UNLIKELY(x) __builtin_expect((x) && true, 0)
