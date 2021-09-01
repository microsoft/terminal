//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_PMR_MEMORY_RESOURCE_HPP
#define BOOST_CONTAINER_PMR_MEMORY_RESOURCE_HPP

#if defined (_MSC_VER)
#  pragma once 
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/container_fwd.hpp>
#include <boost/move/detail/type_traits.hpp>
#include <cstddef>

namespace boost {
namespace container {
namespace pmr {

//! The memory_resource class is an abstract interface to an
//! unbounded set of classes encapsulating memory resources.
class BOOST_CONTAINER_DECL memory_resource
{
   public:
   // For exposition only
   static BOOST_CONSTEXPR_OR_CONST std::size_t max_align =
      boost::move_detail::alignment_of<boost::move_detail::max_align_t>::value;

   //! <b>Effects</b>: Destroys
   //! this memory_resource.
   virtual ~memory_resource(){}

   //! <b>Effects</b>: Equivalent to
   //! `return do_allocate(bytes, alignment);`
   void* allocate(std::size_t bytes, std::size_t alignment = max_align)
   {  return this->do_allocate(bytes, alignment);  }

   //! <b>Effects</b>: Equivalent to
   //! `return do_deallocate(bytes, alignment);`
   void  deallocate(void* p, std::size_t bytes, std::size_t alignment = max_align)
   {  return this->do_deallocate(p, bytes, alignment);  }

   //! <b>Effects</b>: Equivalent to
   //! `return return do_is_equal(other);`
   bool is_equal(const memory_resource& other) const BOOST_NOEXCEPT
   {  return this->do_is_equal(other);  }

   //! <b>Returns</b>:
   //!   `&a == &b || a.is_equal(b)`.
   friend bool operator==(const memory_resource& a, const memory_resource& b) BOOST_NOEXCEPT
   {  return &a == &b || a.is_equal(b);   }

   //! <b>Returns</b>:
   //!   !(a == b).
   friend bool operator!=(const memory_resource& a, const memory_resource& b) BOOST_NOEXCEPT
   {  return !(a == b); }

   protected:
   //! <b>Requires</b>: Alignment shall be a power of two.
   //!
   //! <b>Returns</b>: A derived class shall implement this function to return a pointer
   //!   to allocated storage with a size of at least bytes. The returned storage is
   //!   aligned to the specified alignment, if such alignment is supported; otherwise
   //!   it is aligned to max_align.
   //!
   //! <b>Throws</b>: A derived class implementation shall throw an appropriate exception if
   //!   it is unable to allocate memory with the requested size and alignment.
   virtual void* do_allocate(std::size_t bytes, std::size_t alignment) = 0;

   //! <b>Requires</b>: p shall have been returned from a prior call to
   //!   `allocate(bytes, alignment)` on a memory resource equal to *this, and the storage
   //!   at p shall not yet have been deallocated.
   //!
   //! <b>Effects</b>: A derived class shall implement this function to dispose of allocated storage.
   //!
   //! <b>Throws</b>: Nothing.
   virtual void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) = 0;

   //! <b>Returns</b>: A derived class shall implement this function to return true if memory
   //!   allocated from this can be deallocated from other and vice-versa; otherwise it shall
   //!   return false. <i>[Note: The most-derived type of other might not match the type of this.
   //!   For a derived class, D, a typical implementation of this function will compute
   //!   `dynamic_cast<const D*>(&other)` and go no further (i.e., return false)
   //!   if it returns nullptr. - end note]</i>.
   virtual bool do_is_equal(const memory_resource& other) const BOOST_NOEXCEPT = 0;
};

}  //namespace pmr {
}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_PMR_MEMORY_RESOURCE_HPP
