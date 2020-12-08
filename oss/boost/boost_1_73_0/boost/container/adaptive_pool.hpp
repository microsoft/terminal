//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_ADAPTIVE_POOL_HPP
#define BOOST_CONTAINER_ADAPTIVE_POOL_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/container_fwd.hpp>
#include <boost/container/detail/version_type.hpp>
#include <boost/container/throw_exception.hpp>
#include <boost/container/detail/adaptive_node_pool.hpp>
#include <boost/container/detail/multiallocation_chain.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/dlmalloc.hpp>
#include <boost/container/detail/singleton.hpp>
#include <boost/container/detail/placement_new.hpp>

#include <boost/assert.hpp>
#include <boost/static_assert.hpp>
#include <boost/move/utility_core.hpp>
#include <cstddef>


namespace boost {
namespace container {

//!An STL node allocator that uses a modified DLMalloc as memory
//!source.
//!
//!This node allocator shares a segregated storage between all instances
//!of adaptive_pool with equal sizeof(T).
//!
//!NodesPerBlock is the number of nodes allocated at once when the allocator
//!needs runs out of nodes. MaxFreeBlocks is the maximum number of totally free blocks
//!that the adaptive node pool will hold. The rest of the totally free blocks will be
//!deallocated to the memory manager.
//!
//!OverheadPercent is the (approximated) maximum size overhead (1-20%) of the allocator:
//!(memory usable for nodes / total memory allocated from the memory allocator)
template < class T
         , std::size_t NodesPerBlock   BOOST_CONTAINER_DOCONLY(= ADP_nodes_per_block)
         , std::size_t MaxFreeBlocks   BOOST_CONTAINER_DOCONLY(= ADP_max_free_blocks)
         , std::size_t OverheadPercent BOOST_CONTAINER_DOCONLY(= ADP_overhead_percent)
         BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I unsigned Version)
         >
class adaptive_pool
{
   //!If Version is 1, the allocator is a STL conforming allocator. If Version is 2,
   //!the allocator offers advanced expand in place and burst allocation capabilities.
   public:
   typedef unsigned int allocation_type;
   typedef adaptive_pool
      <T, NodesPerBlock, MaxFreeBlocks, OverheadPercent
         BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I Version)
         >   self_t;

   static const std::size_t nodes_per_block        = NodesPerBlock;
   static const std::size_t max_free_blocks        = MaxFreeBlocks;
   static const std::size_t overhead_percent       = OverheadPercent;
   static const std::size_t real_nodes_per_block   = NodesPerBlock;

   BOOST_CONTAINER_DOCIGN(BOOST_STATIC_ASSERT((Version <=2)));

   public:
   //-------
   typedef T                                    value_type;
   typedef T *                                  pointer;
   typedef const T *                            const_pointer;
   typedef typename ::boost::container::
      dtl::unvoid_ref<T>::type     reference;
   typedef typename ::boost::container::
      dtl::unvoid_ref<const T>::type     const_reference;
   typedef std::size_t                          size_type;
   typedef std::ptrdiff_t                       difference_type;

