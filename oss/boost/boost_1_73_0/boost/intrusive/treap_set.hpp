/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_INTRUSIVE_TREAP_SET_HPP
#define BOOST_INTRUSIVE_TREAP_SET_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/treap.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/static_assert.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

#if !defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
template<class ValueTraits, class VoidOrKeyOfValue, class VoidOrKeyComp, class VoidOrPrioOfValue, class VoidOrPrioComp, class SizeType, bool ConstantTimeSize, typename HeaderHolder>
class treap_multiset_impl;
#endif

//! The class template treap_set is an intrusive container, that mimics most of
//! the interface of std::set as described in the C++ standard.
//!
//! The template parameter \c T is the type to be managed by the container.
//! The user can specify additional options and if no options are provided
//! default options are used.
//!
//! The container supports the following options:
//! \c base_hook<>/member_hook<>/value_traits<>,
//! \c constant_time_size<>, \c size_type<>,
//! \c compare<>, \c priority<> and \c priority_of_value<>
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
template<class T, class ...Options>
#else
template<class ValueTraits, class VoidOrKeyOfValue, class VoidOrKeyComp, class VoidOrPrioOfValue, class VoidOrPrioComp, class SizeType, bool ConstantTimeSize, typename HeaderHolder>
#endif
class treap_set_impl
#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   : public treap_impl<ValueTraits, VoidOrKeyOfValue, VoidOrKeyComp, VoidOrPrioOfValue, VoidOrPrioComp, SizeType, ConstantTimeSize, HeaderHolder>
#endif
{
   /// @cond
   public:
   typedef treap_impl<ValueTraits, VoidOrKeyOfValue, VoidOrKeyComp, VoidOrPrioOfValue, VoidOrPrioComp, SizeType, ConstantTimeSize, HeaderHolder> tree_type;
   BOOST_MOVABLE_BUT_NOT_COPYABLE(treap_set_impl)

   typedef tree_type implementation_defined;
   /// @endcond

   public:
   typedef typename implementation_defined::value_type               value_type;
   typedef typename implementation_defined::value_traits             value_traits;
   typedef typename implementation_defined::key_type                 key_type;
   typedef typename implementation_defined::key_of_value             key_of_value;
   typedef typename implementation_defined::pointer                  pointer;
   typedef typename implementation_defined::const_pointer            const_pointer;
   typedef typename implementation_defined::reference                reference;
   typedef typename implementation_defined::const_reference          const_reference;
   typedef typename implementation_defined::difference_type          difference_type;
   typedef typename implementation_defined::size_type                size_type;
   typedef typename implementation_defined::value_compare            value_compare;
   typedef typename implementation_defined::key_compare              key_compare;
   typedef typename implementation_defined::priority_type            priority_type;
   typedef typename implementation_defined::priority_compare         priority_compare;
   typedef typename implementation_defined::iterator                 iterator;
   typedef typename implementation_defined::const_iterator           const_iterator;
   typedef typename implementation_defined::reverse_iterator         reverse_iterator;
   typedef typename implementation_defined::const_reverse_iterator   const_reverse_iterator;
   typedef typename implementation_defined::insert_commit_data       insert_commit_data;
   typedef typename implementation_defined::node_traits              node_traits;
   typedef typename implementation_defined::node                     node;
   typedef typename implementation_defined::node_ptr                 node_ptr;
   typedef typename implementation_defined::const_node_ptr           const_node_ptr;
   typedef typename implementation_defined::node_algorithms          node_algorithms;

   static const bool constant_time_size = implementation_defined::constant_time_size;

   public:
   //! @copydoc ::boost::intrusive::treap::treap()
   treap_set_impl()
      :  tree_type()
   {}

   //! @copydoc ::boost::intrusive::treap::treap(const key_compare &,const priority_compare &,const value_traits &)
   explicit treap_set_impl( const key_compare &cmp
                          , const priority_compare &pcmp  = priority_compare()
                          , const value_traits &v_traits  = value_traits())
      :  tree_type(cmp, pcmp, v_traits)
   {}

   //! @copydoc ::boost::intrusive::treap::treap(bool,Iterator,Iterator,const key_compare &,const priority_compare &,const value_traits &)
   template<class Iterator>
   treap_set_impl( Iterator b, Iterator e
           , const key_compare &cmp = key_compare()
           , const priority_compare &pcmp  = priority_compare()
           , const value_traits &v_traits = value_traits())
      : tree_type(true, b, e, cmp, pcmp, v_traits)
   {}

   //! <b>Effects</b>: to-do
   //!
   treap_set_impl(BOOST_RV_REF(treap_set_impl) x)
      :  tree_type(BOOST_MOVE_BASE(tree_type, x))
   {}

   //! <b>Effects</b>: to-do
   //!
   treap_set_impl& operator=(BOOST_RV_REF(treap_set_impl) x)
   {  return static_cast<treap_set_impl&>(tree_type::operator=(BOOST_MOVE_BASE(tree_type, x)));  }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::treap::~treap()
   ~treap_set_impl();

   //! @copydoc ::boost::intrusive::treap::begin()
   iterator begin();

   //! @copydoc ::boost::intrusive::treap::begin()const
   const_iterator begin() const;

   //! @copydoc ::boost::intrusive::treap::cbegin()const
   const_iterator cbegin() const;

   //! @copydoc ::boost::intrusive::treap::end()
   iterator end();

   //! @copydoc ::boost::intrusive::treap::end()const
   const_iterator end() const;

   //! @copydoc ::boost::intrusive::treap::cend()const
   const_iterator cend() const;

   //! @copydoc ::boost::intrusive::treap::rbegin()
   reverse_iterator rbegin();

   //! @copydoc ::boost::intrusive::treap::rbegin()const
   const_reverse_iterator rbegin() const;

   //! @copydoc ::boost::intrusive::treap::crbegin()const
   const_reverse_iterator crbegin() const;

   //! @copydoc ::boost::intrusive::treap::rend()
   reverse_iterator rend();

   //! @copydoc ::boost::intrusive::treap::rend()const
   const_reverse_iterator rend() const;

   //! @copydoc ::boost::intrusive::treap::crend()const
   const_reverse_iterator crend() const;

   //! @copydoc ::boost::intrusive::treap::root()
   iterator root();

   //! @copydoc ::boost::intrusive::treap::root()const
   const_iterator root() const;

   //! @copydoc ::boost::intrusive::treap::croot()const
   const_iterator croot() const;

   //! @copydoc ::boost::intrusive::treap::container_from_end_iterator(iterator)
   static treap_set_impl &container_from_end_iterator(iterator end_iterator);

   //! @copydoc ::boost::intrusive::treap::container_from_end_iterator(const_iterator)
   static const treap_set_impl &container_from_end_iterator(const_iterator end_iterator);

   //! @copydoc ::boost::intrusive::treap::container_from_iterator(iterator)
   static treap_set_impl &container_from_iterator(iterator it);

   //! @copydoc ::boost::intrusive::treap::container_from_iterator(const_iterator)
   static const treap_set_impl &container_from_iterator(const_iterator it);

   //! @copydoc ::boost::intrusive::treap::key_comp()const
   key_compare key_comp() const;

   //! @copydoc ::boost::intrusive::treap::value_comp()const
   value_compare value_comp() const;

   //! @copydoc ::boost::intrusive::treap::empty()const
   bool empty() const;

   //! @copydoc ::boost::intrusive::treap::size()const
   size_type size() const;

   //! @copydoc ::boost::intrusive::treap::swap
   void swap(treap_set_impl& other);

   //! @copydoc ::boost::intrusive::treap::clone_from(const treap&,Cloner,Disposer)
   template <class Cloner, class Disposer>
   void clone_from(const treap_set_impl &src, Cloner cloner, Disposer disposer);

   #else

   using tree_type::clone_from;

   #endif

   //! @copydoc ::boost::intrusive::treap::clone_from(treap&&,Cloner,Disposer)
   template <class Cloner, class Disposer>
   void clone_from(BOOST_RV_REF(treap_set_impl) src, Cloner cloner, Disposer disposer)
   {  tree_type::clone_from(BOOST_MOVE_BASE(tree_type, src), cloner, disposer);  }

   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)

   //! @copydoc ::boost::intrusive::treap::top()
   iterator top();

   //! @copydoc ::boost::intrusive::treap::top()const
   const_iterator top() const;

   //! @copydoc ::boost::intrusive::treap::ctop()const
   const_iterator ctop() const;

   //! @copydoc ::boost::intrusive::treap::rtop()
   reverse_iterator rtop();

   //! @copydoc ::boost::intrusive::treap::rtop()const
   const_reverse_iterator rtop() const;

   //! @copydoc ::boost::intrusive::treap::crtop()const
   const_reverse_iterator crtop() const;

   //! @copydoc ::boost::intrusive::treap::crtop() const
   priority_compare priority_comp() const;
   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::treap::insert_unique(reference)
   std::pair<iterator, bool> insert(reference value)
   {  return tree_type::insert_unique(value);  }

   //! @copydoc ::boost::intrusive::treap::insert_unique(const_iterator,reference)
   iterator insert(const_iterator hint, reference value)
   {  return tree_type::insert_unique(hint, value);  }

   //! @copydoc ::boost::intrusive::treap::insert_unique_check(const key_type&,const priority_type &,insert_commit_data&)
   std::pair<iterator, bool> insert_check( const key_type &key, const priority_type &prio, insert_commit_data &commit_data)
   {  return tree_type::insert_unique_check(key, prio, commit_data); }

   //! @copydoc ::boost::intrusive::treap::insert_unique_check(const_iterator,const key_type&,const priority_type &,insert_commit_data&)
   std::pair<iterator, bool> insert_check
      ( const_iterator hint, const key_type &key, const priority_type &prio, insert_commit_data &commit_data)
   {  return tree_type::insert_unique_check(hint, key, prio, commit_data); }

   //! @copydoc ::boost::intrusive::treap::insert_unique_check(const KeyType&,KeyTypeKeyCompare,PrioValuePrioCompare,insert_commit_data&)
   template<class KeyType, class KeyTypeKeyCompare, class PrioType, class PrioValuePrioCompare>
   std::pair<iterator, bool> insert_check
      ( const KeyType &key, KeyTypeKeyCompare comp, const PrioType &prio, PrioValuePrioCompare pcomp
      , insert_commit_data &commit_data)
   {  return tree_type::insert_unique_check(key, comp, prio, pcomp, commit_data); }

   //! @copydoc ::boost::intrusive::treap::insert_unique_check(const_iterator,const KeyType&,KeyTypeKeyCompare,PrioValuePrioCompare,insert_commit_data&)
   template<class KeyType, class KeyTypeKeyCompare, class PrioType, class PrioValuePrioCompare>
   std::pair<iterator, bool> insert_check
      ( const_iterator hint
      , const KeyType &key, KeyTypeKeyCompare comp
      , const PrioType &prio, PrioValuePrioCompare pcomp
      , insert_commit_data &commit_data)
   {  return tree_type::insert_unique_check(hint, key, comp, prio, pcomp, commit_data); }

   //! @copydoc ::boost::intrusive::treap::insert_unique(Iterator,Iterator)
   template<class Iterator>
   void insert(Iterator b, Iterator e)
   {  tree_type::insert_unique(b, e);  }

   //! @copydoc ::boost::intrusive::treap::insert_unique_commit
   iterator insert_commit(reference value, const insert_commit_data &commit_data)
   {  return tree_type::insert_unique_commit(value, commit_data);  }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::treap::insert_before
   iterator insert_before(const_iterator pos, reference value);

   //! @copydoc ::boost::intrusive::treap::push_back
   void push_back(reference value);

   //! @copydoc ::boost::intrusive::treap::push_front
   void push_front(reference value);

   //! @copydoc ::boost::intrusive::treap::erase(const_iterator)
   iterator erase(const_iterator i);

   //! @copydoc ::boost::intrusive::treap::erase(const_iterator,const_iterator)
   iterator erase(const_iterator b, const_iterator e);

   //! @copydoc ::boost::intrusive::treap::erase(const key_type &)
   size_type erase(const key_type &key);

   //! @copydoc ::boost::intrusive::treap::erase(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   size_type erase(const KeyType& key, KeyTypeKeyCompare comp);

   //! @copydoc ::boost::intrusive::treap::erase_and_dispose(const_iterator,Disposer)
   template<class Disposer>
   iterator erase_and_dispose(const_iterator i, Disposer disposer);

   //! @copydoc ::boost::intrusive::treap::erase_and_dispose(const_iterator,const_iterator,Disposer)
   template<class Disposer>
   iterator erase_and_dispose(const_iterator b, const_iterator e, Disposer disposer);

   //! @copydoc ::boost::intrusive::treap::erase_and_dispose(const key_type &, Disposer)
   template<class Disposer>
   size_type erase_and_dispose(const key_type &key, Disposer disposer);

   //! @copydoc ::boost::intrusive::treap::erase_and_dispose(const KeyType&,KeyTypeKeyCompare,Disposer)
   template<class KeyType, class KeyTypeKeyCompare, class Disposer>
   size_type erase_and_dispose(const KeyType& key, KeyTypeKeyCompare comp, Disposer disposer);

   //! @copydoc ::boost::intrusive::treap::clear
   void clear();

   //! @copydoc ::boost::intrusive::treap::clear_and_dispose
   template<class Disposer>
   void clear_and_dispose(Disposer disposer);

   #endif   //   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::treap::count(const key_type &)const
   size_type count(const key_type &key) const
   {  return static_cast<size_type>(this->tree_type::find(key) != this->tree_type::cend()); }

   //! @copydoc ::boost::intrusive::treap::count(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   size_type count(const KeyType& key, KeyTypeKeyCompare comp) const
   {  return static_cast<size_type>(this->tree_type::find(key, comp) != this->tree_type::cend()); }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::treap::lower_bound(const key_type &)
   iterator lower_bound(const key_type &key);

   //! @copydoc ::boost::intrusive::treap::lower_bound(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   iterator lower_bound(const KeyType& key, KeyTypeKeyCompare comp);

   //! @copydoc ::boost::intrusive::treap::lower_bound(const key_type &)const
   const_iterator lower_bound(const key_type &key) const;

   //! @copydoc ::boost::intrusive::treap::lower_bound(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   const_iterator lower_bound(const KeyType& key, KeyTypeKeyCompare comp) const;

   //! @copydoc ::boost::intrusive::treap::upper_bound(const key_type &)
   iterator upper_bound(const key_type &key);

   //! @copydoc ::boost::intrusive::treap::upper_bound(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   iterator upper_bound(const KeyType& key, KeyTypeKeyCompare comp);

   //! @copydoc ::boost::intrusive::treap::upper_bound(const key_type &)const
   const_iterator upper_bound(const key_type &key) const;

   //! @copydoc ::boost::intrusive::treap::upper_bound(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   const_iterator upper_bound(const KeyType& key, KeyTypeKeyCompare comp) const;

   //! @copydoc ::boost::intrusive::treap::find(const key_type &)
   iterator find(const key_type &key);

   //! @copydoc ::boost::intrusive::treap::find(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   iterator find(const KeyType& key, KeyTypeKeyCompare comp);

   //! @copydoc ::boost::intrusive::treap::find(const key_type &)const
   const_iterator find(const key_type &key) const;

   //! @copydoc ::boost::intrusive::treap::find(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   const_iterator find(const KeyType& key, KeyTypeKeyCompare comp) const;

   #endif   //   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::treap::equal_range(const key_type &)
   std::pair<iterator,iterator> equal_range(const key_type &key)
   {  return this->tree_type::lower_bound_range(key); }

   //! @copydoc ::boost::intrusive::treap::equal_range(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   std::pair<iterator,iterator> equal_range(const KeyType& key, KeyTypeKeyCompare comp)
   {  return this->tree_type::equal_range(key, comp); }

   //! @copydoc ::boost::intrusive::treap::equal_range(const key_type &)const
   std::pair<const_iterator, const_iterator>
      equal_range(const key_type &key) const
   {  return this->tree_type::lower_bound_range(key); }

   //! @copydoc ::boost::intrusive::treap::equal_range(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   std::pair<const_iterator, const_iterator>
      equal_range(const KeyType& key, KeyTypeKeyCompare comp) const
   {  return this->tree_type::equal_range(key, comp); }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::treap::bounded_range(const key_type &,const key_type &,bool,bool)
   std::pair<iterator,iterator> bounded_range
      (const key_type &lower_key, const key_type &upper_key, bool left_closed, bool right_closed);

   //! @copydoc ::boost::intrusive::treap::bounded_range(const KeyType&,const KeyType&,KeyTypeKeyCompare,bool,bool)
   template<class KeyType, class KeyTypeKeyCompare>
   std::pair<iterator,iterator> bounded_range
      (const KeyType& lower_key, const KeyType& upper_key, KeyTypeKeyCompare comp, bool left_closed, bool right_closed);

   //! @copydoc ::boost::intrusive::treap::bounded_range(const key_type &,const key_type &,bool,bool)const
   std::pair<const_iterator, const_iterator>
      bounded_range(const key_type &lower_key, const key_type &upper_key, bool left_closed, bool right_closed) const;

   //! @copydoc ::boost::intrusive::treap::bounded_range(const KeyType&,const KeyType&,KeyTypeKeyCompare,bool,bool)const
   template<class KeyType, class KeyTypeKeyCompare>
   std::pair<const_iterator, const_iterator> bounded_range
         (const KeyType& lower_key, const KeyType& upper_key, KeyTypeKeyCompare comp, bool left_closed, bool right_closed) const;

   //! @copydoc ::boost::intrusive::treap::s_iterator_to(reference)
   static iterator s_iterator_to(reference value);

   //! @copydoc ::boost::intrusive::treap::s_iterator_to(const_reference)
   static const_iterator s_iterator_to(const_reference value);

   //! @copydoc ::boost::intrusive::treap::iterator_to(reference)
   iterator iterator_to(reference value);

   //! @copydoc ::boost::intrusive::treap::iterator_to(const_reference)const
   const_iterator iterator_to(const_reference value) const;

   //! @copydoc ::boost::intrusive::treap::init_node(reference)
   static void init_node(reference value);

   //! @copydoc ::boost::intrusive::treap::unlink_leftmost_without_rebalance
   pointer unlink_leftmost_without_rebalance();

   //! @copydoc ::boost::intrusive::treap::replace_node
   void replace_node(iterator replace_this, reference with_this);

   //! @copydoc ::boost::intrusive::treap::remove_node
   void remove_node(reference value);


   //! @copydoc ::boost::intrusive::treap::merge_unique
   template<class ...Options2>
   void merge(treap_set<T, Options2...> &source);

   //! @copydoc ::boost::intrusive::treap::merge_unique
   template<class ...Options2>
   void merge(treap_multiset<T, Options2...> &source);

   #else

   template<class Compare2>
   void merge(treap_set_impl<ValueTraits, VoidOrKeyOfValue, Compare2, VoidOrPrioOfValue, VoidOrPrioComp, SizeType, ConstantTimeSize, HeaderHolder> &source)
   {  return tree_type::merge_unique(source);  }

   template<class Compare2>
   void merge(treap_multiset_impl<ValueTraits, VoidOrKeyOfValue, Compare2, VoidOrPrioOfValue, VoidOrPrioComp, SizeType, ConstantTimeSize, HeaderHolder> &source)
   {  return tree_type::merge_unique(source);  }

   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
};


