//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_PMR_UNSYNCHRONIZED_POOL_RESOURCE_HPP
#define BOOST_CONTAINER_PMR_UNSYNCHRONIZED_POOL_RESOURCE_HPP

#if defined (_MSC_VER)
#  pragma once 
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/auto_link.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/detail/pool_resource.hpp>

#include <cstddef>

namespace boost {
namespace container {
namespace pmr {

//! A unsynchronized_pool_resource is a general-purpose memory resources having
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
//! An unsynchronized_pool_resource class may not be accessed from multiple threads
//! simultaneously and thus avoids the cost of synchronization entirely in
//! single-threaded applications.
class BOOST_CONTAINER_DECL unsynchronized_pool_resource
   : public memory_resource
{
   pool_resource m_resource;

   public:

   //! <b>Requires</b>: `upstream` is the address of a valid memory resource.
   //!
   //! <b>Effects</b>: Constructs a pool resource object that will obtain memory
   //!   from upstream whenever the pool resource is unable to satisfy a memory
   //!   request from its own internal data structures. The resulting object will hold
   //!   a copy of upstream, but will not own the resource to which upstream points.
   //!   [ Note: The intention is that calls to upstream->allocate() will be
   //!   substantially fewer than calls to this->allocate() in most cases. - end note 
   //!   The behavior of the pooling mechanism is tuned according to the value of
   //!   the opts argument.
   //!
   //! <b>Throws</b>: Nothing unless upstream->allocate() throws. It is unspecified if
   //!   or under what conditions this constructor calls upstream->allocate().
   unsynchronized_pool_resource(const pool_options& opts, memory_resource* upstream) BOOST_NOEXCEPT;

   //! <b>Effects</b>: Same as
   //!   `unsynchronized_pool_resource(pool_options(), get_default_resource())`.
   unsynchronized_pool_resource() BOOST_NOEXCEPT;

   //! <b>Effects</b>: Same as
   //!   `unsynchronized_pool_resource(pool_options(), upstream)`.
   explicit unsynchronized_pool_resource(memory_resource* upstream) BOOST_NOEXCEPT;

   //! <b>Effects</b>: Same as
   //!   `unsynchronized_pool_resource(opts, get_default_resource())`.
   explicit unsynchronized_pool_resource(const pool_options& opts) BOOST_NOEXCEPT;

   #if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS) || defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
   unsynchronized_pool_resource(const unsynchronized_pool_resource&) = delete;
   unsynchronized_pool_resource operator=(const unsynchronized_pool_resource&) = delete;
   #else
   private:
   unsynchronized_pool_resource          (const unsynchronized_pool_resource&);
   unsynchronized_pool_resource operator=(const unsynchronized_pool_resource&);
   public:
   #endif

   //! <b>Effects</b>: Calls
   //!   `this->release()`.
   virtual ~unsynchronized_pool_resource();

   //! <b>Effects</b>: Calls Calls `upstream_resource()->deallocate()` as necessary
   //!   to release all allocated memory. [ Note: memory is released back to
   //!   `upstream_resource()` even if deallocate has not been called for some
   //!   of the allocated blocks. - end note ]
   void release();

   //! <b>Returns</b>: The value of the upstream argument provided to the
   //!   constructor of this object.
   memory_resource* upstream_resource() const;

   //! <b>Returns</b>: The options that control the pooling behavior of this resource.
   //!   The values in the returned struct may differ from those supplied to the pool
   //!   resource constructor in that values of zero will be replaced with
   //!   implementation-defined defaults and sizes may be rounded to unspecified granularity.
   pool_options options() const;

   protected:

   //! <b>Returns</b>: A pointer to allocated storage with a size of at least `bytes`.
   //!   The size and alignment of the allocated memory shall meet the requirements for
   //!   a class derived from `memory_resource`.
   //!
   //! <b>Effects</b>: If the pool selected for a block of size bytes is unable to
   //!   satisfy the memory request from its own internal data structures, it will call
   //!   `upstream_resource()->allocate()` to obtain more memory. If `bytes` is larger
   //!   than that which the largest pool can handle, then memory will be allocated
   //!   using `upstream_resource()->allocate()`.
   //!
   //! <b>Throws</b>: Nothing unless `upstream_resource()->allocate()` throws.
   virtual void* do_allocate(std::size_t bytes, std::size_t alignment);

   //! <b>Effects</b>: Return the memory at p to the pool. It is unspecified if or under
   //!   what circumstances this operation will result in a call to
   //!   `upstream_resource()->deallocate()`.
   //!
   //! <b>Throws</b>: Nothing.
   virtual void do_deallocate(void* p, std::size_t bytes, std::size_t alignment);

   //! <b>Returns</b>:
   //!   `this == dynamic_cast<const unsynchronized_pool_resource*>(&other)`.
   virtual bool do_is_equal(const memory_resource& other) const BOOST_NOEXCEPT;

   //Non-standard observers
   public:
   //! <b>Returns</b>: The number of pools that will be used in the pool resource.
   //!
   //! <b>Note</b>: Non-standard extension.
   std::size_t pool_count() const;

   //! <b>Returns</b>: The index of the pool that will be used to serve the allocation of `bytes`.
   //!   Returns `pool_count()` if `bytes` is bigger
   //!   than `options().largest_required_pool_block` (no pool will be used to serve this).
   //!
   //! <b>Note</b>: Non-standard extension.
   std::size_t pool_index(std::size_t bytes) const;

   //! <b>Requires</b>: `pool_idx < pool_index()`
   //!
   //! <b>Returns</b>: The number blocks that will be allocated in the next chunk
   //!   from the pool specified by `pool_idx`.
   //!
   //! <b>Note</b>: Non-standard extension.
   std::size_t pool_next_blocks_per_chunk(std::size_t pool_idx) const;

   //! <b>Requires</b>: `pool_idx < pool_index()`
   //!
   //! <b>Returns</b>: The number of bytes of the block that the specified `pool_idx` pool manages.
   //!
   //! <b>Note</b>: Non-standard extension.
   std::size_t pool_block(std::size_t pool_idx) const;

   //! <b>Requires</b>: `pool_idx < pool_index()`
   //!
   //! <b>Returns</b>: The number of blocks that the specified `pool_idx` pool has cached
   //!   and will be served without calling the upstream_allocator.
   //!
   //! <b>Note</b>: Non-standard extension.
   std::size_t pool_cached_blocks(std::size_t pool_idx) const;
};

}  //namespace pmr {
}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_PMR_UNSYNCHRONIZED_POOL_RESOURCE_HPP
