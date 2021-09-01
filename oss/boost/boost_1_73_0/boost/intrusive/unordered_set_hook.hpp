/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Olaf Krzikalla 2004-2006.
// (C) Copyright Ion Gaztanaga  2006-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_UNORDERED_SET_HOOK_HPP
#define BOOST_INTRUSIVE_UNORDERED_SET_HOOK_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>

#include <boost/intrusive/pointer_traits.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/detail/generic_hook.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

/// @cond

template<class VoidPointer, bool StoreHash, bool OptimizeMultiKey>
struct unordered_node
   :  public slist_node<VoidPointer>
{
   typedef typename pointer_traits
      <VoidPointer>::template rebind_pointer
         < unordered_node<VoidPointer, StoreHash, OptimizeMultiKey> >::type
      node_ptr;
   node_ptr    prev_in_group_;
   std::size_t hash_;
};

template<class VoidPointer>
struct unordered_node<VoidPointer, false, true>
   :  public slist_node<VoidPointer>
{
   typedef typename pointer_traits
      <VoidPointer>::template rebind_pointer
         < unordered_node<VoidPointer, false, true> >::type
      node_ptr;
   node_ptr    prev_in_group_;
};

template<class VoidPointer>
struct unordered_node<VoidPointer, true, false>
   :  public slist_node<VoidPointer>
{
   typedef typename pointer_traits
      <VoidPointer>::template rebind_pointer
         < unordered_node<VoidPointer, true, false> >::type
      node_ptr;
   std::size_t hash_;
};

