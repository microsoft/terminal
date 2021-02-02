//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2016-2016. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_NODE_HANDLE_HPP
#define BOOST_CONTAINER_NODE_HANDLE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/static_assert.hpp>
#include <boost/container/detail/placement_new.hpp>
#include <boost/move/detail/to_raw_pointer.hpp>
#include <boost/container/allocator_traits.hpp>
#include <boost/container/detail/mpl.hpp>

#include <boost/move/utility_core.hpp>
#include <boost/move/adl_move_swap.hpp>

#include <boost/type_traits/aligned_storage.hpp>


//!\file

namespace boost {
namespace container {

///@cond

template<class Value, class KeyMapped>
struct node_handle_keymapped_traits
{
   typedef typename KeyMapped::key_type      key_type;
   typedef typename KeyMapped::mapped_type   mapped_type;
};

template<class Value>
struct node_handle_keymapped_traits<Value, void>
{
   typedef Value key_type;
   typedef Value mapped_type;
};

class node_handle_friend
{
   public:

   template<class NH>
   BOOST_CONTAINER_FORCEINLINE static void destroy_alloc(NH &nh) BOOST_NOEXCEPT
   {  nh.destroy_alloc();  }

   template<class NH>
   BOOST_CONTAINER_FORCEINLINE static typename NH::node_pointer &get_node_pointer(NH &nh) BOOST_NOEXCEPT
   {  return nh.get_node_pointer();  }
};


///@endcond

//! A node_handle is an object that accepts ownership of a single element from an associative container.
//! It may be used to transfer that ownership to another container with compatible nodes. Containers
//! with compatible nodes have the same node handle type. Elements may be transferred in either direction
//! between container types in the same row:.
//!
//! Container types with compatible nodes
//!
//! map<K, T, C1, A> <-> map<K, T, C2, A>
//!
//! map<K, T, C1, A> <-> multimap<K, T, C2, A>
//! 
//! set<K, C1, A> <-> set<K, C2, A>
//! 
//! set<K, C1, A> <-> multiset<K, C2, A>
//! 
//! If a node handle is not empty, then it contains an allocator that is equal to the allocator of the container
//! when the element was extracted. If a node handle is empty, it contains no allocator.
template <class NodeAllocator, class KeyMapped = void>
class node_handle
{
   typedef NodeAllocator                                          nallocator_type;
   typedef allocator_traits<NodeAllocator>                        nator_traits;
   typedef typename nator_traits::value_type                      priv_node_t;
   typedef typename priv_node_t::value_type                       priv_value_t;
   typedef node_handle_keymapped_traits<priv_value_t, KeyMapped>  keymapped_t;

   public:
   typedef priv_value_t                                           value_type;
   typedef typename keymapped_t::key_type                         key_type;
   typedef typename keymapped_t::mapped_type                      mapped_type;
   typedef typename nator_traits::template portable_rebind_alloc
      <value_type>::type                                          allocator_type;

   typedef priv_node_t                                            container_node_type;
   friend class node_handle_friend;

   ///@cond
   private:
   BOOST_MOVABLE_BUT_NOT_COPYABLE(node_handle)

   typedef typename nator_traits::pointer                         node_pointer;
   typedef ::boost::aligned_storage
      < sizeof(nallocator_type)
      , boost::alignment_of<nallocator_type>::value>              nalloc_storage_t;

   node_pointer      m_ptr;
   nalloc_storage_t  m_nalloc_storage;

   void move_construct_alloc(nallocator_type &al)
   {  ::new(m_nalloc_storage.address(), boost_container_new_t()) nallocator_type(::boost::move(al));   }

   void destroy_deallocate_node()
   {
      nator_traits::destroy(this->node_alloc(), boost::movelib::to_raw_pointer(m_ptr));
      nator_traits::deallocate(this->node_alloc(), m_ptr, 1u);
   }

