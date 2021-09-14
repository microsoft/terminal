//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_DETAIL_BLOCK_LIST_HEADER
#define BOOST_CONTAINER_DETAIL_BLOCK_LIST_HEADER

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/container_fwd.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/throw_exception.hpp>
#include <boost/intrusive/circular_list_algorithms.hpp>
#include <boost/move/detail/type_traits.hpp>
#include <boost/assert.hpp>
#include <boost/container/detail/placement_new.hpp>

#include <cstddef>

namespace boost {
namespace container {
namespace pmr {

struct list_node
{
   list_node *next;
   list_node *previous;
};

struct list_node_traits
{
   typedef list_node         node;
   typedef list_node*        node_ptr;
   typedef const list_node*  const_node_ptr;

   static node_ptr get_next(const_node_ptr n)
   {  return n->next;  }

   static node_ptr get_previous(const_node_ptr n)
   {  return n->previous;  }

   static void set_next(const node_ptr & n, const node_ptr & next)
   {  n->next = next;  }

   static void set_previous(const node_ptr & n, const node_ptr & previous)
   {  n->previous = previous;  }
};

struct block_list_header
   : public list_node
{
   std::size_t size;
};

typedef bi::circular_list_algorithms<list_node_traits> list_algo;


template<class DerivedFromBlockListHeader = block_list_header>
class block_list_base
{
   list_node m_list;

   static const std::size_t MaxAlignMinus1 = memory_resource::max_align-1u;

   public:

   static const std::size_t header_size = std::size_t(sizeof(DerivedFromBlockListHeader) + MaxAlignMinus1) & std::size_t(~MaxAlignMinus1);

   explicit block_list_base()
   {  list_algo::init_header(&m_list);  }

   #if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS) || defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
   block_list_base(const block_list_base&) = delete;
   block_list_base operator=(const block_list_base&) = delete;
   #else
   private:
   block_list_base          (const block_list_base&);
   block_list_base operator=(const block_list_base&);
   public:
   #endif

   ~block_list_base()
   {}

   void *allocate(std::size_t size, memory_resource &mr)
   {
      if((size_t(-1) - header_size) < size)
         throw_bad_alloc();
      void *p = mr.allocate(size+header_size);
      block_list_header &mb  = *::new((void*)p, boost_container_new_t()) DerivedFromBlockListHeader;
      mb.size = size+header_size;
      list_algo::link_after(&m_list, &mb);
      return (char *)p + header_size;
   }

   void deallocate(void *p, memory_resource &mr) BOOST_NOEXCEPT
   {
      DerivedFromBlockListHeader *pheader = static_cast<DerivedFromBlockListHeader*>
         (static_cast<void*>((char*)p - header_size));
      list_algo::unlink(pheader);
      const std::size_t size = pheader->size;
      static_cast<DerivedFromBlockListHeader*>(pheader)->~DerivedFromBlockListHeader();
      mr.deallocate(pheader, size, memory_resource::max_align);
   }

   void release(memory_resource &mr) BOOST_NOEXCEPT
   {
      list_node *n = list_algo::node_traits::get_next(&m_list);
      while(n != &m_list){
         DerivedFromBlockListHeader &d = static_cast<DerivedFromBlockListHeader&>(*n);
         n = list_algo::node_traits::get_next(n);
         std::size_t size = d.size;
         d.~DerivedFromBlockListHeader();
         mr.deallocate(reinterpret_cast<char*>(&d), size, memory_resource::max_align);         
      }
      list_algo::init_header(&m_list);
   }
};

}  //namespace pmr {
}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_DETAIL_BLOCK_LIST_HEADER
