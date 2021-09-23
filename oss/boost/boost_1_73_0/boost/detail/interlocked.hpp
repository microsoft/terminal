#ifndef BOOST_DETAIL_INTERLOCKED_HPP_INCLUDED
#define BOOST_DETAIL_INTERLOCKED_HPP_INCLUDED

//
//  boost/detail/interlocked.hpp
//
//  Copyright 2005 Peter Dimov
//  Copyright 2018, 2019 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

// BOOST_INTERLOCKED_HAS_INTRIN_H

// VC9 has intrin.h, but it collides with <utility>
#if defined( BOOST_MSVC ) && BOOST_MSVC >= 1600

# define BOOST_INTERLOCKED_HAS_INTRIN_H

// Unlike __MINGW64__, __MINGW64_VERSION_MAJOR is defined by MinGW-w64 for both 32 and 64-bit targets.
#elif defined( __MINGW64_VERSION_MAJOR )

// MinGW-w64 provides intrin.h for both 32 and 64-bit targets.
# define BOOST_INTERLOCKED_HAS_INTRIN_H

#elif defined( __CYGWIN__ )

// Cygwin and Cygwin64 provide intrin.h. On Cygwin64 we have to use intrin.h because it's an LP64 target,
// where long is 64-bit and therefore _Interlocked* functions have different signatures.
# define BOOST_INTERLOCKED_HAS_INTRIN_H

// Intel C++ on Windows on VC10+ stdlib
#elif defined( BOOST_INTEL_WIN ) && defined( _CPPLIB_VER ) && _CPPLIB_VER >= 520

# define BOOST_INTERLOCKED_HAS_INTRIN_H

// clang-cl on Windows on VC10+ stdlib
#elif defined( __clang__ ) && defined( _MSC_VER ) && defined( _CPPLIB_VER ) && _CPPLIB_VER >= 520

# define BOOST_INTERLOCKED_HAS_INTRIN_H

#endif

#if !defined(__LP64__)
#define BOOST_INTERLOCKED_LONG32 long
#else
#define BOOST_INTERLOCKED_LONG32 int
#endif

#if defined( BOOST_USE_WINDOWS_H )

# include <windows.h>

# define BOOST_INTERLOCKED_INCREMENT(dest) \
    InterlockedIncrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_DECREMENT(dest) \
    InterlockedDecrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_COMPARE_EXCHANGE(dest, exchange, compare) \
    InterlockedCompareExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange), (BOOST_INTERLOCKED_LONG32)(compare))
# define BOOST_INTERLOCKED_EXCHANGE(dest, exchange) \
    InterlockedExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange))
# define BOOST_INTERLOCKED_EXCHANGE_ADD(dest, add) \
    InterlockedExchangeAdd((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(add))
# define BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(dest, exchange, compare) \
    InterlockedCompareExchangePointer((void**)(dest), (void*)(exchange), (void*)(compare))
# define BOOST_INTERLOCKED_EXCHANGE_POINTER(dest, exchange) \
    InterlockedExchangePointer((void**)(dest), (void*)(exchange))

#elif defined( BOOST_USE_INTRIN_H ) || defined( BOOST_INTERLOCKED_HAS_INTRIN_H )

#include <intrin.h>

# define BOOST_INTERLOCKED_INCREMENT(dest) \
    _InterlockedIncrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_DECREMENT(dest) \
    _InterlockedDecrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_COMPARE_EXCHANGE(dest, exchange, compare) \
    _InterlockedCompareExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange), (BOOST_INTERLOCKED_LONG32)(compare))
# define BOOST_INTERLOCKED_EXCHANGE(dest, exchange) \
    _InterlockedExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange))
