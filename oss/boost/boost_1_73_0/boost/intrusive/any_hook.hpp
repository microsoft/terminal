/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2006-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_ANY_HOOK_HPP
#define BOOST_INTRUSIVE_ANY_HOOK_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/detail/any_node_and_algorithms.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/detail/generic_hook.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/intrusive/pointer_rebind.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

//! Helper metafunction to define a \c \c any_base_hook that yields to the same
//! type when the same options (either explicitly or implicitly) are used.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void>
#endif
struct make_any_base_hook
{
   /// @cond
   typedef typename pack_options
      < hook_defaults,
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      O1, O2, O3
      #else
      Options...
      #endif
      >::type packed_options;

   typedef generic_hook
   < AnyAlgorithm
   , any_node_traits<typename packed_options::void_pointer>
   , typename packed_options::tag
   , packed_options::link_mode
   , AnyBaseHookId
   > implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

//! Derive a class from this hook in order to store objects of that class
//! in an intrusive container.
//!
//! The hook admits the following options: \c tag<>, \c void_pointer<> and
//! \c link_mode<>.
//!
//! \c tag<> defines a tag to identify the node.
//! The same tag value can be used in different classes, but if a class is
//! derived from more than one \c any_base_hook, then each \c any_base_hook needs its
//! unique tag.
//!
//! \c link_mode<> will specify the linking mode of the hook (\c normal_link, \c safe_link).
//!
//! \c void_pointer<> is the pointer type that will be used internally in the hook
//! and the container configured to use this hook.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1, class O2, class O3>
#endif
class any_base_hook
   :  public make_any_base_hook
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      <O1, O2, O3>
      #else
      <Options...>
      #endif
      ::type
{
   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
   public:
   //! <b>Effects</b>: If link_mode is or \c safe_link
   //!   initializes the node to an unlinked state.
   //!
   //! <b>Throws</b>: Nothing.
   any_base_hook();

   //! <b>Effects</b>: If link_mode is or \c safe_link
   //!   initializes the node to an unlinked state. The argument is ignored.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Rationale</b>: Providing a copy-constructor
   //!   makes classes using the hook STL-compliant without forcing the
   //!   user to do some additional work. \c swap can be used to emulate
   //!   move-semantics.
   any_base_hook(const any_base_hook& );

   //! <b>Effects</b>: Empty function. The argument is ignored.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Rationale</b>: Providing an assignment operator
   //!   makes classes using the hook STL-compliant without forcing the
   //!   user to do some additional work. \c swap can be used to emulate
   //!   move-semantics.
   any_base_hook& operator=(const any_base_hook& );

   //! <b>Effects</b>: If link_mode is \c normal_link, the destructor does
   //!   nothing (ie. no code is generated). If link_mode is \c safe_link and the
   //!   object is stored in a container an assertion is raised.
   //!
   //! <b>Throws</b>: Nothing.
   ~any_base_hook();

   //! <b>Precondition</b>: link_mode must be \c safe_link.
   //!
   //! <b>Returns</b>: true, if the node belongs to a container, false
   //!   otherwise. This function can be used to test whether \c container::iterator_to
   //!   will return a valid iterator.
   //!
   //! <b>Complexity</b>: Constant
   bool is_linked() const;
   #endif
};

//! Helper metafunction to define a \c \c any_member_hook that yields to the same
//! type when the same options (either explicitly or implicitly) are used.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void>
#endif
struct make_any_member_hook
{
   /// @cond
   typedef typename pack_options
      < hook_defaults,
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      O1, O2, O3
      #else
      Options...
      #endif
      >::type packed_options;

