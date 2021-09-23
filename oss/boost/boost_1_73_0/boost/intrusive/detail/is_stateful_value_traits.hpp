/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2009-2013.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_DETAIL_IS_STATEFUL_VALUE_TRAITS_HPP
#define BOOST_INTRUSIVE_DETAIL_IS_STATEFUL_VALUE_TRAITS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#if defined(_MSC_VER) && (_MSC_VER <= 1310)

#include <boost/intrusive/detail/mpl.hpp>

namespace boost {
namespace intrusive {
namespace detail {

template<class ValueTraits>
struct is_stateful_value_traits
{
   static const bool value = !detail::is_empty<ValueTraits>::value;
};

}}}

#else

#include <boost/intrusive/detail/function_detector.hpp>

BOOST_INTRUSIVE_CREATE_FUNCTION_DETECTOR(to_node_ptr, boost_intrusive)
BOOST_INTRUSIVE_CREATE_FUNCTION_DETECTOR(to_value_ptr, boost_intrusive)

namespace boost {
namespace intrusive {
namespace detail {

template<class ValueTraits>
struct is_stateful_value_traits
{
   typedef typename ValueTraits::node_ptr       node_ptr;
   typedef typename ValueTraits::pointer        pointer;
   typedef typename ValueTraits::value_type     value_type;
   typedef typename ValueTraits::const_node_ptr const_node_ptr;
   typedef typename ValueTraits::const_pointer  const_pointer;

   typedef ValueTraits value_traits;

   static const bool value =
      (boost::intrusive::function_detector::NonStaticFunction ==
         (BOOST_INTRUSIVE_DETECT_FUNCTION(ValueTraits, boost_intrusive, node_ptr, to_node_ptr, (value_type&) )))
      ||
      (boost::intrusive::function_detector::NonStaticFunction ==
         (BOOST_INTRUSIVE_DETECT_FUNCTION(ValueTraits, boost_intrusive, pointer, to_value_ptr, (node_ptr) )))
      ||
      (boost::intrusive::function_detector::NonStaticFunction ==
         (BOOST_INTRUSIVE_DETECT_FUNCTION(ValueTraits, boost_intrusive, const_node_ptr, to_node_ptr, (const value_type&) )))
      ||
      (boost::intrusive::function_detector::NonStaticFunction ==
         (BOOST_INTRUSIVE_DETECT_FUNCTION(ValueTraits, boost_intrusive, const_pointer, to_value_ptr, (const_node_ptr) )))
      ;
};

}}}

#endif

#endif   //@ifndef BOOST_INTRUSIVE_DETAIL_IS_STATEFUL_VALUE_TRAITS_HPP
