/// Json-cpp amalgamated forward header (http://jsoncpp.sourceforge.net/).
/// It is intended to be used with #include "json/json-forwards.h"
/// This header provides forward declaration for all JsonCpp types.

// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: LICENSE
// //////////////////////////////////////////////////////////////////////

/*
The JsonCpp library's source code, including accompanying documentation, 
tests and demonstration applications, are licensed under the following
conditions...

Baptiste Lepilleur and The JsonCpp Authors explicitly disclaim copyright in all 
jurisdictions which recognize such a disclaimer. In such jurisdictions, 
this software is released into the Public Domain.

In jurisdictions which do not recognize Public Domain property (e.g. Germany as of
2010), this software is Copyright (c) 2007-2010 by Baptiste Lepilleur and
The JsonCpp Authors, and is released under the terms of the MIT License (see below).

In jurisdictions which recognize Public Domain property, the user of this 
software may choose to accept it either as 1) Public Domain, 2) under the 
conditions of the MIT License (see below), or 3) under the terms of dual 
Public Domain/MIT License conditions described here, as they choose.

The MIT License is about as close to Public Domain as a license can get, and is
described in clear, concise terms at:

   http://en.wikipedia.org/wiki/MIT_License
   
The full text of the MIT License follows:

========================================================================
Copyright (c) 2007-2010 Baptiste Lepilleur and The JsonCpp Authors

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
========================================================================
(END LICENSE TEXT)

The MIT license is compatible with both the GPL and commercial
software, affording one all of the rights of Public Domain with the
minor nuisance of being required to keep the above copyright notice
and license text in the source code. Note also that by accepting the
Public Domain "license" you can re-license your copy using whatever
license you like.

*/

// //////////////////////////////////////////////////////////////////////
// End of content of file: LICENSE
// //////////////////////////////////////////////////////////////////////





#ifndef JSON_FORWARD_AMALGAMATED_H_INCLUDED
# define JSON_FORWARD_AMALGAMATED_H_INCLUDED
/// If defined, indicates that the source file is amalgamated
/// to prevent private header inclusion.
#define JSON_IS_AMALGAMATION

// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: include/json/config.h
// //////////////////////////////////////////////////////////////////////

// Copyright 2007-2010 Baptiste Lepilleur and The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#ifndef JSON_CONFIG_H_INCLUDED
#define JSON_CONFIG_H_INCLUDED
#include <stddef.h>
#include <string> //typedef String
#include <stdint.h> //typedef int64_t, uint64_t

/// If defined, indicates that json library is embedded in CppTL library.
//# define JSON_IN_CPPTL 1

/// If defined, indicates that json may leverage CppTL library
//#  define JSON_USE_CPPTL 1
/// If defined, indicates that cpptl vector based map should be used instead of
/// std::map
/// as Value container.
//#  define JSON_USE_CPPTL_SMALLMAP 1

// If non-zero, the library uses exceptions to report bad input instead of C
// assertion macros. The default is to use exceptions.
#ifndef JSON_USE_EXCEPTION
#define JSON_USE_EXCEPTION 1
#endif

/// If defined, indicates that the source file is amalgamated
/// to prevent private header inclusion.
/// Remarks: it is automatically defined in the generated amalgamated header.
// #define JSON_IS_AMALGAMATION

#ifdef JSON_IN_CPPTL
#include <cpptl/config.h>
#ifndef JSON_USE_CPPTL
#define JSON_USE_CPPTL 1
#endif
#endif

#ifdef JSON_IN_CPPTL
#define JSON_API CPPTL_API
#elif defined(JSON_DLL_BUILD)
#if defined(_MSC_VER) || defined(__MINGW32__)
#define JSON_API __declspec(dllexport)
#define JSONCPP_DISABLE_DLL_INTERFACE_WARNING
#endif // if defined(_MSC_VER)
#elif defined(JSON_DLL)
#if defined(_MSC_VER) || defined(__MINGW32__)
#define JSON_API __declspec(dllimport)
#define JSONCPP_DISABLE_DLL_INTERFACE_WARNING
#endif // if defined(_MSC_VER)
#endif // ifdef JSON_IN_CPPTL
#if !defined(JSON_API)
#define JSON_API
#endif

// If JSON_NO_INT64 is defined, then Json only support C++ "int" type for
// integer
// Storages, and 64 bits integer support is disabled.
// #define JSON_NO_INT64 1

#if defined(_MSC_VER) // MSVC
#  if _MSC_VER <= 1200 // MSVC 6
    // Microsoft Visual Studio 6 only support conversion from __int64 to double
    // (no conversion from unsigned __int64).
#    define JSON_USE_INT64_DOUBLE_CONVERSION 1
    // Disable warning 4786 for VS6 caused by STL (identifier was truncated to '255'
    // characters in the debug information)
    // All projects I've ever seen with VS6 were using this globally (not bothering
    // with pragma push/pop).
#    pragma warning(disable : 4786)
#  endif // MSVC 6

#  if _MSC_VER >= 1500 // MSVC 2008
    /// Indicates that the following function is deprecated.
#    define JSONCPP_DEPRECATED(message) __declspec(deprecated(message))
#  endif

#endif // defined(_MSC_VER)

