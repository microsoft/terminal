//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_DEQUE_HPP
#define BOOST_CONTAINER_DEQUE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
// container
#include <boost/container/allocator_traits.hpp>
#include <boost/container/container_fwd.hpp>
#include <boost/container/new_allocator.hpp> //new_allocator
#include <boost/container/throw_exception.hpp>
#include <boost/container/options.hpp>
// container/detail
#include <boost/container/detail/advanced_insert_int.hpp>
#include <boost/container/detail/algorithm.hpp> //algo_equal(), algo_lexicographical_compare
#include <boost/container/detail/alloc_helpers.hpp>
#include <boost/container/detail/copy_move_algo.hpp>
#include <boost/container/detail/iterator.hpp>
#include <boost/move/detail/iterator_to_raw_pointer.hpp>
#include <boost/container/detail/iterators.hpp>
#include <boost/container/detail/min_max.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/move/detail/to_raw_pointer.hpp>
#include <boost/container/detail/type_traits.hpp>
// move
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/iterator.hpp>
#include <boost/move/traits.hpp>
#include <boost/move/utility_core.hpp>
// move/detail
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/move/detail/fwd_macros.hpp>
#endif
#include <boost/move/detail/move_helpers.hpp>
// other
#include <boost/assert.hpp>
#include <boost/core/no_exceptions_support.hpp>
// std
#include <cstddef>

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
#include <initializer_list>
#endif

namespace boost {
namespace container {

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
template <class T, class Allocator, class Options>
class deque;

template <class T>
struct deque_value_traits
{
   typedef T value_type;
   static const bool trivial_dctr = dtl::is_trivially_destructible<value_type>::value;
   static const bool trivial_dctr_after_move = ::boost::has_trivial_destructor_after_move<value_type>::value;
};

template<class T, std::size_t BlockBytes, std::size_t BlockSize>
struct deque_block_size
{
   BOOST_STATIC_ASSERT_MSG(!(BlockBytes && BlockSize), "BlockBytes and BlockSize can't be specified at the same time");
   static const std::size_t block_bytes = BlockBytes ? BlockBytes : 512u;
   static const std::size_t value       = BlockSize ? BlockSize : (sizeof(T) < block_bytes ? (block_bytes/sizeof(T)) : std::size_t(1));
};

namespace dtl {

// Class invariants:
//  For any nonsingular iterator i:
//    i.node is the address of an element in the map array.  The
//      contents of i.node is a pointer to the beginning of a node.
//    i.first == //(i.node)
//    i.last  == i.first + node_size
//    i.cur is a pointer in the range [i.first, i.last).  NOTE:
//      the implication of this is that i.cur is always a dereferenceable
//      pointer, even if i is a past-the-end iterator.
//  Start and Finish are always nonsingular iterators.  NOTE: this means
//    that an empty deque must have one node, and that a deque
//    with N elements, where N is the buffer size, must have two nodes.
//  For every node other than start.node and finish.node, every element
//    in the node is an initialized object.  If start.node == finish.node,
//    then [start.cur, finish.cur) are initialized objects, and
//    the elements outside that range are uninitialized storage.  Otherwise,
//    [start.cur, start.last) and [finish.first, finish.cur) are initialized
//    objects, and [start.first, start.cur) and [finish.cur, finish.last)
//    are uninitialized storage.
//  [map, map + map_size) is a valid, non-empty range.
//  [start.node, finish.node] is a valid range contained within
//    [map, map + map_size).
//  A pointer in the range [map, map + map_size) points to an allocated node
//    if and only if the pointer is in the range [start.node, finish.node].
template<class Pointer, bool IsConst>
class deque_iterator
{
   public:
   typedef std::random_access_iterator_tag                                          iterator_category;
   typedef typename boost::intrusive::pointer_traits<Pointer>::element_type         value_type;
   typedef typename boost::intrusive::pointer_traits<Pointer>::difference_type      difference_type;
   typedef typename if_c
      < IsConst
      , typename boost::intrusive::pointer_traits<Pointer>::template
                                 rebind_pointer<const value_type>::type
      , Pointer
      >::type                                                                       pointer;
   typedef typename if_c
      < IsConst
      , const value_type&
      , value_type&
      >::type                                                                       reference;

   class nat;
   typedef typename dtl::if_c< IsConst
                             , deque_iterator<Pointer, false>
                             , nat>::type                                           nonconst_iterator;

   typedef Pointer                                                                  val_alloc_ptr;
   typedef typename boost::intrusive::pointer_traits<Pointer>::
      template rebind_pointer<Pointer>::type                                        index_pointer;

   Pointer m_cur;
   Pointer m_first;
   Pointer m_last;
   index_pointer  m_node;

   public:

   BOOST_CONTAINER_FORCEINLINE Pointer get_cur()          const  {  return m_cur;  }
   BOOST_CONTAINER_FORCEINLINE Pointer get_first()        const  {  return m_first;  }
   BOOST_CONTAINER_FORCEINLINE Pointer get_last()         const  {  return m_last;  }
   BOOST_CONTAINER_FORCEINLINE index_pointer get_node()   const  {  return m_node;  }

   BOOST_CONTAINER_FORCEINLINE deque_iterator(val_alloc_ptr x, index_pointer y, difference_type block_size) BOOST_NOEXCEPT_OR_NOTHROW
      : m_cur(x), m_first(*y), m_last(*y + block_size), m_node(y)
   {}

   BOOST_CONTAINER_FORCEINLINE deque_iterator() BOOST_NOEXCEPT_OR_NOTHROW
      : m_cur(), m_first(), m_last(), m_node()  //Value initialization to achieve "null iterators" (N3644)
   {}

   BOOST_CONTAINER_FORCEINLINE deque_iterator(const deque_iterator& x) BOOST_NOEXCEPT_OR_NOTHROW
      : m_cur(x.get_cur()), m_first(x.get_first()), m_last(x.get_last()), m_node(x.get_node())
   {}

   BOOST_CONTAINER_FORCEINLINE deque_iterator(const nonconst_iterator& x) BOOST_NOEXCEPT_OR_NOTHROW
      : m_cur(x.get_cur()), m_first(x.get_first()), m_last(x.get_last()), m_node(x.get_node())
   {}

   BOOST_CONTAINER_FORCEINLINE deque_iterator(Pointer cur, Pointer first, Pointer last, index_pointer node) BOOST_NOEXCEPT_OR_NOTHROW
      : m_cur(cur), m_first(first), m_last(last), m_node(node)
   {}

   BOOST_CONTAINER_FORCEINLINE deque_iterator& operator=(const deque_iterator& x) BOOST_NOEXCEPT_OR_NOTHROW
   {  m_cur = x.get_cur(); m_first = x.get_first(); m_last = x.get_last(); m_node = x.get_node(); return *this; }

   BOOST_CONTAINER_FORCEINLINE deque_iterator<Pointer, false> unconst() const BOOST_NOEXCEPT_OR_NOTHROW
   {
      return deque_iterator<Pointer, false>(this->get_cur(), this->get_first(), this->get_last(), this->get_node());
   }

   BOOST_CONTAINER_FORCEINLINE reference operator*() const BOOST_NOEXCEPT_OR_NOTHROW
      { return *this->m_cur; }

   BOOST_CONTAINER_FORCEINLINE pointer operator->() const BOOST_NOEXCEPT_OR_NOTHROW
      { return this->m_cur; }

   difference_type operator-(const deque_iterator& x) const BOOST_NOEXCEPT_OR_NOTHROW
   {
      if(!this->m_cur && !x.m_cur){
         return 0;
      }
      const difference_type block_size = this->m_last - this->m_first;
      BOOST_ASSERT(block_size);
      return block_size * (this->m_node - x.m_node - 1) +
         (this->m_cur - this->m_first) + (x.m_last - x.m_cur);
   }

   deque_iterator& operator++() BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(!!m_cur);
      ++this->m_cur;
      if (this->m_cur == this->m_last) {
         const difference_type block_size = m_last - m_first;
         BOOST_ASSERT(block_size);
         this->priv_set_node(this->m_node + 1, block_size);
         this->m_cur = this->m_first;
      }
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE deque_iterator operator++(int) BOOST_NOEXCEPT_OR_NOTHROW
   {
      deque_iterator tmp(*this);
      ++*this;
      return tmp;
   }

   deque_iterator& operator--() BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(!!m_cur);
      if (this->m_cur == this->m_first) {
         const difference_type block_size = m_last - m_first;
         BOOST_ASSERT(block_size);
         this->priv_set_node(this->m_node - 1, block_size);
         this->m_cur = this->m_last;
      }
      --this->m_cur;
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE deque_iterator operator--(int) BOOST_NOEXCEPT_OR_NOTHROW
   {
      deque_iterator tmp(*this);
      --*this;
      return tmp;
   }

   deque_iterator& operator+=(difference_type n) BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(!!m_cur);
      difference_type offset = n + (this->m_cur - this->m_first);
      const difference_type block_size = this->m_last - this->m_first;
      BOOST_ASSERT(block_size);
      if (offset >= 0 && offset < block_size)
         this->m_cur += n;
      else {
         difference_type node_offset =
         offset > 0 ? (offset / block_size)
                    : (-difference_type((-offset - 1) / block_size) - 1);
         this->priv_set_node(this->m_node + node_offset, block_size);
         this->m_cur = this->m_first +
         (offset - node_offset * block_size);
      }
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE deque_iterator operator+(difference_type n) const BOOST_NOEXCEPT_OR_NOTHROW
      {  deque_iterator tmp(*this); return tmp += n;  }

   BOOST_CONTAINER_FORCEINLINE deque_iterator& operator-=(difference_type n) BOOST_NOEXCEPT_OR_NOTHROW
      { return *this += -n; }

   BOOST_CONTAINER_FORCEINLINE deque_iterator operator-(difference_type n) const BOOST_NOEXCEPT_OR_NOTHROW
      {  deque_iterator tmp(*this); return tmp -= n;  }

