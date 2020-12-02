//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2012-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONFIG_HPP
#include <boost/config.hpp>
#endif

#ifdef BOOST_MSVC
#  pragma warning (push)
#  pragma warning (disable : 4324) // structure was padded due to __declspec(align())
#  pragma warning (disable : 4675) // "function":  resolved overload was found by argument-dependent lookup
#  pragma warning (disable : 4996) // "function": was declared deprecated (_CRT_SECURE_NO_DEPRECATE/_SCL_SECURE_NO_WARNINGS)
#  pragma warning (disable : 4714) // "function": marked as __forceinline not inlined
#  pragma warning (disable : 4127) // conditional expression is constant
#endif