   typedef generic_hook
   < AnyAlgorithm
   , any_node_traits<typename packed_options::void_pointer>
   , member_tag
   , packed_options::link_mode
   , NoBaseHookId
   > implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

//! Store this hook in a class to be inserted
//! in an intrusive container.
//!
//! The hook admits the following options: \c void_pointer<> and
//! \c link_mode<>.
//!
//! \c link_mode<> will specify the linking mode of the hook (\c normal_link or \c safe_link).
//!
//! \c void_pointer<> is the pointer type that will be used internally in the hook
//! and the container configured to use this hook.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1, class O2, class O3>
#endif
class any_member_hook
   :  public make_any_member_hook
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      <O1, O2, O3>
      #else
      <Options...>
      #endif
      ::type
{
   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
   public:
   //! <b>Effects</b>: If link_mode is or \c safe_link
   //!   initializes the node to an unlinked state.
   //!
   //! <b>Throws</b>: Nothing.
   any_member_hook();

   //! <b>Effects</b>: If link_mode is or \c safe_link
   //!   initializes the node to an unlinked state. The argument is ignored.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Rationale</b>: Providing a copy-constructor
   //!   makes classes using the hook STL-compliant without forcing the
   //!   user to do some additional work. \c swap can be used to emulate
   //!   move-semantics.
   any_member_hook(const any_member_hook& );

   //! <b>Effects</b>: Empty function. The argument is ignored.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Rationale</b>: Providing an assignment operator
   //!   makes classes using the hook STL-compliant without forcing the
   //!   user to do some additional work. \c swap can be used to emulate
   //!   move-semantics.
   any_member_hook& operator=(const any_member_hook& );

   //! <b>Effects</b>: If link_mode is \c normal_link, the destructor does
   //!   nothing (ie. no code is generated). If link_mode is \c safe_link and the
   //!   object is stored in a container an assertion is raised.
   //!
   //! <b>Throws</b>: Nothing.
   ~any_member_hook();

   //! <b>Precondition</b>: link_mode must be \c safe_link.
   //!
   //! <b>Returns</b>: true, if the node belongs to a container, false
   //!   otherwise. This function can be used to test whether \c container::iterator_to
   //!   will return a valid iterator.
   //!
   //! <b>Complexity</b>: Constant
   bool is_linked() const;
   #endif
};

/// @cond

namespace detail{

BOOST_INTRUSIVE_INTERNAL_STATIC_BOOL_IS_TRUE(old_proto_value_traits_base_hook, hooktags::is_base_hook)

//!This option setter specifies that the container
//!must use the specified base hook
template<class BasicHook, template <class> class NodeTraits>
struct any_to_some_hook
{
   typedef typename BasicHook::template pack<empty>::proto_value_traits old_proto_value_traits;

   template<class Base>
   struct pack : public Base
   {
      struct proto_value_traits
      {
         //proto_value_traits::hooktags::is_base_hook is used by get_value_traits
         //to detect base hooks, so mark it in case BasicHook has it.
         struct hooktags
         {
            static const bool is_base_hook = old_proto_value_traits_base_hook_bool_is_true
               <old_proto_value_traits>::value;
         };

         typedef old_proto_value_traits basic_hook_t;
         static const bool is_any_hook = true;

         template<class VoidPtr>
         struct node_traits_from_voidptr
         {  typedef NodeTraits<VoidPtr> type;  };
      };
   };
};

}  //namespace detail{

/// @endcond

//!This option setter specifies that
//!any hook should behave as an slist hook
template<class BasicHook>
struct any_to_slist_hook
/// @cond
   :  public detail::any_to_some_hook<BasicHook, any_slist_node_traits>
/// @endcond
{};

//!This option setter specifies that
//!any hook should behave as an list hook
template<class BasicHook>
struct any_to_list_hook
/// @cond
   :  public detail::any_to_some_hook<BasicHook, any_list_node_traits>
/// @endcond
{};

//!This option setter specifies that
//!any hook should behave as a set hook
template<class BasicHook>
struct any_to_set_hook
/// @cond
   :  public detail::any_to_some_hook<BasicHook, any_rbtree_node_traits>
/// @endcond
{};

//!This option setter specifies that
//!any hook should behave as an avl_set hook
template<class BasicHook>
struct any_to_avl_set_hook
/// @cond
   :  public detail::any_to_some_hook<BasicHook, any_avltree_node_traits>
/// @endcond
{};

//!This option setter specifies that any
//!hook should behave as a bs_set hook
template<class BasicHook>
struct any_to_bs_set_hook
/// @cond
   :  public detail::any_to_some_hook<BasicHook, any_tree_node_traits>
/// @endcond
{};

//!This option setter specifies that any hook
//!should behave as an unordered set hook
template<class BasicHook>
struct any_to_unordered_set_hook
/// @cond
   :  public detail::any_to_some_hook<BasicHook, any_unordered_node_traits>
/// @endcond
{};


} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_ANY_HOOK_HPP