# define BOOST_INTERLOCKED_EXCHANGE_ADD(dest, add) \
    _InterlockedExchangeAdd((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(add))

// Note: Though MSVC-12 defines _InterlockedCompareExchangePointer and _InterlockedExchangePointer in intrin.h, the latter
//       is actually broken as it conflicts with winnt.h from Windows SDK 8.1.
# if (defined(_MSC_VER) && _MSC_VER >= 1900) || \
  (defined(_M_IA64) || defined(_M_AMD64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_ARM64))

#  define BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(dest, exchange, compare) \
     _InterlockedCompareExchangePointer((void**)(dest), (void*)(exchange), (void*)(compare))
#  define BOOST_INTERLOCKED_EXCHANGE_POINTER(dest, exchange) \
     _InterlockedExchangePointer((void**)(dest), (void*)(exchange))

# else

#  define BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(dest, exchange, compare) \
    ((void*)BOOST_INTERLOCKED_COMPARE_EXCHANGE((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange), (BOOST_INTERLOCKED_LONG32)(compare)))
#  define BOOST_INTERLOCKED_EXCHANGE_POINTER(dest, exchange) \
    ((void*)BOOST_INTERLOCKED_EXCHANGE((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange)))

# endif

#elif defined(_WIN32_WCE)

#if _WIN32_WCE >= 0x600

extern "C" BOOST_INTERLOCKED_LONG32 __cdecl _InterlockedIncrement( BOOST_INTERLOCKED_LONG32 volatile * );
extern "C" BOOST_INTERLOCKED_LONG32 __cdecl _InterlockedDecrement( BOOST_INTERLOCKED_LONG32 volatile * );
extern "C" BOOST_INTERLOCKED_LONG32 __cdecl _InterlockedCompareExchange( BOOST_INTERLOCKED_LONG32 volatile *, BOOST_INTERLOCKED_LONG32, BOOST_INTERLOCKED_LONG32 );
extern "C" BOOST_INTERLOCKED_LONG32 __cdecl _InterlockedExchange( BOOST_INTERLOCKED_LONG32 volatile *, BOOST_INTERLOCKED_LONG32 );
extern "C" BOOST_INTERLOCKED_LONG32 __cdecl _InterlockedExchangeAdd( BOOST_INTERLOCKED_LONG32 volatile *, BOOST_INTERLOCKED_LONG32 );

# define BOOST_INTERLOCKED_INCREMENT(dest) \
    _InterlockedIncrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_DECREMENT(dest) \
    _InterlockedDecrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_COMPARE_EXCHANGE(dest, exchange, compare) \
    _InterlockedCompareExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange), (BOOST_INTERLOCKED_LONG32)(compare))
# define BOOST_INTERLOCKED_EXCHANGE(dest, exchange) \
    _InterlockedExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange))
# define BOOST_INTERLOCKED_EXCHANGE_ADD(dest, add) \
    _InterlockedExchangeAdd((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(add))

#else // _WIN32_WCE >= 0x600

// under Windows CE we still have old-style Interlocked* functions

extern "C" BOOST_INTERLOCKED_LONG32 __cdecl InterlockedIncrement( BOOST_INTERLOCKED_LONG32 * );
extern "C" BOOST_INTERLOCKED_LONG32 __cdecl InterlockedDecrement( BOOST_INTERLOCKED_LONG32 * );
extern "C" BOOST_INTERLOCKED_LONG32 __cdecl InterlockedCompareExchange( BOOST_INTERLOCKED_LONG32 *, BOOST_INTERLOCKED_LONG32, BOOST_INTERLOCKED_LONG32 );
extern "C" BOOST_INTERLOCKED_LONG32 __cdecl InterlockedExchange( BOOST_INTERLOCKED_LONG32 *, BOOST_INTERLOCKED_LONG32 );
extern "C" BOOST_INTERLOCKED_LONG32 __cdecl InterlockedExchangeAdd( BOOST_INTERLOCKED_LONG32 *, BOOST_INTERLOCKED_LONG32 );

# define BOOST_INTERLOCKED_INCREMENT(dest) \
    InterlockedIncrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_DECREMENT(dest) \
    InterlockedDecrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_COMPARE_EXCHANGE(dest, exchange, compare) \
    InterlockedCompareExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange), (BOOST_INTERLOCKED_LONG32)(compare))
# define BOOST_INTERLOCKED_EXCHANGE(dest, exchange) \
    InterlockedExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange))