   template<class OtherNodeHandle>
   void move_construct_end(OtherNodeHandle &nh)
   {
      if(m_ptr){
         ::new (m_nalloc_storage.address(), boost_container_new_t()) nallocator_type(::boost::move(nh.node_alloc()));
         node_handle_friend::destroy_alloc(nh);
         node_handle_friend::get_node_pointer(nh) = node_pointer();
      }
      BOOST_ASSERT(nh.empty());
   }

   void destroy_alloc() BOOST_NOEXCEPT
   {  static_cast<nallocator_type*>(m_nalloc_storage.address())->~nallocator_type();  }

   node_pointer &get_node_pointer() BOOST_NOEXCEPT
   {  return m_ptr;  }

   ///@endcond

   public:
   //! <b>Effects</b>: Initializes m_ptr to nullptr.
   //!
   //! <b>Postcondition</b>: this->empty()
   BOOST_CXX14_CONSTEXPR node_handle() BOOST_NOEXCEPT
      :  m_ptr()
   { }

   //! <b>Effects</b>: Constructs a node_handle object initializing internal pointer with p.
   //!   If p != nullptr copy constructs internal allocator from al.
   node_handle(node_pointer p, const nallocator_type &al) BOOST_NOEXCEPT
      :  m_ptr(p)
   {
      if(m_ptr){
         ::new (m_nalloc_storage.address(), boost_container_new_t()) nallocator_type(al);
      }
   }

   //! <b>Effects</b>: Constructs a node_handle object initializing internal pointer with a related nh's internal pointer
   //!   and assigns nullptr to the later. If nh's internal pointer was not nullptr, move constructs internal
   //!   allocator with nh's internal allocator and destroy nh's internal allocator.
   //!
   //! <b>Postcondition</b>: nh.empty()
   //!
   //! <b>Note</b>: Two node_handle's are related if only one of KeyMapped template parameter
   //!   of a node handle is void.
   template<class KeyMapped2>
   node_handle( BOOST_RV_REF_BEG node_handle<NodeAllocator, KeyMapped2> BOOST_RV_REF_END nh
               , typename dtl::enable_if_c
                  < ((unsigned)dtl::is_same<KeyMapped,  void>::value +
                     (unsigned)dtl::is_same<KeyMapped2, void>::value) == 1u
                  >::type* = 0) BOOST_NOEXCEPT
      :  m_ptr(nh.get())
   {  this->move_construct_end(nh);  }

   //! <b>Effects</b>: Constructs a node_handle object initializing internal pointer with nh's internal pointer
   //!   and assigns nullptr to the later. If nh's internal pointer was not nullptr, move constructs internal
   //!   allocator with nh's internal allocator and destroy nh's internal allocator.
   //!
   //! <b>Postcondition</b>: nh.empty()
   node_handle (BOOST_RV_REF(node_handle) nh) BOOST_NOEXCEPT
      : m_ptr(nh.m_ptr)
   {  this->move_construct_end(nh);  }

   //! <b>Effects</b>: If !this->empty(), destroys the value_type subobject in the container_node_type object
   //!   pointed to by c by calling allocator_traits<impl_defined>::destroy, then deallocates m_ptr by calling
   //!   nator_traits::rebind_traits<container_node_type>::deallocate.
   ~node_handle() BOOST_NOEXCEPT
   {
      if(!this->empty()){
         this->destroy_deallocate_node();
         this->destroy_alloc();
      }
   }