   typedef boost::container::dtl::
      version_type<self_t, Version>             version;

   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   typedef boost::container::dtl::
      basic_multiallocation_chain<void*>              multiallocation_chain_void;
   typedef boost::container::dtl::
      transform_multiallocation_chain
         <multiallocation_chain_void, T>              multiallocation_chain;
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   //!Obtains adaptive_pool from
   //!adaptive_pool
   template<class T2>
   struct rebind
   {
      typedef adaptive_pool
         < T2
         , NodesPerBlock
         , MaxFreeBlocks
         , OverheadPercent
         BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I Version)
         >       other;
   };

   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   private:
   //!Not assignable from related adaptive_pool
   template<class T2, std::size_t N2, std::size_t F2, std::size_t O2, unsigned Version2>
   adaptive_pool& operator=
      (const adaptive_pool<T2, N2, F2, O2, Version2>&);

   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   public:
   //!Default constructor
   adaptive_pool() BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!Copy constructor from other adaptive_pool.
   adaptive_pool(const adaptive_pool &) BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!Copy constructor from related adaptive_pool.
   template<class T2>
   adaptive_pool
      (const adaptive_pool<T2, NodesPerBlock, MaxFreeBlocks, OverheadPercent
            BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I Version)> &) BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!Destructor
   ~adaptive_pool() BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!Returns the number of elements that could be allocated.
   //!Never throws
   size_type max_size() const BOOST_NOEXCEPT_OR_NOTHROW
   {  return size_type(-1)/(2u*sizeof(T));   }

   //!Allocate memory for an array of count elements.
   //!Throws std::bad_alloc if there is no enough memory
   pointer allocate(size_type count, const void * = 0)
   {
      if(BOOST_UNLIKELY(count > size_type(-1)/(2u*sizeof(T))))
         boost::container::throw_bad_alloc();

      if(Version == 1 && count == 1){
         typedef typename dtl::shared_adaptive_node_pool
            <sizeof(T), NodesPerBlock, MaxFreeBlocks, OverheadPercent> shared_pool_t;
         typedef dtl::singleton_default<shared_pool_t> singleton_t;
         return pointer(static_cast<T*>(singleton_t::instance().allocate_node()));
      }
      else{
         return static_cast<pointer>(dlmalloc_malloc(count*sizeof(T)));
      }
   }

   //!Deallocate allocated memory.
   //!Never throws
   void deallocate(const pointer &ptr, size_type count) BOOST_NOEXCEPT_OR_NOTHROW
   {
      (void)count;
      if(Version == 1 && count == 1){
         typedef dtl::shared_adaptive_node_pool
            <sizeof(T), NodesPerBlock, MaxFreeBlocks, OverheadPercent> shared_pool_t;
         typedef dtl::singleton_default<shared_pool_t> singleton_t;
         singleton_t::instance().deallocate_node(ptr);
      }
      else{
         dlmalloc_free(ptr);
      }
   }

   pointer allocation_command(allocation_type command,
                         size_type limit_size,
                         size_type &prefer_in_recvd_out_size,
                         pointer &reuse)
   {
      pointer ret = this->priv_allocation_command(command, limit_size, prefer_in_recvd_out_size, reuse);
      if(BOOST_UNLIKELY(!ret && !(command & BOOST_CONTAINER_NOTHROW_ALLOCATION)))
         boost::container::throw_bad_alloc();
      return ret;
   }

   //!Returns maximum the number of objects the previously allocated memory
   //!pointed by p can hold.
   size_type size(pointer p) const BOOST_NOEXCEPT_OR_NOTHROW
   {  return dlmalloc_size(p);  }

   //!Allocates just one object. Memory allocated with this function
   //!must be deallocated only with deallocate_one().
   //!Throws bad_alloc if there is no enough memory
   pointer allocate_one()
   {
      typedef dtl::shared_adaptive_node_pool
         <sizeof(T), NodesPerBlock, MaxFreeBlocks, OverheadPercent> shared_pool_t;
      typedef dtl::singleton_default<shared_pool_t> singleton_t;
      return (pointer)singleton_t::instance().allocate_node();
   }

   //!Allocates many elements of size == 1.
   //!Elements must be individually deallocated with deallocate_one()
   void allocate_individual(std::size_t num_elements, multiallocation_chain &chain)
   {
      typedef dtl::shared_adaptive_node_pool
         <sizeof(T), NodesPerBlock, MaxFreeBlocks, OverheadPercent> shared_pool_t;
      typedef dtl::singleton_default<shared_pool_t> singleton_t;
      singleton_t::instance().allocate_nodes(num_elements, static_cast<typename shared_pool_t::multiallocation_chain&>(chain));
      //typename shared_pool_t::multiallocation_chain ch;
      //singleton_t::instance().allocate_nodes(num_elements, ch);
      //chain.incorporate_after
         //(chain.before_begin(), (T*)&*ch.begin(), (T*)&*ch.last(), ch.size());
   }

   //!Deallocates memory previously allocated with allocate_one().
   //!You should never use deallocate_one to deallocate memory allocated
   //!with other functions different from allocate_one(). Never throws
   void deallocate_one(pointer p) BOOST_NOEXCEPT_OR_NOTHROW
   {
      typedef dtl::shared_adaptive_node_pool
         <sizeof(T), NodesPerBlock, MaxFreeBlocks, OverheadPercent> shared_pool_t;
      typedef dtl::singleton_default<shared_pool_t> singleton_t;
      singleton_t::instance().deallocate_node(p);
   }

   void deallocate_individual(multiallocation_chain &chain) BOOST_NOEXCEPT_OR_NOTHROW
   {
      typedef dtl::shared_adaptive_node_pool
         <sizeof(T), NodesPerBlock, MaxFreeBlocks, OverheadPercent> shared_pool_t;
      typedef dtl::singleton_default<shared_pool_t> singleton_t;
      //typename shared_pool_t::multiallocation_chain ch(&*chain.begin(), &*chain.last(), chain.size());
      //singleton_t::instance().deallocate_nodes(ch);
      singleton_t::instance().deallocate_nodes(chain);
   }

   //!Allocates many elements of size elem_size.
   //!Elements must be individually deallocated with deallocate()
   void allocate_many(size_type elem_size, std::size_t n_elements, multiallocation_chain &chain)
   {
      BOOST_STATIC_ASSERT(( Version > 1 ));/*
      dlmalloc_memchain ch;
      BOOST_CONTAINER_MEMCHAIN_INIT(&ch);
      if(BOOST_UNLIKELY(!dlmalloc_multialloc_nodes(n_elements, elem_size*sizeof(T), DL_MULTIALLOC_DEFAULT_CONTIGUOUS, &ch))){
         boost::container::throw_bad_alloc();
      }
      chain.incorporate_after(chain.before_begin()
                             ,(T*)BOOST_CONTAINER_MEMCHAIN_FIRSTMEM(&ch)
                             ,(T*)BOOST_CONTAINER_MEMCHAIN_LASTMEM(&ch)
                             ,BOOST_CONTAINER_MEMCHAIN_SIZE(&ch) );*/
      if(BOOST_UNLIKELY(!dlmalloc_multialloc_nodes
            (n_elements, elem_size*sizeof(T), DL_MULTIALLOC_DEFAULT_CONTIGUOUS, reinterpret_cast<dlmalloc_memchain *>(&chain)))){
         boost::container::throw_bad_alloc();
      }
   }

   //!Allocates n_elements elements, each one of size elem_sizes[i]
   //!Elements must be individually deallocated with deallocate()
   void allocate_many(const size_type *elem_sizes, size_type n_elements, multiallocation_chain &chain)
   {
      BOOST_STATIC_ASSERT(( Version > 1 ));/*
      dlmalloc_memchain ch;
      BOOST_CONTAINER_MEMCHAIN_INIT(&ch);
      if(BOOST_UNLIKELY(!dlmalloc_multialloc_arrays(n_elements, elem_sizes, sizeof(T), DL_MULTIALLOC_DEFAULT_CONTIGUOUS, &ch))){
         boost::container::throw_bad_alloc();
      }
      chain.incorporate_after(chain.before_begin()
                             ,(T*)BOOST_CONTAINER_MEMCHAIN_FIRSTMEM(&ch)
                             ,(T*)BOOST_CONTAINER_MEMCHAIN_LASTMEM(&ch)
                             ,BOOST_CONTAINER_MEMCHAIN_SIZE(&ch) );*/
      if(BOOST_UNLIKELY(!dlmalloc_multialloc_arrays
         (n_elements, elem_sizes, sizeof(T), DL_MULTIALLOC_DEFAULT_CONTIGUOUS, reinterpret_cast<dlmalloc_memchain *>(&chain)))){
         boost::container::throw_bad_alloc();
      }
   }

   void deallocate_many(multiallocation_chain &chain) BOOST_NOEXCEPT_OR_NOTHROW
   {/*
      dlmalloc_memchain ch;
      void *beg(&*chain.begin()), *last(&*chain.last());
      size_t size(chain.size());
      BOOST_CONTAINER_MEMCHAIN_INIT_FROM(&ch, beg, last, size);
      dlmalloc_multidealloc(&ch);*/
      dlmalloc_multidealloc(reinterpret_cast<dlmalloc_memchain *>(&chain));
   }

   //!Deallocates all free blocks of the pool
   static void deallocate_free_blocks() BOOST_NOEXCEPT_OR_NOTHROW
   {
      typedef dtl::shared_adaptive_node_pool
         <sizeof(T), NodesPerBlock, MaxFreeBlocks, OverheadPercent> shared_pool_t;
      typedef dtl::singleton_default<shared_pool_t> singleton_t;
      singleton_t::instance().deallocate_free_blocks();
   }

   //!Swaps allocators. Does not throw. If each allocator is placed in a
   //!different memory segment, the result is undefined.
   friend void swap(adaptive_pool &, adaptive_pool &) BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!An allocator always compares to true, as memory allocated with one
   //!instance can be deallocated by another instance
   friend bool operator==(const adaptive_pool &, const adaptive_pool &) BOOST_NOEXCEPT_OR_NOTHROW
   {  return true;   }

   //!An allocator always compares to false, as memory allocated with one
   //!instance can be deallocated by another instance
   friend bool operator!=(const adaptive_pool &, const adaptive_pool &) BOOST_NOEXCEPT_OR_NOTHROW
   {  return false;   }

   private:
   pointer priv_allocation_command
      (allocation_type command,   std::size_t limit_size
      ,size_type &prefer_in_recvd_out_size, pointer &reuse_ptr)
   {
      std::size_t const preferred_size = prefer_in_recvd_out_size;
      dlmalloc_command_ret_t ret = {0 , 0};
      if(BOOST_UNLIKELY(limit_size > this->max_size() || preferred_size > this->max_size())){
         return pointer();
      }
      std::size_t l_size = limit_size*sizeof(T);
      std::size_t p_size = preferred_size*sizeof(T);
      std::size_t r_size;
      {
         void* reuse_ptr_void = reuse_ptr;
         ret = dlmalloc_allocation_command(command, sizeof(T), l_size, p_size, &r_size, reuse_ptr_void);
         reuse_ptr = ret.second ? static_cast<T*>(reuse_ptr_void) : 0;
      }
      prefer_in_recvd_out_size = r_size/sizeof(T);
      return (pointer)ret.first;
   }
};




