// In c++11 the override keyword allows you to explicitly define that a function
// is intended to override the base-class version.  This makes the code more
// managable and fixes a set of common hard-to-find bugs.
#if __cplusplus >= 201103L
# define JSONCPP_OVERRIDE override
# define JSONCPP_NOEXCEPT noexcept
#elif defined(_MSC_VER) && _MSC_VER > 1600 && _MSC_VER < 1900
# define JSONCPP_OVERRIDE override
# define JSONCPP_NOEXCEPT throw()
#elif defined(_MSC_VER) && _MSC_VER >= 1900
# define JSONCPP_OVERRIDE override
# define JSONCPP_NOEXCEPT noexcept
#else
# define JSONCPP_OVERRIDE
# define JSONCPP_NOEXCEPT throw()
#endif

#ifndef JSON_HAS_RVALUE_REFERENCES

#if defined(_MSC_VER) && _MSC_VER >= 1600 // MSVC >= 2010
#define JSON_HAS_RVALUE_REFERENCES 1
#endif // MSVC >= 2010

#ifdef __clang__
#if __has_feature(cxx_rvalue_references)
#define JSON_HAS_RVALUE_REFERENCES 1
#endif  // has_feature

#elif defined __GNUC__ // not clang (gcc comes later since clang emulates gcc)
#if defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L)
#define JSON_HAS_RVALUE_REFERENCES 1
#endif  // GXX_EXPERIMENTAL

#endif // __clang__ || __GNUC__

#endif // not defined JSON_HAS_RVALUE_REFERENCES

#ifndef JSON_HAS_RVALUE_REFERENCES
#define JSON_HAS_RVALUE_REFERENCES 0
#endif

#ifdef __clang__
#  if __has_extension(attribute_deprecated_with_message)
#    define JSONCPP_DEPRECATED(message)  __attribute__ ((deprecated(message)))
#  endif
#elif defined __GNUC__ // not clang (gcc comes later since clang emulates gcc)
#  if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
#    define JSONCPP_DEPRECATED(message)  __attribute__ ((deprecated(message)))
#  elif (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#    define JSONCPP_DEPRECATED(message)  __attribute__((__deprecated__))
#  endif  // GNUC version
#endif // __clang__ || __GNUC__

#if !defined(JSONCPP_DEPRECATED)
#define JSONCPP_DEPRECATED(message)
#endif // if !defined(JSONCPP_DEPRECATED)

#if __GNUC__ >= 6
#  define JSON_USE_INT64_DOUBLE_CONVERSION 1
#endif

#if !defined(JSON_IS_AMALGAMATION)

# include "version.h"

# if JSONCPP_USING_SECURE_MEMORY
#  include "allocator.h" //typedef Allocator
# endif

#endif // if !defined(JSON_IS_AMALGAMATION)

namespace Json {
typedef int Int;
typedef unsigned int UInt;
#if defined(JSON_NO_INT64)
typedef int LargestInt;
typedef unsigned int LargestUInt;
#undef JSON_HAS_INT64
#else                 // if defined(JSON_NO_INT64)
// For Microsoft Visual use specific types as long long is not supported
#if defined(_MSC_VER) // Microsoft Visual Studio
typedef __int64 Int64;
typedef unsigned __int64 UInt64;
#else                 // if defined(_MSC_VER) // Other platforms, use long long
typedef int64_t Int64;
typedef uint64_t UInt64;
#endif // if defined(_MSC_VER)
typedef Int64 LargestInt;
typedef UInt64 LargestUInt;
#define JSON_HAS_INT64
#endif // if defined(JSON_NO_INT64)
#if JSONCPP_USING_SECURE_MEMORY
#define JSONCPP_STRING        std::basic_string<char, std::char_traits<char>, Json::SecureAllocator<char> >
#define JSONCPP_OSTRINGSTREAM std::basic_ostringstream<char, std::char_traits<char>, Json::SecureAllocator<char> >
#define JSONCPP_OSTREAM       std::basic_ostream<char, std::char_traits<char>>
#define JSONCPP_ISTRINGSTREAM std::basic_istringstream<char, std::char_traits<char>, Json::SecureAllocator<char> >
#define JSONCPP_ISTREAM       std::istream
#else
#define JSONCPP_STRING        std::string
#define JSONCPP_OSTRINGSTREAM std::ostringstream
#define JSONCPP_OSTREAM       std::ostream
#define JSONCPP_ISTRINGSTREAM std::istringstream
#define JSONCPP_ISTREAM       std::istream
#endif // if JSONCPP_USING_SECURE_MEMORY
} // end namespace Json

#endif // JSON_CONFIG_H_INCLUDED

// //////////////////////////////////////////////////////////////////////
// End of content of file: include/json/config.h
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: include/json/forwards.h
// //////////////////////////////////////////////////////////////////////

// Copyright 2007-2010 Baptiste Lepilleur and The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#ifndef JSON_FORWARDS_H_INCLUDED
#define JSON_FORWARDS_H_INCLUDED

#if !defined(JSON_IS_AMALGAMATION)
#include "config.h"
#endif // if !defined(JSON_IS_AMALGAMATION)

namespace Json {

// writer.h
class FastWriter;
class StyledWriter;

// reader.h
class Reader;

// features.h
class Features;

// value.h
typedef unsigned int ArrayIndex;
class StaticString;
class Path;
class PathArgument;
class Value;
class ValueIteratorBase;
class ValueIterator;
class ValueConstIterator;

} // namespace Json

#endif // JSON_FORWARDS_H_INCLUDED

// //////////////////////////////////////////////////////////////////////
// End of content of file: include/json/forwards.h
// //////////////////////////////////////////////////////////////////////





#endif //ifndef JSON_FORWARD_AMALGAMATED_H_INCLUDED
