/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Olaf Krzikalla 2004-2006.
// (C) Copyright Ion Gaztanaga  2006-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_SLIST_HPP
#define BOOST_INTRUSIVE_SLIST_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>

#include <boost/intrusive/detail/assert.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <boost/intrusive/circular_slist_algorithms.hpp>
#include <boost/intrusive/linear_slist_algorithms.hpp>
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/detail/get_value_traits.hpp>
#include <boost/intrusive/detail/is_stateful_value_traits.hpp>
#include <boost/intrusive/detail/default_header_holder.hpp>
#include <boost/intrusive/detail/uncast.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/intrusive/detail/iterator.hpp>
#include <boost/intrusive/detail/slist_iterator.hpp>
#include <boost/intrusive/detail/array_initializer.hpp>
#include <boost/intrusive/detail/exception_disposer.hpp>
#include <boost/intrusive/detail/equal_to_value.hpp>
#include <boost/intrusive/detail/key_nodeptr_comp.hpp>
#include <boost/intrusive/detail/simple_disposers.hpp>
#include <boost/intrusive/detail/size_holder.hpp>
#include <boost/intrusive/detail/algorithm.hpp>

#include <boost/move/utility_core.hpp>
#include <boost/static_assert.hpp>

#include <boost/intrusive/detail/minimal_less_equal_header.hpp>//std::less
#include <cstddef>   //std::size_t
#include <boost/intrusive/detail/minimal_pair_header.hpp>   //std::pair

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

/// @cond

template<class HeaderHolder, class NodePtr, bool>
struct header_holder_plus_last
{
   HeaderHolder header_holder_;
   NodePtr  last_;
};

template<class HeaderHolder, class NodePtr>
struct header_holder_plus_last<HeaderHolder, NodePtr, false>
{
   HeaderHolder header_holder_;
};

struct default_slist_hook_applier
{  template <class T> struct apply{ typedef typename T::default_slist_hook type;  };  };

template<>
struct is_default_hook_tag<default_slist_hook_applier>
{  static const bool value = true;  };

struct slist_defaults
{
   typedef default_slist_hook_applier proto_value_traits;
   static const bool constant_time_size = true;
   static const bool linear = false;
   typedef std::size_t size_type;
   static const bool cache_last = false;
   typedef void header_holder_type;
};

struct slist_bool_flags
{
   static const std::size_t linear_pos             = 1u;
   static const std::size_t constant_time_size_pos = 2u;
   static const std::size_t cache_last_pos         = 4u;
};


/// @endcond

//! The class template slist is an intrusive container, that encapsulates
//! a singly-linked list. You can use such a list to squeeze the last bit
//! of performance from your application. Unfortunately, the little gains
//! come with some huge drawbacks. A lot of member functions can't be
//! implemented as efficiently as for standard containers. To overcome
//! this limitation some other member functions with rather unusual semantics
//! have to be introduced.
//!
//! The template parameter \c T is the type to be managed by the container.
//! The user can specify additional options and if no options are provided
//! default options are used.
//!
//! The container supports the following options:
//! \c base_hook<>/member_hook<>/value_traits<>,
//! \c constant_time_size<>, \c size_type<>,
//! \c linear<> and \c cache_last<>.
//!
//! The iterators of slist are forward iterators. slist provides a static
//! function called "previous" to compute the previous iterator of a given iterator.
//! This function has linear complexity. To improve the usability esp. with
//! the '*_after' functions, ++end() == begin() and previous(begin()) == end()
//! are defined. An new special function "before_begin()" is defined, which returns
//! an iterator that points one less the beginning of the list: ++before_begin() == begin()
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
template<class T, class ...Options>
#else
template<class ValueTraits, class SizeType, std::size_t BoolFlags, typename HeaderHolder>
#endif
class slist_impl
{
   //Public typedefs
   public:
   typedef ValueTraits                                               value_traits;
   typedef typename value_traits::pointer                            pointer;
   typedef typename value_traits::const_pointer                      const_pointer;
   typedef typename pointer_traits<pointer>::element_type            value_type;
   typedef typename pointer_traits<pointer>::reference               reference;
   typedef typename pointer_traits<const_pointer>::reference         const_reference;
   typedef typename pointer_traits<pointer>::difference_type         difference_type;
   typedef SizeType                                                  size_type;
   typedef slist_iterator<value_traits, false>                       iterator;
   typedef slist_iterator<value_traits, true>                        const_iterator;
   typedef typename value_traits::node_traits                        node_traits;
   typedef typename node_traits::node                                node;
   typedef typename node_traits::node_ptr                            node_ptr;
   typedef typename node_traits::const_node_ptr                      const_node_ptr;
   typedef typename detail::get_header_holder_type
      < value_traits, HeaderHolder >::type                           header_holder_type;

   static const bool constant_time_size = 0 != (BoolFlags & slist_bool_flags::constant_time_size_pos);
   static const bool stateful_value_traits = detail::is_stateful_value_traits<value_traits>::value;
   static const bool linear = 0 != (BoolFlags & slist_bool_flags::linear_pos);
   static const bool cache_last = 0 != (BoolFlags & slist_bool_flags::cache_last_pos);
   static const bool has_container_from_iterator =
        detail::is_same< header_holder_type, detail::default_header_holder< node_traits > >::value;

   typedef typename detail::if_c
      < linear
      , linear_slist_algorithms<node_traits>
      , circular_slist_algorithms<node_traits>
      >::type                                                        node_algorithms;

   /// @cond
   private:
   typedef detail::size_holder<constant_time_size, size_type>        size_traits;

   //noncopyable
   BOOST_MOVABLE_BUT_NOT_COPYABLE(slist_impl)

   static const bool safemode_or_autounlink = is_safe_autounlink<value_traits::link_mode>::value;

   //Constant-time size is incompatible with auto-unlink hooks!
   BOOST_STATIC_ASSERT(!(constant_time_size && ((int)value_traits::link_mode == (int)auto_unlink)));
   //Linear singly linked lists are incompatible with auto-unlink hooks!
   BOOST_STATIC_ASSERT(!(linear && ((int)value_traits::link_mode == (int)auto_unlink)));
   //A list with cached last node is incompatible with auto-unlink hooks!
   BOOST_STATIC_ASSERT(!(cache_last && ((int)value_traits::link_mode == (int)auto_unlink)));

   node_ptr get_end_node()
   {  return node_ptr(linear ? node_ptr() : this->get_root_node());  }

   const_node_ptr get_end_node() const
   {
      return const_node_ptr
         (linear ? const_node_ptr() : this->get_root_node());  }

   node_ptr get_root_node()
   { return data_.root_plus_size_.header_holder_.get_node(); }

   const_node_ptr get_root_node() const
   { return data_.root_plus_size_.header_holder_.get_node(); }

   node_ptr get_last_node()
   {  return this->get_last_node(detail::bool_<cache_last>());  }

   const_node_ptr get_last_node() const
   {  return this->get_last_node(detail::bool_<cache_last>());  }

   void set_last_node(const node_ptr &n)
   {  return this->set_last_node(n, detail::bool_<cache_last>());  }

   static node_ptr get_last_node(detail::bool_<false>)
   {
      //This function shall not be used if cache_last is not true
      BOOST_INTRUSIVE_INVARIANT_ASSERT(cache_last);
      return node_ptr();
   }

   static void set_last_node(const node_ptr &, detail::bool_<false>)
   {
      //This function shall not be used if cache_last is not true
      BOOST_INTRUSIVE_INVARIANT_ASSERT(cache_last);
   }

   node_ptr get_last_node(detail::bool_<true>)
   {  return node_ptr(data_.root_plus_size_.last_);  }

   const_node_ptr get_last_node(detail::bool_<true>) const
   {  return const_node_ptr(data_.root_plus_size_.last_);  }

   void set_last_node(const node_ptr & n, detail::bool_<true>)
   {  data_.root_plus_size_.last_ = n;  }

   void set_default_constructed_state()
   {
      node_algorithms::init_header(this->get_root_node());
      this->priv_size_traits().set_size(size_type(0));
      if(cache_last){
         this->set_last_node(this->get_root_node());
      }
   }

   typedef header_holder_plus_last<header_holder_type, node_ptr, cache_last> header_holder_plus_last_t;
   struct root_plus_size
      :  public size_traits
      ,  public header_holder_plus_last_t
   {};

   struct data_t
      :  public value_traits
   {
      typedef typename slist_impl::value_traits value_traits;
      explicit data_t(const value_traits &val_traits)
         :  value_traits(val_traits)
      {}

      root_plus_size root_plus_size_;
   } data_;

   size_traits &priv_size_traits()
   {  return data_.root_plus_size_;  }

   const size_traits &priv_size_traits() const
   {  return data_.root_plus_size_;  }

   const value_traits &priv_value_traits() const
   {  return data_;  }

   value_traits &priv_value_traits()
   {  return data_;  }

   typedef typename boost::intrusive::value_traits_pointers
      <ValueTraits>::const_value_traits_ptr const_value_traits_ptr;

   const_value_traits_ptr priv_value_traits_ptr() const
   {  return pointer_traits<const_value_traits_ptr>::pointer_to(this->priv_value_traits());  }

   /// @endcond

   public:

   ///@cond

   //! <b>Requires</b>: f and before_l belong to another slist.
   //!
   //! <b>Effects</b>: Transfers the range [f, before_l] to this
   //!   list, after the element pointed by prev_pos.
   //!   No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements transferred
   //!   if constant_time_size is true. Constant-time otherwise.
   //!
   //! <b>Note</b>: Iterators of values obtained from list x now point to elements of this
   //!   list. Iterators of this list and all the references are not invalidated.
   //!
   //! <b>Warning</b>: Experimental function, don't use it!
   slist_impl( const node_ptr & f, const node_ptr & before_l
             , size_type n, const value_traits &v_traits = value_traits())
      :  data_(v_traits)
   {
      if(n){
         this->priv_size_traits().set_size(n);
         if(cache_last){
            this->set_last_node(before_l);
         }
         node_traits::set_next(this->get_root_node(), f);
         node_traits::set_next(before_l, this->get_end_node());
      }
      else{
         this->set_default_constructed_state();
      }
   }

   ///@endcond

   //! <b>Effects</b>: constructs an empty list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: If value_traits::node_traits::node
   //!   constructor throws (this does not happen with predefined Boost.Intrusive hooks).
   slist_impl()
      :  data_(value_traits())
   {  this->set_default_constructed_state(); }

   //! <b>Effects</b>: constructs an empty list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: If value_traits::node_traits::node
   //!   constructor throws (this does not happen with predefined Boost.Intrusive hooks).
   explicit slist_impl(const value_traits &v_traits)
      :  data_(v_traits)
   {  this->set_default_constructed_state(); }

   //! <b>Requires</b>: Dereferencing iterator must yield an lvalue of type value_type.
   //!
   //! <b>Effects</b>: Constructs a list equal to [b ,e).
   //!
   //! <b>Complexity</b>: Linear in distance(b, e). No copy constructors are called.
   //!
   //! <b>Throws</b>: If value_traits::node_traits::node
   //!   constructor throws (this does not happen with predefined Boost.Intrusive hooks).
   template<class Iterator>
   slist_impl(Iterator b, Iterator e, const value_traits &v_traits = value_traits())
      :  data_(v_traits)
   {
      this->set_default_constructed_state();
      //nothrow, no need to rollback to release elements on exception
      this->insert_after(this->cbefore_begin(), b, e);
   }

   //! <b>Effects</b>: Constructs a container moving resources from another container.
   //!   Internal value traits are move constructed and
   //!   nodes belonging to x (except the node representing the "end") are linked to *this.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: If value_traits::node_traits::node's
   //!   move constructor throws (this does not happen with predefined Boost.Intrusive hooks)
   //!   or the move constructor of value traits throws.
   slist_impl(BOOST_RV_REF(slist_impl) x)
      : data_(::boost::move(x.priv_value_traits()))
   {
      this->set_default_constructed_state();
      //nothrow, no need to rollback to release elements on exception
      this->swap(x);
   }

   //! <b>Effects</b>: Equivalent to swap
   //!
   slist_impl& operator=(BOOST_RV_REF(slist_impl) x)
   {  this->swap(x); return *this;  }

   //! <b>Effects</b>: If it's a safe-mode
   //!   or auto-unlink value, the destructor does nothing
   //!   (ie. no code is generated). Otherwise it detaches all elements from this.
   //!   In this case the objects in the list are not deleted (i.e. no destructors
   //!   are called), but the hooks according to the value_traits template parameter
   //!   are set to their default value.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the list, if
   //!   it's a safe-mode or auto-unlink value. Otherwise constant.
   ~slist_impl()
   {
      if(is_safe_autounlink<ValueTraits::link_mode>::value){
         this->clear();
         node_algorithms::init(this->get_root_node());
      }
   }

   //! <b>Effects</b>: Erases all the elements of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements of the list.
   //!   if it's a safe-mode or auto-unlink value_type. Constant time otherwise.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the erased elements.
   void clear()
   {
      if(safemode_or_autounlink){
         this->clear_and_dispose(detail::null_disposer());
      }
      else{
         this->set_default_constructed_state();
      }
   }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Erases all the elements of the container
   //!   Disposer::operator()(pointer) is called for the removed elements.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements of the list.
   //!
   //! <b>Note</b>: Invalidates the iterators to the erased elements.
   template <class Disposer>
   void clear_and_dispose(Disposer disposer)
   {
      const_iterator it(this->begin()), itend(this->end());
      while(it != itend){
         node_ptr to_erase(it.pointed_node());
         ++it;
         if(safemode_or_autounlink)
            node_algorithms::init(to_erase);
         disposer(priv_value_traits().to_value_ptr(to_erase));
      }
      this->set_default_constructed_state();
   }

   //! <b>Requires</b>: value must be an lvalue.
   //!
   //! <b>Effects</b>: Inserts the value in the front of the list.
   //!   No copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   void push_front(reference value)
   {
      node_ptr to_insert = priv_value_traits().to_node_ptr(value);
      BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT(!safemode_or_autounlink || node_algorithms::inited(to_insert));
      if(cache_last){
         if(this->empty()){
            this->set_last_node(to_insert);
         }
      }
      node_algorithms::link_after(this->get_root_node(), to_insert);
      this->priv_size_traits().increment();
   }

   //! <b>Requires</b>: value must be an lvalue.
   //!
   //! <b>Effects</b>: Inserts the value in the back of the list.
   //!   No copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   //!   This function is only available is cache_last<> is true.
   void push_back(reference value)
   {
      BOOST_STATIC_ASSERT((cache_last));
      node_ptr n = priv_value_traits().to_node_ptr(value);
      BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT(!safemode_or_autounlink || node_algorithms::inited(n));
      node_algorithms::link_after(this->get_last_node(), n);
      if(cache_last){
         this->set_last_node(n);
      }
      this->priv_size_traits().increment();
   }

   //! <b>Effects</b>: Erases the first element of the list.
   //!   No destructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the erased element.
   void pop_front()
   {  return this->pop_front_and_dispose(detail::null_disposer());   }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Erases the first element of the list.
   //!   Disposer::operator()(pointer) is called for the removed element.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Invalidates the iterators to the erased element.
   template<class Disposer>
   void pop_front_and_dispose(Disposer disposer)
   {
      node_ptr to_erase = node_traits::get_next(this->get_root_node());
      node_algorithms::unlink_after(this->get_root_node());
      this->priv_size_traits().decrement();
      if(safemode_or_autounlink)
         node_algorithms::init(to_erase);
      disposer(priv_value_traits().to_value_ptr(to_erase));
      if(cache_last){
         if(this->empty()){
            this->set_last_node(this->get_root_node());
         }
      }
   }

   //! <b>Effects</b>: Returns a reference to the first element of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   reference front()
   { return *this->priv_value_traits().to_value_ptr(node_traits::get_next(this->get_root_node())); }

   //! <b>Effects</b>: Returns a const_reference to the first element of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   const_reference front() const
   { return *this->priv_value_traits().to_value_ptr(detail::uncast(node_traits::get_next(this->get_root_node()))); }

   //! <b>Effects</b>: Returns a reference to the last element of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   //!   This function is only available is cache_last<> is true.
   reference back()
   {
      BOOST_STATIC_ASSERT((cache_last));
      return *this->priv_value_traits().to_value_ptr(this->get_last_node());
   }

   //! <b>Effects</b>: Returns a const_reference to the last element of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   //!   This function is only available is cache_last<> is true.
   const_reference back() const
   {
      BOOST_STATIC_ASSERT((cache_last));
      return *this->priv_value_traits().to_value_ptr(this->get_last_node());
   }

   //! <b>Effects</b>: Returns an iterator to the first element contained in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   iterator begin()
   { return iterator (node_traits::get_next(this->get_root_node()), this->priv_value_traits_ptr()); }

   //! <b>Effects</b>: Returns a const_iterator to the first element contained in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   const_iterator begin() const
   { return const_iterator (node_traits::get_next(this->get_root_node()), this->priv_value_traits_ptr()); }

   //! <b>Effects</b>: Returns a const_iterator to the first element contained in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   const_iterator cbegin() const
   { return const_iterator(node_traits::get_next(this->get_root_node()), this->priv_value_traits_ptr()); }

   //! <b>Effects</b>: Returns an iterator to the end of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   iterator end()
   { return iterator(this->get_end_node(), this->priv_value_traits_ptr()); }

   //! <b>Effects</b>: Returns a const_iterator to the end of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   const_iterator end() const
   { return const_iterator(detail::uncast(this->get_end_node()), this->priv_value_traits_ptr()); }

   //! <b>Effects</b>: Returns a const_iterator to the end of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   const_iterator cend() const
   { return this->end(); }

   //! <b>Effects</b>: Returns an iterator that points to a position
   //!   before the first element. Equivalent to "end()"
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   iterator before_begin()
   { return iterator(this->get_root_node(), this->priv_value_traits_ptr()); }

   //! <b>Effects</b>: Returns an iterator that points to a position
   //!   before the first element. Equivalent to "end()"
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   const_iterator before_begin() const
   { return const_iterator(detail::uncast(this->get_root_node()), this->priv_value_traits_ptr()); }