//! Helper metafunction to define a \c treap_set that yields to the same type when the
//! same options (either explicitly or implicitly) are used.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class ...Options>
#else
template<class T, class O1 = void, class O2 = void
                , class O3 = void, class O4 = void
                , class O5 = void, class O6 = void
                , class O7 = void>
#endif
struct make_treap_set
{
   typedef typename pack_options
      < treap_defaults,
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      O1, O2, O3, O4, O5, O6, O7
      #else
      Options...
      #endif
      >::type packed_options;

   typedef typename detail::get_value_traits
      <T, typename packed_options::proto_value_traits>::type value_traits;

   typedef treap_set_impl
         < value_traits
         , typename packed_options::key_of_value
         , typename packed_options::compare
         , typename packed_options::priority_of_value
         , typename packed_options::priority
         , typename packed_options::size_type
         , packed_options::constant_time_size
         , typename packed_options::header_holder_type
         > implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class O1, class O2, class O3, class O4, class O5, class O6, class O7>
#else
template<class T, class ...Options>
#endif
class treap_set
   :  public make_treap_set<T,
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      O1, O2, O3, O4, O5, O6, O7
      #else
      Options...
      #endif
      >::type
{
   typedef typename make_treap_set
      <T,
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      O1, O2, O3, O4, O5, O6, O7
      #else
      Options...
      #endif
      >::type   Base;
   BOOST_MOVABLE_BUT_NOT_COPYABLE(treap_set)

   public:
   typedef typename Base::key_compare        key_compare;
   typedef typename Base::priority_compare   priority_compare;
   typedef typename Base::value_traits       value_traits;
   typedef typename Base::iterator           iterator;
   typedef typename Base::const_iterator     const_iterator;

   //Assert if passed value traits are compatible with the type
   BOOST_STATIC_ASSERT((detail::is_same<typename value_traits::value_type, T>::value));

   BOOST_INTRUSIVE_FORCEINLINE treap_set()
      :  Base()
   {}

   BOOST_INTRUSIVE_FORCEINLINE explicit treap_set( const key_compare &cmp
                     , const priority_compare &pcmp = priority_compare()
                     , const value_traits &v_traits = value_traits())
      :  Base(cmp, pcmp, v_traits)
   {}

   template<class Iterator>
   BOOST_INTRUSIVE_FORCEINLINE treap_set( Iterator b, Iterator e
      , const key_compare &cmp = key_compare()
      , const priority_compare &pcmp = priority_compare()
      , const value_traits &v_traits = value_traits())
      :  Base(b, e, cmp, pcmp, v_traits)
   {}

   BOOST_INTRUSIVE_FORCEINLINE treap_set(BOOST_RV_REF(treap_set) x)
      :  Base(BOOST_MOVE_BASE(Base, x))
   {}

   BOOST_INTRUSIVE_FORCEINLINE treap_set& operator=(BOOST_RV_REF(treap_set) x)
   {  return static_cast<treap_set &>(this->Base::operator=(BOOST_MOVE_BASE(Base, x)));  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(const treap_set &src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(src, cloner, disposer);  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(BOOST_RV_REF(treap_set) src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(BOOST_MOVE_BASE(Base, src), cloner, disposer);  }

   BOOST_INTRUSIVE_FORCEINLINE static treap_set &container_from_end_iterator(iterator end_iterator)
   {  return static_cast<treap_set &>(Base::container_from_end_iterator(end_iterator));   }

   BOOST_INTRUSIVE_FORCEINLINE static const treap_set &container_from_end_iterator(const_iterator end_iterator)
   {  return static_cast<const treap_set &>(Base::container_from_end_iterator(end_iterator));   }

   BOOST_INTRUSIVE_FORCEINLINE static treap_set &container_from_iterator(iterator it)
   {  return static_cast<treap_set &>(Base::container_from_iterator(it));   }

   BOOST_INTRUSIVE_FORCEINLINE static const treap_set &container_from_iterator(const_iterator it)
   {  return static_cast<const treap_set &>(Base::container_from_iterator(it));   }
};

#endif

//! The class template treap_multiset is an intrusive container, that mimics most of
//! the interface of std::treap_multiset as described in the C++ standard.
//!
//! The template parameter \c T is the type to be managed by the container.
//! The user can specify additional options and if no options are provided
//! default options are used.
//!
//! The container supports the following options:
//! \c base_hook<>/member_hook<>/value_traits<>,
//! \c constant_time_size<>, \c size_type<>,
//! \c compare<>, \c priority<> and \c priority_of_value<>
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
template<class T, class ...Options>
#else
template<class ValueTraits, class VoidOrKeyOfValue, class VoidOrKeyComp, class VoidOrPrioOfValue, class VoidOrPrioComp, class SizeType, bool ConstantTimeSize, typename HeaderHolder>
#endif
class treap_multiset_impl
#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   : public treap_impl<ValueTraits, VoidOrKeyOfValue, VoidOrKeyComp, VoidOrPrioOfValue, VoidOrPrioComp, SizeType, ConstantTimeSize, HeaderHolder>
#endif
{
   /// @cond
   typedef treap_impl<ValueTraits, VoidOrKeyOfValue, VoidOrKeyComp, VoidOrPrioOfValue, VoidOrPrioComp, SizeType, ConstantTimeSize, HeaderHolder> tree_type;
   BOOST_MOVABLE_BUT_NOT_COPYABLE(treap_multiset_impl)

   typedef tree_type implementation_defined;
   /// @endcond

   public:
   typedef typename implementation_defined::value_type               value_type;
   typedef typename implementation_defined::value_traits             value_traits;
   typedef typename implementation_defined::key_type                 key_type;
   typedef typename implementation_defined::key_of_value             key_of_value;
   typedef typename implementation_defined::pointer                  pointer;
   typedef typename implementation_defined::const_pointer            const_pointer;
   typedef typename implementation_defined::reference                reference;
   typedef typename implementation_defined::const_reference          const_reference;
   typedef typename implementation_defined::difference_type          difference_type;
   typedef typename implementation_defined::size_type                size_type;
   typedef typename implementation_defined::value_compare            value_compare;
   typedef typename implementation_defined::key_compare              key_compare;
   typedef typename implementation_defined::priority_type            priority_type;
   typedef typename implementation_defined::priority_compare         priority_compare;
   typedef typename implementation_defined::iterator                 iterator;
   typedef typename implementation_defined::const_iterator           const_iterator;
   typedef typename implementation_defined::reverse_iterator         reverse_iterator;
   typedef typename implementation_defined::const_reverse_iterator   const_reverse_iterator;
   typedef typename implementation_defined::insert_commit_data       insert_commit_data;
   typedef typename implementation_defined::node_traits              node_traits;
   typedef typename implementation_defined::node                     node;
   typedef typename implementation_defined::node_ptr                 node_ptr;
   typedef typename implementation_defined::const_node_ptr           const_node_ptr;
   typedef typename implementation_defined::node_algorithms          node_algorithms;

   static const bool constant_time_size = implementation_defined::constant_time_size;

   public:

   //! @copydoc ::boost::intrusive::treap::treap()
   treap_multiset_impl()
      :  tree_type()
   {}

   //! @copydoc ::boost::intrusive::treap::treap(const key_compare &,const priority_compare &,const value_traits &)
   explicit treap_multiset_impl( const key_compare &cmp
                          , const priority_compare &pcmp  = priority_compare()
                          , const value_traits &v_traits  = value_traits())
      :  tree_type(cmp, pcmp, v_traits)
   {}

   //! @copydoc ::boost::intrusive::treap::treap(bool,Iterator,Iterator,const key_compare &,const priority_compare &,const value_traits &)
   template<class Iterator>
   treap_multiset_impl( Iterator b, Iterator e
           , const key_compare &cmp = key_compare()
           , const priority_compare &pcmp  = priority_compare()
           , const value_traits &v_traits = value_traits())
      : tree_type(false, b, e, cmp, pcmp, v_traits)
   {}

   //! <b>Effects</b>: to-do
   //!
   treap_multiset_impl(BOOST_RV_REF(treap_multiset_impl) x)
      :  tree_type(BOOST_MOVE_BASE(tree_type, x))
   {}

   //! <b>Effects</b>: to-do
   //!
   treap_multiset_impl& operator=(BOOST_RV_REF(treap_multiset_impl) x)
   {  return static_cast<treap_multiset_impl&>(tree_type::operator=(BOOST_MOVE_BASE(tree_type, x)));  }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::treap::~treap()
   ~treap_multiset_impl();

   //! @copydoc ::boost::intrusive::treap::begin()
   iterator begin();

   //! @copydoc ::boost::intrusive::treap::begin()const
   const_iterator begin() const;

   //! @copydoc ::boost::intrusive::treap::cbegin()const
   const_iterator cbegin() const;

   //! @copydoc ::boost::intrusive::treap::end()
   iterator end();

   //! @copydoc ::boost::intrusive::treap::end()const
   const_iterator end() const;

   //! @copydoc ::boost::intrusive::treap::cend()const
   const_iterator cend() const;

   //! @copydoc ::boost::intrusive::treap::rbegin()
   reverse_iterator rbegin();

   //! @copydoc ::boost::intrusive::treap::rbegin()const
   const_reverse_iterator rbegin() const;

   //! @copydoc ::boost::intrusive::treap::crbegin()const
   const_reverse_iterator crbegin() const;

   //! @copydoc ::boost::intrusive::treap::rend()
   reverse_iterator rend();

   //! @copydoc ::boost::intrusive::treap::rend()const
   const_reverse_iterator rend() const;

   //! @copydoc ::boost::intrusive::treap::crend()const
   const_reverse_iterator crend() const;

   //! @copydoc ::boost::intrusive::treap::root()
   iterator root();

   //! @copydoc ::boost::intrusive::treap::root()const
   const_iterator root() const;

   //! @copydoc ::boost::intrusive::treap::croot()const
   const_iterator croot() const;

   //! @copydoc ::boost::intrusive::treap::container_from_end_iterator(iterator)
   static treap_multiset_impl &container_from_end_iterator(iterator end_iterator);

   //! @copydoc ::boost::intrusive::treap::container_from_end_iterator(const_iterator)
   static const treap_multiset_impl &container_from_end_iterator(const_iterator end_iterator);

   //! @copydoc ::boost::intrusive::treap::container_from_iterator(iterator)
   static treap_multiset_impl &container_from_iterator(iterator it);

   //! @copydoc ::boost::intrusive::treap::container_from_iterator(const_iterator)
   static const treap_multiset_impl &container_from_iterator(const_iterator it);

   //! @copydoc ::boost::intrusive::treap::key_comp()const
   key_compare key_comp() const;

   //! @copydoc ::boost::intrusive::treap::value_comp()const
   value_compare value_comp() const;

   //! @copydoc ::boost::intrusive::treap::empty()const
   bool empty() const;

   //! @copydoc ::boost::intrusive::treap::size()const
   size_type size() const;

   //! @copydoc ::boost::intrusive::treap::swap
   void swap(treap_multiset_impl& other);

   //! @copydoc ::boost::intrusive::treap::clone_from(const treap&,Cloner,Disposer)
   template <class Cloner, class Disposer>
   void clone_from(const treap_multiset_impl &src, Cloner cloner, Disposer disposer);

   #else

   using tree_type::clone_from;

   #endif

   //! @copydoc ::boost::intrusive::treap::clone_from(treap&&,Cloner,Disposer)
   template <class Cloner, class Disposer>
   void clone_from(BOOST_RV_REF(treap_multiset_impl) src, Cloner cloner, Disposer disposer)
   {  tree_type::clone_from(BOOST_MOVE_BASE(tree_type, src), cloner, disposer);  }

   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)

   //! @copydoc ::boost::intrusive::treap::top()
   iterator top();

   //! @copydoc ::boost::intrusive::treap::top()const
   const_iterator top() const;

   //! @copydoc ::boost::intrusive::treap::ctop()const
   const_iterator ctop() const;

   //! @copydoc ::boost::intrusive::treap::rtop()
   reverse_iterator rtop();

   //! @copydoc ::boost::intrusive::treap::rtop()const
   const_reverse_iterator rtop() const;

   //! @copydoc ::boost::intrusive::treap::crtop()const
   const_reverse_iterator crtop() const;

   //! @copydoc ::boost::intrusive::treap::crtop() const
   priority_compare priority_comp() const;
   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::treap::insert_equal(reference)
   iterator insert(reference value)
   {  return tree_type::insert_equal(value);  }

   //! @copydoc ::boost::intrusive::treap::insert_equal(const_iterator,reference)
   iterator insert(const_iterator hint, reference value)
   {  return tree_type::insert_equal(hint, value);  }

   //! @copydoc ::boost::intrusive::treap::insert_equal(Iterator,Iterator)
   template<class Iterator>
   void insert(Iterator b, Iterator e)
   {  tree_type::insert_equal(b, e);  }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::treap::insert_before
   iterator insert_before(const_iterator pos, reference value);

   //! @copydoc ::boost::intrusive::treap::push_back
   void push_back(reference value);

   //! @copydoc ::boost::intrusive::treap::push_front
   void push_front(reference value);

   //! @copydoc ::boost::intrusive::treap::erase(const_iterator)
   iterator erase(const_iterator i);

   //! @copydoc ::boost::intrusive::treap::erase(const_iterator,const_iterator)
   iterator erase(const_iterator b, const_iterator e);

   //! @copydoc ::boost::intrusive::treap::erase(const key_type &)
   size_type erase(const key_type &key);

   //! @copydoc ::boost::intrusive::treap::erase(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   size_type erase(const KeyType& key, KeyTypeKeyCompare comp);

   //! @copydoc ::boost::intrusive::treap::erase_and_dispose(const_iterator,Disposer)
   template<class Disposer>
   iterator erase_and_dispose(const_iterator i, Disposer disposer);

   //! @copydoc ::boost::intrusive::treap::erase_and_dispose(const_iterator,const_iterator,Disposer)
   template<class Disposer>
   iterator erase_and_dispose(const_iterator b, const_iterator e, Disposer disposer);

   //! @copydoc ::boost::intrusive::treap::erase_and_dispose(const key_type &, Disposer)
   template<class Disposer>
   size_type erase_and_dispose(const key_type &key, Disposer disposer);

   //! @copydoc ::boost::intrusive::treap::erase_and_dispose(const KeyType&,KeyTypeKeyCompare,Disposer)
   template<class KeyType, class KeyTypeKeyCompare, class Disposer>
   size_type erase_and_dispose(const KeyType& key, KeyTypeKeyCompare comp, Disposer disposer);

   //! @copydoc ::boost::intrusive::treap::clear
   void clear();

   //! @copydoc ::boost::intrusive::treap::clear_and_dispose
   template<class Disposer>
   void clear_and_dispose(Disposer disposer);

   //! @copydoc ::boost::intrusive::treap::count(const key_type &)const
   size_type count(const key_type &key) const;

   //! @copydoc ::boost::intrusive::treap::count(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   size_type count(const KeyType& key, KeyTypeKeyCompare comp) const;

   //! @copydoc ::boost::intrusive::treap::lower_bound(const key_type &)
   iterator lower_bound(const key_type &key);

   //! @copydoc ::boost::intrusive::treap::lower_bound(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   iterator lower_bound(const KeyType& key, KeyTypeKeyCompare comp);

   //! @copydoc ::boost::intrusive::treap::lower_bound(const key_type &)const
   const_iterator lower_bound(const key_type &key) const;

   //! @copydoc ::boost::intrusive::treap::lower_bound(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   const_iterator lower_bound(const KeyType& key, KeyTypeKeyCompare comp) const;

   //! @copydoc ::boost::intrusive::treap::upper_bound(const key_type &)
   iterator upper_bound(const key_type &key);

   //! @copydoc ::boost::intrusive::treap::upper_bound(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   iterator upper_bound(const KeyType& key, KeyTypeKeyCompare comp);

   //! @copydoc ::boost::intrusive::treap::upper_bound(const key_type &)const
   const_iterator upper_bound(const key_type &key) const;

   //! @copydoc ::boost::intrusive::treap::upper_bound(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   const_iterator upper_bound(const KeyType& key, KeyTypeKeyCompare comp) const;

   //! @copydoc ::boost::intrusive::treap::find(const key_type &)
   iterator find(const key_type &key);

   //! @copydoc ::boost::intrusive::treap::find(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   iterator find(const KeyType& key, KeyTypeKeyCompare comp);

   //! @copydoc ::boost::intrusive::treap::find(const key_type &)const
   const_iterator find(const key_type &key) const;

   //! @copydoc ::boost::intrusive::treap::find(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   const_iterator find(const KeyType& key, KeyTypeKeyCompare comp) const;

   //! @copydoc ::boost::intrusive::treap::equal_range(const key_type &)
   std::pair<iterator,iterator> equal_range(const key_type &key);

   //! @copydoc ::boost::intrusive::treap::equal_range(const KeyType&,KeyTypeKeyCompare)
   template<class KeyType, class KeyTypeKeyCompare>
   std::pair<iterator,iterator> equal_range(const KeyType& key, KeyTypeKeyCompare comp);

   //! @copydoc ::boost::intrusive::treap::equal_range(const key_type &)const
   std::pair<const_iterator, const_iterator>
      equal_range(const key_type &key) const;

   //! @copydoc ::boost::intrusive::treap::equal_range(const KeyType&,KeyTypeKeyCompare)const
   template<class KeyType, class KeyTypeKeyCompare>
   std::pair<const_iterator, const_iterator>
      equal_range(const KeyType& key, KeyTypeKeyCompare comp) const;

   //! @copydoc ::boost::intrusive::treap::bounded_range(const key_type &,const key_type &,bool,bool)
   std::pair<iterator,iterator> bounded_range
      (const key_type &lower_key, const key_type &upper_key, bool left_closed, bool right_closed);

   //! @copydoc ::boost::intrusive::treap::bounded_range(const KeyType&,const KeyType&,KeyTypeKeyCompare,bool,bool)
   template<class KeyType, class KeyTypeKeyCompare>
   std::pair<iterator,iterator> bounded_range
      (const KeyType& lower_key, const KeyType& upper_key, KeyTypeKeyCompare comp, bool left_closed, bool right_closed);

   //! @copydoc ::boost::intrusive::treap::bounded_range(const key_type &,const key_type &,bool,bool)const
   std::pair<const_iterator, const_iterator>
      bounded_range(const key_type &lower_key, const key_type &upper_key, bool left_closed, bool right_closed) const;

   //! @copydoc ::boost::intrusive::treap::bounded_range(const KeyType&,const KeyType&,KeyTypeKeyCompare,bool,bool)const
   template<class KeyType, class KeyTypeKeyCompare>
   std::pair<const_iterator, const_iterator> bounded_range
         (const KeyType& lower_key, const KeyType& upper_key, KeyTypeKeyCompare comp, bool left_closed, bool right_closed) const;

   //! @copydoc ::boost::intrusive::treap::s_iterator_to(reference)
   static iterator s_iterator_to(reference value);

   //! @copydoc ::boost::intrusive::treap::s_iterator_to(const_reference)
   static const_iterator s_iterator_to(const_reference value);

   //! @copydoc ::boost::intrusive::treap::iterator_to(reference)
   iterator iterator_to(reference value);

   //! @copydoc ::boost::intrusive::treap::iterator_to(const_reference)const
   const_iterator iterator_to(const_reference value) const;

   //! @copydoc ::boost::intrusive::treap::init_node(reference)
   static void init_node(reference value);

   //! @copydoc ::boost::intrusive::treap::unlink_leftmost_without_rebalance
   pointer unlink_leftmost_without_rebalance();

   //! @copydoc ::boost::intrusive::treap::replace_node
   void replace_node(iterator replace_this, reference with_this);

   //! @copydoc ::boost::intrusive::treap::remove_node
   void remove_node(reference value);

   //! @copydoc ::boost::intrusive::treap::merge_unique
   template<class ...Options2>
   void merge(treap_multiset<T, Options2...> &source);

   //! @copydoc ::boost::intrusive::treap::merge_unique
   template<class ...Options2>
   void merge(treap_set<T, Options2...> &source);

   #else

   template<class Compare2>
   void merge(treap_multiset_impl<ValueTraits, VoidOrKeyOfValue, Compare2, VoidOrPrioOfValue, VoidOrPrioComp, SizeType, ConstantTimeSize, HeaderHolder> &source)
   {  return tree_type::merge_equal(source);  }

   template<class Compare2>
   void merge(treap_set_impl<ValueTraits, VoidOrKeyOfValue, Compare2, VoidOrPrioOfValue, VoidOrPrioComp, SizeType, ConstantTimeSize, HeaderHolder> &source)
   {  return tree_type::merge_equal(source);  }

   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
};


//! Helper metafunction to define a \c treap_multiset that yields to the same type when the
//! same options (either explicitly or implicitly) are used.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class ...Options>
#else
template<class T, class O1 = void, class O2 = void
                , class O3 = void, class O4 = void
                , class O5 = void, class O6 = void
                , class O7 = void>
#endif
struct make_treap_multiset
{
   typedef typename pack_options
      < treap_defaults,
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      O1, O2, O3, O4, O5, O6, O7
      #else
      Options...
      #endif
      >::type packed_options;

   typedef typename detail::get_value_traits
      <T, typename packed_options::proto_value_traits>::type value_traits;

   typedef treap_multiset_impl
         < value_traits
         , typename packed_options::key_of_value
         , typename packed_options::compare
         , typename packed_options::priority_of_value
         , typename packed_options::priority
         , typename packed_options::size_type
         , packed_options::constant_time_size
         , typename packed_options::header_holder_type
         > implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class O1, class O2, class O3, class O4, class O5, class O6, class O7>
#else
template<class T, class ...Options>
#endif
class treap_multiset
   :  public make_treap_multiset<T,
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      O1, O2, O3, O4, O5, O6, O7
      #else
      Options...
      #endif
      >::type
{
   typedef typename make_treap_multiset
      <T,
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      O1, O2, O3, O4, O5, O6, O7
      #else
      Options...
      #endif
      >::type   Base;
   BOOST_MOVABLE_BUT_NOT_COPYABLE(treap_multiset)

   public:
   typedef typename Base::key_compare        key_compare;
   typedef typename Base::priority_compare   priority_compare;
   typedef typename Base::value_traits       value_traits;
   typedef typename Base::iterator           iterator;
   typedef typename Base::const_iterator     const_iterator;

   //Assert if passed value traits are compatible with the type
   BOOST_STATIC_ASSERT((detail::is_same<typename value_traits::value_type, T>::value));

   BOOST_INTRUSIVE_FORCEINLINE treap_multiset()
      :  Base()
   {}

   BOOST_INTRUSIVE_FORCEINLINE explicit treap_multiset( const key_compare &cmp
                          , const priority_compare &pcmp = priority_compare()
                          , const value_traits &v_traits = value_traits())
      :  Base(cmp, pcmp, v_traits)
   {}

   template<class Iterator>
   BOOST_INTRUSIVE_FORCEINLINE treap_multiset( Iterator b, Iterator e
      , const key_compare &cmp = key_compare()
      , const priority_compare &pcmp = priority_compare()
      , const value_traits &v_traits = value_traits())
      :  Base(b, e, cmp, pcmp, v_traits)
   {}

   BOOST_INTRUSIVE_FORCEINLINE treap_multiset(BOOST_RV_REF(treap_multiset) x)
      :  Base(BOOST_MOVE_BASE(Base, x))
   {}

   BOOST_INTRUSIVE_FORCEINLINE treap_multiset& operator=(BOOST_RV_REF(treap_multiset) x)
   {  return static_cast<treap_multiset &>(this->Base::operator=(BOOST_MOVE_BASE(Base, x)));  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(const treap_multiset &src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(src, cloner, disposer);  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(BOOST_RV_REF(treap_multiset) src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(BOOST_MOVE_BASE(Base, src), cloner, disposer);  }

   BOOST_INTRUSIVE_FORCEINLINE static treap_multiset &container_from_end_iterator(iterator end_iterator)
   {  return static_cast<treap_multiset &>(Base::container_from_end_iterator(end_iterator));   }

   BOOST_INTRUSIVE_FORCEINLINE static const treap_multiset &container_from_end_iterator(const_iterator end_iterator)
   {  return static_cast<const treap_multiset &>(Base::container_from_end_iterator(end_iterator));   }

   BOOST_INTRUSIVE_FORCEINLINE static treap_multiset &container_from_iterator(iterator it)
   {  return static_cast<treap_multiset &>(Base::container_from_iterator(it));   }

   BOOST_INTRUSIVE_FORCEINLINE static const treap_multiset &container_from_iterator(const_iterator it)
   {  return static_cast<const treap_multiset &>(Base::container_from_iterator(it));   }
};

#endif

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_TREAP_SET_HPP
