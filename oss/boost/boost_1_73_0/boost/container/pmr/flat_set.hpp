//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_PMR_SET_HPP
#define BOOST_CONTAINER_PMR_SET_HPP

#if defined (_MSC_VER)
#  pragma once 
#endif

#include <boost/container/flat_set.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>

namespace boost {
namespace container {
namespace pmr {

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

template <class Key
         ,class Compare = std::less<Key> >
using flat_set = boost::container::flat_set<Key, Compare, polymorphic_allocator<Key> >;

template <class Key
         ,class Compare = std::less<Key> >
using flat_multiset = boost::container::flat_multiset<Key, Compare, polymorphic_allocator<Key> >;

#endif

//! A portable metafunction to obtain a flat_set
//! that uses a polymorphic allocator
template <class Key
         ,class Compare = std::less<Key> >
struct flat_set_of
{
   typedef boost::container::flat_set<Key, Compare, polymorphic_allocator<Key> > type;
};

//! A portable metafunction to obtain a flat_multiset
//! that uses a polymorphic allocator
template <class Key
         ,class Compare = std::less<Key> >
struct flat_multiset_of
{
   typedef boost::container::flat_multiset<Key, Compare, polymorphic_allocator<Key> > type;
};

}  //namespace pmr {
}  //namespace container {
}  //namespace boost {

#endif   //BOOST_CONTAINER_PMR_SET_HPP