   //! <b>Effects</b>: Returns an iterator that points to a position
   //!   before the first element. Equivalent to "end()"
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   const_iterator cbefore_begin() const
   { return this->before_begin(); }

   //! <b>Effects</b>: Returns an iterator to the last element contained in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: This function is present only if cached_last<> option is true.
   iterator last()
   {
      //This function shall not be used if cache_last is not true
      BOOST_INTRUSIVE_INVARIANT_ASSERT(cache_last);
      return iterator (this->get_last_node(), this->priv_value_traits_ptr());
   }

   //! <b>Effects</b>: Returns a const_iterator to the last element contained in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: This function is present only if cached_last<> option is true.
   const_iterator last() const
   {
      //This function shall not be used if cache_last is not true
      BOOST_INTRUSIVE_INVARIANT_ASSERT(cache_last);
      return const_iterator (this->get_last_node(), this->priv_value_traits_ptr());
   }

   //! <b>Effects</b>: Returns a const_iterator to the last element contained in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: This function is present only if cached_last<> option is true.
   const_iterator clast() const
   { return const_iterator(this->get_last_node(), this->priv_value_traits_ptr()); }

   //! <b>Precondition</b>: end_iterator must be a valid end iterator
   //!   of slist.
   //!
   //! <b>Effects</b>: Returns a const reference to the slist associated to the end iterator
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   static slist_impl &container_from_end_iterator(iterator end_iterator)
   {  return slist_impl::priv_container_from_end_iterator(end_iterator);   }

   //! <b>Precondition</b>: end_iterator must be a valid end const_iterator
   //!   of slist.
   //!
   //! <b>Effects</b>: Returns a const reference to the slist associated to the end iterator
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   static const slist_impl &container_from_end_iterator(const_iterator end_iterator)
   {  return slist_impl::priv_container_from_end_iterator(end_iterator);   }

   //! <b>Effects</b>: Returns the number of the elements contained in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements contained in the list.
   //!   if constant_time_size is false. Constant time otherwise.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   size_type size() const
   {
      if(constant_time_size)
         return this->priv_size_traits().get_size();
      else
         return node_algorithms::count(this->get_root_node()) - 1;
   }

   //! <b>Effects</b>: Returns true if the list contains no elements.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   bool empty() const
   { return node_algorithms::unique(this->get_root_node()); }

   //! <b>Effects</b>: Swaps the elements of x and *this.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements of both lists.
   //!  Constant-time if linear<> and/or cache_last<> options are used.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   void swap(slist_impl& other)
   {
      if(cache_last){
         priv_swap_cache_last(this, &other);
      }
      else{
         this->priv_swap_lists(this->get_root_node(), other.get_root_node(), detail::bool_<linear>());
      }
      this->priv_size_traits().swap(other.priv_size_traits());
   }

   //! <b>Effects</b>: Moves backwards all the elements, so that the first
   //!   element becomes the second, the second becomes the third...
   //!   the last element becomes the first one.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements plus the number shifts.
   //!
   //! <b>Note</b>: Iterators Does not affect the validity of iterators and references.
   void shift_backwards(size_type n = 1)
   {  this->priv_shift_backwards(n, detail::bool_<linear>());  }

   //! <b>Effects</b>: Moves forward all the elements, so that the second
   //!   element becomes the first, the third becomes the second...
   //!   the first element becomes the last one.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements plus the number shifts.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   void shift_forward(size_type n = 1)
   {  this->priv_shift_forward(n, detail::bool_<linear>()); }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!   Cloner should yield to nodes equivalent to the original nodes.
   //!
   //! <b>Effects</b>: Erases all the elements from *this
   //!   calling Disposer::operator()(pointer), clones all the
   //!   elements from src calling Cloner::operator()(const_reference )
   //!   and inserts them on *this.
   //!
   //!   If cloner throws, all cloned elements are unlinked and disposed
   //!   calling Disposer::operator()(pointer).
   //!
   //! <b>Complexity</b>: Linear to erased plus inserted elements.
   //!
   //! <b>Throws</b>: If cloner throws.
   template <class Cloner, class Disposer>
   void clone_from(const slist_impl &src, Cloner cloner, Disposer disposer)
   {
      this->clear_and_dispose(disposer);
      detail::exception_disposer<slist_impl, Disposer>
         rollback(*this, disposer);
      const_iterator prev(this->cbefore_begin());
      const_iterator b(src.begin()), e(src.end());
      for(; b != e; ++b){
         prev = this->insert_after(prev, *cloner(*b));
      }
      rollback.release();
   }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!   Cloner should yield to nodes equivalent to the original nodes.
   //!
   //! <b>Effects</b>: Erases all the elements from *this
   //!   calling Disposer::operator()(pointer), clones all the
   //!   elements from src calling Cloner::operator()(reference)
   //!   and inserts them on *this.
   //!
   //!   If cloner throws, all cloned elements are unlinked and disposed
   //!   calling Disposer::operator()(pointer).
   //!
   //! <b>Complexity</b>: Linear to erased plus inserted elements.
   //!
   //! <b>Throws</b>: If cloner throws.
   template <class Cloner, class Disposer>
   void clone_from(BOOST_RV_REF(slist_impl) src, Cloner cloner, Disposer disposer)
   {
      this->clear_and_dispose(disposer);
      detail::exception_disposer<slist_impl, Disposer>
         rollback(*this, disposer);
      iterator prev(this->cbefore_begin());
      iterator b(src.begin()), e(src.end());
      for(; b != e; ++b){
         prev = this->insert_after(prev, *cloner(*b));
      }
      rollback.release();
   }

   //! <b>Requires</b>: value must be an lvalue and prev_p must point to an element
   //!   contained by the list or to end().
   //!
   //! <b>Effects</b>: Inserts the value after the position pointed by prev_p.
   //!    No copy constructor is called.
   //!
   //! <b>Returns</b>: An iterator to the inserted element.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   iterator insert_after(const_iterator prev_p, reference value)
   {
      node_ptr n = priv_value_traits().to_node_ptr(value);
      BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT(!safemode_or_autounlink || node_algorithms::inited(n));
      node_ptr prev_n(prev_p.pointed_node());
      node_algorithms::link_after(prev_n, n);
      if(cache_last && (this->get_last_node() == prev_n)){
         this->set_last_node(n);
      }
      this->priv_size_traits().increment();
      return iterator (n, this->priv_value_traits_ptr());
   }

   //! <b>Requires</b>: Dereferencing iterator must yield
   //!   an lvalue of type value_type and prev_p must point to an element
   //!   contained by the list or to the end node.
   //!
   //! <b>Effects</b>: Inserts the [f, l)
   //!   after the position prev_p.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements inserted.
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   template<class Iterator>
   void insert_after(const_iterator prev_p, Iterator f, Iterator l)
   {
      //Insert first nodes avoiding cache and size checks
      size_type count = 0;
      node_ptr prev_n(prev_p.pointed_node());
      for (; f != l; ++f, ++count){
         const node_ptr n = priv_value_traits().to_node_ptr(*f);
         BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT(!safemode_or_autounlink || node_algorithms::inited(n));
         node_algorithms::link_after(prev_n, n);
         prev_n = n;
      }
      //Now fix special cases if needed
      if(cache_last && (this->get_last_node() == prev_p.pointed_node())){
         this->set_last_node(prev_n);
      }
      if(constant_time_size){
         this->priv_size_traits().increase(count);
      }
   }

   //! <b>Requires</b>: value must be an lvalue and p must point to an element
   //!   contained by the list or to end().
   //!
   //! <b>Effects</b>: Inserts the value before the position pointed by p.
   //!   No copy constructor is called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements before p.
   //!  Constant-time if cache_last<> is true and p == end().
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   iterator insert(const_iterator p, reference value)
   {  return this->insert_after(this->previous(p), value);  }

   //! <b>Requires</b>: Dereferencing iterator must yield
   //!   an lvalue of type value_type and p must point to an element
   //!   contained by the list or to the end node.
   //!
   //! <b>Effects</b>: Inserts the pointed by b and e
   //!   before the position p. No copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements inserted plus linear
   //!   to the elements before b.
   //!   Linear to the number of elements to insert if cache_last<> option is true and p == end().
   //!
   //! <b>Note</b>: Does not affect the validity of iterators and references.
   template<class Iterator>
   void insert(const_iterator p, Iterator b, Iterator e)
   {  return this->insert_after(this->previous(p), b, e);  }

   //! <b>Effects</b>: Erases the element after the element pointed by prev of
   //!   the list. No destructors are called.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed elements,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the
   //!   erased element.
   iterator erase_after(const_iterator prev)
   {  return this->erase_after_and_dispose(prev, detail::null_disposer());  }

   //! <b>Effects</b>: Erases the range (before_f, l) from
   //!   the list. No destructors are called.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed elements,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of erased elements if it's a safe-mode
   //!   , auto-unlink value or constant-time size is activated. Constant time otherwise.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the
   //!   erased element.
   iterator erase_after(const_iterator before_f, const_iterator l)
   {
      if(safemode_or_autounlink || constant_time_size){
         return this->erase_after_and_dispose(before_f, l, detail::null_disposer());
      }
      else{
         const node_ptr bfp = before_f.pointed_node();
         const node_ptr lp = l.pointed_node();
         if(cache_last){
            if(lp == this->get_end_node()){
               this->set_last_node(bfp);
            }
         }
         node_algorithms::unlink_after(bfp, lp);
         return l.unconst();
      }
   }