   BOOST_CONTAINER_FORCEINLINE reference operator[](difference_type n) const BOOST_NOEXCEPT_OR_NOTHROW
      { return *(*this + n); }

   BOOST_CONTAINER_FORCEINLINE friend bool operator==(const deque_iterator& l, const deque_iterator& r) BOOST_NOEXCEPT_OR_NOTHROW
      { return l.m_cur == r.m_cur; }

   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(const deque_iterator& l, const deque_iterator& r) BOOST_NOEXCEPT_OR_NOTHROW
      { return l.m_cur != r.m_cur; }

   BOOST_CONTAINER_FORCEINLINE friend bool operator<(const deque_iterator& l, const deque_iterator& r) BOOST_NOEXCEPT_OR_NOTHROW
      {  return (l.m_node == r.m_node) ? (l.m_cur < r.m_cur) : (l.m_node < r.m_node);  }

   BOOST_CONTAINER_FORCEINLINE friend bool operator>(const deque_iterator& l, const deque_iterator& r) BOOST_NOEXCEPT_OR_NOTHROW
      { return r < l; }

   BOOST_CONTAINER_FORCEINLINE friend bool operator<=(const deque_iterator& l, const deque_iterator& r) BOOST_NOEXCEPT_OR_NOTHROW
      { return !(r < l); }

   BOOST_CONTAINER_FORCEINLINE friend bool operator>=(const deque_iterator& l, const deque_iterator& r) BOOST_NOEXCEPT_OR_NOTHROW
      { return !(l < r); }

   BOOST_CONTAINER_FORCEINLINE void priv_set_node(index_pointer new_node, difference_type block_size) BOOST_NOEXCEPT_OR_NOTHROW
   {
      this->m_node = new_node;
      this->m_first = *new_node;
      this->m_last = this->m_first + block_size;
   }

   BOOST_CONTAINER_FORCEINLINE friend deque_iterator operator+(difference_type n, deque_iterator x) BOOST_NOEXCEPT_OR_NOTHROW
      {  return x += n;  }
};

}  //namespace dtl {

template<class Options>
struct get_deque_opt
{
   typedef Options type;
};

template<>
struct get_deque_opt<void>
{
   typedef deque_null_opt type;
};

// Deque base class.  It has two purposes.  First, its constructor
//  and destructor allocate (but don't initialize) storage.  This makes
//  exception safety easier.
template <class Allocator, class Options>
class deque_base
{
   BOOST_COPYABLE_AND_MOVABLE(deque_base)
   public:
   typedef allocator_traits<Allocator>                            val_alloc_traits_type;
   typedef typename val_alloc_traits_type::value_type             val_alloc_val;
   typedef typename val_alloc_traits_type::pointer                val_alloc_ptr;
   typedef typename val_alloc_traits_type::const_pointer          val_alloc_cptr;
   typedef typename val_alloc_traits_type::reference              val_alloc_ref;
   typedef typename val_alloc_traits_type::const_reference        val_alloc_cref;
   typedef typename val_alloc_traits_type::difference_type        val_alloc_diff;
   typedef typename val_alloc_traits_type::size_type              val_alloc_size;
   typedef typename val_alloc_traits_type::template
      portable_rebind_alloc<val_alloc_ptr>::type                  ptr_alloc_t;
   typedef allocator_traits<ptr_alloc_t>                          ptr_alloc_traits_type;
   typedef typename ptr_alloc_traits_type::value_type             ptr_alloc_val;
   typedef typename ptr_alloc_traits_type::pointer                ptr_alloc_ptr;
   typedef typename ptr_alloc_traits_type::const_pointer          ptr_alloc_cptr;
   typedef typename ptr_alloc_traits_type::reference              ptr_alloc_ref;
   typedef typename ptr_alloc_traits_type::const_reference        ptr_alloc_cref;
   typedef Allocator                                                      allocator_type;
   typedef allocator_type                                         stored_allocator_type;
   typedef val_alloc_size                                         size_type;

   private:
   typedef typename get_deque_opt<Options>::type                  options_type;

   protected:
   typedef dtl::deque_iterator<val_alloc_ptr, false> iterator;
   typedef dtl::deque_iterator<val_alloc_ptr, true > const_iterator;

   BOOST_CONSTEXPR BOOST_CONTAINER_FORCEINLINE static size_type get_block_size() BOOST_NOEXCEPT_OR_NOTHROW
      { return deque_block_size<val_alloc_val, options_type::block_bytes, options_type::block_size>::value; }

   typedef deque_value_traits<val_alloc_val>             traits_t;
   typedef ptr_alloc_t                                   map_allocator_type;

   BOOST_CONTAINER_FORCEINLINE val_alloc_ptr priv_allocate_node()
      {  return this->alloc().allocate(get_block_size());  }

   BOOST_CONTAINER_FORCEINLINE void priv_deallocate_node(val_alloc_ptr p) BOOST_NOEXCEPT_OR_NOTHROW
      {  this->alloc().deallocate(p, get_block_size());  }

   BOOST_CONTAINER_FORCEINLINE ptr_alloc_ptr priv_allocate_map(size_type n)
      { return this->ptr_alloc().allocate(n); }

   BOOST_CONTAINER_FORCEINLINE void priv_deallocate_map(ptr_alloc_ptr p, size_type n) BOOST_NOEXCEPT_OR_NOTHROW
      { this->ptr_alloc().deallocate(p, n); }

   BOOST_CONTAINER_FORCEINLINE deque_base(size_type num_elements, const allocator_type& a)
      :  members_(a)
   { this->priv_initialize_map(num_elements); }

   BOOST_CONTAINER_FORCEINLINE explicit deque_base(const allocator_type& a)
      :  members_(a)
   {}

   BOOST_CONTAINER_FORCEINLINE deque_base()
      :  members_()
   {}

   BOOST_CONTAINER_FORCEINLINE explicit deque_base(BOOST_RV_REF(deque_base) x)
      :  members_( boost::move(x.ptr_alloc())
                 , boost::move(x.alloc()) )
   {}

   ~deque_base()
   {
      if (this->members_.m_map) {
         this->priv_destroy_nodes(this->members_.m_start.m_node, this->members_.m_finish.m_node + 1);
         this->priv_deallocate_map(this->members_.m_map, this->members_.m_map_size);
      }
   }

   private:
   deque_base(const deque_base&);

   protected:

   void swap_members(deque_base &x) BOOST_NOEXCEPT_OR_NOTHROW
   {
      ::boost::adl_move_swap(this->members_.m_start, x.members_.m_start);
      ::boost::adl_move_swap(this->members_.m_finish, x.members_.m_finish);
      ::boost::adl_move_swap(this->members_.m_map, x.members_.m_map);
      ::boost::adl_move_swap(this->members_.m_map_size, x.members_.m_map_size);
   }

   void priv_initialize_map(size_type num_elements)
   {
//      if(num_elements){
         size_type num_nodes = num_elements / get_block_size() + 1;

         this->members_.m_map_size = dtl::max_value((size_type) InitialMapSize, num_nodes + 2);
         this->members_.m_map = this->priv_allocate_map(this->members_.m_map_size);

         ptr_alloc_ptr nstart = this->members_.m_map + (this->members_.m_map_size - num_nodes) / 2;
         ptr_alloc_ptr nfinish = nstart + num_nodes;

         BOOST_TRY {
            this->priv_create_nodes(nstart, nfinish);
         }
         BOOST_CATCH(...){
            this->priv_deallocate_map(this->members_.m_map, this->members_.m_map_size);
            this->members_.m_map = 0;
            this->members_.m_map_size = 0;
            BOOST_RETHROW
         }
         BOOST_CATCH_END

         this->members_.m_start.priv_set_node(nstart, get_block_size());
         this->members_.m_finish.priv_set_node(nfinish - 1, get_block_size());
         this->members_.m_start.m_cur = this->members_.m_start.m_first;
         this->members_.m_finish.m_cur = this->members_.m_finish.m_first +
                        num_elements % get_block_size();
//      }
   }

   void priv_create_nodes(ptr_alloc_ptr nstart, ptr_alloc_ptr nfinish)
   {
      ptr_alloc_ptr cur = nstart;
      BOOST_TRY {
         for (; cur < nfinish; ++cur)
            *cur = this->priv_allocate_node();
      }
      BOOST_CATCH(...){
         this->priv_destroy_nodes(nstart, cur);
         BOOST_RETHROW
      }
      BOOST_CATCH_END
   }

   void priv_destroy_nodes(ptr_alloc_ptr nstart, ptr_alloc_ptr nfinish) BOOST_NOEXCEPT_OR_NOTHROW
   {
      for (ptr_alloc_ptr n = nstart; n < nfinish; ++n)
         this->priv_deallocate_node(*n);
   }

   void priv_clear_map() BOOST_NOEXCEPT_OR_NOTHROW
   {
      if (this->members_.m_map) {
         this->priv_destroy_nodes(this->members_.m_start.m_node, this->members_.m_finish.m_node + 1);
         this->priv_deallocate_map(this->members_.m_map, this->members_.m_map_size);
         this->members_.m_map = 0;
         this->members_.m_map_size = 0;
         this->members_.m_start = iterator();
         this->members_.m_finish = this->members_.m_start;
      }
   }

   enum { InitialMapSize = 8 };

   protected:
   struct members_holder
      :  public ptr_alloc_t
      ,  public allocator_type
   {
      members_holder()
         :  map_allocator_type(), allocator_type()
         ,  m_map(0), m_map_size(0)
         ,  m_start(), m_finish(m_start)
      {}

      explicit members_holder(const allocator_type &a)
         :  map_allocator_type(a), allocator_type(a)
         ,  m_map(0), m_map_size(0)
         ,  m_start(), m_finish(m_start)
      {}

      template<class ValAllocConvertible, class PtrAllocConvertible>
      members_holder(BOOST_FWD_REF(PtrAllocConvertible) pa, BOOST_FWD_REF(ValAllocConvertible) va)
         : map_allocator_type(boost::forward<PtrAllocConvertible>(pa))
         , allocator_type    (boost::forward<ValAllocConvertible>(va))
         , m_map(0), m_map_size(0)
         , m_start(), m_finish(m_start)
      {}

      ptr_alloc_ptr   m_map;
      val_alloc_size  m_map_size;
      iterator        m_start;
      iterator        m_finish;
   } members_;

