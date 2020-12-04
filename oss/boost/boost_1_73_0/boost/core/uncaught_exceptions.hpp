/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          https://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   uncaught_exceptions.hpp
 * \author Andrey Semashev
 * \date   2018-11-10
 *
 * \brief  This header provides an `uncaught_exceptions` function implementation, which was introduced in C++17.
 *
 * The code in this file is based on the implementation by Evgeny Panasyuk:
 *
 * https://github.com/panaseleus/stack_unwinding/blob/master/boost/exception/uncaught_exception_count.hpp
 */

#ifndef BOOST_CORE_UNCAUGHT_EXCEPTIONS_HPP_INCLUDED_
#define BOOST_CORE_UNCAUGHT_EXCEPTIONS_HPP_INCLUDED_

#include <exception>
#include <boost/config.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#pragma once
#endif

// Visual Studio 14 supports N4152 std::uncaught_exceptions()
#if (defined(__cpp_lib_uncaught_exceptions) && __cpp_lib_uncaught_exceptions >= 201411) || \
    (defined(_MSC_VER) && _MSC_VER >= 1900)
#define BOOST_CORE_HAS_UNCAUGHT_EXCEPTIONS
#endif

#if !defined(BOOST_CORE_HAS_UNCAUGHT_EXCEPTIONS)

// cxxabi.h availability macro
#if defined(__has_include) && (!defined(BOOST_GCC) || (__GNUC__ >= 5))
#   if __has_include(<cxxabi.h>)
#       define BOOST_CORE_HAS_CXXABI_H
#   endif
#elif defined(__GLIBCXX__) || defined(__GLIBCPP__)
#   define BOOST_CORE_HAS_CXXABI_H
#endif

#if defined(BOOST_CORE_HAS_CXXABI_H)
// MinGW GCC 4.4 seem to not work the same way the newer GCC versions do. As a result, __cxa_get_globals based implementation will always return 0.
// Just disable it for now and fall back to std::uncaught_exception().
#if !(defined(__MINGW32__) && (defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 405))
#include <cxxabi.h>
#include <cstring>
#define BOOST_CORE_HAS_CXA_GET_GLOBALS
// At least on MinGW and Linux, only GCC since 4.7 declares __cxa_get_globals() in cxxabi.h. Older versions of GCC do not expose this function but it's there.
// On OpenBSD, it seems, the declaration is also missing.
// Note that at least on FreeBSD 11, cxxabi.h declares __cxa_get_globals with a different exception specification, so we can't declare the function unconditionally.
// On Linux with clang and libc++ and on OS X, there is a version of cxxabi.h from libc++abi that doesn't declare __cxa_get_globals, but provides __cxa_uncaught_exceptions.
// The function only appeared in version _LIBCPPABI_VERSION >= 1002 of the library. Unfortunately, there are linking errors about undefined reference to __cxa_uncaught_exceptions
// on Ubuntu Trusty and OS X, so we avoid using it and forward-declare __cxa_get_globals instead.
// On QNX SDP 7.0 (QCC 5.4.0), there are multiple cxxabi.h, one from glibcxx from gcc and another from libc++abi from LLVM. Which one is included will be determined by the qcc
// command line arguments (-V and/or -Y; http://www.qnx.com/developers/docs/7.0.0/#com.qnx.doc.neutrino.utilities/topic/q/qcc.html). The LLVM libc++abi is missing the declaration
// of __cxa_get_globals but it is also patched by QNX developers to not define _LIBCPPABI_VERSION. Older QNX SDP versions, up to and including 6.6, don't provide LLVM and libc++abi.
// See https://github.com/boostorg/core/issues/59.
#if !defined(__FreeBSD__) && \
    ( \
        (defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 407) || \
        defined(__OpenBSD__) || \
        (defined(__QNXNTO__) && !defined(__GLIBCXX__) && !defined(__GLIBCPP__)) || \
        defined(_LIBCPPABI_VERSION) \
    )
namespace __cxxabiv1 {
struct __cxa_eh_globals;
#if defined(__OpenBSD__)
extern "C" __cxa_eh_globals* __cxa_get_globals();
#else
extern "C" __cxa_eh_globals* __cxa_get_globals() BOOST_NOEXCEPT_OR_NOTHROW __attribute__((__const__));
#endif
} // namespace __cxxabiv1
#endif
#endif // !(defined(__MINGW32__) && (defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 405))
#endif // defined(BOOST_CORE_HAS_CXXABI_H)

#if defined(_MSC_VER) && _MSC_VER >= 1400
#include <cstring>
#define BOOST_CORE_HAS_GETPTD
namespace boost {
namespace core {
namespace detail {
extern "C" void* _getptd();
} // namespace detail
} // namespace core
} // namespace boost
#endif // defined(_MSC_VER) && _MSC_VER >= 1400

#endif // !defined(BOOST_CORE_HAS_UNCAUGHT_EXCEPTIONS)

#if !defined(BOOST_CORE_HAS_UNCAUGHT_EXCEPTIONS) && !defined(BOOST_CORE_HAS_CXA_GET_GLOBALS) && !defined(BOOST_CORE_HAS_GETPTD)
//! This macro is defined when `uncaught_exceptions` is not guaranteed to return values greater than 1 if multiple exceptions are pending
#define BOOST_CORE_UNCAUGHT_EXCEPTIONS_EMULATED
#endif

namespace boost {

namespace core {

//! Returns the number of currently pending exceptions
inline unsigned int uncaught_exceptions() BOOST_NOEXCEPT
{
#if defined(BOOST_CORE_HAS_UNCAUGHT_EXCEPTIONS)
    // C++17 implementation
    return static_cast< unsigned int >(std::uncaught_exceptions());
#elif defined(BOOST_CORE_HAS_CXA_GET_GLOBALS)
    // Tested on {clang 3.2,GCC 3.5.6,GCC 4.1.2,GCC 4.4.6,GCC 4.4.7}x{x32,x64}
    unsigned int count;
    std::memcpy(&count, reinterpret_cast< const unsigned char* >(::abi::__cxa_get_globals()) + sizeof(void*), sizeof(count)); // __cxa_eh_globals::uncaughtExceptions, x32 offset - 0x4, x64 - 0x8
    return count;
#elif defined(BOOST_CORE_HAS_GETPTD)
    // MSVC specific. Tested on {MSVC2005SP1,MSVC2008SP1,MSVC2010SP1,MSVC2012}x{x32,x64}.
    unsigned int count;
    std::memcpy(&count, static_cast< const unsigned char* >(boost::core::detail::_getptd()) + (sizeof(void*) == 8u ? 0x100 : 0x90), sizeof(count)); // _tiddata::_ProcessingThrow, x32 offset - 0x90, x64 - 0x100
    return count;
#else
    // Portable C++03 implementation. Does not allow to detect multiple nested exceptions.
    return static_cast< unsigned int >(std::uncaught_exception());
#endif
}

} // namespace core

} // namespace boost

#undef BOOST_CORE_HAS_CXXABI_H
#undef BOOST_CORE_HAS_CXA_GET_GLOBALS
#undef BOOST_CORE_HAS_UNCAUGHT_EXCEPTIONS
#undef BOOST_CORE_HAS_GETPTD

#endif // BOOST_CORE_UNCAUGHT_EXCEPTIONS_HPP_INCLUDED_
