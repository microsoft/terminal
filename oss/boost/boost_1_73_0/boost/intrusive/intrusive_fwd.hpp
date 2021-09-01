/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2007-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_FWD_HPP
#define BOOST_INTRUSIVE_FWD_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#ifndef BOOST_CSTDINT_HPP
#  include <boost/cstdint.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

//! \file
//! This header file forward declares most Intrusive classes.
//!
//! It forward declares the following containers and hooks:
//!   - boost::intrusive::slist / boost::intrusive::slist_base_hook / boost::intrusive::slist_member_hook
//!   - boost::intrusive::list / boost::intrusive::list_base_hook / boost::intrusive::list_member_hook
//!   - boost::intrusive::bstree / boost::intrusive::bs_set / boost::intrusive::bs_multiset /
//!      boost::intrusive::bs_set_base_hook / boost::intrusive::bs_set_member_hook
//!   - boost::intrusive::rbtree / boost::intrusive::set / boost::intrusive::multiset /
//!      boost::intrusive::set_base_hook / boost::intrusive::set_member_hook
//!   - boost::intrusive::avltree / boost::intrusive::avl_set / boost::intrusive::avl_multiset /
//!      boost::intrusive::avl_set_base_hook / boost::intrusive::avl_set_member_hook
//!   - boost::intrusive::splaytree / boost::intrusive::splay_set / boost::intrusive::splay_multiset
//!   - boost::intrusive::sgtree / boost::intrusive::sg_set / boost::intrusive::sg_multiset
//!   - boost::intrusive::treap / boost::intrusive::treap_set / boost::intrusive::treap_multiset
//!   - boost::intrusive::hashtable / boost::intrusive::unordered_set / boost::intrusive::unordered_multiset /
//!      boost::intrusive::unordered_set_base_hook / boost::intrusive::unordered_set_member_hook /
//!   - boost::intrusive::any_base_hook / boost::intrusive::any_member_hook
//!
//! It forward declares the following container or hook options:
//!   - boost::intrusive::constant_time_size / boost::intrusive::size_type / boost::intrusive::compare / boost::intrusive::equal
//!   - boost::intrusive::floating_point / boost::intrusive::priority / boost::intrusive::hash
//!   - boost::intrusive::value_traits / boost::intrusive::member_hook / boost::intrusive::function_hook / boost::intrusive::base_hook
//!   - boost::intrusive::void_pointer / boost::intrusive::tag / boost::intrusive::link_mode
//!   - boost::intrusive::optimize_size / boost::intrusive::linear / boost::intrusive::cache_last
//!   - boost::intrusive::bucket_traits / boost::intrusive::store_hash / boost::intrusive::optimize_multikey
//!   - boost::intrusive::power_2_buckets / boost::intrusive::cache_begin / boost::intrusive::compare_hash / boost::intrusive::incremental
//!
//! It forward declares the following value traits utilities:
//!   - boost::intrusive::value_traits / boost::intrusive::derivation_value_traits /
//!      boost::intrusive::trivial_value_traits
//!
//! Finally it forward declares the following general purpose utilities:
//!   - boost::intrusive::pointer_plus_bits / boost::intrusive::priority_compare.

#if !defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)

#include <cstddef>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/detail/workaround.hpp>

