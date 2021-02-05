/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2014-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_DETAIL_NODE_CLONER_DISPOSER_HPP
#define BOOST_INTRUSIVE_DETAIL_NODE_CLONER_DISPOSER_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/intrusive/detail/ebo_functor_holder.hpp>
#include <boost/intrusive/detail/algo_type.hpp>
#include <boost/intrusive/detail/assert.hpp>

namespace boost {
namespace intrusive {
namespace detail {

template<class F, class ValueTraits, algo_types AlgoType, bool IsConst = true>
struct node_cloner
   //Use public inheritance to avoid MSVC bugs with closures
   :  public ebo_functor_holder<F>
{
   typedef ValueTraits                                      value_traits;
   typedef typename value_traits::node_traits               node_traits;
   typedef typename node_traits::node_ptr                   node_ptr;
   typedef ebo_functor_holder<F>                            base_t;
   typedef typename get_algo< AlgoType
                            , node_traits>::type            node_algorithms;
   static const bool safemode_or_autounlink =
      is_safe_autounlink<value_traits::link_mode>::value;
   typedef typename value_traits::value_type                value_type;
   typedef typename value_traits::pointer                   pointer;
   typedef typename value_traits::const_pointer             const_pointer;
   typedef typename node_traits::node                       node;
   typedef typename value_traits::const_node_ptr            const_node_ptr;
   typedef typename pointer_traits<pointer>::reference      reference;
   typedef typename pointer_traits
      <const_pointer>::reference                            const_reference;
   typedef typename if_c<IsConst, const_reference, reference>::type reference_type;

   node_cloner(F f, const ValueTraits *traits)
      :  base_t(f), traits_(traits)
   {}

   // tree-based containers use this method, which is proxy-reference friendly
   BOOST_INTRUSIVE_FORCEINLINE node_ptr operator()(const node_ptr & p)
   {
      reference_type v = *traits_->to_value_ptr(p);
      node_ptr n = traits_->to_node_ptr(*base_t::get()(v));
      //Cloned node must be in default mode if the linking mode requires it
      BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT(!safemode_or_autounlink || node_algorithms::unique(n));
      return n;
   }

   const ValueTraits * const traits_;
};

template<class F, class ValueTraits, algo_types AlgoType>
struct node_disposer
   //Use public inheritance to avoid MSVC bugs with closures
   :  public ebo_functor_holder<F>
{
   typedef ValueTraits                          value_traits;
   typedef typename value_traits::node_traits   node_traits;
   typedef typename node_traits::node_ptr       node_ptr;
   typedef ebo_functor_holder<F>                base_t;
   typedef typename get_algo< AlgoType
                            , node_traits>::type   node_algorithms;
   static const bool safemode_or_autounlink =
      is_safe_autounlink<value_traits::link_mode>::value;

   node_disposer(F f, const ValueTraits *cont)
      :  base_t(f), traits_(cont)
   {}

   BOOST_INTRUSIVE_FORCEINLINE void operator()(const node_ptr & p)
   {
      if(safemode_or_autounlink)
         node_algorithms::init(p);
      base_t::get()(traits_->to_value_ptr(p));
   }
   const ValueTraits * const traits_;
};

}  //namespace detail{
}  //namespace intrusive{
}  //namespace boost{

#endif //BOOST_INTRUSIVE_DETAIL_NODE_CLONER_DISPOSER_HPP