# define BOOST_INTERLOCKED_EXCHANGE_ADD(dest, add) \
    InterlockedExchangeAdd((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(add))

#endif // _WIN32_WCE >= 0x600

# define BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(dest, exchange, compare) \
    ((void*)BOOST_INTERLOCKED_COMPARE_EXCHANGE((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange), (BOOST_INTERLOCKED_LONG32)(compare)))
# define BOOST_INTERLOCKED_EXCHANGE_POINTER(dest, exchange) \
    ((void*)BOOST_INTERLOCKED_EXCHANGE((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange)))

#elif defined( BOOST_MSVC ) || defined( BOOST_INTEL_WIN )

# if defined( __CLRCALL_PURE_OR_CDECL )
#  define BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL __CLRCALL_PURE_OR_CDECL
# else
#  define BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL __cdecl
# endif

extern "C" BOOST_INTERLOCKED_LONG32 BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL _InterlockedIncrement( BOOST_INTERLOCKED_LONG32 volatile * );
extern "C" BOOST_INTERLOCKED_LONG32 BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL _InterlockedDecrement( BOOST_INTERLOCKED_LONG32 volatile * );
extern "C" BOOST_INTERLOCKED_LONG32 BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL _InterlockedCompareExchange( BOOST_INTERLOCKED_LONG32 volatile *, BOOST_INTERLOCKED_LONG32, BOOST_INTERLOCKED_LONG32 );
extern "C" BOOST_INTERLOCKED_LONG32 BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL _InterlockedExchange( BOOST_INTERLOCKED_LONG32 volatile *, BOOST_INTERLOCKED_LONG32 );
extern "C" BOOST_INTERLOCKED_LONG32 BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL _InterlockedExchangeAdd( BOOST_INTERLOCKED_LONG32 volatile *, BOOST_INTERLOCKED_LONG32 );

# if defined( BOOST_MSVC ) && BOOST_MSVC >= 1310
#  pragma intrinsic( _InterlockedIncrement )
#  pragma intrinsic( _InterlockedDecrement )
#  pragma intrinsic( _InterlockedCompareExchange )
#  pragma intrinsic( _InterlockedExchange )
#  pragma intrinsic( _InterlockedExchangeAdd )
# endif

# if defined(_M_IA64) || defined(_M_AMD64) || defined(_M_ARM64)

extern "C" void* BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL _InterlockedCompareExchangePointer( void* volatile *, void*, void* );
extern "C" void* BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL _InterlockedExchangePointer( void* volatile *, void* );

#  if defined( BOOST_MSVC ) && BOOST_MSVC >= 1310
#   pragma intrinsic( _InterlockedCompareExchangePointer )
#   pragma intrinsic( _InterlockedExchangePointer )
#  endif

#  define BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(dest, exchange, compare) \
     _InterlockedCompareExchangePointer((void**)(dest), (void*)(exchange), (void*)(compare))
#  define BOOST_INTERLOCKED_EXCHANGE_POINTER(dest, exchange) \
     _InterlockedExchangePointer((void**)(dest), (void*)(exchange))

# else

#  define BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(dest, exchange, compare) \
    ((void*)BOOST_INTERLOCKED_COMPARE_EXCHANGE((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange), (BOOST_INTERLOCKED_LONG32)(compare)))
#  define BOOST_INTERLOCKED_EXCHANGE_POINTER(dest, exchange) \
    ((void*)BOOST_INTERLOCKED_EXCHANGE((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange)))

# endif

# undef BOOST_INTERLOCKED_CLRCALL_PURE_OR_CDECL

# define BOOST_INTERLOCKED_INCREMENT(dest) \
    _InterlockedIncrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_DECREMENT(dest) \
    _InterlockedDecrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_COMPARE_EXCHANGE(dest, exchange, compare) \
    _InterlockedCompareExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange), (BOOST_INTERLOCKED_LONG32)(compare))
# define BOOST_INTERLOCKED_EXCHANGE(dest, exchange) \
    _InterlockedExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange))