   //! <b>Effects</b>: Erases the range (before_f, l) from
   //!   the list. n must be distance(before_f, l) - 1.
   //!   No destructors are called.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed elements,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: constant-time if link_mode is normal_link.
   //!   Linear to the elements (l - before_f) otherwise.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the
   //!   erased element.
   iterator erase_after(const_iterator before_f, const_iterator l, size_type n)
   {
      BOOST_INTRUSIVE_INVARIANT_ASSERT(node_algorithms::distance((++const_iterator(before_f)).pointed_node(), l.pointed_node()) == n);
      if(safemode_or_autounlink){
         return this->erase_after(before_f, l);
      }
      else{
         const node_ptr bfp = before_f.pointed_node();
         const node_ptr lp = l.pointed_node();
         if(cache_last){
            if((lp == this->get_end_node())){
               this->set_last_node(bfp);
            }
         }
         node_algorithms::unlink_after(bfp, lp);
         if(constant_time_size){
            this->priv_size_traits().decrease(n);
         }
         return l.unconst();
      }
   }

   //! <b>Effects</b>: Erases the element pointed by i of the list.
   //!   No destructors are called.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed element,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the elements before i.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the
   //!   erased element.
   iterator erase(const_iterator i)
   {  return this->erase_after(this->previous(i));  }

   //! <b>Requires</b>: f and l must be valid iterator to elements in *this.
   //!
   //! <b>Effects</b>: Erases the range pointed by b and e.
   //!   No destructors are called.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed elements,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the elements before l.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the
   //!   erased elements.
   iterator erase(const_iterator f, const_iterator l)
   {  return this->erase_after(this->previous(f), l);  }

   //! <b>Effects</b>: Erases the range [f, l) from
   //!   the list. n must be distance(f, l).
   //!   No destructors are called.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed elements,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: linear to the elements before f if link_mode is normal_link
   //!   and constant_time_size is activated. Linear to the elements before l otherwise.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the
   //!   erased element.
   iterator erase(const_iterator f, const_iterator l, size_type n)
   {  return this->erase_after(this->previous(f), l, n);  }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Erases the element after the element pointed by prev of
   //!   the list.
   //!   Disposer::operator()(pointer) is called for the removed element.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed elements,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Invalidates the iterators to the erased element.
   template<class Disposer>
   iterator erase_after_and_dispose(const_iterator prev, Disposer disposer)
   {
      const_iterator it(prev);
      ++it;
      node_ptr to_erase(it.pointed_node());
      ++it;
      node_ptr prev_n(prev.pointed_node());
      node_algorithms::unlink_after(prev_n);
      if(cache_last && (to_erase == this->get_last_node())){
         this->set_last_node(prev_n);
      }
      if(safemode_or_autounlink)
         node_algorithms::init(to_erase);
      disposer(priv_value_traits().to_value_ptr(to_erase));
      this->priv_size_traits().decrement();
      return it.unconst();
   }

   /// @cond

   static iterator s_insert_after(const_iterator const prev_p, reference value)
   {
      BOOST_STATIC_ASSERT(((!cache_last)&&(!constant_time_size)&&(!stateful_value_traits)));
      node_ptr const n = value_traits::to_node_ptr(value);
      BOOST_INTRUSIVE_SAFE_HOOK_DEFAULT_ASSERT(!safemode_or_autounlink || node_algorithms::inited(n));
      node_algorithms::link_after(prev_p.pointed_node(), n);
      return iterator (n, const_value_traits_ptr());
   }

   template<class Disposer>
   static iterator s_erase_after_and_dispose(const_iterator prev, Disposer disposer)
   {
      BOOST_STATIC_ASSERT(((!cache_last)&&(!constant_time_size)&&(!stateful_value_traits)));
      const_iterator it(prev);
      ++it;
      node_ptr to_erase(it.pointed_node());
      ++it;
      node_ptr prev_n(prev.pointed_node());
      node_algorithms::unlink_after(prev_n);
      if(safemode_or_autounlink)
         node_algorithms::init(to_erase);
      disposer(value_traits::to_value_ptr(to_erase));
      return it.unconst();
   }

   template<class Disposer>
   static iterator s_erase_after_and_dispose(const_iterator before_f, const_iterator l, Disposer disposer)
   {
      BOOST_STATIC_ASSERT(((!cache_last)&&(!constant_time_size)&&(!stateful_value_traits)));
      node_ptr bfp(before_f.pointed_node()), lp(l.pointed_node());
      node_ptr fp(node_traits::get_next(bfp));
      node_algorithms::unlink_after(bfp, lp);
      while(fp != lp){
         node_ptr to_erase(fp);
         fp = node_traits::get_next(fp);
         if(safemode_or_autounlink)
            node_algorithms::init(to_erase);
         disposer(value_traits::to_value_ptr(to_erase));
      }
      return l.unconst();
   }

   static iterator s_erase_after(const_iterator prev)
   {  return s_erase_after_and_dispose(prev, detail::null_disposer());  }

   /// @endcond

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Erases the range (before_f, l) from
   //!   the list.
   //!   Disposer::operator()(pointer) is called for the removed elements.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed elements,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the elements (l - before_f + 1).
   //!
   //! <b>Note</b>: Invalidates the iterators to the erased element.
   template<class Disposer>
   iterator erase_after_and_dispose(const_iterator before_f, const_iterator l, Disposer disposer)
   {
      node_ptr bfp(before_f.pointed_node()), lp(l.pointed_node());
      node_ptr fp(node_traits::get_next(bfp));
      node_algorithms::unlink_after(bfp, lp);
      while(fp != lp){
         node_ptr to_erase(fp);
         fp = node_traits::get_next(fp);
         if(safemode_or_autounlink)
            node_algorithms::init(to_erase);
         disposer(priv_value_traits().to_value_ptr(to_erase));
         this->priv_size_traits().decrement();
      }
      if(cache_last && (node_traits::get_next(bfp) == this->get_end_node())){
         this->set_last_node(bfp);
      }
      return l.unconst();
   }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Erases the element pointed by i of the list.
   //!   No destructors are called.
   //!   Disposer::operator()(pointer) is called for the removed element.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed element,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the elements before i.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the
   //!   erased element.
   template<class Disposer>
   iterator erase_and_dispose(const_iterator i, Disposer disposer)
   {  return this->erase_after_and_dispose(this->previous(i), disposer);  }

   #if !defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
   template<class Disposer>
   iterator erase_and_dispose(iterator i, Disposer disposer)
   {  return this->erase_and_dispose(const_iterator(i), disposer);   }
   #endif

   //! <b>Requires</b>: f and l must be valid iterator to elements in *this.
   //!                  Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Erases the range pointed by b and e.
   //!   No destructors are called.
   //!   Disposer::operator()(pointer) is called for the removed elements.
   //!
   //! <b>Returns</b>: the first element remaining beyond the removed elements,
   //!   or end() if no such element exists.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of erased elements plus linear
   //!   to the elements before f.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references) to the
   //!   erased elements.
   template<class Disposer>
   iterator erase_and_dispose(const_iterator f, const_iterator l, Disposer disposer)
   {  return this->erase_after_and_dispose(this->previous(f), l, disposer);  }

   //! <b>Requires</b>: Dereferencing iterator must yield
   //!   an lvalue of type value_type.
   //!
   //! <b>Effects</b>: Clears the list and inserts the range pointed by b and e.
   //!   No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements inserted plus
   //!   linear to the elements contained in the list if it's a safe-mode
   //!   or auto-unlink value.
   //!   Linear to the number of elements inserted in the list otherwise.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references)
   //!   to the erased elements.
   template<class Iterator>
   void assign(Iterator b, Iterator e)
   {
      this->clear();
      this->insert_after(this->cbefore_begin(), b, e);
   }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Requires</b>: Dereferencing iterator must yield
   //!   an lvalue of type value_type.
   //!
   //! <b>Effects</b>: Clears the list and inserts the range pointed by b and e.
   //!   No destructors or copy constructors are called.
   //!   Disposer::operator()(pointer) is called for the removed elements.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements inserted plus
   //!   linear to the elements contained in the list.
   //!
   //! <b>Note</b>: Invalidates the iterators (but not the references)
   //!   to the erased elements.
   template<class Iterator, class Disposer>
   void dispose_and_assign(Disposer disposer, Iterator b, Iterator e)
   {
      this->clear_and_dispose(disposer);
      this->insert_after(this->cbefore_begin(), b, e, disposer);
   }

