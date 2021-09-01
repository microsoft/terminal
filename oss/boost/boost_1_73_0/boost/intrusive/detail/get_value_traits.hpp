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

#ifndef BOOST_INTRUSIVE_DETAIL_GET_VALUE_TRAITS_HPP
#define BOOST_INTRUSIVE_DETAIL_GET_VALUE_TRAITS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/intrusive/detail/hook_traits.hpp>

namespace boost {
namespace intrusive {

#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

template<class SupposedValueTraits>
struct is_default_hook_tag
{  static const bool value = false;  };

namespace detail{

template <class T, class BaseHook>
struct concrete_hook_base_value_traits
{
   typedef typename BaseHook::hooktags tags;
   typedef bhtraits
      < T
      , typename tags::node_traits
      , tags::link_mode
      , typename tags::tag
      , tags::type> type;
};

template <class BaseHook>
struct concrete_hook_base_value_traits<void, BaseHook>
{
   typedef typename BaseHook::hooktags type;
};

template <class T, class AnyToSomeHook_ProtoValueTraits>
struct any_hook_base_value_traits
{
   //AnyToSomeHook value_traits derive from a generic_hook
   //The generic_hook is configured with any_node_traits
   //and AnyToSomeHook::value_traits with the correct
   //node traits for the container, so use node_traits
   //from AnyToSomeHook_ProtoValueTraits and the rest of
   //elements from the hooktags member of the generic_hook

   typedef typename AnyToSomeHook_ProtoValueTraits::basic_hook_t     basic_hook_t;
   typedef typename pointer_rebind
      < typename basic_hook_t::hooktags::node_traits::node_ptr
      , void>::type                                                  void_pointer;
   typedef typename AnyToSomeHook_ProtoValueTraits::template
      node_traits_from_voidptr<void_pointer>::type                   node_traits;

   typedef bhtraits
      < T
      , node_traits
      , basic_hook_t::hooktags::link_mode
      , typename basic_hook_t::hooktags::tag
      , basic_hook_t::hooktags::type
      > type;
};

template <class AnyToSomeHook_ProtoValueTraits>
struct any_hook_base_value_traits<void, AnyToSomeHook_ProtoValueTraits>
{
   typedef typename AnyToSomeHook_ProtoValueTraits::basic_hook_t     basic_hook_t;
   typedef typename pointer_rebind
      < typename basic_hook_t::hooktags::node_traits::node_ptr
      , void>::type                                                  void_pointer;

   struct type
   {
      typedef typename AnyToSomeHook_ProtoValueTraits::template
         node_traits_from_voidptr<void_pointer>::type                node_traits;
   };
};

template<class MemberHook>
struct get_member_value_traits
{
   typedef typename MemberHook::member_value_traits type;
};

BOOST_INTRUSIVE_INTERNAL_STATIC_BOOL_IS_TRUE(internal_any_hook, is_any_hook)
BOOST_INTRUSIVE_INTERNAL_STATIC_BOOL_IS_TRUE(internal_base_hook, hooktags::is_base_hook)

template <class T>
struct internal_member_value_traits
{
   template <class U> static yes_type test(...);
   template <class U> static no_type test(typename U::member_value_traits* = 0);
   static const bool value = sizeof(test<T>(0)) == sizeof(no_type);
};

template<class SupposedValueTraits, class T, bool = is_default_hook_tag<SupposedValueTraits>::value>
struct supposed_value_traits;

template<class T, class BaseHook, bool = internal_any_hook_bool_is_true<BaseHook>::value>
struct get_base_value_traits;

template<class SupposedValueTraits, class T, bool = internal_base_hook_bool_is_true<SupposedValueTraits>::value>
struct supposed_base_value_traits;

template<class SupposedValueTraits, bool = internal_member_value_traits<SupposedValueTraits>::value>
struct supposed_member_value_traits;

template<class SupposedValueTraits, bool = internal_any_hook_bool_is_true<SupposedValueTraits>::value>
struct any_or_concrete_value_traits;

//Base any hook
template<class T, class BaseHook>
struct get_base_value_traits<T, BaseHook, true>
   : any_hook_base_value_traits<T, BaseHook>
{};

//Non-any base hook
template<class T, class BaseHook>
struct get_base_value_traits<T, BaseHook, false>
   : concrete_hook_base_value_traits<T, BaseHook>
{};

//...It's a default hook
template<class SupposedValueTraits, class T>
struct supposed_value_traits<SupposedValueTraits, T, true>
{  typedef typename SupposedValueTraits::template apply<T>::type type;  };

//...Not  a default hook
template<class SupposedValueTraits, class T>
struct supposed_value_traits<SupposedValueTraits, T, false>
{  typedef SupposedValueTraits type;   };

//...It's a base hook
template<class BaseHook, class T>
struct supposed_base_value_traits<BaseHook, T, true>
   : get_base_value_traits<T, BaseHook>
{};

//...Not a base hook, try if it's a member or value_traits
template<class SupposedValueTraits, class T>
struct supposed_base_value_traits<SupposedValueTraits, T, false>
   : supposed_member_value_traits<SupposedValueTraits>
{};

//...It's a member hook
template<class MemberHook>
struct supposed_member_value_traits<MemberHook, true>
   : get_member_value_traits<MemberHook>
{};

//...Not a member hook
template<class SupposedValueTraits>
struct supposed_member_value_traits<SupposedValueTraits, false>
   : any_or_concrete_value_traits<SupposedValueTraits>
{};

template<class AnyToSomeHook_ProtoValueTraits>
struct any_or_concrete_value_traits<AnyToSomeHook_ProtoValueTraits, true>
{
   //A hook node (non-base, e.g.: member or other value traits
   typedef typename AnyToSomeHook_ProtoValueTraits::basic_hook_t        basic_hook_t;
   typedef typename pointer_rebind
      <typename basic_hook_t::node_ptr, void>::type                     void_pointer;
   typedef typename AnyToSomeHook_ProtoValueTraits::template
      node_traits_from_voidptr<void_pointer>::type                      any_node_traits;

   struct type : basic_hook_t
   {
      typedef any_node_traits node_traits;
   };
};

template<class SupposedValueTraits>
struct any_or_concrete_value_traits<SupposedValueTraits, false>
{
   typedef SupposedValueTraits type;
};

////////////////////////////////////////
//  get_value_traits / get_node_traits
////////////////////////////////////////

template<class T, class SupposedValueTraits>
struct get_value_traits
   : supposed_base_value_traits<typename supposed_value_traits<SupposedValueTraits, T>::type, T>
{};

template<class SupposedValueTraits>
struct get_node_traits
{
   typedef typename get_value_traits<void, SupposedValueTraits>::type::node_traits type;
};

}  //namespace detail{

#endif   //BOOST_INTRUSIVE_DOXYGEN_INVOKED

}  //namespace intrusive {
}  //namespace boost {

#include <boost/intrusive/detail/config_end.hpp>

#endif   //#ifndef BOOST_INTRUSIVE_DETAIL_GET_VALUE_TRAITS_HPP
