
// Copyright 2005-2011 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Note: if you change this include guard, you also need to change
// container_fwd_compile_fail.cpp
#if !defined(BOOST_DETAIL_CONTAINER_FWD_HPP)
#define BOOST_DETAIL_CONTAINER_FWD_HPP

#if defined(_MSC_VER) && \
    !defined(BOOST_DETAIL_TEST_CONFIG_ONLY)
# pragma once
#endif

#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Define BOOST_DETAIL_NO_CONTAINER_FWD if you don't want this header to      //
// forward declare standard containers.                                       //
//                                                                            //
// BOOST_DETAIL_CONTAINER_FWD to make it foward declare containers even if it //
// normally doesn't.                                                          //
//                                                                            //
// BOOST_DETAIL_NO_CONTAINER_FWD overrides BOOST_DETAIL_CONTAINER_FWD.        //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#if !defined(BOOST_DETAIL_NO_CONTAINER_FWD)
#  if defined(BOOST_DETAIL_CONTAINER_FWD)
     // Force forward declarations.
#  elif defined(__SGI_STL_PORT) || defined(_STLPORT_VERSION)
     // STLport
#    define BOOST_DETAIL_NO_CONTAINER_FWD
#  elif defined(__LIBCOMO__)
     // Comeau STL:
#    define BOOST_DETAIL_NO_CONTAINER_FWD
#  elif defined(__STD_RWCOMPILER_H__) || defined(_RWSTD_VER)
     // Rogue Wave library:
#    define BOOST_DETAIL_NO_CONTAINER_FWD
#  elif defined(_LIBCPP_VERSION)
     // libc++
#    define BOOST_DETAIL_NO_CONTAINER_FWD
#  elif defined(__GLIBCPP__) || defined(__GLIBCXX__)
     // GNU libstdc++ 3
     //
     // Disable forwarding for all recent versions, as the library has a
     // versioned namespace mode, and I don't know how to detect it.
#    if __GLIBCXX__ >= 20070513 \
        || defined(_GLIBCXX_DEBUG) \
        || defined(_GLIBCXX_PARALLEL) \
        || defined(_GLIBCXX_PROFILE)
#      define BOOST_DETAIL_NO_CONTAINER_FWD
#    else
#      if defined(__GLIBCXX__) && __GLIBCXX__ >= 20040530
#        define BOOST_CONTAINER_FWD_COMPLEX_STRUCT
#      endif
#    endif
#  elif defined(__STL_CONFIG_H)
     // generic SGI STL
     //
     // Forward declaration seems to be okay, but it has a couple of odd
     // implementations.
#    define BOOST_CONTAINER_FWD_BAD_BITSET
#    if !defined(__STL_NON_TYPE_TMPL_PARAM_BUG)
#      define BOOST_CONTAINER_FWD_BAD_DEQUE
#     endif
#  elif defined(__MSL_CPP__)
     // MSL standard lib:
#    define BOOST_DETAIL_NO_CONTAINER_FWD
#  elif defined(__IBMCPP__)
     // The default VACPP std lib, forward declaration seems to be fine.
#  elif defined(MSIPL_COMPILE_H)
     // Modena C++ standard library
#    define BOOST_DETAIL_NO_CONTAINER_FWD
#  elif (defined(_YVALS) && !defined(__IBMCPP__)) || defined(_CPPLIB_VER)
     // Dinkumware Library (this has to appear after any possible replacement
     // libraries)
#  else
#    define BOOST_DETAIL_NO_CONTAINER_FWD
#  endif
#endif

#if !defined(BOOST_DETAIL_TEST_CONFIG_ONLY)

#if defined(BOOST_DETAIL_NO_CONTAINER_FWD) && \
    !defined(BOOST_DETAIL_TEST_FORCE_CONTAINER_FWD)

#include <deque>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <bitset>
#include <string>
#include <complex>

#else

#include <cstddef>

#if defined(BOOST_CONTAINER_FWD_BAD_DEQUE)
#include <deque>
#endif

#if defined(BOOST_CONTAINER_FWD_BAD_BITSET)
#include <bitset>
#endif

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4099) // struct/class mismatch in fwd declarations
#endif

namespace std
{
    template <class T> class allocator;
    template <class charT, class traits, class Allocator> class basic_string;

    template <class charT> struct char_traits;

#if defined(BOOST_CONTAINER_FWD_COMPLEX_STRUCT)
    template <class T> struct complex;
#else
    template <class T> class complex;
#endif

#if !defined(BOOST_CONTAINER_FWD_BAD_DEQUE)
    template <class T, class Allocator> class deque;
#endif

    template <class T, class Allocator> class list;
    template <class T, class Allocator> class vector;
    template <class Key, class T, class Compare, class Allocator> class map;
    template <class Key, class T, class Compare, class Allocator>
    class multimap;
    template <class Key, class Compare, class Allocator> class set;
    template <class Key, class Compare, class Allocator> class multiset;

#if !defined(BOOST_CONTAINER_FWD_BAD_BITSET)
    template <size_t N> class bitset;
#endif
    template <class T1, class T2> struct pair;
}

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

#endif // BOOST_DETAIL_NO_CONTAINER_FWD &&
       // !defined(BOOST_DETAIL_TEST_FORCE_CONTAINER_FWD)

#endif // BOOST_DETAIL_TEST_CONFIG_ONLY

#endif