   //! <b>Requires</b>: prev must point to an element contained by this list or
   //!   to the before_begin() element
   //!
   //! <b>Effects</b>: Transfers all the elements of list x to this list, after the
   //! the element pointed by prev. No destructors or copy constructors are called.
   //!
   //! <b>Returns</b>: Nothing.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: In general, linear to the elements contained in x.
   //!   Constant-time if cache_last<> option is true and also constant-time if
   //!   linear<> option is true "this" is empty and "l" is not used.
   //!
   //! <b>Note</b>: Iterators of values obtained from list x now point to elements of this
   //! list. Iterators of this list and all the references are not invalidated.
   //!
   //! <b>Additional note</b>: If the optional parameter "l" is provided, it will be
   //!   assigned to the last spliced element or prev if x is empty.
   //!   This iterator can be used as new "prev" iterator for a new splice_after call.
   //!   that will splice new values after the previously spliced values.
   void splice_after(const_iterator prev, slist_impl &x, const_iterator *l = 0)
   {
      if(x.empty()){
         if(l) *l = prev;
      }
      else if(linear && this->empty()){
         this->swap(x);
         if(l) *l = this->previous(this->cend());
      }
      else{
         const_iterator last_x(x.previous(x.end()));  //constant time if cache_last is active
         node_ptr prev_n(prev.pointed_node());
         node_ptr last_x_n(last_x.pointed_node());
         if(cache_last){
            x.set_last_node(x.get_root_node());
            if(node_traits::get_next(prev_n) == this->get_end_node()){
               this->set_last_node(last_x_n);
            }
         }
         node_algorithms::transfer_after( prev_n, x.before_begin().pointed_node(), last_x_n);
         this->priv_size_traits().increase(x.priv_size_traits().get_size());
         x.priv_size_traits().set_size(size_type(0));
         if(l) *l = last_x;
      }
   }

   //! <b>Requires</b>: prev must point to an element contained by this list or
   //!   to the before_begin() element. prev_ele must point to an element contained in list
   //!   x or must be x.before_begin().
   //!
   //! <b>Effects</b>: Transfers the element after prev_ele, from list x to this list,
   //!   after the element pointed by prev. No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Iterators of values obtained from list x now point to elements of this
   //! list. Iterators of this list and all the references are not invalidated.
   void splice_after(const_iterator prev_pos, slist_impl &x, const_iterator prev_ele)
   {
      const_iterator elem = prev_ele;
      this->splice_after(prev_pos, x, prev_ele, ++elem, 1);
   }

   //! <b>Requires</b>: prev_pos must be a dereferenceable iterator in *this or be
   //!   before_begin(), and before_f and before_l belong to x and
   //!   ++before_f != x.end() && before_l != x.end().
   //!
   //! <b>Effects</b>: Transfers the range (before_f, before_l] from list x to this
   //!   list, after the element pointed by prev_pos.
   //!   No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements transferred
   //!   if constant_time_size is true. Constant-time otherwise.
   //!
   //! <b>Note</b>: Iterators of values obtained from list x now point to elements of this
   //!   list. Iterators of this list and all the references are not invalidated.
   void splice_after(const_iterator prev_pos, slist_impl &x, const_iterator before_f, const_iterator before_l)
   {
      if(constant_time_size)
         this->splice_after(prev_pos, x, before_f, before_l, node_algorithms::distance(before_f.pointed_node(), before_l.pointed_node()));
      else
         this->priv_splice_after
            (prev_pos.pointed_node(), x, before_f.pointed_node(), before_l.pointed_node());
   }

   //! <b>Requires</b>: prev_pos must be a dereferenceable iterator in *this or be
   //!   before_begin(), and before_f and before_l belong to x and
   //!   ++before_f != x.end() && before_l != x.end() and
   //!   n == distance(before_f, before_l).
   //!
   //! <b>Effects</b>: Transfers the range (before_f, before_l] from list x to this
   //!   list, after the element pointed by p. No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Note</b>: Iterators of values obtained from list x now point to elements of this
   //!   list. Iterators of this list and all the references are not invalidated.
   void splice_after(const_iterator prev_pos, slist_impl &x, const_iterator before_f, const_iterator before_l, size_type n)
   {
      BOOST_INTRUSIVE_INVARIANT_ASSERT(node_algorithms::distance(before_f.pointed_node(), before_l.pointed_node()) == n);
      this->priv_splice_after
         (prev_pos.pointed_node(), x, before_f.pointed_node(), before_l.pointed_node());
      if(constant_time_size){
         this->priv_size_traits().increase(n);
         x.priv_size_traits().decrease(n);
      }
   }

   //! <b>Requires</b>: it is an iterator to an element in *this.
   //!
   //! <b>Effects</b>: Transfers all the elements of list x to this list, before the
   //! the element pointed by it. No destructors or copy constructors are called.
   //!
   //! <b>Returns</b>: Nothing.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the elements contained in x plus linear to
   //!   the elements before it.
   //!   Linear to the elements before it if cache_last<> option is true.
   //!   Constant-time if cache_last<> option is true and it == end().
   //!
   //! <b>Note</b>: Iterators of values obtained from list x now point to elements of this
   //! list. Iterators of this list and all the references are not invalidated.
   //!
   //! <b>Additional note</b>: If the optional parameter "l" is provided, it will be
   //!   assigned to the last spliced element or prev if x is empty.
   //!   This iterator can be used as new "prev" iterator for a new splice_after call.
   //!   that will splice new values after the previously spliced values.
   void splice(const_iterator it, slist_impl &x, const_iterator *l = 0)
   {  this->splice_after(this->previous(it), x, l);   }

   //! <b>Requires</b>: it p must be a valid iterator of *this.
   //!   elem must point to an element contained in list
   //!   x.
   //!
   //! <b>Effects</b>: Transfers the element elem, from list x to this list,
   //!   before the element pointed by pos. No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the elements before pos and before elem.
   //!   Linear to the elements before elem if cache_last<> option is true and pos == end().
   //!
   //! <b>Note</b>: Iterators of values obtained from list x now point to elements of this
   //! list. Iterators of this list and all the references are not invalidated.
   void splice(const_iterator pos, slist_impl &x, const_iterator elem)
   {  return this->splice_after(this->previous(pos), x, x.previous(elem));  }

   //! <b>Requires</b>: pos must be a dereferenceable iterator in *this
   //!   and f and f belong to x and f and f a valid range on x.
   //!
   //! <b>Effects</b>: Transfers the range [f, l) from list x to this
   //!   list, before the element pointed by pos.
   //!   No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the sum of elements before pos, f, and l
   //!   plus linear to the number of elements transferred if constant_time_size is true.
   //!   Linear to the sum of elements before f, and l
   //!   plus linear to the number of elements transferred if constant_time_size is true
   //!   if cache_last<> is true and pos == end()
   //!
   //! <b>Note</b>: Iterators of values obtained from list x now point to elements of this
   //!   list. Iterators of this list and all the references are not invalidated.
   void splice(const_iterator pos, slist_impl &x, const_iterator f, const_iterator l)
   {  return this->splice_after(this->previous(pos), x, x.previous(f), x.previous(l));  }

   //! <b>Requires</b>: pos must be a dereferenceable iterator in *this
   //!   and f and l belong to x and f and l a valid range on x.
   //!   n == distance(f, l).
   //!
   //! <b>Effects</b>: Transfers the range [f, l) from list x to this
   //!   list, before the element pointed by pos.
   //!   No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the sum of elements before pos, f, and l.
   //!   Linear to the sum of elements before f and l
   //!   if cache_last<> is true and pos == end().
   //!
   //! <b>Note</b>: Iterators of values obtained from list x now point to elements of this
   //!   list. Iterators of this list and all the references are not invalidated.
   void splice(const_iterator pos, slist_impl &x, const_iterator f, const_iterator l, size_type n)
   {  return this->splice_after(this->previous(pos), x, x.previous(f), x.previous(l), n);  }

   //! <b>Effects</b>: This function sorts the list *this according to std::less<value_type>.
   //!   The sort is stable, that is, the relative order of equivalent elements is preserved.
   //!
   //! <b>Throws</b>: If value_traits::node_traits::node
   //!   constructor throws (this does not happen with predefined Boost.Intrusive hooks)
   //!   or the predicate throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: The number of comparisons is approximately N log N, where N
   //!   is the list's size.
   //!
   //! <b>Note</b>: Iterators and references are not invalidated
   template<class Predicate>
   void sort(Predicate p)
   {
      if (node_traits::get_next(node_traits::get_next(this->get_root_node()))
         != this->get_root_node()) {

         slist_impl carry(this->priv_value_traits());
         detail::array_initializer<slist_impl, 64> counter(this->priv_value_traits());
         int fill = 0;
         const_iterator last_inserted;
         while(!this->empty()){
            last_inserted = this->cbegin();
            carry.splice_after(carry.cbefore_begin(), *this, this->cbefore_begin());
            int i = 0;
            while(i < fill && !counter[i].empty()) {
               carry.swap(counter[i]);
               carry.merge(counter[i++], p, &last_inserted);
            }
            BOOST_INTRUSIVE_INVARIANT_ASSERT(counter[i].empty());
            const_iterator last_element(carry.previous(last_inserted, carry.end()));

            if(constant_time_size){
               counter[i].splice_after( counter[i].cbefore_begin(), carry
                                    , carry.cbefore_begin(), last_element
                                    , carry.size());
            }
            else{
               counter[i].splice_after( counter[i].cbefore_begin(), carry
                                    , carry.cbefore_begin(), last_element);
            }
            if(i == fill)
               ++fill;
         }

         for (int i = 1; i < fill; ++i)
            counter[i].merge(counter[i-1], p, &last_inserted);
         --fill;
         const_iterator last_element(counter[fill].previous(last_inserted, counter[fill].end()));
         if(constant_time_size){
            this->splice_after( cbefore_begin(), counter[fill], counter[fill].cbefore_begin()
                              , last_element, counter[fill].size());
         }
         else{
            this->splice_after( cbefore_begin(), counter[fill], counter[fill].cbefore_begin()
                              , last_element);
         }
      }
   }