namespace boost {
namespace intrusive {

#if !defined(BOOST_INTERPROCESS_DOXYGEN_INVOKED)
#  ifdef BOOST_HAS_INTPTR_T
      using ::boost::uintptr_t;
#  else
      typedef std::size_t uintptr_t;
#  endif
#endif

////////////////////////////
//     Node algorithms
////////////////////////////

//Algorithms predeclarations
template<class NodeTraits>
class circular_list_algorithms;

template<class NodeTraits>
class circular_slist_algorithms;

template<class NodeTraits>
class linear_slist_algorithms;

template<class NodeTraits>
class bstree_algorithms;

template<class NodeTraits>
class rbtree_algorithms;

template<class NodeTraits>
class avltree_algorithms;

template<class NodeTraits>
class sgtree_algorithms;

template<class NodeTraits>
class splaytree_algorithms;

template<class NodeTraits>
class treap_algorithms;

////////////////////////////
//       Containers
////////////////////////////

//slist
#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class slist;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   >
#else
template<class ...Options>
#endif
class slist_base_hook;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   >
#else
template<class ...Options>
#endif
class slist_member_hook;

//list
#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   >
#else
template<class T, class ...Options>
#endif
class list;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   >
#else
template<class ...Options>
#endif
class list_base_hook;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   >
#else
template<class ...Options>
#endif
class list_member_hook;

//rbtree/set/multiset
#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class rbtree;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class set;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class multiset;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   >
#else
template<class ...Options>
#endif
class set_base_hook;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   >
#else
template<class ...Options>
#endif
class set_member_hook;

//splaytree/splay_set/splay_multiset
#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class splaytree;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class splay_set;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class splay_multiset;

//avltree/avl_set/avl_multiset
#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class avltree;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class avl_set;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class avl_multiset;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   >
#else
template<class ...Options>
#endif
class avl_set_base_hook;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   >
#else
template<class ...Options>
#endif
class avl_set_member_hook;


//treap/treap_set/treap_multiset
#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   , class O7  = void
   >
#else
template<class T, class ...Options>
#endif
class treap;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   , class O7  = void
   >
#else
template<class T, class ...Options>
#endif
class treap_set;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   , class O7  = void
   >
#else
template<class T, class ...Options>
#endif
class treap_multiset;

//sgtree/sg_set/sg_multiset
#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class sgtree;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class sg_set;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class sg_multiset;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class bstree;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class bs_set;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   >
#else
template<class T, class ...Options>
#endif
class bs_multiset;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   >
#else
template<class ...Options>
#endif
class bs_set_base_hook;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   >
#else
template<class ...Options>
#endif
class bs_set_member_hook;

//hashtable/unordered_set/unordered_multiset

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   , class O7  = void
   , class O8  = void
   , class O9  = void
   , class O10 = void
   >
#else
template<class T, class ...Options>
#endif
class hashtable;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   , class O7  = void
   , class O8  = void
   , class O9  = void
   , class O10 = void
   >
#else
template<class T, class ...Options>
#endif
class unordered_set;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class T
   , class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   , class O5  = void
   , class O6  = void
   , class O7  = void
   , class O8  = void
   , class O9  = void
   , class O10 = void
   >
#else
template<class T, class ...Options>
#endif
class unordered_multiset;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   >
#else
template<class ...Options>
#endif
class unordered_set_base_hook;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   , class O4  = void
   >
#else
template<class ...Options>
#endif
class unordered_set_member_hook;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   >
#else
template<class ...Options>
#endif
class any_base_hook;

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template
   < class O1  = void
   , class O2  = void
   , class O3  = void
   >
#else
template<class ...Options>
#endif
class any_member_hook;

//Options

template<bool Enabled>
struct constant_time_size;

template<typename SizeType>
struct size_type;

template<typename Compare>
struct compare;

template<bool Enabled>
struct floating_point;

template<typename Equal>
struct equal;

template<typename Priority>
struct priority;

template<typename Hash>
struct hash;

template<typename ValueTraits> struct value_traits;

template< typename Parent
        , typename MemberHook
        , MemberHook Parent::* PtrToMember>
struct member_hook;

template<typename Functor>
struct function_hook;

template<typename BaseHook>
struct base_hook;

template<typename VoidPointer>
struct void_pointer;

template<typename Tag>
struct tag;

template<link_mode_type LinkType>
struct link_mode;

template<bool Enabled> struct
optimize_size;

template<bool Enabled>
struct linear;

template<bool Enabled>
struct cache_last;

template<typename BucketTraits>
struct bucket_traits;

template<bool Enabled>
struct store_hash;

template<bool Enabled>
struct optimize_multikey;

template<bool Enabled>
struct power_2_buckets;

template<bool Enabled>
struct cache_begin;

template<bool Enabled>
struct compare_hash;

template<bool Enabled>
struct incremental;

//Value traits

template<typename ValueTraits>
struct value_traits;

template< typename Parent
        , typename MemberHook
        , MemberHook Parent::* PtrToMember>
struct member_hook;

template< typename Functor>
struct function_hook;

template<typename BaseHook>
struct base_hook;

template<class T, class NodeTraits, link_mode_type LinkMode = safe_link>
struct derivation_value_traits;

template<class NodeTraits, link_mode_type LinkMode = normal_link>
struct trivial_value_traits;

//Additional utilities

template<typename VoidPointer, std::size_t Alignment>
struct max_pointer_plus_bits;

template<std::size_t Alignment>
struct max_pointer_plus_bits<void *, Alignment>;

template<typename Pointer, std::size_t NumBits>
struct pointer_plus_bits;

template<typename T, std::size_t NumBits>
struct pointer_plus_bits<T *, NumBits>;

template<typename Ptr>
struct pointer_traits;

template<typename T>
struct pointer_traits<T *>;

}  //namespace intrusive {
}  //namespace boost {

#endif   //#if !defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)

#endif   //#ifndef BOOST_INTRUSIVE_FWD_HPP