   BOOST_CONTAINER_FORCEINLINE ptr_alloc_t &ptr_alloc() BOOST_NOEXCEPT_OR_NOTHROW
   {  return members_;  }

   BOOST_CONTAINER_FORCEINLINE const ptr_alloc_t &ptr_alloc() const BOOST_NOEXCEPT_OR_NOTHROW
   {  return members_;  }

   BOOST_CONTAINER_FORCEINLINE allocator_type &alloc() BOOST_NOEXCEPT_OR_NOTHROW
   {  return members_;  }

   BOOST_CONTAINER_FORCEINLINE const allocator_type &alloc() const BOOST_NOEXCEPT_OR_NOTHROW
   {  return members_;  }
};
#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

#ifdef BOOST_CONTAINER_DOXYGEN_INVOKED
//! A double-ended queue is a sequence that supports random access to elements, constant time insertion
//! and removal of elements at the end of the sequence, and linear time insertion and removal of elements in the middle.
//!
//! \tparam T The type of object that is stored in the deque
//! \tparam A The allocator used for all internal memory management, use void
//!   for the default allocator
//! \tparam Options A type produced from \c boost::container::deque_options.
template <class T, class Allocator = void, class Options = void>
#else
template <class T, class Allocator, class Options>
#endif
class deque : protected deque_base<typename real_allocator<T, Allocator>::type, Options>
{
   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   private:
   typedef deque_base<typename real_allocator<T, Allocator>::type, Options> Base;
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   typedef typename real_allocator<T, Allocator>::type ValAllocator;

   public:

   //////////////////////////////////////////////
   //
   //                    types
   //
   //////////////////////////////////////////////

   typedef T                                                                           value_type;
   typedef ValAllocator                                                                allocator_type;
   typedef typename ::boost::container::allocator_traits<ValAllocator>::pointer           pointer;
   typedef typename ::boost::container::allocator_traits<ValAllocator>::const_pointer     const_pointer;
   typedef typename ::boost::container::allocator_traits<ValAllocator>::reference         reference;
   typedef typename ::boost::container::allocator_traits<ValAllocator>::const_reference   const_reference;
   typedef typename ::boost::container::allocator_traits<ValAllocator>::size_type         size_type;
   typedef typename ::boost::container::allocator_traits<ValAllocator>::difference_type   difference_type;
   typedef BOOST_CONTAINER_IMPDEF(allocator_type)                                      stored_allocator_type;
   typedef BOOST_CONTAINER_IMPDEF(typename Base::iterator)                             iterator;
   typedef BOOST_CONTAINER_IMPDEF(typename Base::const_iterator)                       const_iterator;
   typedef BOOST_CONTAINER_IMPDEF(boost::container::reverse_iterator<iterator>)        reverse_iterator;
   typedef BOOST_CONTAINER_IMPDEF(boost::container::reverse_iterator<const_iterator>)  const_reverse_iterator;

   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   private:                      // Internal typedefs
   BOOST_COPYABLE_AND_MOVABLE(deque)
   typedef typename Base::ptr_alloc_ptr index_pointer;
   typedef allocator_traits<ValAllocator>                  allocator_traits_type;

   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   public:

   BOOST_CONSTEXPR BOOST_CONTAINER_FORCEINLINE static size_type get_block_size() BOOST_NOEXCEPT_OR_NOTHROW
      { return Base::get_block_size(); }

   //////////////////////////////////////////////
   //
   //          construct/copy/destroy
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Default constructors a deque.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor throws.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE deque() BOOST_NOEXCEPT_IF(dtl::is_nothrow_default_constructible<ValAllocator>::value)
      : Base()
   {}

   //! <b>Effects</b>: Constructs a deque taking the allocator as parameter.
   //!
   //! <b>Throws</b>: Nothing
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE explicit deque(const allocator_type& a) BOOST_NOEXCEPT_OR_NOTHROW
      : Base(a)
   {}

   //! <b>Effects</b>: Constructs a deque
   //!   and inserts n value initialized values.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's value initialization throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   BOOST_CONTAINER_FORCEINLINE explicit deque(size_type n)
      : Base(n, allocator_type())
   {
      dtl::insert_value_initialized_n_proxy<ValAllocator, iterator> proxy;
      proxy.uninitialized_copy_n_and_update(this->alloc(), this->begin(), n);
      //deque_base will deallocate in case of exception...
   }

   //! <b>Effects</b>: Constructs a deque
   //!   and inserts n default initialized values.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's default initialization or copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   //!
   //! <b>Note</b>: Non-standard extension
   BOOST_CONTAINER_FORCEINLINE deque(size_type n, default_init_t)
      : Base(n, allocator_type())
   {
      dtl::insert_default_initialized_n_proxy<ValAllocator, iterator> proxy;
      proxy.uninitialized_copy_n_and_update(this->alloc(), this->begin(), n);
      //deque_base will deallocate in case of exception...
   }

   //! <b>Effects</b>: Constructs a deque that will use a copy of allocator a
   //!   and inserts n value initialized values.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's value initialization throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   BOOST_CONTAINER_FORCEINLINE explicit deque(size_type n, const allocator_type &a)
      : Base(n, a)
   {
      dtl::insert_value_initialized_n_proxy<ValAllocator, iterator> proxy;
      proxy.uninitialized_copy_n_and_update(this->alloc(), this->begin(), n);
      //deque_base will deallocate in case of exception...
   }

   //! <b>Effects</b>: Constructs a deque that will use a copy of allocator a
   //!   and inserts n default initialized values.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's default initialization or copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   //!
   //! <b>Note</b>: Non-standard extension
   BOOST_CONTAINER_FORCEINLINE deque(size_type n, default_init_t, const allocator_type &a)
      : Base(n, a)
   {
      dtl::insert_default_initialized_n_proxy<ValAllocator, iterator> proxy;
      proxy.uninitialized_copy_n_and_update(this->alloc(), this->begin(), n);
      //deque_base will deallocate in case of exception...
   }

   //! <b>Effects</b>: Constructs a deque that will use a copy of allocator a
   //!   and inserts n copies of value.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   BOOST_CONTAINER_FORCEINLINE deque(size_type n, const value_type& value)
      : Base(n, allocator_type())
   { this->priv_fill_initialize(value); }

   //! <b>Effects</b>: Constructs a deque that will use a copy of allocator a
   //!   and inserts n copies of value.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   BOOST_CONTAINER_FORCEINLINE deque(size_type n, const value_type& value, const allocator_type& a)
      : Base(n, a)
   { this->priv_fill_initialize(value); }

   //! <b>Effects</b>: Constructs a deque that will use a copy of allocator a
   //!   and inserts a copy of the range [first, last) in the deque.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's constructor taking a dereferenced InIt throws.
   //!
   //! <b>Complexity</b>: Linear to the range [first, last).
   template <class InIt>
   BOOST_CONTAINER_FORCEINLINE deque(InIt first, InIt last
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_convertible
         <InIt, size_type>::type * = 0
      #endif
      )
      : Base(allocator_type())
   {
      this->priv_range_initialize(first, last);
   }

   //! <b>Effects</b>: Constructs a deque that will use a copy of allocator a
   //!   and inserts a copy of the range [first, last) in the deque.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's constructor taking a dereferenced InIt throws.
   //!
   //! <b>Complexity</b>: Linear to the range [first, last).
   template <class InIt>
   BOOST_CONTAINER_FORCEINLINE deque(InIt first, InIt last, const allocator_type& a
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_convertible
         <InIt, size_type>::type * = 0
      #endif
      )
      : Base(a)
   {
      this->priv_range_initialize(first, last);
   }

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Effects</b>: Constructs a deque that will use a copy of allocator a
   //!   and inserts a copy of the range [il.begin(), il.end()) in the deque.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's constructor taking a dereferenced std::initializer_list iterator throws.
   //!
   //! <b>Complexity</b>: Linear to the range [il.begin(), il.end()).
   BOOST_CONTAINER_FORCEINLINE deque(std::initializer_list<value_type> il, const allocator_type& a = allocator_type())
      : Base(a)
   {
      this->priv_range_initialize(il.begin(), il.end());
   }
#endif

   //! <b>Effects</b>: Copy constructs a deque.
   //!
   //! <b>Postcondition</b>: x == *this.
   //!
   //! <b>Complexity</b>: Linear to the elements x contains.
   BOOST_CONTAINER_FORCEINLINE deque(const deque& x)
      :  Base(allocator_traits_type::select_on_container_copy_construction(x.alloc()))
   {
      if(x.size()){
         this->priv_initialize_map(x.size());
         boost::container::uninitialized_copy_alloc
            (this->alloc(), x.begin(), x.end(), this->members_.m_start);
      }
   }

   //! <b>Effects</b>: Move constructor. Moves x's resources to *this.
   //!
   //! <b>Throws</b>: If allocator_type's copy constructor throws.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE deque(BOOST_RV_REF(deque) x) BOOST_NOEXCEPT_OR_NOTHROW
      :  Base(BOOST_MOVE_BASE(Base, x))
   {  this->swap_members(x);   }

   //! <b>Effects</b>: Copy constructs a vector using the specified allocator.
   //!
   //! <b>Postcondition</b>: x == *this.
   //!
   //! <b>Throws</b>: If allocation
   //!   throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the elements x contains.
   deque(const deque& x, const allocator_type &a)
      :  Base(a)
   {
      if(x.size()){
         this->priv_initialize_map(x.size());
         boost::container::uninitialized_copy_alloc
            (this->alloc(), x.begin(), x.end(), this->members_.m_start);
      }
   }

