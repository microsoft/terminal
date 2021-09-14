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

#ifndef BOOST_INTRUSIVE_DETAIL_KEY_NODEPTR_COMP_HPP
#define BOOST_INTRUSIVE_DETAIL_KEY_NODEPTR_COMP_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/intrusive/detail/mpl.hpp>
#include <boost/intrusive/detail/ebo_functor_holder.hpp>
#include <boost/intrusive/detail/tree_value_compare.hpp>


namespace boost {
namespace intrusive {
namespace detail {

template < class KeyTypeKeyCompare
         , class ValueTraits
         , class KeyOfValue
         >
struct key_nodeptr_comp_types
{
   typedef ValueTraits                                   value_traits;
   typedef typename value_traits::value_type             value_type;
   typedef typename value_traits::node_ptr               node_ptr;
   typedef typename value_traits::const_node_ptr         const_node_ptr;
   typedef typename detail::if_c
            < detail::is_same<KeyOfValue, void>::value
            , detail::identity<value_type>
            , KeyOfValue
            >::type                                      key_of_value;
   typedef tree_value_compare
      <typename ValueTraits::pointer, KeyTypeKeyCompare, key_of_value>      base_t;
};

//This function object transforms a key comparison type to
//a function that can compare nodes or nodes with nodes or keys.
template < class KeyTypeKeyCompare
         , class ValueTraits
         , class KeyOfValue = void
         >
struct key_nodeptr_comp
   //Use public inheritance to avoid MSVC bugs with closures
   :  public key_nodeptr_comp_types<KeyTypeKeyCompare, ValueTraits, KeyOfValue>::base_t
{
private:
   struct sfinae_type;

public:
   typedef key_nodeptr_comp_types<KeyTypeKeyCompare, ValueTraits, KeyOfValue> types_t;
   typedef typename types_t::value_traits          value_traits;
   typedef typename types_t::value_type            value_type;
   typedef typename types_t::node_ptr              node_ptr;
   typedef typename types_t::const_node_ptr        const_node_ptr;
   typedef typename types_t::base_t                base_t;
   typedef typename types_t::key_of_value          key_of_value;

   template <class P1>
   struct is_same_or_nodeptr_convertible
   {
      static const bool same_type = is_same<P1,const_node_ptr>::value || is_same<P1,node_ptr>::value;
      static const bool value = same_type || is_convertible<P1, const_node_ptr>::value;
   };

   BOOST_INTRUSIVE_FORCEINLINE base_t base() const
   {  return static_cast<const base_t&>(*this); }

   BOOST_INTRUSIVE_FORCEINLINE key_nodeptr_comp(KeyTypeKeyCompare kcomp, const ValueTraits *traits)
      :  base_t(kcomp), traits_(traits)
   {}

   //pred(pnode)
   template<class T1>
   BOOST_INTRUSIVE_FORCEINLINE bool operator()(const T1 &t1, typename enable_if_c< is_same_or_nodeptr_convertible<T1>::value, sfinae_type* >::type = 0) const
   {  return base().get()(key_of_value()(*traits_->to_value_ptr(t1)));  }

   //operator() 2 arg
   //pred(pnode, pnode)
   template<class T1, class T2>
   BOOST_INTRUSIVE_FORCEINLINE bool operator()
      (const T1 &t1, const T2 &t2, typename enable_if_c< is_same_or_nodeptr_convertible<T1>::value && is_same_or_nodeptr_convertible<T2>::value, sfinae_type* >::type = 0) const
   {  return base()(*traits_->to_value_ptr(t1), *traits_->to_value_ptr(t2));  }

   //pred(pnode, key)
   template<class T1, class T2>
   BOOST_INTRUSIVE_FORCEINLINE bool operator()
      (const T1 &t1, const T2 &t2, typename enable_if_c< is_same_or_nodeptr_convertible<T1>::value && !is_same_or_nodeptr_convertible<T2>::value, sfinae_type* >::type = 0) const
   {  return base()(*traits_->to_value_ptr(t1), t2);  }

   //pred(key, pnode)
   template<class T1, class T2>
   BOOST_INTRUSIVE_FORCEINLINE bool operator()
      (const T1 &t1, const T2 &t2, typename enable_if_c< !is_same_or_nodeptr_convertible<T1>::value && is_same_or_nodeptr_convertible<T2>::value, sfinae_type* >::type = 0) const
   {  return base()(t1, *traits_->to_value_ptr(t2));  }

   //pred(key, key)
   template<class T1, class T2>
   BOOST_INTRUSIVE_FORCEINLINE bool operator()
      (const T1 &t1, const T2 &t2, typename enable_if_c< !is_same_or_nodeptr_convertible<T1>::value && !is_same_or_nodeptr_convertible<T2>::value, sfinae_type* >::type = 0) const
   {  return base()(t1, t2);  }

   const ValueTraits *const traits_;
};

}  //namespace detail{
}  //namespace intrusive{
}  //namespace boost{

#endif //BOOST_INTRUSIVE_DETAIL_KEY_NODEPTR_COMP_HPP