   //! <b>Requires</b>: p must be a comparison function that induces a strict weak
   //!   ordering and both *this and x must be sorted according to that ordering
   //!   The lists x and *this must be distinct.
   //!
   //! <b>Effects</b>: This function removes all of x's elements and inserts them
   //!   in order into *this. The merge is stable; that is, if an element from *this is
   //!   equivalent to one from x, then the element from *this will precede the one from x.
   //!
   //! <b>Throws</b>: If value_traits::node_traits::node
   //!   constructor throws (this does not happen with predefined Boost.Intrusive hooks)
   //!   or std::less<value_type> throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: This function is linear time: it performs at most
   //!   size() + x.size() - 1 comparisons.
   //!
   //! <b>Note</b>: Iterators and references are not invalidated.
   void sort()
   { this->sort(std::less<value_type>()); }

   //! <b>Requires</b>: p must be a comparison function that induces a strict weak
   //!   ordering and both *this and x must be sorted according to that ordering
   //!   The lists x and *this must be distinct.
   //!
   //! <b>Effects</b>: This function removes all of x's elements and inserts them
   //!   in order into *this. The merge is stable; that is, if an element from *this is
   //!   equivalent to one from x, then the element from *this will precede the one from x.
   //!
   //! <b>Returns</b>: Nothing.
   //!
   //! <b>Throws</b>: If the predicate throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: This function is linear time: it performs at most
   //!   size() + x.size() - 1 comparisons.
   //!
   //! <b>Note</b>: Iterators and references are not invalidated.
   //!
   //! <b>Additional note</b>: If optional "l" argument is passed, it is assigned
   //! to an iterator to the last transferred value or end() is x is empty.
   template<class Predicate>
   void merge(slist_impl& x, Predicate p, const_iterator *l = 0)
   {
      const_iterator e(this->cend()), ex(x.cend()), bb(this->cbefore_begin()),
                     bb_next;
      if(l) *l = e.unconst();
      while(!x.empty()){
         const_iterator ibx_next(x.cbefore_begin()), ibx(ibx_next++);
         while (++(bb_next = bb) != e && !p(*ibx_next, *bb_next)){
            bb = bb_next;
         }
         if(bb_next == e){
            //Now transfer the rest to the end of the container
            this->splice_after(bb, x, l);
            break;
         }
         else{
            size_type n(0);
            do{
               ibx = ibx_next; ++n;
            } while(++(ibx_next = ibx) != ex && p(*ibx_next, *bb_next));
            this->splice_after(bb, x, x.before_begin(), ibx, n);
            if(l) *l = ibx;
         }
      }
   }

   //! <b>Effects</b>: This function removes all of x's elements and inserts them
   //!   in order into *this according to std::less<value_type>. The merge is stable;
   //!   that is, if an element from *this is equivalent to one from x, then the element
   //!   from *this will precede the one from x.
   //!
   //! <b>Throws</b>: if std::less<value_type> throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: This function is linear time: it performs at most
   //!   size() + x.size() - 1 comparisons.
   //!
   //! <b>Note</b>: Iterators and references are not invalidated
   void merge(slist_impl& x)
   {  this->merge(x, std::less<value_type>());  }

   //! <b>Effects</b>: Reverses the order of elements in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: This function is linear to the contained elements.
   //!
   //! <b>Note</b>: Iterators and references are not invalidated
   void reverse()
   {
      if(cache_last && !this->empty()){
         this->set_last_node(node_traits::get_next(this->get_root_node()));
      }
      this->priv_reverse(detail::bool_<linear>());
   }

   //! <b>Effects</b>: Removes all the elements that compare equal to value.
   //!   No destructors are called.
   //!
   //! <b>Throws</b>: If std::equal_to<value_type> throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: Linear time. It performs exactly size() comparisons for equality.
   //!
   //! <b>Note</b>: The relative order of elements that are not removed is unchanged,
   //!   and iterators to elements that are not removed remain valid. This function is
   //!   linear time: it performs exactly size() comparisons for equality.
   void remove(const_reference value)
   {  this->remove_if(detail::equal_to_value<const_reference>(value));  }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Removes all the elements that compare equal to value.
   //!   Disposer::operator()(pointer) is called for every removed element.
   //!
   //! <b>Throws</b>: If std::equal_to<value_type> throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: Linear time. It performs exactly size() comparisons for equality.
   //!
   //! <b>Note</b>: The relative order of elements that are not removed is unchanged,
   //!   and iterators to elements that are not removed remain valid.
   template<class Disposer>
   void remove_and_dispose(const_reference value, Disposer disposer)
   {  this->remove_and_dispose_if(detail::equal_to_value<const_reference>(value), disposer);  }

   //! <b>Effects</b>: Removes all the elements for which a specified
   //!   predicate is satisfied. No destructors are called.
   //!
   //! <b>Throws</b>: If pred throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: Linear time. It performs exactly size() calls to the predicate.
   //!
   //! <b>Note</b>: The relative order of elements that are not removed is unchanged,
   //!   and iterators to elements that are not removed remain valid.
   template<class Pred>
   void remove_if(Pred pred)
   {
      const node_ptr bbeg = this->get_root_node();
      typename node_algorithms::stable_partition_info info;
      node_algorithms::stable_partition
         (bbeg, this->get_end_node(), detail::key_nodeptr_comp<Pred, value_traits>(pred, &this->priv_value_traits()), info);
      //After cache last is set, slist invariants are preserved...
      if(cache_last){
         this->set_last_node(info.new_last_node);
      }
      //...so erase can be safely called
      this->erase_after( const_iterator(bbeg, this->priv_value_traits_ptr())
                       , const_iterator(info.beg_2st_partition, this->priv_value_traits_ptr())
                       , info.num_1st_partition);
   }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Removes all the elements for which a specified
   //!   predicate is satisfied.
   //!   Disposer::operator()(pointer) is called for every removed element.
   //!
   //! <b>Throws</b>: If pred throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: Linear time. It performs exactly size() comparisons for equality.
   //!
   //! <b>Note</b>: The relative order of elements that are not removed is unchanged,
   //!   and iterators to elements that are not removed remain valid.
   template<class Pred, class Disposer>
   void remove_and_dispose_if(Pred pred, Disposer disposer)
   {
      const node_ptr bbeg = this->get_root_node();
      typename node_algorithms::stable_partition_info info;
      node_algorithms::stable_partition
         (bbeg, this->get_end_node(), detail::key_nodeptr_comp<Pred, value_traits>(pred, &this->priv_value_traits()), info);
      //After cache last is set, slist invariants are preserved...
      if(cache_last){
         this->set_last_node(info.new_last_node);
      }
      //...so erase can be safely called
      this->erase_after_and_dispose( const_iterator(bbeg, this->priv_value_traits_ptr())
                                   , const_iterator(info.beg_2st_partition, this->priv_value_traits_ptr())
                                   , disposer);
   }

   //! <b>Effects</b>: Removes adjacent duplicate elements or adjacent
   //!   elements that are equal from the list. No destructors are called.
   //!
   //! <b>Throws</b>: If std::equal_to<value_type> throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: Linear time (size()-1) comparisons calls to pred()).
   //!
   //! <b>Note</b>: The relative order of elements that are not removed is unchanged,
   //!   and iterators to elements that are not removed remain valid.
   void unique()
   {  this->unique_and_dispose(std::equal_to<value_type>(), detail::null_disposer());  }

   //! <b>Effects</b>: Removes adjacent duplicate elements or adjacent
   //!   elements that satisfy some binary predicate from the list.
   //!   No destructors are called.
   //!
   //! <b>Throws</b>: If the predicate throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: Linear time (size()-1) comparisons equality comparisons.
   //!
   //! <b>Note</b>: The relative order of elements that are not removed is unchanged,
   //!   and iterators to elements that are not removed remain valid.
   template<class BinaryPredicate>
   void unique(BinaryPredicate pred)
   {  this->unique_and_dispose(pred, detail::null_disposer());  }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Removes adjacent duplicate elements or adjacent
   //!   elements that satisfy some binary predicate from the list.
   //!   Disposer::operator()(pointer) is called for every removed element.
   //!
   //! <b>Throws</b>: If std::equal_to<value_type> throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: Linear time (size()-1) comparisons equality comparisons.
   //!
   //! <b>Note</b>: The relative order of elements that are not removed is unchanged,
   //!   and iterators to elements that are not removed remain valid.
   template<class Disposer>
   void unique_and_dispose(Disposer disposer)
   {  this->unique(std::equal_to<value_type>(), disposer);  }