   //! <b>Effects</b>: Move constructor using the specified allocator.
   //!                 Moves x's resources to *this if a == allocator_type().
   //!                 Otherwise copies values from x to *this.
   //!
   //! <b>Throws</b>: If allocation or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Constant if a == x.get_allocator(), linear otherwise.
   deque(BOOST_RV_REF(deque) x, const allocator_type &a)
      :  Base(a)
   {
      if(x.alloc() == a){
         this->swap_members(x);
      }
      else{
         if(x.size()){
            this->priv_initialize_map(x.size());
            boost::container::uninitialized_copy_alloc
               ( this->alloc(), boost::make_move_iterator(x.begin())
               , boost::make_move_iterator(x.end()), this->members_.m_start);
         }
      }
   }

   //! <b>Effects</b>: Destroys the deque. All stored values are destroyed
   //!   and used memory is deallocated.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements.
   BOOST_CONTAINER_FORCEINLINE ~deque() BOOST_NOEXCEPT_OR_NOTHROW
   {
      this->priv_destroy_range(this->members_.m_start, this->members_.m_finish);
   }

   //! <b>Effects</b>: Makes *this contain the same elements as x.
   //!
   //! <b>Postcondition</b>: this->size() == x.size(). *this contains a copy
   //! of each of x's elements.
   //!
   //! <b>Throws</b>: If memory allocation throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in x.
   deque& operator= (BOOST_COPY_ASSIGN_REF(deque) x)
   {
      if (BOOST_LIKELY(&x != this)){
         allocator_type &this_alloc     = this->alloc();
         const allocator_type &x_alloc  = x.alloc();
         dtl::bool_<allocator_traits_type::
            propagate_on_container_copy_assignment::value> flag;
         if(flag && this_alloc != x_alloc){
            this->clear();
            this->shrink_to_fit();
         }
         dtl::assign_alloc(this->alloc(), x.alloc(), flag);
         dtl::assign_alloc(this->ptr_alloc(), x.ptr_alloc(), flag);
         this->assign(x.cbegin(), x.cend());
      }
      return *this;
   }

   //! <b>Effects</b>: Move assignment. All x's values are transferred to *this.
   //!
   //! <b>Throws</b>: If allocator_traits_type::propagate_on_container_move_assignment
   //!   is false and (allocation throws or value_type's move constructor throws)
   //!
   //! <b>Complexity</b>: Constant if allocator_traits_type::
   //!   propagate_on_container_move_assignment is true or
   //!   this->get>allocator() == x.get_allocator(). Linear otherwise.
   deque& operator= (BOOST_RV_REF(deque) x)
      BOOST_NOEXCEPT_IF(allocator_traits_type::propagate_on_container_move_assignment::value
                                  || allocator_traits_type::is_always_equal::value)
   {
      if (BOOST_LIKELY(this != &x)) {
         allocator_type &this_alloc = this->alloc();
         allocator_type &x_alloc    = x.alloc();
         const bool propagate_alloc = allocator_traits_type::
               propagate_on_container_move_assignment::value;
         dtl::bool_<propagate_alloc> flag;
         const bool allocators_equal = this_alloc == x_alloc; (void)allocators_equal;
         //Resources can be transferred if both allocators are
         //going to be equal after this function (either propagated or already equal)
         if(propagate_alloc || allocators_equal){
            //Destroy objects but retain memory in case x reuses it in the future
            this->clear();
            //Move allocator if needed
            dtl::move_alloc(this_alloc, x_alloc, flag);
            dtl::move_alloc(this->ptr_alloc(), x.ptr_alloc(), flag);
            //Nothrow swap
            this->swap_members(x);
         }
         //Else do a one by one move
         else{
            this->assign( boost::make_move_iterator(x.begin())
                        , boost::make_move_iterator(x.end()));
         }
      }
      return *this;
   }

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Effects</b>: Makes *this contain the same elements as il.
   //!
   //! <b>Postcondition</b>: this->size() == il.size(). *this contains a copy
   //! of each of x's elements.
   //!
   //! <b>Throws</b>: If memory allocation throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in il.
   BOOST_CONTAINER_FORCEINLINE deque& operator=(std::initializer_list<value_type> il)
   {
      this->assign(il.begin(), il.end());
      return *this;
   }
#endif

   //! <b>Effects</b>: Assigns the n copies of val to *this.
   //!
   //! <b>Throws</b>: If memory allocation throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   BOOST_CONTAINER_FORCEINLINE void assign(size_type n, const T& val)
   {
      typedef constant_iterator<value_type, difference_type> c_it;
      this->assign(c_it(val, n), c_it());
   }

   //! <b>Effects</b>: Assigns the the range [first, last) to *this.
   //!
   //! <b>Throws</b>: If memory allocation throws or
   //!   T's constructor from dereferencing InIt throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   template <class InIt>
   void assign(InIt first, InIt last
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_or
         < void
         , dtl::is_convertible<InIt, size_type>
         , dtl::is_not_input_iterator<InIt>
         >::type * = 0
      #endif
      )
   {
      iterator cur = this->begin();
      for ( ; first != last && cur != end(); ++cur, ++first){
         *cur = *first;
      }
      if (first == last){
         this->erase(cur, this->cend());
      }
      else{
         this->insert(this->cend(), first, last);
      }
   }

   #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
   template <class FwdIt>
   void assign(FwdIt first, FwdIt last
      , typename dtl::disable_if_or
         < void
         , dtl::is_convertible<FwdIt, size_type>
         , dtl::is_input_iterator<FwdIt>
         >::type * = 0
      )
   {
      const size_type len = boost::container::iterator_distance(first, last);
      if (len > size()) {
         FwdIt mid = first;
         boost::container::iterator_advance(mid, this->size());
         boost::container::copy(first, mid, begin());
         this->insert(this->cend(), mid, last);
      }
      else{
         this->erase(boost::container::copy(first, last, this->begin()), cend());
      }
   }
   #endif

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Effects</b>: Assigns the the range [il.begin(), il.end()) to *this.
   //!
   //! <b>Throws</b>: If memory allocation throws or
   //!   T's constructor from dereferencing std::initializer_list iterator throws.
   //!
   //! <b>Complexity</b>: Linear to il.size().
   BOOST_CONTAINER_FORCEINLINE void assign(std::initializer_list<value_type> il)
   {   this->assign(il.begin(), il.end());   }
#endif

   //! <b>Effects</b>: Returns a copy of the internal allocator.
   //!
   //! <b>Throws</b>: If allocator's copy constructor throws.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE allocator_type get_allocator() const BOOST_NOEXCEPT_OR_NOTHROW
   { return Base::alloc(); }

   //! <b>Effects</b>: Returns a reference to the internal allocator.
   //!
   //! <b>Throws</b>: Nothing
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Non-standard extension.
   BOOST_CONTAINER_FORCEINLINE const stored_allocator_type &get_stored_allocator() const BOOST_NOEXCEPT_OR_NOTHROW
   {  return Base::alloc(); }

   //////////////////////////////////////////////
   //
   //                iterators
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Returns a reference to the internal allocator.
   //!
   //! <b>Throws</b>: Nothing
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Non-standard extension.
   BOOST_CONTAINER_FORCEINLINE stored_allocator_type &get_stored_allocator() BOOST_NOEXCEPT_OR_NOTHROW
   {  return Base::alloc(); }

   //! <b>Effects</b>: Returns an iterator to the first element contained in the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE iterator begin() BOOST_NOEXCEPT_OR_NOTHROW
      { return this->members_.m_start; }

   //! <b>Effects</b>: Returns a const_iterator to the first element contained in the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_iterator begin() const BOOST_NOEXCEPT_OR_NOTHROW
      { return this->members_.m_start; }

   //! <b>Effects</b>: Returns an iterator to the end of the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE iterator end() BOOST_NOEXCEPT_OR_NOTHROW
      { return this->members_.m_finish; }

   //! <b>Effects</b>: Returns a const_iterator to the end of the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_iterator end() const BOOST_NOEXCEPT_OR_NOTHROW
      { return this->members_.m_finish; }

   //! <b>Effects</b>: Returns a reverse_iterator pointing to the beginning
   //! of the reversed deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE reverse_iterator rbegin() BOOST_NOEXCEPT_OR_NOTHROW
      { return reverse_iterator(this->members_.m_finish); }

   //! <b>Effects</b>: Returns a const_reverse_iterator pointing to the beginning
   //! of the reversed deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_reverse_iterator rbegin() const BOOST_NOEXCEPT_OR_NOTHROW
      { return const_reverse_iterator(this->members_.m_finish); }

   //! <b>Effects</b>: Returns a reverse_iterator pointing to the end
   //! of the reversed deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE reverse_iterator rend() BOOST_NOEXCEPT_OR_NOTHROW
      { return reverse_iterator(this->members_.m_start); }

   //! <b>Effects</b>: Returns a const_reverse_iterator pointing to the end
   //! of the reversed deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_reverse_iterator rend() const BOOST_NOEXCEPT_OR_NOTHROW
      { return const_reverse_iterator(this->members_.m_start); }

   //! <b>Effects</b>: Returns a const_iterator to the first element contained in the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_iterator cbegin() const BOOST_NOEXCEPT_OR_NOTHROW
      { return this->members_.m_start; }

   //! <b>Effects</b>: Returns a const_iterator to the end of the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_iterator cend() const BOOST_NOEXCEPT_OR_NOTHROW
      { return this->members_.m_finish; }

   //! <b>Effects</b>: Returns a const_reverse_iterator pointing to the beginning
   //! of the reversed deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_reverse_iterator crbegin() const BOOST_NOEXCEPT_OR_NOTHROW
      { return const_reverse_iterator(this->members_.m_finish); }

   //! <b>Effects</b>: Returns a const_reverse_iterator pointing to the end
   //! of the reversed deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_reverse_iterator crend() const BOOST_NOEXCEPT_OR_NOTHROW
      { return const_reverse_iterator(this->members_.m_start); }

