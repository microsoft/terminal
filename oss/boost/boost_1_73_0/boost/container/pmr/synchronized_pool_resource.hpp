//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_PMR_SYNCHRONIZED_POOL_RESOURCE_HPP
#define BOOST_CONTAINER_PMR_SYNCHRONIZED_POOL_RESOURCE_HPP

#if defined (_MSC_VER)
#  pragma once 
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/auto_link.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/detail/pool_resource.hpp>
#include <boost/container/detail/thread_mutex.hpp>

#include <cstddef>

namespace boost {
namespace container {
namespace pmr {

//! A synchronized_pool_resource is a general-purpose memory resources having
//! the following qualities:
//!
//! - Each resource owns the allocated memory, and frees it on destruction,
//!   even if deallocate has not been called for some of the allocated blocks.
//!
//! - A pool resource consists of a collection of pools, serving
//!   requests for different block sizes. Each individual pool manages a
//!   collection of chunks that are in turn divided into blocks of uniform size,
//!   returned via calls to do_allocate. Each call to do_allocate(size, alignment)
//!   is dispatched to the pool serving the smallest blocks accommodating at
//!   least size bytes.
//!
//! - When a particular pool is exhausted, allocating a block from that pool
//!   results in the allocation of an additional chunk of memory from the upstream
//!   allocator (supplied at construction), thus replenishing the pool. With
//!   each successive replenishment, the chunk size obtained increases
//!   geometrically. [ Note: By allocating memory in chunks, the pooling strategy
//!   increases the chance that consecutive allocations will be close together
//!   in memory. - end note ]
//!
//! - Allocation requests that exceed the largest block size of any pool are
//!   fulfilled directly from the upstream allocator.
//!
//! - A pool_options struct may be passed to the pool resource constructors to
//!   tune the largest block size and the maximum chunk size.
//!
//! A synchronized_pool_resource may be accessed from multiple threads without
//! external synchronization and may have thread-specific pools to reduce
//! synchronization costs.
class BOOST_CONTAINER_DECL synchronized_pool_resource
   : public memory_resource
{
   dtl::thread_mutex m_mut;
   pool_resource     m_pool_resource;

   public:

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::unsynchronized_pool_resource(const pool_options&,memory_resource*)
   synchronized_pool_resource(const pool_options& opts, memory_resource* upstream) BOOST_NOEXCEPT;

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::unsynchronized_pool_resource()
   synchronized_pool_resource() BOOST_NOEXCEPT;

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::unsynchronized_pool_resource(memory_resource*)
   explicit synchronized_pool_resource(memory_resource* upstream) BOOST_NOEXCEPT;

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::unsynchronized_pool_resource(const pool_options&)
   explicit synchronized_pool_resource(const pool_options& opts) BOOST_NOEXCEPT;

   #if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS) || defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
   synchronized_pool_resource(const synchronized_pool_resource&) = delete;
   synchronized_pool_resource operator=(const synchronized_pool_resource&) = delete;
   #else
   private:
   synchronized_pool_resource          (const synchronized_pool_resource&);
   synchronized_pool_resource operator=(const synchronized_pool_resource&);
   public:
   #endif

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::~unsynchronized_pool_resource()
   virtual ~synchronized_pool_resource();

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::release()
   void release();

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::upstream_resource()const
   memory_resource* upstream_resource() const;

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::options()const
   pool_options options() const;

   protected:

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::do_allocate()
   virtual void* do_allocate(std::size_t bytes, std::size_t alignment);

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::do_deallocate(void*,std::size_t,std::size_t)
   virtual void do_deallocate(void* p, std::size_t bytes, std::size_t alignment);

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::do_is_equal(const memory_resource&)const
   virtual bool do_is_equal(const memory_resource& other) const BOOST_NOEXCEPT;

   //Non-standard observers
   public:
   
   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::pool_count()
   std::size_t pool_count() const;

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::pool_index(std::size_t)const
   std::size_t pool_index(std::size_t bytes) const;

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::pool_next_blocks_per_chunk(std::size_t)const
   std::size_t pool_next_blocks_per_chunk(std::size_t pool_idx) const;

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::pool_block(std::size_t)const
   std::size_t pool_block(std::size_t pool_idx) const;

   //! @copydoc ::boost::container::pmr::unsynchronized_pool_resource::pool_cached_blocks(std::size_t)const
   std::size_t pool_cached_blocks(std::size_t pool_idx) const;
};

}  //namespace pmr {
}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_PMR_SYNCHRONIZED_POOL_RESOURCE_HPP