   //! <b>Requires</b>: Disposer::operator()(pointer) shouldn't throw.
   //!
   //! <b>Effects</b>: Removes adjacent duplicate elements or adjacent
   //!   elements that satisfy some binary predicate from the list.
   //!   Disposer::operator()(pointer) is called for every removed element.
   //!
   //! <b>Throws</b>: If the predicate throws. Basic guarantee.
   //!
   //! <b>Complexity</b>: Linear time (size()-1) comparisons equality comparisons.
   //!
   //! <b>Note</b>: The relative order of elements that are not removed is unchanged,
   //!   and iterators to elements that are not removed remain valid.
   template<class BinaryPredicate, class Disposer>
   void unique_and_dispose(BinaryPredicate pred, Disposer disposer)
   {
      const_iterator end_n(this->cend());
      const_iterator bcur(this->cbegin());
      if(bcur != end_n){
         const_iterator cur(bcur);
         ++cur;
         while(cur != end_n) {
            if (pred(*bcur, *cur)){
               cur = this->erase_after_and_dispose(bcur, disposer);
            }
            else{
               bcur = cur;
               ++cur;
            }
         }
         if(cache_last){
            this->set_last_node(bcur.pointed_node());
         }
      }
   }

   //! <b>Requires</b>: value must be a reference to a value inserted in a list.
   //!
   //! <b>Effects</b>: This function returns a const_iterator pointing to the element
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Note</b>: Iterators and references are not invalidated.
   //!   This static function is available only if the <i>value traits</i>
   //!   is stateless.
   static iterator s_iterator_to(reference value)
   {
      BOOST_STATIC_ASSERT((!stateful_value_traits));
      return iterator (value_traits::to_node_ptr(value), const_value_traits_ptr());
   }

   //! <b>Requires</b>: value must be a const reference to a value inserted in a list.
   //!
   //! <b>Effects</b>: This function returns an iterator pointing to the element.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Note</b>: Iterators and references are not invalidated.
   //!   This static function is available only if the <i>value traits</i>
   //!   is stateless.
   static const_iterator s_iterator_to(const_reference value)
   {
      BOOST_STATIC_ASSERT((!stateful_value_traits));
      reference r =*detail::uncast(pointer_traits<const_pointer>::pointer_to(value));
      return const_iterator(value_traits::to_node_ptr(r), const_value_traits_ptr());
   }

   //! <b>Requires</b>: value must be a reference to a value inserted in a list.
   //!
   //! <b>Effects</b>: This function returns a const_iterator pointing to the element
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Note</b>: Iterators and references are not invalidated.
   iterator iterator_to(reference value)
   {
      BOOST_INTRUSIVE_INVARIANT_ASSERT(linear || !node_algorithms::inited(this->priv_value_traits().to_node_ptr(value)));
      return iterator (this->priv_value_traits().to_node_ptr(value), this->priv_value_traits_ptr());
   }

   //! <b>Requires</b>: value must be a const reference to a value inserted in a list.
   //!
   //! <b>Effects</b>: This function returns an iterator pointing to the element.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Note</b>: Iterators and references are not invalidated.
   const_iterator iterator_to(const_reference value) const
   {
      reference r =*detail::uncast(pointer_traits<const_pointer>::pointer_to(value));
      BOOST_INTRUSIVE_INVARIANT_ASSERT (linear || !node_algorithms::inited(this->priv_value_traits().to_node_ptr(r)));
      return const_iterator(this->priv_value_traits().to_node_ptr(r), this->priv_value_traits_ptr());
   }

   //! <b>Returns</b>: The iterator to the element before i in the list.
   //!   Returns the end-iterator, if either i is the begin-iterator or the
   //!   list is empty.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements before i.
   //!   Constant if cache_last<> is true and i == end().
   iterator previous(iterator i)
   {  return this->previous(this->cbefore_begin(), i); }

   //! <b>Returns</b>: The const_iterator to the element before i in the list.
   //!   Returns the end-const_iterator, if either i is the begin-const_iterator or
   //!   the list is empty.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements before i.
   //!   Constant if cache_last<> is true and i == end().
   const_iterator previous(const_iterator i) const
   {  return this->previous(this->cbefore_begin(), i); }

   //! <b>Returns</b>: The iterator to the element before i in the list,
   //!   starting the search on element after prev_from.
   //!   Returns the end-iterator, if either i is the begin-iterator or the
   //!   list is empty.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements before i.
   //!   Constant if cache_last<> is true and i == end().
   iterator previous(const_iterator prev_from, iterator i)
   {  return this->previous(prev_from, const_iterator(i)).unconst(); }

   //! <b>Returns</b>: The const_iterator to the element before i in the list,
   //!   starting the search on element after prev_from.
   //!   Returns the end-const_iterator, if either i is the begin-const_iterator or
   //!   the list is empty.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements before i.
   //!   Constant if cache_last<> is true and i == end().
   const_iterator previous(const_iterator prev_from, const_iterator i) const
   {
      if(cache_last && (i.pointed_node() == this->get_end_node())){
         return const_iterator(detail::uncast(this->get_last_node()), this->priv_value_traits_ptr());
      }
      return const_iterator
         (node_algorithms::get_previous_node
            (prev_from.pointed_node(), i.pointed_node()), this->priv_value_traits_ptr());
   }

   ///@cond

   //! <b>Requires</b>: prev_pos must be a dereferenceable iterator in *this or be
   //!   before_begin(), and f and before_l belong to another slist.
   //!
   //! <b>Effects</b>: Transfers the range [f, before_l] to this
   //!   list, after the element pointed by prev_pos.
   //!   No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements transferred
   //!   if constant_time_size is true. Constant-time otherwise.
   //!
   //! <b>Note</b>: Iterators of values obtained from the list that owned f and before_l now
   //!   point to elements of this list. Iterators of this list and all the references are not invalidated.
   //!
   //! <b>Warning</b>: Experimental function, don't use it!
   void incorporate_after(const_iterator prev_pos, const node_ptr & f, const node_ptr & before_l)
   {
      if(constant_time_size)
         this->incorporate_after(prev_pos, f, before_l, node_algorithms::distance(f.pointed_node(), before_l.pointed_node())+1);
      else
         this->priv_incorporate_after(prev_pos.pointed_node(), f, before_l);
   }

   //! <b>Requires</b>: prev_pos must be a dereferenceable iterator in *this or be
   //!   before_begin(), and f and before_l belong to another slist.
   //!   n == distance(f, before_l) + 1.
   //!
   //! <b>Effects</b>: Transfers the range [f, before_l] to this
   //!   list, after the element pointed by prev_pos.
   //!   No destructors or copy constructors are called.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Note</b>: Iterators of values obtained from the list that owned f and before_l now
   //!   point to elements of this list. Iterators of this list and all the references are not invalidated.
   //!
   //! <b>Warning</b>: Experimental function, don't use it!
   void incorporate_after(const_iterator prev_pos, const node_ptr & f, const node_ptr & before_l, size_type n)
   {
      if(n){
         BOOST_INTRUSIVE_INVARIANT_ASSERT(n > 0);
         BOOST_INTRUSIVE_INVARIANT_ASSERT
            (size_type(boost::intrusive::iterator_distance
               ( iterator(f, this->priv_value_traits_ptr())
               , iterator(before_l, this->priv_value_traits_ptr())))
            +1 == n);
         this->priv_incorporate_after(prev_pos.pointed_node(), f, before_l);
         if(constant_time_size){
            this->priv_size_traits().increase(n);
         }
      }
   }

   ///@endcond

   //! <b>Effects</b>: Asserts the integrity of the container.
   //!
   //! <b>Complexity</b>: Linear time.
   //!
   //! <b>Note</b>: The method has no effect when asserts are turned off (e.g., with NDEBUG).
   //!   Experimental function, interface might change in future versions.
   void check() const
   {
      const_node_ptr header_ptr = get_root_node();
      // header's next is never null
      BOOST_INTRUSIVE_INVARIANT_ASSERT(node_traits::get_next(header_ptr));
      if (node_traits::get_next(header_ptr) == header_ptr)
      {
         if (constant_time_size)
            BOOST_INTRUSIVE_INVARIANT_ASSERT(this->priv_size_traits().get_size() == 0);
         return;
      }
      size_t node_count = 0;
      const_node_ptr p = header_ptr;
      while (true)
      {
         const_node_ptr next_p = node_traits::get_next(p);
         if (!linear)
         {
            BOOST_INTRUSIVE_INVARIANT_ASSERT(next_p);
         }
         else
         {
            BOOST_INTRUSIVE_INVARIANT_ASSERT(next_p != header_ptr);
         }
         if ((!linear && next_p == header_ptr) || (linear && !next_p))
         {
            if (cache_last)
               BOOST_INTRUSIVE_INVARIANT_ASSERT(get_last_node() == p);
            break;
         }
         p = next_p;
         ++node_count;
      }
      if (constant_time_size)
         BOOST_INTRUSIVE_INVARIANT_ASSERT(this->priv_size_traits().get_size() == node_count);
   }


   friend bool operator==(const slist_impl &x, const slist_impl &y)
   {
      if(constant_time_size && x.size() != y.size()){
         return false;
      }
      return ::boost::intrusive::algo_equal(x.cbegin(), x.cend(), y.cbegin(), y.cend());
   }

   friend bool operator!=(const slist_impl &x, const slist_impl &y)
   {  return !(x == y); }