   //! <b>Requires</b>: Either this->empty(), or nator_traits::propagate_on_container_move_assignment is true, or
   //!   node_alloc() == nh.node_alloc().
   //!
   //! <b>Effects</b>: If m_ptr != nullptr, destroys the value_type subobject in the container_node_type object
   //!   pointed to by m_ptr by calling nator_traits::destroy, then deallocates m_ptr by calling
   //!   nator_traits::deallocate. Assigns nh.m_ptr to m_ptr. If this->empty()
   //!   or nator_traits::propagate_on_container_move_assignment is true, move assigns nh.node_alloc() to
   //!   node_alloc(). Assigns nullptr to nh.m_ptr and assigns nullopt to nh.node_alloc().
   //!
   //! <b>Returns</b>: *this.
   //!
   //! <b>Throws</b>: Nothing.
   node_handle & operator=(BOOST_RV_REF(node_handle) nh) BOOST_NOEXCEPT
   {
      BOOST_ASSERT(this->empty() || nator_traits::propagate_on_container_move_assignment::value 
                   || nator_traits::equal(node_alloc(), nh.node_alloc()));

      bool const was_this_non_null = !this->empty();
      bool const was_nh_non_null   = !nh.empty();

      if(was_nh_non_null){
         if(was_this_non_null){
            this->destroy_deallocate_node();
            if(nator_traits::propagate_on_container_move_assignment::value){
               this->node_alloc() = ::boost::move(nh.node_alloc());
            }
         }
         else{
            this->move_construct_alloc(nh.node_alloc());
         }
         m_ptr = nh.m_ptr;
         nh.m_ptr = node_pointer();
         nh.destroy_alloc();
      }
      else if(was_this_non_null){
         this->destroy_deallocate_node();
         this->destroy_alloc();
         m_ptr = node_pointer();
      }
      return *this;
   }

   //! <b>Requires</b>: empty() == false.
   //!
   //! <b>Returns</b>: A reference to the value_type subobject in the container_node_type object pointed to by m_ptr
   //!
   //! <b>Throws</b>: Nothing.
   value_type& value() const BOOST_NOEXCEPT
   {
      BOOST_STATIC_ASSERT((dtl::is_same<KeyMapped, void>::value));
      BOOST_ASSERT(!empty());
      return m_ptr->get_data();
   }

   //! <b>Requires</b>: empty() == false.
   //!
   //! <b>Returns</b>: A non-const reference to the key_type member of the value_type subobject in the 
   //!   container_node_type object pointed to by m_ptr.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Requires</b>: Modifying the key through the returned reference is permitted.
   key_type& key() const BOOST_NOEXCEPT
   {
      BOOST_STATIC_ASSERT((!dtl::is_same<KeyMapped, void>::value));
      BOOST_ASSERT(!empty());
      return const_cast<key_type &>(KeyMapped().key_of_value(m_ptr->get_data()));
   }

   //! <b>Requires</b>: empty() == false.
   //!
   //! <b>Returns</b>: A reference to the mapped_type member of the value_type subobject
   //!   in the container_node_type object pointed to by m_ptr
   //!
   //! <b>Throws</b>: Nothing.
   mapped_type& mapped() const BOOST_NOEXCEPT
   {
      BOOST_STATIC_ASSERT((!dtl::is_same<KeyMapped, void>::value));
      BOOST_ASSERT(!empty());
      return KeyMapped().mapped_of_value(m_ptr->get_data());
   }

   //! <b>Requires</b>: empty() == false.
   //!
   //! <b>Returns</b>: A copy of the internally hold allocator.
   //!
   //! <b>Throws</b>: Nothing.
   allocator_type get_allocator() const
   {
      BOOST_ASSERT(!empty());
      return this->node_alloc();
   }

   //! <b>Returns</b>: m_ptr != nullptr.
   //!
   #ifdef BOOST_CONTAINER_DOXYGEN_INVOKED
   BOOST_CONTAINER_FORCEINLINE explicit operator bool
   #else
   private: struct bool_conversion {int for_bool; int for_arg(); }; typedef int bool_conversion::* explicit_bool_arg;
   public: BOOST_CONTAINER_FORCEINLINE operator explicit_bool_arg
   #endif
      ()const BOOST_NOEXCEPT
   {  return m_ptr ? &bool_conversion::for_bool  : explicit_bool_arg(0);  }

   //! <b>Returns</b>: m_ptr == nullptr.
   //!
   bool empty() const BOOST_NOEXCEPT
   {
      return !this->m_ptr;
   }