   //////////////////////////////////////////////
   //
   //                capacity
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Returns true if the deque contains no elements.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE bool empty() const BOOST_NOEXCEPT_OR_NOTHROW
   { return this->members_.m_finish == this->members_.m_start; }

   //! <b>Effects</b>: Returns the number of the elements contained in the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE size_type size() const BOOST_NOEXCEPT_OR_NOTHROW
      { return this->members_.m_finish - this->members_.m_start; }

   //! <b>Effects</b>: Returns the largest possible size of the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE size_type max_size() const BOOST_NOEXCEPT_OR_NOTHROW
      { return allocator_traits_type::max_size(this->alloc()); }

   //! <b>Effects</b>: Inserts or erases elements at the end such that
   //!   the size becomes n. New elements are value initialized.
   //!
   //! <b>Throws</b>: If memory allocation throws, or T's constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the difference between size() and new_size.
   void resize(size_type new_size)
   {
      const size_type len = size();
      if (new_size < len)
         this->priv_erase_last_n(len - new_size);
      else{
         const size_type n = new_size - this->size();
         dtl::insert_value_initialized_n_proxy<ValAllocator, iterator> proxy;
         priv_insert_back_aux_impl(n, proxy);
      }
   }

   //! <b>Effects</b>: Inserts or erases elements at the end such that
   //!   the size becomes n. New elements are default initialized.
   //!
   //! <b>Throws</b>: If memory allocation throws, or T's constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the difference between size() and new_size.
   //!
   //! <b>Note</b>: Non-standard extension
   void resize(size_type new_size, default_init_t)
   {
      const size_type len = size();
      if (new_size < len)
         this->priv_erase_last_n(len - new_size);
      else{
         const size_type n = new_size - this->size();
         dtl::insert_default_initialized_n_proxy<ValAllocator, iterator> proxy;
         priv_insert_back_aux_impl(n, proxy);
      }
   }

   //! <b>Effects</b>: Inserts or erases elements at the end such that
   //!   the size becomes n. New elements are copy constructed from x.
   //!
   //! <b>Throws</b>: If memory allocation throws, or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the difference between size() and new_size.
   void resize(size_type new_size, const value_type& x)
   {
      const size_type len = size();
      if (new_size < len)
         this->erase(this->members_.m_start + new_size, this->members_.m_finish);
      else
         this->insert(this->members_.m_finish, new_size - len, x);
   }

   //! <b>Effects</b>: Tries to deallocate the excess of memory created
   //!   with previous allocations. The size of the deque is unchanged
   //!
   //! <b>Throws</b>: If memory allocation throws.
   //!
   //! <b>Complexity</b>: Constant.
   void shrink_to_fit()
   {
      //This deque implementation already
      //deallocates excess nodes when erasing
      //so there is nothing to do except for
      //empty deque
      if(this->empty()){
         this->priv_clear_map();
      }
   }

   //////////////////////////////////////////////
   //
   //               element access
   //
   //////////////////////////////////////////////

   //! <b>Requires</b>: !empty()
   //!
   //! <b>Effects</b>: Returns a reference to the first
   //!   element of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE reference front() BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(!this->empty());
      return *this->members_.m_start;
   }

   //! <b>Requires</b>: !empty()
   //!
   //! <b>Effects</b>: Returns a const reference to the first element
   //!   from the beginning of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_reference front() const BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(!this->empty());
      return *this->members_.m_start;
   }