# define BOOST_INTERLOCKED_EXCHANGE_ADD(dest, add) \
    _InterlockedExchangeAdd((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(add))

#elif defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ )

#define BOOST_INTERLOCKED_IMPORT __declspec(dllimport)

namespace boost
{

namespace detail
{

extern "C" BOOST_INTERLOCKED_IMPORT BOOST_INTERLOCKED_LONG32 __stdcall InterlockedIncrement( BOOST_INTERLOCKED_LONG32 volatile * );
extern "C" BOOST_INTERLOCKED_IMPORT BOOST_INTERLOCKED_LONG32 __stdcall InterlockedDecrement( BOOST_INTERLOCKED_LONG32 volatile * );
extern "C" BOOST_INTERLOCKED_IMPORT BOOST_INTERLOCKED_LONG32 __stdcall InterlockedCompareExchange( BOOST_INTERLOCKED_LONG32 volatile *, BOOST_INTERLOCKED_LONG32, BOOST_INTERLOCKED_LONG32 );
extern "C" BOOST_INTERLOCKED_IMPORT BOOST_INTERLOCKED_LONG32 __stdcall InterlockedExchange( BOOST_INTERLOCKED_LONG32 volatile *, BOOST_INTERLOCKED_LONG32 );
extern "C" BOOST_INTERLOCKED_IMPORT BOOST_INTERLOCKED_LONG32 __stdcall InterlockedExchangeAdd( BOOST_INTERLOCKED_LONG32 volatile *, BOOST_INTERLOCKED_LONG32 );

# if defined(_M_IA64) || defined(_M_AMD64) || defined(_M_ARM64)
extern "C" BOOST_INTERLOCKED_IMPORT void* __stdcall InterlockedCompareExchangePointer( void* volatile *, void*, void* );
extern "C" BOOST_INTERLOCKED_IMPORT void* __stdcall InterlockedExchangePointer( void* volatile *, void* );
# endif

} // namespace detail

} // namespace boost

# define BOOST_INTERLOCKED_INCREMENT(dest) \
    ::boost::detail::InterlockedIncrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_DECREMENT(dest) \
    ::boost::detail::InterlockedDecrement((BOOST_INTERLOCKED_LONG32*)(dest))
# define BOOST_INTERLOCKED_COMPARE_EXCHANGE(dest, exchange, compare) \
    ::boost::detail::InterlockedCompareExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange), (BOOST_INTERLOCKED_LONG32)(compare))
# define BOOST_INTERLOCKED_EXCHANGE(dest, exchange) \
    ::boost::detail::InterlockedExchange((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(exchange))
# define BOOST_INTERLOCKED_EXCHANGE_ADD(dest, add) \
    ::boost::detail::InterlockedExchangeAdd((BOOST_INTERLOCKED_LONG32*)(dest), (BOOST_INTERLOCKED_LONG32)(add))

# if defined(_M_IA64) || defined(_M_AMD64) || defined(_M_ARM64)
#  define BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(dest, exchange, compare) \
     ::boost::detail::InterlockedCompareExchangePointer((void**)(dest), (void*)(exchange), (void*)(compare))
#  define BOOST_INTERLOCKED_EXCHANGE_POINTER(dest, exchange) \
     ::boost::detail::InterlockedExchangePointer((void**)(dest), (void*)(exchange))
# else
#  define BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(dest, exchange, compare) \
    ((void*)BOOST_INTERLOCKED_COMPARE_EXCHANGE((BOOST_INTERLOCKED_LONG32 volatile*)(dest),(BOOST_INTERLOCKED_LONG32)(exchange),(BOOST_INTERLOCKED_LONG32)(compare)))
#  define BOOST_INTERLOCKED_EXCHANGE_POINTER(dest, exchange) \
    ((void*)BOOST_INTERLOCKED_EXCHANGE((BOOST_INTERLOCKED_LONG32*)(dest),(BOOST_INTERLOCKED_LONG32)(exchange)))
# endif

#else

# error "Interlocked intrinsics not available"

#endif

#endif // #ifndef BOOST_DETAIL_INTERLOCKED_HPP_INCLUDED