   //! <b>Requires</b>: this->empty(), or nh.empty(), or nator_traits::propagate_on_container_swap is true, or
   //!   node_alloc() == nh.node_alloc().
   //!
   //! <b>Effects</b>: Calls swap(m_ptr, nh.m_ptr). If this->empty(), or nh.empty(), or nator_traits::propagate_on_-
   //!   container_swap is true calls swap(node_alloc(), nh.node_alloc()).
   void swap(node_handle &nh)
      BOOST_NOEXCEPT_IF(nator_traits::propagate_on_container_swap::value || nator_traits::is_always_equal::value)
   {
      BOOST_ASSERT(this->empty() || nh.empty() || nator_traits::propagate_on_container_swap::value
                   || nator_traits::equal(node_alloc(), nh.node_alloc()));

      bool const was_this_non_null = !this->empty();
      bool const was_nh_non_null   = !nh.empty();

      if(was_nh_non_null){
         if(was_this_non_null){
            if(nator_traits::propagate_on_container_swap::value){
               ::boost::adl_move_swap(this->node_alloc(), nh.node_alloc());
            }
         }
         else{
            this->move_construct_alloc(nh.node_alloc());
            nh.destroy_alloc();
         }
      }
      else if(was_this_non_null){
         nh.move_construct_alloc(this->node_alloc());
         this->destroy_alloc();
      }
      ::boost::adl_move_swap(m_ptr, nh.m_ptr);
   }

   //! <b>Effects</b>: If this->empty() returns nullptr, otherwise returns m_ptr
   //!   resets m_ptr to nullptr and destroys the internal allocator.
   //!
   //! <b>Postcondition</b>: this->empty()
   //!
   //! <b>Note</b>: Non-standard extensions
   node_pointer release() BOOST_NOEXCEPT
   {
      node_pointer p(m_ptr);
      m_ptr = node_pointer();
      if(p)
         this->destroy_alloc();
      return p;
   }

   //! <b>Effects</b>: Returns m_ptr.
   //!
   //! <b>Note</b>: Non-standard extensions
   node_pointer get() const BOOST_NOEXCEPT
   {
      return m_ptr;
   }

   //! <b>Effects</b>: Returns a reference to the internal node allocator.
   //!
   //! <b>Note</b>: Non-standard extensions
   nallocator_type &node_alloc() BOOST_NOEXCEPT
   {
      BOOST_ASSERT(!empty());
      return *static_cast<nallocator_type*>(m_nalloc_storage.address());
   }


   //! <b>Effects</b>: Returns a reference to the internal node allocator.
   //!
   //! <b>Note</b>: Non-standard extensions
   const nallocator_type &node_alloc() const BOOST_NOEXCEPT
   {
      BOOST_ASSERT(!empty());
      return *static_cast<const nallocator_type*>(m_nalloc_storage.address());
   }

   //! <b>Effects</b>: x.swap(y).
   //!
   friend void swap(node_handle & x, node_handle & y) BOOST_NOEXCEPT_IF(BOOST_NOEXCEPT(x.swap(y)))
   {  x.swap(y);  }
};

//! A class template used to describe the results of inserting a
//! Container::node_type in a Container with unique keys.
//! Includes at least the following non-static public data members:
//!
//! <ul><li>bool inserted</li>;
//! <li>Iterator position</li>;
//! <li>NodeType node</li></ul>
//!
//! This type is MoveConstructible, MoveAssignable, DefaultConstructible,
//! Destructible, and lvalues of that type are swappable
template<class Iterator, class NodeType>
struct insert_return_type_base
{
   private:
   BOOST_MOVABLE_BUT_NOT_COPYABLE(insert_return_type_base)

   public:
   insert_return_type_base()
      : inserted(false), position(), node()
   {}

   insert_return_type_base(BOOST_RV_REF(insert_return_type_base) other)
      : inserted(other.inserted), position(other.position), node(boost::move(other.node))
   {}

   template<class RelatedIt, class RelatedNode>
   insert_return_type_base(bool insert, RelatedIt it, BOOST_RV_REF(RelatedNode) node)
      : inserted(insert), position(it), node(boost::move(node))
   {}

   insert_return_type_base & operator=(BOOST_RV_REF(insert_return_type_base) other)
   {
      inserted = other.inserted;
      position = other.position;
      node = boost::move(other.node);
      return *this;
   }

   bool  inserted;
   Iterator position;
   NodeType node;
};

}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_NODE_HANDLE_HPP
