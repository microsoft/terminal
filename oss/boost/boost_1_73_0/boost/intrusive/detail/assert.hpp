/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2006-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_DETAIL_ASSERT_HPP
#define BOOST_INTRUSIVE_DETAIL_ASSERT_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#pragma once
#endif

#if !defined(BOOST_INTRUSIVE_INVARIANT_ASSERT)
   #include <boost/assert.hpp>
   #define BOOST_INTRUSIVE_INVARIANT_ASSERT BOOST_ASSERT
#elif defined(BOOST_INTRUSIVE_INVARIANT_ASSERT_INCLUDE)
   #include BOOST_INTRUSIVE_INVARIANT_ASSERT_INCLUDE
#endif

#if !defined(BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT)
   #include <boost/assert.hpp>
   #define BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT BOOST_ASSERT
#elif defined(BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT_INCLUDE)
   #include BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT_INCLUDE
#endif

#if !defined(BOOST_INTRUSIVE_SAFE_HOOK_DESTRUCTOR_ASSERT)
   #include <boost/assert.hpp>
   #define BOOST_INTRUSIVE_SAFE_HOOK_DESTRUCTOR_ASSERT BOOST_ASSERT
#elif defined(BOOST_INTRUSIVE_SAFE_HOOK_DESTRUCTOR_ASSERT_INCLUDE)
   #include BOOST_INTRUSIVE_SAFE_HOOK_DESTRUCTOR_ASSERT_INCLUDE
#endif

#endif //BOOST_INTRUSIVE_DETAIL_ASSERT_HPP