template<class VoidPointer, bool StoreHash, bool OptimizeMultiKey>
struct unordered_node_traits
   :  public slist_node_traits<VoidPointer>
{
   typedef slist_node_traits<VoidPointer> reduced_slist_node_traits;
   typedef unordered_node<VoidPointer, StoreHash, OptimizeMultiKey> node;

   typedef typename pointer_traits
      <VoidPointer>::template rebind_pointer
         < node >::type node_ptr;
   typedef typename pointer_traits
      <VoidPointer>::template rebind_pointer
         < const node >::type const_node_ptr;

   static const bool store_hash        = StoreHash;
   static const bool optimize_multikey = OptimizeMultiKey;

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_next(const const_node_ptr & n)
   {  return pointer_traits<node_ptr>::static_cast_from(n->next_);  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_next(node_ptr n, node_ptr next)
   {  n->next_ = next;  }

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_prev_in_group(const const_node_ptr & n)
   {  return n->prev_in_group_;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_prev_in_group(node_ptr n, node_ptr prev)
   {  n->prev_in_group_ = prev;  }

   BOOST_INTRUSIVE_FORCEINLINE static std::size_t get_hash(const const_node_ptr & n)
   {  return n->hash_;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_hash(const node_ptr & n, std::size_t h)
   {  n->hash_ = h;  }
};

template<class NodeTraits>
struct unordered_group_adapter
{
   typedef typename NodeTraits::node            node;
   typedef typename NodeTraits::node_ptr        node_ptr;
   typedef typename NodeTraits::const_node_ptr  const_node_ptr;

   static node_ptr get_next(const const_node_ptr & n)
   {  return NodeTraits::get_prev_in_group(n);  }

   static void set_next(node_ptr n, node_ptr next)
   {  NodeTraits::set_prev_in_group(n, next);   }
};

template<class NodeTraits>
struct unordered_algorithms
   : public circular_slist_algorithms<NodeTraits>
{
   typedef circular_slist_algorithms<NodeTraits>   base_type;
   typedef unordered_group_adapter<NodeTraits>     group_traits;
   typedef circular_slist_algorithms<group_traits> group_algorithms;
   typedef NodeTraits                              node_traits;
   typedef typename NodeTraits::node               node;
   typedef typename NodeTraits::node_ptr           node_ptr;
   typedef typename NodeTraits::const_node_ptr     const_node_ptr;

   BOOST_INTRUSIVE_FORCEINLINE static void init(typename base_type::node_ptr n)
   {
      base_type::init(n);
      group_algorithms::init(n);
   }

   BOOST_INTRUSIVE_FORCEINLINE static void init_header(typename base_type::node_ptr n)
   {
      base_type::init_header(n);
      group_algorithms::init_header(n);
   }

   BOOST_INTRUSIVE_FORCEINLINE static void unlink(typename base_type::node_ptr n)
   {
      base_type::unlink(n);
      group_algorithms::unlink(n);
   }
};

//Class to avoid defining the same algo as a circular list, as hooks would be ambiguous between them
template<class Algo>
struct uset_algo_wrapper : public Algo
{};

template<class VoidPointer, bool StoreHash, bool OptimizeMultiKey>
struct get_uset_node_traits
{
   typedef typename detail::if_c
      < (StoreHash || OptimizeMultiKey)
      , unordered_node_traits<VoidPointer, StoreHash, OptimizeMultiKey>
      , slist_node_traits<VoidPointer>
      >::type type;
};

template<bool OptimizeMultiKey>
struct get_uset_algo_type
{
   static const algo_types value = OptimizeMultiKey ? UnorderedAlgorithms : UnorderedCircularSlistAlgorithms;
};

template<class NodeTraits>
struct get_algo<UnorderedAlgorithms, NodeTraits>
{
   typedef unordered_algorithms<NodeTraits> type;
};

template<class NodeTraits>
struct get_algo<UnorderedCircularSlistAlgorithms, NodeTraits>
{
   typedef uset_algo_wrapper< circular_slist_algorithms<NodeTraits> > type;
};

/// @endcond

//! Helper metafunction to define a \c unordered_set_base_hook that yields to the same
//! type when the same options (either explicitly or implicitly) are used.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void, class O4 = void>
#endif
struct make_unordered_set_base_hook
{
   /// @cond
   typedef typename pack_options
      < hook_defaults,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4
         #else
         Options...
         #endif
      >::type packed_options;

   typedef generic_hook
   < get_uset_algo_type <packed_options::optimize_multikey>::value
   , typename get_uset_node_traits < typename packed_options::void_pointer
                                   , packed_options::store_hash
                                   , packed_options::optimize_multikey
                                   >::type
   , typename packed_options::tag
   , packed_options::link_mode
   , HashBaseHookId
   > implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

//! Derive a class from unordered_set_base_hook in order to store objects in
//! in an unordered_set/unordered_multi_set. unordered_set_base_hook holds the data necessary to maintain
//! the unordered_set/unordered_multi_set and provides an appropriate value_traits class for unordered_set/unordered_multi_set.
//!
//! The hook admits the following options: \c tag<>, \c void_pointer<>,
//! \c link_mode<>, \c store_hash<> and \c optimize_multikey<>.
//!
//! \c tag<> defines a tag to identify the node.
//! The same tag value can be used in different classes, but if a class is
//! derived from more than one \c list_base_hook, then each \c list_base_hook needs its
//! unique tag.
//!
//! \c void_pointer<> is the pointer type that will be used internally in the hook
//! and the container configured to use this hook.
//!
//! \c link_mode<> will specify the linking mode of the hook (\c normal_link,
//! \c auto_unlink or \c safe_link).
//!
//! \c store_hash<> will tell the hook to store the hash of the value
//! to speed up rehashings.
//!
//! \c optimize_multikey<> will tell the hook to store a link to form a group
//! with other value with the same value to speed up searches and insertions
//! in unordered_multisets with a great number of with equivalent keys.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1, class O2, class O3, class O4>
#endif
class unordered_set_base_hook
   :  public make_unordered_set_base_hook<
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4
         #else
         Options...
         #endif
      >::type
{
   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
   public:
   //! <b>Effects</b>: If link_mode is \c auto_unlink or \c safe_link
   //!   initializes the node to an unlinked state.
   //!
   //! <b>Throws</b>: Nothing.
   unordered_set_base_hook();

   //! <b>Effects</b>: If link_mode is \c auto_unlink or \c safe_link
   //!   initializes the node to an unlinked state. The argument is ignored.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Rationale</b>: Providing a copy-constructor
   //!   makes classes using the hook STL-compliant without forcing the
   //!   user to do some additional work. \c swap can be used to emulate
   //!   move-semantics.
   unordered_set_base_hook(const unordered_set_base_hook& );

   //! <b>Effects</b>: Empty function. The argument is ignored.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Rationale</b>: Providing an assignment operator
   //!   makes classes using the hook STL-compliant without forcing the
   //!   user to do some additional work. \c swap can be used to emulate
   //!   move-semantics.
   unordered_set_base_hook& operator=(const unordered_set_base_hook& );

   //! <b>Effects</b>: If link_mode is \c normal_link, the destructor does
   //!   nothing (ie. no code is generated). If link_mode is \c safe_link and the
   //!   object is stored in an unordered_set an assertion is raised. If link_mode is
   //!   \c auto_unlink and \c is_linked() is true, the node is unlinked.
   //!
   //! <b>Throws</b>: Nothing.
   ~unordered_set_base_hook();

   //! <b>Effects</b>: Swapping two nodes swaps the position of the elements
   //!   related to those nodes in one or two containers. That is, if the node
   //!   this is part of the element e1, the node x is part of the element e2
   //!   and both elements are included in the containers s1 and s2, then after
   //!   the swap-operation e1 is in s2 at the position of e2 and e2 is in s1
   //!   at the position of e1. If one element is not in a container, then
   //!   after the swap-operation the other element is not in a container.
   //!   Iterators to e1 and e2 related to those nodes are invalidated.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   void swap_nodes(unordered_set_base_hook &other);

   //! <b>Precondition</b>: link_mode must be \c safe_link or \c auto_unlink.
   //!
   //! <b>Returns</b>: true, if the node belongs to a container, false
   //!   otherwise. This function can be used to test whether \c unordered_set::iterator_to
   //!   will return a valid iterator.
   //!
   //! <b>Complexity</b>: Constant
   bool is_linked() const;

   //! <b>Effects</b>: Removes the node if it's inserted in a container.
   //!   This function is only allowed if link_mode is \c auto_unlink.
   //!
   //! <b>Throws</b>: Nothing.
   void unlink();
   #endif
};


//! Helper metafunction to define a \c unordered_set_member_hook that yields to the same
//! type when the same options (either explicitly or implicitly) are used.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void, class O4 = void>
#endif
struct make_unordered_set_member_hook
{
   /// @cond
   typedef typename pack_options
      < hook_defaults,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4
         #else
         Options...
         #endif
      >::type packed_options;

   typedef generic_hook
   < get_uset_algo_type <packed_options::optimize_multikey>::value
   , typename get_uset_node_traits < typename packed_options::void_pointer
                                   , packed_options::store_hash
                                   , packed_options::optimize_multikey
                                   >::type
   , member_tag
   , packed_options::link_mode
   , NoBaseHookId
   > implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

//! Put a public data member unordered_set_member_hook in order to store objects of this class in
//! an unordered_set/unordered_multi_set. unordered_set_member_hook holds the data necessary for maintaining the
//! unordered_set/unordered_multi_set and provides an appropriate value_traits class for unordered_set/unordered_multi_set.
//!
//! The hook admits the following options: \c void_pointer<>,
//! \c link_mode<> and \c store_hash<>.
//!
//! \c void_pointer<> is the pointer type that will be used internally in the hook
//! and the container configured to use this hook.
//!
//! \c link_mode<> will specify the linking mode of the hook (\c normal_link,
//! \c auto_unlink or \c safe_link).
//!
//! \c store_hash<> will tell the hook to store the hash of the value
//! to speed up rehashings.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1, class O2, class O3, class O4>
#endif
class unordered_set_member_hook
   :  public make_unordered_set_member_hook<
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4
         #else
         Options...
         #endif
   >::type
{
   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
   public:
   //! <b>Effects</b>: If link_mode is \c auto_unlink or \c safe_link
   //!   initializes the node to an unlinked state.
   //!
   //! <b>Throws</b>: Nothing.
   unordered_set_member_hook();

   //! <b>Effects</b>: If link_mode is \c auto_unlink or \c safe_link
   //!   initializes the node to an unlinked state. The argument is ignored.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Rationale</b>: Providing a copy-constructor
   //!   makes classes using the hook STL-compliant without forcing the
   //!   user to do some additional work. \c swap can be used to emulate
   //!   move-semantics.
   unordered_set_member_hook(const unordered_set_member_hook& );

   //! <b>Effects</b>: Empty function. The argument is ignored.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Rationale</b>: Providing an assignment operator
   //!   makes classes using the hook STL-compliant without forcing the
   //!   user to do some additional work. \c swap can be used to emulate
   //!   move-semantics.
   unordered_set_member_hook& operator=(const unordered_set_member_hook& );

   //! <b>Effects</b>: If link_mode is \c normal_link, the destructor does
   //!   nothing (ie. no code is generated). If link_mode is \c safe_link and the
   //!   object is stored in an unordered_set an assertion is raised. If link_mode is
   //!   \c auto_unlink and \c is_linked() is true, the node is unlinked.
   //!
   //! <b>Throws</b>: Nothing.
   ~unordered_set_member_hook();

   //! <b>Effects</b>: Swapping two nodes swaps the position of the elements
   //!   related to those nodes in one or two containers. That is, if the node
   //!   this is part of the element e1, the node x is part of the element e2
   //!   and both elements are included in the containers s1 and s2, then after
   //!   the swap-operation e1 is in s2 at the position of e2 and e2 is in s1
   //!   at the position of e1. If one element is not in a container, then
   //!   after the swap-operation the other element is not in a container.
   //!   Iterators to e1 and e2 related to those nodes are invalidated.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   void swap_nodes(unordered_set_member_hook &other);

   //! <b>Precondition</b>: link_mode must be \c safe_link or \c auto_unlink.
   //!
   //! <b>Returns</b>: true, if the node belongs to a container, false
   //!   otherwise. This function can be used to test whether \c unordered_set::iterator_to
   //!   will return a valid iterator.
   //!
   //! <b>Complexity</b>: Constant
   bool is_linked() const;

   //! <b>Effects</b>: Removes the node if it's inserted in a container.
   //!   This function is only allowed if link_mode is \c auto_unlink.
   //!
   //! <b>Throws</b>: Nothing.
   void unlink();
   #endif
};

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_UNORDERED_SET_HOOK_HPP
