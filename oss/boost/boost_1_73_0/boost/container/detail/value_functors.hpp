#ifndef BOOST_CONTAINER_DETAIL_VALUE_FUNCTORS_HPP
#define BOOST_CONTAINER_DETAIL_VALUE_FUNCTORS_HPP
///////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2017-2017. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

//Functors for member algorithm defaults
template<class ValueType>
struct value_less
{
   bool operator()(const ValueType &a, const ValueType &b) const
      {  return a < b;  }
};

template<class ValueType>
struct value_equal
{
   bool operator()(const ValueType &a, const ValueType &b) const
      {  return a == b;  }
};

#endif   //BOOST_CONTAINER_DETAIL_VALUE_FUNCTORS_HPP