   //! <b>Requires</b>: !empty()
   //!
   //! <b>Effects</b>: Returns a reference to the last
   //!   element of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE reference back() BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(!this->empty());
      return *(end()-1);
   }

   //! <b>Requires</b>: !empty()
   //!
   //! <b>Effects</b>: Returns a const reference to the last
   //!   element of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_reference back() const BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(!this->empty());
      return *(cend()-1);
   }

   //! <b>Requires</b>: size() > n.
   //!
   //! <b>Effects</b>: Returns a reference to the nth element
   //!   from the beginning of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE reference operator[](size_type n) BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(this->size() > n);
      return this->members_.m_start[difference_type(n)];
   }

   //! <b>Requires</b>: size() > n.
   //!
   //! <b>Effects</b>: Returns a const reference to the nth element
   //!   from the beginning of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_reference operator[](size_type n) const BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(this->size() > n);
      return this->members_.m_start[difference_type(n)];
   }

   //! <b>Requires</b>: size() >= n.
   //!
   //! <b>Effects</b>: Returns an iterator to the nth element
   //!   from the beginning of the container. Returns end()
   //!   if n == size().
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Non-standard extension
   BOOST_CONTAINER_FORCEINLINE iterator nth(size_type n) BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(this->size() >= n);
      return iterator(this->begin()+n);
   }

   //! <b>Requires</b>: size() >= n.
   //!
   //! <b>Effects</b>: Returns a const_iterator to the nth element
   //!   from the beginning of the container. Returns end()
   //!   if n == size().
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Non-standard extension
   BOOST_CONTAINER_FORCEINLINE const_iterator nth(size_type n) const BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(this->size() >= n);
      return const_iterator(this->cbegin()+n);
   }

   //! <b>Requires</b>: begin() <= p <= end().
   //!
   //! <b>Effects</b>: Returns the index of the element pointed by p
   //!   and size() if p == end().
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Non-standard extension
   BOOST_CONTAINER_FORCEINLINE size_type index_of(iterator p) BOOST_NOEXCEPT_OR_NOTHROW
   {
      //Range checked priv_index_of
      return this->priv_index_of(p);
   }

   //! <b>Requires</b>: begin() <= p <= end().
   //!
   //! <b>Effects</b>: Returns the index of the element pointed by p
   //!   and size() if p == end().
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Non-standard extension
   BOOST_CONTAINER_FORCEINLINE size_type index_of(const_iterator p) const BOOST_NOEXCEPT_OR_NOTHROW
   {
      //Range checked priv_index_of
      return this->priv_index_of(p);
   }

   //! <b>Requires</b>: size() > n.
   //!
   //! <b>Effects</b>: Returns a reference to the nth element
   //!   from the beginning of the container.
   //!
   //! <b>Throws</b>: std::range_error if n >= size()
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE reference at(size_type n)
   {
      this->priv_throw_if_out_of_range(n);
      return (*this)[n];
   }

   //! <b>Requires</b>: size() > n.
   //!
   //! <b>Effects</b>: Returns a const reference to the nth element
   //!   from the beginning of the container.
   //!
   //! <b>Throws</b>: std::range_error if n >= size()
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE const_reference at(size_type n) const
   {
      this->priv_throw_if_out_of_range(n);
      return (*this)[n];
   }

   //////////////////////////////////////////////
   //
   //                modifiers
   //
   //////////////////////////////////////////////

   #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) || defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

   //! <b>Effects</b>: Inserts an object of type T constructed with
   //!   std::forward<Args>(args)... in the beginning of the deque.
   //!
   //! <b>Returns</b>: A reference to the created object.
   //!
   //! <b>Throws</b>: If memory allocation throws or the in-place constructor throws.
   //!
   //! <b>Complexity</b>: Amortized constant time
   template <class... Args>
   reference emplace_front(BOOST_FWD_REF(Args)... args)
   {
      if(this->priv_push_front_simple_available()){
         reference r = *this->priv_push_front_simple_pos();
         allocator_traits_type::construct
            ( this->alloc()
            , this->priv_push_front_simple_pos()
            , boost::forward<Args>(args)...);
         this->priv_push_front_simple_commit();
         return r;
      }
      else{
         typedef dtl::insert_nonmovable_emplace_proxy<ValAllocator, iterator, Args...> type;
         return *this->priv_insert_front_aux_impl(1, type(boost::forward<Args>(args)...));
      }
   }

   //! <b>Effects</b>: Inserts an object of type T constructed with
   //!   std::forward<Args>(args)... in the end of the deque.
   //!
   //! <b>Returns</b>: A reference to the created object.
   //!
   //! <b>Throws</b>: If memory allocation throws or the in-place constructor throws.
   //!
   //! <b>Complexity</b>: Amortized constant time
   template <class... Args>
   reference emplace_back(BOOST_FWD_REF(Args)... args)
   {
      if(this->priv_push_back_simple_available()){
         reference r = *this->priv_push_back_simple_pos();
         allocator_traits_type::construct
            ( this->alloc()
            , this->priv_push_back_simple_pos()
            , boost::forward<Args>(args)...);
         this->priv_push_back_simple_commit();
         return r;
      }
      else{
         typedef dtl::insert_nonmovable_emplace_proxy<ValAllocator, iterator, Args...> type;
         return *this->priv_insert_back_aux_impl(1, type(boost::forward<Args>(args)...));
      }
   }

   //! <b>Requires</b>: p must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Inserts an object of type T constructed with
   //!   std::forward<Args>(args)... before p
   //!
   //! <b>Throws</b>: If memory allocation throws or the in-place constructor throws.
   //!
   //! <b>Complexity</b>: If p is end(), amortized constant time
   //!   Linear time otherwise.
   template <class... Args>
   iterator emplace(const_iterator p, BOOST_FWD_REF(Args)... args)
   {
      BOOST_ASSERT(this->priv_in_range_or_end(p));
      if(p == this->cbegin()){
         this->emplace_front(boost::forward<Args>(args)...);
         return this->begin();
      }
      else if(p == this->cend()){
         this->emplace_back(boost::forward<Args>(args)...);
         return (this->end()-1);
      }
      else{
         typedef dtl::insert_emplace_proxy<ValAllocator, iterator, Args...> type;
         return this->priv_insert_aux_impl(p, 1, type(boost::forward<Args>(args)...));
      }
   }

   #else //!defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   #define BOOST_CONTAINER_DEQUE_EMPLACE_CODE(N) \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N\
   reference emplace_front(BOOST_MOVE_UREF##N)\
   {\
      if(priv_push_front_simple_available()){\
         reference r = *this->priv_push_front_simple_pos();\
         allocator_traits_type::construct\
            ( this->alloc(), this->priv_push_front_simple_pos() BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
         priv_push_front_simple_commit();\
         return r;\
      }\
      else{\
         typedef dtl::insert_nonmovable_emplace_proxy##N\
               <ValAllocator, iterator BOOST_MOVE_I##N BOOST_MOVE_TARG##N> type;\
         return *priv_insert_front_aux_impl(1, type(BOOST_MOVE_FWD##N));\
      }\
   }\
   \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N\
   reference emplace_back(BOOST_MOVE_UREF##N)\
   {\
      if(priv_push_back_simple_available()){\
         reference r = *this->priv_push_back_simple_pos();\
         allocator_traits_type::construct\
            ( this->alloc(), this->priv_push_back_simple_pos() BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
         priv_push_back_simple_commit();\
         return r;\
      }\
      else{\
         typedef dtl::insert_nonmovable_emplace_proxy##N\
               <ValAllocator, iterator BOOST_MOVE_I##N BOOST_MOVE_TARG##N> type;\
         return *priv_insert_back_aux_impl(1, type(BOOST_MOVE_FWD##N));\
      }\
   }\
   \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N\
   iterator emplace(const_iterator p BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      BOOST_ASSERT(this->priv_in_range_or_end(p));\
      if(p == this->cbegin()){\
         this->emplace_front(BOOST_MOVE_FWD##N);\
         return this->begin();\
      }\
      else if(p == cend()){\
         this->emplace_back(BOOST_MOVE_FWD##N);\
         return (--this->end());\
      }\
      else{\
         typedef dtl::insert_emplace_proxy_arg##N\
               <ValAllocator, iterator BOOST_MOVE_I##N BOOST_MOVE_TARG##N> type;\
         return this->priv_insert_aux_impl(p, 1, type(BOOST_MOVE_FWD##N));\
      }\
   }
   //
   BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_DEQUE_EMPLACE_CODE)
   #undef BOOST_CONTAINER_DEQUE_EMPLACE_CODE

   #endif   // !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   #if defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
   //! <b>Effects</b>: Inserts a copy of x at the front of the deque.
   //!
   //! <b>Throws</b>: If memory allocation throws or
   //!   T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Amortized constant time.
   void push_front(const T &x);

   //! <b>Effects</b>: Constructs a new element in the front of the deque
   //!   and moves the resources of x to this new element.
   //!
   //! <b>Throws</b>: If memory allocation throws.
   //!
   //! <b>Complexity</b>: Amortized constant time.
   void push_front(T &&x);
   #else
   BOOST_MOVE_CONVERSION_AWARE_CATCH(push_front, T, void, priv_push_front)
   #endif

   #if defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
   //! <b>Effects</b>: Inserts a copy of x at the end of the deque.
   //!
   //! <b>Throws</b>: If memory allocation throws or
   //!   T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Amortized constant time.
   void push_back(const T &x);

   //! <b>Effects</b>: Constructs a new element in the end of the deque
   //!   and moves the resources of x to this new element.
   //!
   //! <b>Throws</b>: If memory allocation throws.
   //!
   //! <b>Complexity</b>: Amortized constant time.
   void push_back(T &&x);
   #else
   BOOST_MOVE_CONVERSION_AWARE_CATCH(push_back, T, void, priv_push_back)
   #endif

   #if defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

   //! <b>Requires</b>: p must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert a copy of x before p.
   //!
   //! <b>Returns</b>: an iterator to the inserted element.
   //!
   //! <b>Throws</b>: If memory allocation throws or x's copy constructor throws.
   //!
   //! <b>Complexity</b>: If p is end(), amortized constant time
   //!   Linear time otherwise.
   iterator insert(const_iterator p, const T &x);

   //! <b>Requires</b>: p must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert a new element before p with x's resources.
   //!
   //! <b>Returns</b>: an iterator to the inserted element.
   //!
   //! <b>Throws</b>: If memory allocation throws.
   //!
   //! <b>Complexity</b>: If p is end(), amortized constant time
   //!   Linear time otherwise.
   iterator insert(const_iterator p, T &&x);
   #else
   BOOST_MOVE_CONVERSION_AWARE_CATCH_1ARG(insert, T, iterator, priv_insert, const_iterator, const_iterator)
   #endif

   //! <b>Requires</b>: pos must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert n copies of x before pos.
   //!
   //! <b>Returns</b>: an iterator to the first inserted element or pos if n is 0.
   //!
   //! <b>Throws</b>: If memory allocation throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   BOOST_CONTAINER_FORCEINLINE iterator insert(const_iterator pos, size_type n, const value_type& x)
   {
      //Range check of p is done by insert()
      typedef constant_iterator<value_type, difference_type> c_it;
      return this->insert(pos, c_it(x, n), c_it());
   }

   //! <b>Requires</b>: pos must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert a copy of the [first, last) range before pos.
   //!
   //! <b>Returns</b>: an iterator to the first inserted element or pos if first == last.
   //!
   //! <b>Throws</b>: If memory allocation throws, T's constructor from a
   //!   dereferenced InIt throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to distance [first, last).
   template <class InIt>
   iterator insert(const_iterator pos, InIt first, InIt last
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_or
         < void
         , dtl::is_convertible<InIt, size_type>
         , dtl::is_not_input_iterator<InIt>
         >::type * = 0
      #endif
      )
   {
      BOOST_ASSERT(this->priv_in_range_or_end(pos));
      size_type n = 0;
      iterator it(pos.unconst());
      for(;first != last; ++first, ++n){
         it = this->emplace(it, *first);
         ++it;
      }
      it -= n;
      return it;
   }

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Requires</b>: pos must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert a copy of the [il.begin(), il.end()) range before pos.
   //!
   //! <b>Returns</b>: an iterator to the first inserted element or pos if il.begin() == il.end().
   //!
   //! <b>Throws</b>: If memory allocation throws, T's constructor from a
   //!   dereferenced std::initializer_list throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to distance [il.begin(), il.end()).
   BOOST_CONTAINER_FORCEINLINE iterator insert(const_iterator pos, std::initializer_list<value_type> il)
   {
      //Range check os pos is done in insert()
      return insert(pos, il.begin(), il.end());
   }
#endif

   #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
   template <class FwdIt>
   BOOST_CONTAINER_FORCEINLINE iterator insert(const_iterator p, FwdIt first, FwdIt last
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_or
         < void
         , dtl::is_convertible<FwdIt, size_type>
         , dtl::is_input_iterator<FwdIt>
         >::type * = 0
      #endif
      )
   {
      BOOST_ASSERT(this->priv_in_range_or_end(p));
      dtl::insert_range_proxy<ValAllocator, FwdIt, iterator> proxy(first);
      return priv_insert_aux_impl(p, boost::container::iterator_distance(first, last), proxy);
   }
   #endif

   //! <b>Effects</b>: Removes the first element from the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant time.
   void pop_front() BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(!this->empty());
      if (this->members_.m_start.m_cur != this->members_.m_start.m_last - 1) {
         allocator_traits_type::destroy
            ( this->alloc()
            , boost::movelib::to_raw_pointer(this->members_.m_start.m_cur)
            );
         ++this->members_.m_start.m_cur;
      }
      else
         this->priv_pop_front_aux();
   }

   //! <b>Effects</b>: Removes the last element from the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant time.
   void pop_back() BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(!this->empty());
      if (this->members_.m_finish.m_cur != this->members_.m_finish.m_first) {
         --this->members_.m_finish.m_cur;
         allocator_traits_type::destroy
            ( this->alloc()
            , boost::movelib::to_raw_pointer(this->members_.m_finish.m_cur)
            );
      }
      else
         this->priv_pop_back_aux();
   }

   //! <b>Effects</b>: Erases the element at p.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the elements between pos and the
   //!   last element (if pos is near the end) or the first element
   //!   if(pos is near the beginning).
   //!   Constant if pos is the first or the last element.
   iterator erase(const_iterator pos) BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(this->priv_in_range(pos));
      iterator next = pos.unconst();
      ++next;
      size_type index = pos - this->members_.m_start;
      if (index < (this->size()/2)) {
         boost::container::move_backward(this->begin(), pos.unconst(), next);
         pop_front();
      }
      else {
         boost::container::move(next, this->end(), pos.unconst());
         pop_back();
      }
      return this->members_.m_start + index;
   }

   //! <b>Effects</b>: Erases the elements pointed by [first, last).
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the distance between first and
   //!   last plus the elements between pos and the
   //!   last element (if pos is near the end) or the first element
   //!   if(pos is near the beginning).
   iterator erase(const_iterator first, const_iterator last) BOOST_NOEXCEPT_OR_NOTHROW
   {
      BOOST_ASSERT(first == last ||
         (first < last && this->priv_in_range(first) && this->priv_in_range_or_end(last)));
      if (first == this->members_.m_start && last == this->members_.m_finish) {
         this->clear();
         return this->members_.m_finish;
      }
      else {
         const size_type n = static_cast<size_type>(last - first);
         const size_type elems_before = static_cast<size_type>(first - this->members_.m_start);
         if (elems_before < (this->size() - n) - elems_before) {
            boost::container::move_backward(begin(), first.unconst(), last.unconst());
            iterator new_start = this->members_.m_start + n;
            this->priv_destroy_range(this->members_.m_start, new_start);
            this->priv_destroy_nodes(this->members_.m_start.m_node, new_start.m_node);
            this->members_.m_start = new_start;
         }
         else {
            boost::container::move(last.unconst(), end(), first.unconst());
            iterator new_finish = this->members_.m_finish - n;
            this->priv_destroy_range(new_finish, this->members_.m_finish);
            this->priv_destroy_nodes(new_finish.m_node + 1, this->members_.m_finish.m_node + 1);
            this->members_.m_finish = new_finish;
         }
         return this->members_.m_start + elems_before;
      }
   }

   //! <b>Effects</b>: Swaps the contents of *this and x.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE void swap(deque &x)
      BOOST_NOEXCEPT_IF(allocator_traits_type::propagate_on_container_swap::value
                               || allocator_traits_type::is_always_equal::value)
   {
      this->swap_members(x);
      dtl::bool_<allocator_traits_type::propagate_on_container_swap::value> flag;
      dtl::swap_alloc(this->alloc(), x.alloc(), flag);
      dtl::swap_alloc(this->ptr_alloc(), x.ptr_alloc(), flag);
   }

   //! <b>Effects</b>: Erases all the elements of the deque.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the deque.
   void clear() BOOST_NOEXCEPT_OR_NOTHROW
   {
      for (index_pointer node = this->members_.m_start.m_node + 1;
            node < this->members_.m_finish.m_node;
            ++node) {
         this->priv_destroy_range(*node, *node + get_block_size());
         this->priv_deallocate_node(*node);
      }

      if (this->members_.m_start.m_node != this->members_.m_finish.m_node) {
         this->priv_destroy_range(this->members_.m_start.m_cur, this->members_.m_start.m_last);
         this->priv_destroy_range(this->members_.m_finish.m_first, this->members_.m_finish.m_cur);
         this->priv_deallocate_node(this->members_.m_finish.m_first);
      }
      else
         this->priv_destroy_range(this->members_.m_start.m_cur, this->members_.m_finish.m_cur);

      this->members_.m_finish = this->members_.m_start;
   }

   //! <b>Effects</b>: Returns true if x and y are equal
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_FORCEINLINE friend bool operator==(const deque& x, const deque& y)
   {  return x.size() == y.size() && ::boost::container::algo_equal(x.begin(), x.end(), y.begin());  }

   //! <b>Effects</b>: Returns true if x and y are unequal
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(const deque& x, const deque& y)
   {  return !(x == y); }

   //! <b>Effects</b>: Returns true if x is less than y
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_FORCEINLINE friend bool operator<(const deque& x, const deque& y)
   {  return ::boost::container::algo_lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());  }

   //! <b>Effects</b>: Returns true if x is greater than y
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_FORCEINLINE friend bool operator>(const deque& x, const deque& y)
   {  return y < x;  }

   //! <b>Effects</b>: Returns true if x is equal or less than y
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_FORCEINLINE friend bool operator<=(const deque& x, const deque& y)
   {  return !(y < x);  }

   //! <b>Effects</b>: Returns true if x is equal or greater than y
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_FORCEINLINE friend bool operator>=(const deque& x, const deque& y)
   {  return !(x < y);  }

   //! <b>Effects</b>: x.swap(y)
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE friend void swap(deque& x, deque& y)
   {  x.swap(y);  }

   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   private:

   BOOST_CONTAINER_FORCEINLINE size_type priv_index_of(const_iterator p) const
   {
      BOOST_ASSERT(this->cbegin() <= p);
      BOOST_ASSERT(p <= this->cend());
      return static_cast<size_type>(p - this->cbegin());
   }

   void priv_erase_last_n(size_type n)
   {
      if(n == this->size()) {
         this->clear();
      }
      else {
         iterator new_finish = this->members_.m_finish - n;
         this->priv_destroy_range(new_finish, this->members_.m_finish);
         this->priv_destroy_nodes(new_finish.m_node + 1, this->members_.m_finish.m_node + 1);
         this->members_.m_finish = new_finish;
      }
   }

   void priv_throw_if_out_of_range(size_type n) const
   {
      if (n >= this->size())
         throw_out_of_range("deque::at out of range");
   }

   BOOST_CONTAINER_FORCEINLINE bool priv_in_range(const_iterator pos) const
   {
      return (this->begin() <= pos) && (pos < this->end());
   }

   BOOST_CONTAINER_FORCEINLINE bool priv_in_range_or_end(const_iterator pos) const
   {
      return (this->begin() <= pos) && (pos <= this->end());
   }

   template <class U>
   iterator priv_insert(const_iterator p, BOOST_FWD_REF(U) x)
   {
      BOOST_ASSERT(this->priv_in_range_or_end(p));
      if (p == cbegin()){
         this->push_front(::boost::forward<U>(x));
         return begin();
      }
      else if (p == cend()){
         this->push_back(::boost::forward<U>(x));
         return --end();
      }
      else {
         return priv_insert_aux_impl
            ( p, (size_type)1
            , dtl::get_insert_value_proxy<iterator, ValAllocator>(::boost::forward<U>(x)));
      }
   }

   template <class U>
   void priv_push_front(BOOST_FWD_REF(U) x)
   {
      if(this->priv_push_front_simple_available()){
         allocator_traits_type::construct
            ( this->alloc(), this->priv_push_front_simple_pos(), ::boost::forward<U>(x));
         this->priv_push_front_simple_commit();
      }
      else{
         priv_insert_aux_impl
            ( this->cbegin(), (size_type)1
            , dtl::get_insert_value_proxy<iterator, ValAllocator>(::boost::forward<U>(x)));
      }
   }

   template <class U>
   void priv_push_back(BOOST_FWD_REF(U) x)
   {
      if(this->priv_push_back_simple_available()){
         allocator_traits_type::construct
            ( this->alloc(), this->priv_push_back_simple_pos(), ::boost::forward<U>(x));
         this->priv_push_back_simple_commit();
      }
      else{
         priv_insert_aux_impl
            ( this->cend(), (size_type)1
            , dtl::get_insert_value_proxy<iterator, ValAllocator>(::boost::forward<U>(x)));
      }
   }

   BOOST_CONTAINER_FORCEINLINE bool priv_push_back_simple_available() const
   {
      return this->members_.m_map &&
         (this->members_.m_finish.m_cur != (this->members_.m_finish.m_last - 1));
   }

   BOOST_CONTAINER_FORCEINLINE T *priv_push_back_simple_pos() const
   {
      return boost::movelib::to_raw_pointer(this->members_.m_finish.m_cur);
   }

   BOOST_CONTAINER_FORCEINLINE void priv_push_back_simple_commit()
   {
      ++this->members_.m_finish.m_cur;
   }

   BOOST_CONTAINER_FORCEINLINE bool priv_push_front_simple_available() const
   {
      return this->members_.m_map &&
         (this->members_.m_start.m_cur != this->members_.m_start.m_first);
   }

   BOOST_CONTAINER_FORCEINLINE T *priv_push_front_simple_pos() const
   {  return boost::movelib::to_raw_pointer(this->members_.m_start.m_cur) - 1;  }

   BOOST_CONTAINER_FORCEINLINE void priv_push_front_simple_commit()
   {  --this->members_.m_start.m_cur;   }

   void priv_destroy_range(iterator p, iterator p2)
   {
      if(!Base::traits_t::trivial_dctr){
         for(;p != p2; ++p){
            allocator_traits_type::destroy(this->alloc(), boost::movelib::iterator_to_raw_pointer(p));
         }
      }
   }

   void priv_destroy_range(pointer p, pointer p2)
   {
      if(!Base::traits_t::trivial_dctr){
         for(;p != p2; ++p){
            allocator_traits_type::destroy(this->alloc(), boost::movelib::iterator_to_raw_pointer(p));
         }
      }
   }

   template<class InsertProxy>
   iterator priv_insert_aux_impl(const_iterator p, size_type n, InsertProxy proxy)
   {
      iterator pos(p.unconst());
      const size_type pos_n = p - this->cbegin();
      if(!this->members_.m_map){
         this->priv_initialize_map(0);
         pos = this->begin();
      }

      const size_type elemsbefore = static_cast<size_type>(pos - this->members_.m_start);
      const size_type length = this->size();
      if (elemsbefore < length / 2) {
         const iterator new_start = this->priv_reserve_elements_at_front(n);
         const iterator old_start = this->members_.m_start;
         if(!elemsbefore){
            proxy.uninitialized_copy_n_and_update(this->alloc(), new_start, n);
            this->members_.m_start = new_start;
         }
         else{
            pos = this->members_.m_start + elemsbefore;
            if (elemsbefore >= n) {
               const iterator start_n = this->members_.m_start + n;
               ::boost::container::uninitialized_move_alloc
                  (this->alloc(), this->members_.m_start, start_n, new_start);
               this->members_.m_start = new_start;
               boost::container::move(start_n, pos, old_start);
               proxy.copy_n_and_update(this->alloc(), pos - n, n);
            }
            else {
               const size_type mid_count = n - elemsbefore;
               const iterator mid_start = old_start - mid_count;
               proxy.uninitialized_copy_n_and_update(this->alloc(), mid_start, mid_count);
               this->members_.m_start = mid_start;
               ::boost::container::uninitialized_move_alloc
                  (this->alloc(), old_start, pos, new_start);
               this->members_.m_start = new_start;
               proxy.copy_n_and_update(this->alloc(), old_start, elemsbefore);
            }
         }
      }
      else {
         const iterator new_finish = this->priv_reserve_elements_at_back(n);
         const iterator old_finish = this->members_.m_finish;
         const size_type elemsafter = length - elemsbefore;
         if(!elemsafter){
            proxy.uninitialized_copy_n_and_update(this->alloc(), old_finish, n);
            this->members_.m_finish = new_finish;
         }
         else{
            pos = old_finish - elemsafter;
            if (elemsafter >= n) {
               iterator finish_n = old_finish - difference_type(n);
               ::boost::container::uninitialized_move_alloc
                  (this->alloc(), finish_n, old_finish, old_finish);
               this->members_.m_finish = new_finish;
               boost::container::move_backward(pos, finish_n, old_finish);
               proxy.copy_n_and_update(this->alloc(), pos, n);
            }
            else {
               const size_type raw_gap = n - elemsafter;
               ::boost::container::uninitialized_move_alloc
                  (this->alloc(), pos, old_finish, old_finish + raw_gap);
               BOOST_TRY{
                  proxy.copy_n_and_update(this->alloc(), pos, elemsafter);
                  proxy.uninitialized_copy_n_and_update(this->alloc(), old_finish, raw_gap);
               }
               BOOST_CATCH(...){
                  this->priv_destroy_range(old_finish, old_finish + elemsafter);
                  BOOST_RETHROW
               }
               BOOST_CATCH_END
               this->members_.m_finish = new_finish;
            }
         }
      }
      return this->begin() + pos_n;
   }

   template <class InsertProxy>
   iterator priv_insert_back_aux_impl(size_type n, InsertProxy proxy)
   {
      if(!this->members_.m_map){
         this->priv_initialize_map(0);
      }

      iterator new_finish = this->priv_reserve_elements_at_back(n);
      iterator old_finish = this->members_.m_finish;
      proxy.uninitialized_copy_n_and_update(this->alloc(), old_finish, n);
      this->members_.m_finish = new_finish;
      return iterator(this->members_.m_finish - n);
   }

   template <class InsertProxy>
   iterator priv_insert_front_aux_impl(size_type n, InsertProxy proxy)
   {
      if(!this->members_.m_map){
         this->priv_initialize_map(0);
      }

      iterator new_start = this->priv_reserve_elements_at_front(n);
      proxy.uninitialized_copy_n_and_update(this->alloc(), new_start, n);
      this->members_.m_start = new_start;
      return new_start;
   }

   BOOST_CONTAINER_FORCEINLINE iterator priv_fill_insert(const_iterator pos, size_type n, const value_type& x)
   {
      typedef constant_iterator<value_type, difference_type> c_it;
      return this->insert(pos, c_it(x, n), c_it());
   }

   // Precondition: this->members_.m_start and this->members_.m_finish have already been initialized,
   // but none of the deque's elements have yet been constructed.
   void priv_fill_initialize(const value_type& value)
   {
      index_pointer cur = this->members_.m_start.m_node;
      BOOST_TRY {
         for ( ; cur < this->members_.m_finish.m_node; ++cur){
            boost::container::uninitialized_fill_alloc
               (this->alloc(), *cur, *cur + get_block_size(), value);
         }
         boost::container::uninitialized_fill_alloc
            (this->alloc(), this->members_.m_finish.m_first, this->members_.m_finish.m_cur, value);
      }
      BOOST_CATCH(...){
         this->priv_destroy_range(this->members_.m_start, iterator(*cur, cur, get_block_size()));
         BOOST_RETHROW
      }
      BOOST_CATCH_END
   }

   template <class InIt>
   void priv_range_initialize(InIt first, InIt last, typename iterator_enable_if_tag<InIt, std::input_iterator_tag>::type* =0)
   {
      this->priv_initialize_map(0);
      BOOST_TRY {
         for ( ; first != last; ++first)
            this->emplace_back(*first);
      }
      BOOST_CATCH(...){
         this->clear();
         BOOST_RETHROW
      }
      BOOST_CATCH_END
   }

   template <class FwdIt>
   void priv_range_initialize(FwdIt first, FwdIt last, typename iterator_disable_if_tag<FwdIt, std::input_iterator_tag>::type* =0)
   {
      size_type n = 0;
      n = boost::container::iterator_distance(first, last);
      this->priv_initialize_map(n);

      index_pointer cur_node = this->members_.m_start.m_node;
      BOOST_TRY {
         for (; cur_node < this->members_.m_finish.m_node; ++cur_node) {
            FwdIt mid = first;
            boost::container::iterator_advance(mid, get_block_size());
            ::boost::container::uninitialized_copy_alloc(this->alloc(), first, mid, *cur_node);
            first = mid;
         }
         ::boost::container::uninitialized_copy_alloc(this->alloc(), first, last, this->members_.m_finish.m_first);
      }
      BOOST_CATCH(...){
         this->priv_destroy_range(this->members_.m_start, iterator(*cur_node, cur_node, get_block_size()));
         BOOST_RETHROW
      }
      BOOST_CATCH_END
   }

   // Called only if this->members_.m_finish.m_cur == this->members_.m_finish.m_first.
   void priv_pop_back_aux() BOOST_NOEXCEPT_OR_NOTHROW
   {
      this->priv_deallocate_node(this->members_.m_finish.m_first);
      this->members_.m_finish.priv_set_node(this->members_.m_finish.m_node - 1, get_block_size());
      this->members_.m_finish.m_cur = this->members_.m_finish.m_last - 1;
      allocator_traits_type::destroy
         ( this->alloc()
         , boost::movelib::to_raw_pointer(this->members_.m_finish.m_cur)
         );
   }

   // Called only if this->members_.m_start.m_cur == this->members_.m_start.m_last - 1.  Note that
   // if the deque has at least one element (a precondition for this member
   // function), and if this->members_.m_start.m_cur == this->members_.m_start.m_last, then the deque
   // must have at least two nodes.
   void priv_pop_front_aux() BOOST_NOEXCEPT_OR_NOTHROW
   {
      allocator_traits_type::destroy
         ( this->alloc()
         , boost::movelib::to_raw_pointer(this->members_.m_start.m_cur)
         );
      this->priv_deallocate_node(this->members_.m_start.m_first);
      this->members_.m_start.priv_set_node(this->members_.m_start.m_node + 1, get_block_size());
      this->members_.m_start.m_cur = this->members_.m_start.m_first;
   }

   iterator priv_reserve_elements_at_front(size_type n)
   {
      size_type vacancies = this->members_.m_start.m_cur - this->members_.m_start.m_first;
      if (n > vacancies){
         size_type new_elems = n-vacancies;
         size_type new_nodes = (new_elems + get_block_size() - 1) /
            get_block_size();
         size_type s = (size_type)(this->members_.m_start.m_node - this->members_.m_map);
         if (new_nodes > s){
            this->priv_reallocate_map(new_nodes, true);
         }
         size_type i = 1;
         BOOST_TRY {
            for (; i <= new_nodes; ++i)
               *(this->members_.m_start.m_node - i) = this->priv_allocate_node();
         }
         BOOST_CATCH(...) {
            for (size_type j = 1; j < i; ++j)
               this->priv_deallocate_node(*(this->members_.m_start.m_node - j));
            BOOST_RETHROW
         }
         BOOST_CATCH_END
      }
      return this->members_.m_start - difference_type(n);
   }

   iterator priv_reserve_elements_at_back(size_type n)
   {
      size_type vacancies = (this->members_.m_finish.m_last - this->members_.m_finish.m_cur) - 1;
      if (n > vacancies){
         size_type new_elems = n - vacancies;
         size_type new_nodes = (new_elems + get_block_size() - 1)/get_block_size();
         size_type s = (size_type)(this->members_.m_map_size - (this->members_.m_finish.m_node - this->members_.m_map));
         if (new_nodes + 1 > s){
            this->priv_reallocate_map(new_nodes, false);
         }
         size_type i = 1;
         BOOST_TRY {
            for (; i <= new_nodes; ++i)
               *(this->members_.m_finish.m_node + i) = this->priv_allocate_node();
         }
         BOOST_CATCH(...) {
            for (size_type j = 1; j < i; ++j)
               this->priv_deallocate_node(*(this->members_.m_finish.m_node + j));
            BOOST_RETHROW
         }
         BOOST_CATCH_END
      }
      return this->members_.m_finish + difference_type(n);
   }

   void priv_reallocate_map(size_type nodes_to_add, bool add_at_front)
   {
      size_type old_num_nodes = this->members_.m_finish.m_node - this->members_.m_start.m_node + 1;
      size_type new_num_nodes = old_num_nodes + nodes_to_add;

      index_pointer new_nstart;
      if (this->members_.m_map_size > 2 * new_num_nodes) {
         new_nstart = this->members_.m_map + (this->members_.m_map_size - new_num_nodes) / 2
                           + (add_at_front ? nodes_to_add : 0);
         if (new_nstart < this->members_.m_start.m_node)
            boost::container::move(this->members_.m_start.m_node, this->members_.m_finish.m_node + 1, new_nstart);
         else
            boost::container::move_backward
               (this->members_.m_start.m_node, this->members_.m_finish.m_node + 1, new_nstart + old_num_nodes);
      }
      else {
         size_type new_map_size =
            this->members_.m_map_size + dtl::max_value(this->members_.m_map_size, nodes_to_add) + 2;

         index_pointer new_map = this->priv_allocate_map(new_map_size);
         new_nstart = new_map + (new_map_size - new_num_nodes) / 2
                              + (add_at_front ? nodes_to_add : 0);
         boost::container::move(this->members_.m_start.m_node, this->members_.m_finish.m_node + 1, new_nstart);
         this->priv_deallocate_map(this->members_.m_map, this->members_.m_map_size);

         this->members_.m_map = new_map;
         this->members_.m_map_size = new_map_size;
      }

      this->members_.m_start.priv_set_node(new_nstart, get_block_size());
      this->members_.m_finish.priv_set_node(new_nstart + old_num_nodes - 1, get_block_size());
   }
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
};

#ifndef BOOST_CONTAINER_NO_CXX17_CTAD
template <typename InputIterator>
deque(InputIterator, InputIterator) -> deque<typename iterator_traits<InputIterator>::value_type>;
template <typename InputIterator, typename Allocator>
deque(InputIterator, InputIterator, Allocator const&) -> deque<typename iterator_traits<InputIterator>::value_type, Allocator>;
#endif

}}

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

namespace boost {

//!has_trivial_destructor_after_move<> == true_type
//!specialization for optimizations
template <class T, class Allocator, class Options>
struct has_trivial_destructor_after_move<boost::container::deque<T, Allocator, Options> >
{
   typedef typename boost::container::deque<T, Allocator, Options>::allocator_type allocator_type;
   typedef typename ::boost::container::allocator_traits<allocator_type>::pointer pointer;
   static const bool value = ::boost::has_trivial_destructor_after_move<allocator_type>::value &&
                             ::boost::has_trivial_destructor_after_move<pointer>::value;
};

}

#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

#include <boost/container/detail/config_end.hpp>

#endif //   #ifndef  BOOST_CONTAINER_DEQUE_HPP
