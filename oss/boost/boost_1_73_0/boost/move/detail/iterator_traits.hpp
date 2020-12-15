//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

//! \file

#ifndef BOOST_MOVE_DETAIL_ITERATOR_TRAITS_HPP
#define BOOST_MOVE_DETAIL_ITERATOR_TRAITS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <cstddef>
#include <boost/move/detail/type_traits.hpp>

#include <boost/move/detail/std_ns_begin.hpp>
BOOST_MOVE_STD_NS_BEG

struct input_iterator_tag;
struct forward_iterator_tag;
struct bidirectional_iterator_tag;
struct random_access_iterator_tag;
struct output_iterator_tag;

BOOST_MOVE_STD_NS_END
#include <boost/move/detail/std_ns_end.hpp>

namespace boost{  namespace movelib{

template<class Iterator>
struct iterator_traits
{
   typedef typename Iterator::difference_type   difference_type;
   typedef typename Iterator::value_type        value_type;
   typedef typename Iterator::pointer           pointer;
   typedef typename Iterator::reference         reference;
   typedef typename Iterator::iterator_category iterator_category;
   typedef typename boost::move_detail::make_unsigned<difference_type>::type size_type;
};

template<class T>
struct iterator_traits<T*>
{
   typedef std::ptrdiff_t                    difference_type;
   typedef T                                 value_type;
   typedef T*                                pointer;
   typedef T&                                reference;
   typedef std::random_access_iterator_tag   iterator_category;
   typedef typename boost::move_detail::make_unsigned<difference_type>::type size_type;
};

template<class T>
struct iterator_traits<const T*>
{
   typedef std::ptrdiff_t                    difference_type;
   typedef T                                 value_type;
   typedef const T*                          pointer;
   typedef const T&                          reference;
   typedef std::random_access_iterator_tag   iterator_category;
   typedef typename boost::move_detail::make_unsigned<difference_type>::type size_type;
};

}} //namespace boost {  namespace movelib{

#endif //#ifndef BOOST_MOVE_DETAIL_ITERATOR_TRAITS_HPP
