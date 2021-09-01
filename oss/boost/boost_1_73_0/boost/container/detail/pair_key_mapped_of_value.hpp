//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_PAIR_KEY_MAPPED_OF_VALUE_HPP
#define BOOST_CONTAINER_PAIR_KEY_MAPPED_OF_VALUE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

namespace boost {
namespace container {

template<class Key, class Mapped>
struct pair_key_mapped_of_value
{
   typedef Key    key_type;
   typedef Mapped mapped_type;

   template<class Pair>
   const key_type & key_of_value(const Pair &p) const
   {  return p.first;  }

   template<class Pair>
   const mapped_type & mapped_of_value(const Pair &p) const
   {  return p.second;  }

   template<class Pair>
   key_type & key_of_value(Pair &p) const
   {  return const_cast<key_type&>(p.first);  }

   template<class Pair>
   mapped_type & mapped_of_value(Pair &p) const
   {  return p.second;  }

};

}}

#include <boost/container/detail/config_end.hpp>

#endif   // BOOST_CONTAINER_PAIR_KEY_MAPPED_OF_VALUE_HPP