template < class T
         , std::size_t NodesPerBlock   = ADP_nodes_per_block
         , std::size_t MaxFreeBlocks   = ADP_max_free_blocks
         , std::size_t OverheadPercent = ADP_overhead_percent
         , unsigned Version = 2
         >
class private_adaptive_pool
{
   //!If Version is 1, the allocator is a STL conforming allocator. If Version is 2,
   //!the allocator offers advanced expand in place and burst allocation capabilities.
   public:
   typedef unsigned int allocation_type;
   typedef private_adaptive_pool
      <T, NodesPerBlock, MaxFreeBlocks, OverheadPercent
         BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I Version)
         >   self_t;

   static const std::size_t nodes_per_block        = NodesPerBlock;
   static const std::size_t max_free_blocks        = MaxFreeBlocks;
   static const std::size_t overhead_percent       = OverheadPercent;
   static const std::size_t real_nodes_per_block   = NodesPerBlock;

   BOOST_CONTAINER_DOCIGN(BOOST_STATIC_ASSERT((Version <=2)));

   typedef dtl::private_adaptive_node_pool
      <sizeof(T), NodesPerBlock, MaxFreeBlocks, OverheadPercent> pool_t;
   pool_t m_pool;

   public:
   //-------
   typedef T                                    value_type;
   typedef T *                                  pointer;
   typedef const T *                            const_pointer;
   typedef typename ::boost::container::
      dtl::unvoid_ref<T>::type     reference;
   typedef typename ::boost::container::
      dtl::unvoid_ref<const T>::type            const_reference;
   typedef std::size_t                          size_type;
   typedef std::ptrdiff_t                       difference_type;

   typedef boost::container::dtl::
      version_type<self_t, Version>             version;

   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   typedef boost::container::dtl::
      basic_multiallocation_chain<void*>              multiallocation_chain_void;
   typedef boost::container::dtl::
      transform_multiallocation_chain
         <multiallocation_chain_void, T>              multiallocation_chain;
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   //!Obtains private_adaptive_pool from
   //!private_adaptive_pool
   template<class T2>
   struct rebind
   {
      typedef private_adaptive_pool
         < T2
         , NodesPerBlock
         , MaxFreeBlocks
         , OverheadPercent
         BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I Version)
         >       other;
   };

   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   private:
   //!Not assignable from related private_adaptive_pool
   template<class T2, std::size_t N2, std::size_t F2, std::size_t O2, unsigned Version2>
   private_adaptive_pool& operator=
      (const private_adaptive_pool<T2, N2, F2, O2, Version2>&);
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   public:
   //!Default constructor
   private_adaptive_pool() BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!Copy constructor from other private_adaptive_pool.
   private_adaptive_pool(const private_adaptive_pool &) BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!Copy constructor from related private_adaptive_pool.
   template<class T2>
   private_adaptive_pool
      (const private_adaptive_pool<T2, NodesPerBlock, MaxFreeBlocks, OverheadPercent
            BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I Version)> &) BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!Destructor
   ~private_adaptive_pool() BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!Returns the number of elements that could be allocated.
   //!Never throws
   size_type max_size() const BOOST_NOEXCEPT_OR_NOTHROW
   {  return size_type(-1)/(2u*sizeof(T));   }

   //!Allocate memory for an array of count elements.
   //!Throws std::bad_alloc if there is no enough memory
   pointer allocate(size_type count, const void * = 0)
   {
      if(BOOST_UNLIKELY(count > size_type(-1)/(2u*sizeof(T))))
         boost::container::throw_bad_alloc();

      if(Version == 1 && count == 1){
         return pointer(static_cast<T*>(m_pool.allocate_node()));
      }
      else{
         return static_cast<pointer>(dlmalloc_malloc(count*sizeof(T)));
      }
   }

   //!Deallocate allocated memory.
   //!Never throws
   void deallocate(const pointer &ptr, size_type count) BOOST_NOEXCEPT_OR_NOTHROW
   {
      (void)count;
      if(Version == 1 && count == 1){
         m_pool.deallocate_node(ptr);
      }
      else{
         dlmalloc_free(ptr);
      }
   }

   pointer allocation_command(allocation_type command,
                         size_type limit_size,
                         size_type &prefer_in_recvd_out_size,
                         pointer &reuse)
   {
      pointer ret = this->priv_allocation_command(command, limit_size, prefer_in_recvd_out_size, reuse);
      if(BOOST_UNLIKELY(!ret && !(command & BOOST_CONTAINER_NOTHROW_ALLOCATION)))
         boost::container::throw_bad_alloc();
      return ret;
   }

   //!Returns maximum the number of objects the previously allocated memory
   //!pointed by p can hold.
   size_type size(pointer p) const BOOST_NOEXCEPT_OR_NOTHROW
   {  return dlmalloc_size(p);  }

   //!Allocates just one object. Memory allocated with this function
   //!must be deallocated only with deallocate_one().
   //!Throws bad_alloc if there is no enough memory
   pointer allocate_one()
   {
      return (pointer)m_pool.allocate_node();
   }

   //!Allocates many elements of size == 1.
   //!Elements must be individually deallocated with deallocate_one()
   void allocate_individual(std::size_t num_elements, multiallocation_chain &chain)
   {
      m_pool.allocate_nodes(num_elements, static_cast<typename pool_t::multiallocation_chain&>(chain));
   }

   //!Deallocates memory previously allocated with allocate_one().
   //!You should never use deallocate_one to deallocate memory allocated
   //!with other functions different from allocate_one(). Never throws
   void deallocate_one(pointer p) BOOST_NOEXCEPT_OR_NOTHROW
   {
      m_pool.deallocate_node(p);
   }

   void deallocate_individual(multiallocation_chain &chain) BOOST_NOEXCEPT_OR_NOTHROW
   {
      m_pool.deallocate_nodes(chain);
   }

   //!Allocates many elements of size elem_size.
   //!Elements must be individually deallocated with deallocate()
   void allocate_many(size_type elem_size, std::size_t n_elements, multiallocation_chain &chain)
   {
      BOOST_STATIC_ASSERT(( Version > 1 ));
      if(BOOST_UNLIKELY(!dlmalloc_multialloc_nodes
            (n_elements, elem_size*sizeof(T), DL_MULTIALLOC_DEFAULT_CONTIGUOUS, reinterpret_cast<dlmalloc_memchain *>(&chain)))){
         boost::container::throw_bad_alloc();
      }
   }

   //!Allocates n_elements elements, each one of size elem_sizes[i]
   //!Elements must be individually deallocated with deallocate()
   void allocate_many(const size_type *elem_sizes, size_type n_elements, multiallocation_chain &chain)
   {
      BOOST_STATIC_ASSERT(( Version > 1 ));
      if(BOOST_UNLIKELY(!dlmalloc_multialloc_arrays
         (n_elements, elem_sizes, sizeof(T), DL_MULTIALLOC_DEFAULT_CONTIGUOUS, reinterpret_cast<dlmalloc_memchain *>(&chain)))){
         boost::container::throw_bad_alloc();
      }
   }

   void deallocate_many(multiallocation_chain &chain) BOOST_NOEXCEPT_OR_NOTHROW
   {
      dlmalloc_multidealloc(reinterpret_cast<dlmalloc_memchain *>(&chain));
   }

   //!Deallocates all free blocks of the pool
   void deallocate_free_blocks() BOOST_NOEXCEPT_OR_NOTHROW
   {
      m_pool.deallocate_free_blocks();
   }

   //!Swaps allocators. Does not throw. If each allocator is placed in a
   //!different memory segment, the result is undefined.
   friend void swap(private_adaptive_pool &, private_adaptive_pool &) BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //!An allocator always compares to true, as memory allocated with one
   //!instance can be deallocated by another instance
   friend bool operator==(const private_adaptive_pool &, const private_adaptive_pool &) BOOST_NOEXCEPT_OR_NOTHROW
   {  return true;   }

   //!An allocator always compares to false, as memory allocated with one
   //!instance can be deallocated by another instance
   friend bool operator!=(const private_adaptive_pool &, const private_adaptive_pool &) BOOST_NOEXCEPT_OR_NOTHROW
   {  return false;   }

   private:
   pointer priv_allocation_command
      (allocation_type command,   std::size_t limit_size
      ,size_type &prefer_in_recvd_out_size, pointer &reuse_ptr)
   {
      std::size_t const preferred_size = prefer_in_recvd_out_size;
      dlmalloc_command_ret_t ret = {0 , 0};
      if(BOOST_UNLIKELY(limit_size > this->max_size() || preferred_size > this->max_size())){
         return pointer();
      }
      std::size_t l_size = limit_size*sizeof(T);
      std::size_t p_size = preferred_size*sizeof(T);
      std::size_t r_size;
      {
         void* reuse_ptr_void = reuse_ptr;
         ret = dlmalloc_allocation_command(command, sizeof(T), l_size, p_size, &r_size, reuse_ptr_void);
         reuse_ptr = ret.second ? static_cast<T*>(reuse_ptr_void) : 0;
      }
      prefer_in_recvd_out_size = r_size/sizeof(T);
      return (pointer)ret.first;
   }
};

}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //#ifndef BOOST_CONTAINER_ADAPTIVE_POOL_HPP