   friend bool operator<(const slist_impl &x, const slist_impl &y)
   {  return ::boost::intrusive::algo_lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());  }

   friend bool operator>(const slist_impl &x, const slist_impl &y)
   {  return y < x;  }

   friend bool operator<=(const slist_impl &x, const slist_impl &y)
   {  return !(y < x);  }

   friend bool operator>=(const slist_impl &x, const slist_impl &y)
   {  return !(x < y);  }

   friend void swap(slist_impl &x, slist_impl &y)
   {  x.swap(y);  }

   private:
   void priv_splice_after(node_ptr prev_pos_n, slist_impl &x, node_ptr before_f_n, node_ptr before_l_n)
   {
      if (cache_last && (before_f_n != before_l_n)){
         if(prev_pos_n == this->get_last_node()){
            this->set_last_node(before_l_n);
         }
         if(&x != this && node_traits::get_next(before_l_n) == x.get_end_node()){
            x.set_last_node(before_f_n);
         }
      }
      node_algorithms::transfer_after(prev_pos_n, before_f_n, before_l_n);
   }

   void priv_incorporate_after(node_ptr prev_pos_n, node_ptr first_n, node_ptr before_l_n)
   {
      if(cache_last){
         if(prev_pos_n == this->get_last_node()){
            this->set_last_node(before_l_n);
         }
      }
      node_algorithms::incorporate_after(prev_pos_n, first_n, before_l_n);
   }

   void priv_reverse(detail::bool_<false>)
   {  node_algorithms::reverse(this->get_root_node());   }

   void priv_reverse(detail::bool_<true>)
   {
      node_ptr new_first = node_algorithms::reverse
         (node_traits::get_next(this->get_root_node()));
      node_traits::set_next(this->get_root_node(), new_first);
   }

   void priv_shift_backwards(size_type n, detail::bool_<false>)
   {
      node_ptr l = node_algorithms::move_forward(this->get_root_node(), (std::size_t)n);
      if(cache_last && l){
         this->set_last_node(l);
      }
   }

   void priv_shift_backwards(size_type n, detail::bool_<true>)
   {
      std::pair<node_ptr, node_ptr> ret(
         node_algorithms::move_first_n_forward
            (node_traits::get_next(this->get_root_node()), (std::size_t)n));
      if(ret.first){
         node_traits::set_next(this->get_root_node(), ret.first);
         if(cache_last){
            this->set_last_node(ret.second);
         }
      }
   }

   void priv_shift_forward(size_type n, detail::bool_<false>)
   {
      node_ptr l = node_algorithms::move_backwards(this->get_root_node(), (std::size_t)n);
      if(cache_last && l){
         this->set_last_node(l);
      }
   }

   void priv_shift_forward(size_type n, detail::bool_<true>)
   {
      std::pair<node_ptr, node_ptr> ret(
         node_algorithms::move_first_n_backwards
         (node_traits::get_next(this->get_root_node()), (std::size_t)n));
      if(ret.first){
         node_traits::set_next(this->get_root_node(), ret.first);
         if(cache_last){
            this->set_last_node(ret.second);
         }
      }
   }

   static void priv_swap_cache_last(slist_impl *this_impl, slist_impl *other_impl)
   {
      bool other_was_empty = false;
      if(this_impl->empty()){
         //Check if both are empty or
         if(other_impl->empty())
            return;
         //If this is empty swap pointers
         slist_impl *tmp = this_impl;
         this_impl  = other_impl;
         other_impl = tmp;
         other_was_empty = true;
      }
      else{
         other_was_empty = other_impl->empty();
      }

      //Precondition: this is not empty
      node_ptr other_old_last(other_impl->get_last_node());
      node_ptr other_bfirst(other_impl->get_root_node());
      node_ptr this_bfirst(this_impl->get_root_node());
      node_ptr this_old_last(this_impl->get_last_node());

      //Move all nodes from this to other's beginning
      node_algorithms::transfer_after(other_bfirst, this_bfirst, this_old_last);
      other_impl->set_last_node(this_old_last);

      if(other_was_empty){
         this_impl->set_last_node(this_bfirst);
      }
      else{
         //Move trailing nodes from other to this
         node_algorithms::transfer_after(this_bfirst, this_old_last, other_old_last);
         this_impl->set_last_node(other_old_last);
      }
   }

   //circular version
   static void priv_swap_lists(node_ptr this_node, node_ptr other_node, detail::bool_<false>)
   {  node_algorithms::swap_nodes(this_node, other_node); }

   //linear version
   static void priv_swap_lists(node_ptr this_node, node_ptr other_node, detail::bool_<true>)
   {  node_algorithms::swap_trailing_nodes(this_node, other_node); }

   static slist_impl &priv_container_from_end_iterator(const const_iterator &end_iterator)
   {
      //Obtaining the container from the end iterator is not possible with linear
      //singly linked lists (because "end" is represented by the null pointer)
      BOOST_STATIC_ASSERT(!linear);
      BOOST_STATIC_ASSERT((has_container_from_iterator));
      node_ptr p = end_iterator.pointed_node();
      header_holder_type* h = header_holder_type::get_holder(p);
      header_holder_plus_last_t* hpl = detail::parent_from_member< header_holder_plus_last_t, header_holder_type>
                                         (h, &header_holder_plus_last_t::header_holder_);
      root_plus_size* r = static_cast< root_plus_size* >(hpl);
      data_t *d = detail::parent_from_member<data_t, root_plus_size>
         ( r, &data_t::root_plus_size_);
      slist_impl *s  = detail::parent_from_member<slist_impl, data_t>(d, &slist_impl::data_);
      return *s;
   }
};

//! Helper metafunction to define a \c slist that yields to the same type when the
//! same options (either explicitly or implicitly) are used.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class ...Options>
#else
template<class T, class O1 = void, class O2 = void, class O3 = void, class O4 = void, class O5 = void, class O6 = void>
#endif
struct make_slist
{
   /// @cond
   typedef typename pack_options
      < slist_defaults,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4, O5, O6
         #else
         Options...
         #endif
      >::type packed_options;

   typedef typename detail::get_value_traits
      <T, typename packed_options::proto_value_traits>::type value_traits;
   typedef slist_impl
      < value_traits
      , typename packed_options::size_type
      ,  (std::size_t(packed_options::linear)*slist_bool_flags::linear_pos)
        |(std::size_t(packed_options::constant_time_size)*slist_bool_flags::constant_time_size_pos)
        |(std::size_t(packed_options::cache_last)*slist_bool_flags::cache_last_pos)
      , typename packed_options::header_holder_type
      > implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};


#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class O1, class O2, class O3, class O4, class O5, class O6>
#else
template<class T, class ...Options>
#endif
class slist
   :  public make_slist<T,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4, O5, O6
         #else
         Options...
         #endif
      >::type
{
   typedef typename make_slist
      <T,
      #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
      O1, O2, O3, O4, O5, O6
      #else
      Options...
      #endif
      >::type   Base;
   //Assert if passed value traits are compatible with the type
   BOOST_STATIC_ASSERT((detail::is_same<typename Base::value_traits::value_type, T>::value));
   BOOST_MOVABLE_BUT_NOT_COPYABLE(slist)

   public:
   typedef typename Base::value_traits       value_traits;
   typedef typename Base::iterator           iterator;
   typedef typename Base::const_iterator     const_iterator;
   typedef typename Base::size_type          size_type;
   typedef typename Base::node_ptr           node_ptr;

   BOOST_INTRUSIVE_FORCEINLINE slist()
      :  Base()
   {}

   BOOST_INTRUSIVE_FORCEINLINE explicit slist(const value_traits &v_traits)
      :  Base(v_traits)
   {}

   struct incorporate_t{};

   BOOST_INTRUSIVE_FORCEINLINE slist( const node_ptr & f, const node_ptr & before_l
             , size_type n, const value_traits &v_traits = value_traits())
      :  Base(f, before_l, n, v_traits)
   {}

   template<class Iterator>
   BOOST_INTRUSIVE_FORCEINLINE slist(Iterator b, Iterator e, const value_traits &v_traits = value_traits())
      :  Base(b, e, v_traits)
   {}

   BOOST_INTRUSIVE_FORCEINLINE slist(BOOST_RV_REF(slist) x)
      :  Base(BOOST_MOVE_BASE(Base, x))
   {}

   BOOST_INTRUSIVE_FORCEINLINE slist& operator=(BOOST_RV_REF(slist) x)
   {  return static_cast<slist &>(this->Base::operator=(BOOST_MOVE_BASE(Base, x)));  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(const slist &src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(src, cloner, disposer);  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(BOOST_RV_REF(slist) src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(BOOST_MOVE_BASE(Base, src), cloner, disposer);  }

   BOOST_INTRUSIVE_FORCEINLINE static slist &container_from_end_iterator(iterator end_iterator)
   {  return static_cast<slist &>(Base::container_from_end_iterator(end_iterator));   }

   BOOST_INTRUSIVE_FORCEINLINE static const slist &container_from_end_iterator(const_iterator end_iterator)
   {  return static_cast<const slist &>(Base::container_from_end_iterator(end_iterator));   }
};

#endif

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_SLIST_HPP
