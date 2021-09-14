//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_PMR_MONOTONIC_BUFFER_RESOURCE_HPP
#define BOOST_CONTAINER_PMR_MONOTONIC_BUFFER_RESOURCE_HPP

#if defined (_MSC_VER)
#  pragma once 
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/auto_link.hpp>
#include <boost/container/container_fwd.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/detail/block_slist.hpp>

#include <cstddef>

namespace boost {
namespace container {
namespace pmr {

//! A monotonic_buffer_resource is a special-purpose memory resource intended for
//! very fast memory allocations in situations where memory is used to build up a
//! few objects and then is released all at once when the memory resource object
//! is destroyed. It has the following qualities:
//! 
//! - A call to deallocate has no effect, thus the amount of memory consumed
//!   increases monotonically until the resource is destroyed.
//! 
//! - The program can supply an initial buffer, which the allocator uses to satisfy
//!   memory requests.
//! 
//! - When the initial buffer (if any) is exhausted, it obtains additional buffers
//!   from an upstream memory resource supplied at construction. Each additional
//!   buffer is larger than the previous one, following a geometric progression.
//! 
//! - It is intended for access from one thread of control at a time. Specifically,
//!   calls to allocate and deallocate do not synchronize with one another.
//! 
//! - It owns the allocated memory and frees it on destruction, even if deallocate has
//!   not been called for some of the allocated blocks.
class BOOST_CONTAINER_DECL monotonic_buffer_resource
   : public memory_resource
{
   block_slist       m_memory_blocks;
   void *            m_current_buffer;
   std::size_t       m_current_buffer_size;
   std::size_t       m_next_buffer_size;
   void * const      m_initial_buffer;
   std::size_t const m_initial_buffer_size;

   /// @cond
   void increase_next_buffer();
   void increase_next_buffer_at_least_to(std::size_t minimum_size);
   void *allocate_from_current(std::size_t aligner, std::size_t bytes);
   /// @endcond

   public:

   //! The number of bytes that will be requested by the default in the first call
   //! to the upstream allocator
   //!
   //! <b>Note</b>: Non-standard extension.
   static const std::size_t initial_next_buffer_size = 32u*sizeof(void*);

   //! <b>Requires</b>: `upstream` shall be the address of a valid memory resource or `nullptr`
   //!
   //! <b>Effects</b>: If `upstream` is not nullptr, sets the internal resource to `upstream`,
   //!   to get_default_resource() otherwise.
   //!   Sets the internal `current_buffer` to `nullptr` and the internal `next_buffer_size` to an
   //!   implementation-defined size.
   explicit monotonic_buffer_resource(memory_resource* upstream = 0) BOOST_NOEXCEPT;

   //! <b>Requires</b>: `upstream` shall be the address of a valid memory resource or `nullptr`
   //!   and `initial_size` shall be greater than zero.
   //!
   //! <b>Effects</b>: If `upstream` is not nullptr, sets the internal resource to `upstream`,
   //!   to get_default_resource() otherwise. Sets the internal `current_buffer` to `nullptr` and
   //!   `next_buffer_size` to at least `initial_size`.
   explicit monotonic_buffer_resource(std::size_t initial_size, memory_resource* upstream = 0) BOOST_NOEXCEPT;

   //! <b>Requires</b>: `upstream` shall be the address of a valid memory resource or `nullptr`,
   //!   `buffer_size` shall be no larger than the number of bytes in buffer.
   //!
   //! <b>Effects</b>: If `upstream` is not nullptr, sets the internal resource to `upstream`,
   //!   to get_default_resource() otherwise. Sets the internal `current_buffer` to `buffer`,
   //!   and `next_buffer_size` to `buffer_size` (but not less than an implementation-defined size),
   //!   then increases `next_buffer_size` by an implementation-defined growth factor (which need not be integral).
   monotonic_buffer_resource(void* buffer, std::size_t buffer_size, memory_resource* upstream = 0) BOOST_NOEXCEPT;

   #if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS) || defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
   monotonic_buffer_resource(const monotonic_buffer_resource&) = delete;
   monotonic_buffer_resource operator=(const monotonic_buffer_resource&) = delete;
   #else
   private:
   monotonic_buffer_resource          (const monotonic_buffer_resource&);
   monotonic_buffer_resource operator=(const monotonic_buffer_resource&);
   public:
   #endif

   //! <b>Effects</b>: Calls
   //!   `this->release()`.
   virtual ~monotonic_buffer_resource();

   //! <b>Effects</b>: `upstream_resource()->deallocate()` as necessary to release all allocated memory.
   //!   [Note: memory is released back to `upstream_resource()` even if some blocks that were allocated
   //!   from this have not been deallocated from this. - end note]
   void release() BOOST_NOEXCEPT;

   //! <b>Returns</b>: The value of
   //!   the internal resource.
   memory_resource* upstream_resource() const BOOST_NOEXCEPT;

   //! <b>Returns</b>:
   //!   The number of bytes of storage available for the specified alignment and
   //!   the number of bytes wasted due to the requested alignment.
   //!
   //! <b>Note</b>: Non-standard extension.
   std::size_t remaining_storage(std::size_t alignment, std::size_t &wasted_due_to_alignment) const BOOST_NOEXCEPT;
   
   //! <b>Returns</b>:
   //!   The number of bytes of storage available for the specified alignment.
   //!
   //! <b>Note</b>: Non-standard extension.
   std::size_t remaining_storage(std::size_t alignment = 1u) const BOOST_NOEXCEPT;

   //! <b>Returns</b>:
   //!   The address pointing to the start of the current free storage.
   //!
   //! <b>Note</b>: Non-standard extension.
   const void *current_buffer() const BOOST_NOEXCEPT;

   //! <b>Returns</b>:
   //!   The number of bytes that will be requested for the next buffer once the
   //!   current one is exhausted.
   //!
   //! <b>Note</b>: Non-standard extension.
   std::size_t next_buffer_size() const BOOST_NOEXCEPT;

   protected:

   //! <b>Returns</b>: A pointer to allocated storage with a size of at least `bytes`. The size
   //!   and alignment of the allocated memory shall meet the requirements for a class derived
   //!   from `memory_resource`.
   //!
   //! <b>Effects</b>: If the unused space in the internal `current_buffer` can fit a block with the specified
   //!   bytes and alignment, then allocate the return block from the internal `current_buffer`; otherwise sets
   //!   the internal `current_buffer` to `upstream_resource()->allocate(n, m)`, where `n` is not less than
   //!   `max(bytes, next_buffer_size)` and `m` is not less than alignment, and increase
   //!   `next_buffer_size` by an implementation-defined growth factor (which need not be integral),
   //!   then allocate the return block from the newly-allocated internal `current_buffer`.
   //!
   //! <b>Throws</b>: Nothing unless `upstream_resource()->allocate()` throws.
   virtual void* do_allocate(std::size_t bytes, std::size_t alignment);

   //! <b>Effects</b>: None
   //!
   //! <b>Throws</b>: Nothing
   //!
   //! <b>Remarks</b>: Memory used by this resource increases monotonically until its destruction.
   virtual void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) BOOST_NOEXCEPT;

   //! <b>Returns</b>:
   //!   `this == dynamic_cast<const monotonic_buffer_resource*>(&other)`.
   virtual bool do_is_equal(const memory_resource& other) const BOOST_NOEXCEPT;
};

}  //namespace pmr {
}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_PMR_MONOTONIC_BUFFER_RESOURCE_HPP
