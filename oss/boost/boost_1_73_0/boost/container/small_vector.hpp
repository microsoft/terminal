//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_CONTAINER_SMALL_VECTOR_HPP
#define BOOST_CONTAINER_CONTAINER_SMALL_VECTOR_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

// container
#include <boost/container/container_fwd.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/allocator_traits.hpp>
#include <boost/container/new_allocator.hpp> //new_allocator
// container/detail
#include <boost/container/detail/type_traits.hpp>
#include <boost/container/detail/version_type.hpp>

//move
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/iterator.hpp>

//move/detail
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/move/detail/fwd_macros.hpp>
#endif

//std
#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
#include <initializer_list>   //for std::initializer_list
#endif

namespace boost {
namespace container {

namespace dtl{

template<class Options>
struct get_small_vector_opt
{
   typedef Options type;
};

template<>
struct get_small_vector_opt<void>
{
   typedef small_vector_null_opt type;
};

template<class Options>
struct get_vopt_from_svopt
   : get_small_vector_opt<Options>::type
{
   typedef typename get_small_vector_opt<Options>::type options_t;
   typedef vector_opt< typename options_t::growth_factor_type, void> type;
};

template<>
struct get_vopt_from_svopt<void>
{
   typedef void type;
};

template <class T, class SecondaryAllocator, class Options>
struct vector_for_small_vector
{
   typedef vector
      < T
      , small_vector_allocator
         < T 
         , typename allocator_traits<typename real_allocator<T, SecondaryAllocator>::type>::template portable_rebind_alloc<void>::type
         , Options>
      , typename dtl::get_vopt_from_svopt<Options>::type
      > type;
};

}  //namespace dtl

//! A non-standard allocator used to implement `small_vector`.
//! Users should never use it directly. It is described here
//! for documentation purposes.
//! 
//! This allocator inherits from a standard-conforming allocator
//! and forwards member functions to the standard allocator except
//! when internal storage is being used as memory source.
//!
//! This allocator is a "partially_propagable" allocator and
//! defines `is_partially_propagable` as true_type.
//! 
//! A partially propagable allocator means that not all storage
//! allocatod by an instance of `small_vector_allocator` can be
//! deallocated by another instance of this type, even if both
//! instances compare equal or an instance is propagated to another
//! one using the copy/move constructor or assignment. The storage that
//! can never be propagated is identified by `storage_is_unpropagable(p)`.
//!
//! `boost::container::vector` supports partially propagable allocators
//! fallbacking to deep copy/swap/move operations when internal storage
//! is being used to store vector elements.
//!
//! `small_vector_allocator` assumes that will be instantiated as
//! `boost::container::vector< T, small_vector_allocator<T, Allocator> >`
//! and internal storage can be obtained downcasting that vector
//! to `small_vector_base<T>`.
template<class T, class VoidAllocator BOOST_CONTAINER_DOCONLY(= void), class Options BOOST_CONTAINER_DOCONLY(= void)>
class small_vector_allocator
   : public allocator_traits<typename real_allocator<T, VoidAllocator>::type>::template portable_rebind_alloc<T>::type
{
   typedef unsigned int allocation_type;
   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   private:

   typedef typename allocator_traits<typename real_allocator<T, VoidAllocator>::type>::template portable_rebind_alloc<T>::type allocator_type;

   BOOST_COPYABLE_AND_MOVABLE(small_vector_allocator)

   BOOST_CONTAINER_FORCEINLINE const allocator_type &as_base() const BOOST_NOEXCEPT
   {  return static_cast<const allocator_type&>(*this);  }

   BOOST_CONTAINER_FORCEINLINE allocator_type &as_base() BOOST_NOEXCEPT
   {  return static_cast<allocator_type&>(*this);  }

   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   public:
   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   typedef allocator_traits<allocator_type> allocator_traits_type;
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   typedef typename allocator_traits<allocator_type>::value_type          value_type;
   typedef typename allocator_traits<allocator_type>::pointer             pointer;
   typedef typename allocator_traits<allocator_type>::const_pointer       const_pointer;
   typedef typename allocator_traits<allocator_type>::reference           reference;
   typedef typename allocator_traits<allocator_type>::const_reference     const_reference;
   typedef typename allocator_traits<allocator_type>::size_type           size_type;
   typedef typename allocator_traits<allocator_type>::difference_type     difference_type;
   typedef typename allocator_traits<allocator_type>::void_pointer        void_pointer;
   typedef typename allocator_traits<allocator_type>::const_void_pointer  const_void_pointer;

   typedef typename allocator_traits<allocator_type>::propagate_on_container_copy_assignment   propagate_on_container_copy_assignment;
   typedef typename allocator_traits<allocator_type>::propagate_on_container_move_assignment   propagate_on_container_move_assignment;
   typedef typename allocator_traits<allocator_type>::propagate_on_container_swap              propagate_on_container_swap;
   //! An integral constant with member `value == false`
   typedef BOOST_CONTAINER_IMPDEF(dtl::bool_<false>)                         is_always_equal;
   //! An integral constant with member `value == true`
   typedef BOOST_CONTAINER_IMPDEF(dtl::bool_<true>)                          is_partially_propagable;

   BOOST_CONTAINER_DOCIGN(typedef dtl::version_type<small_vector_allocator BOOST_CONTAINER_I 1>  version;)

   //!Obtains an small_vector_allocator that allocates
   //!objects of type T2
   template<class T2>
   struct rebind
   {
      typedef typename allocator_traits<allocator_type>::template portable_rebind_alloc<T2>::type other;
   };

   BOOST_CONTAINER_FORCEINLINE small_vector_allocator() BOOST_NOEXCEPT_IF(dtl::is_nothrow_default_constructible<allocator_type>::value)
   {}

   //!Constructor from other small_vector_allocator.
   //!Never throws
   BOOST_CONTAINER_FORCEINLINE small_vector_allocator
      (const small_vector_allocator &other) BOOST_NOEXCEPT_OR_NOTHROW
      : allocator_type(other.as_base())
   {}

   //!Move constructor from small_vector_allocator.
   //!Never throws
   BOOST_CONTAINER_FORCEINLINE small_vector_allocator
      (BOOST_RV_REF(small_vector_allocator) other) BOOST_NOEXCEPT_OR_NOTHROW
      : allocator_type(::boost::move(other.as_base()))
   {}

   //!Constructor from related small_vector_allocator.
   //!Never throws
   template<class U, class OtherVoidAllocator, class OtherOptions>
   BOOST_CONTAINER_FORCEINLINE small_vector_allocator
      (const small_vector_allocator<U, OtherVoidAllocator, OtherOptions> &other) BOOST_NOEXCEPT_OR_NOTHROW
      : allocator_type(other.as_base())
   {}

   //!Move constructor from related small_vector_allocator.
   //!Never throws
   template<class U, class OtherVoidAllocator, class OtherOptions>
   BOOST_CONTAINER_FORCEINLINE small_vector_allocator
      (BOOST_RV_REF(small_vector_allocator<U BOOST_MOVE_I OtherVoidAllocator BOOST_MOVE_I OtherOptions>) other) BOOST_NOEXCEPT_OR_NOTHROW
      : allocator_type(::boost::move(other.as_base()))
   {}

   //!Constructor from allocator_type.
   //!Never throws
   BOOST_CONTAINER_FORCEINLINE explicit small_vector_allocator
      (const allocator_type &other) BOOST_NOEXCEPT_OR_NOTHROW
      : allocator_type(other)
   {}

   //!Assignment from other small_vector_allocator.
   //!Never throws
   BOOST_CONTAINER_FORCEINLINE small_vector_allocator &
      operator=(BOOST_COPY_ASSIGN_REF(small_vector_allocator) other) BOOST_NOEXCEPT_OR_NOTHROW
   {  return static_cast<small_vector_allocator&>(this->allocator_type::operator=(other.as_base()));  }

   //!Move assignment from other small_vector_allocator.
   //!Never throws
   BOOST_CONTAINER_FORCEINLINE small_vector_allocator &
      operator=(BOOST_RV_REF(small_vector_allocator) other) BOOST_NOEXCEPT_OR_NOTHROW
   {  return static_cast<small_vector_allocator&>(this->allocator_type::operator=(::boost::move(other.as_base())));  }

   //!Assignment from related small_vector_allocator.
   //!Never throws
   template<class U, class OtherVoidAllocator>
   BOOST_CONTAINER_FORCEINLINE small_vector_allocator &
      operator=(BOOST_COPY_ASSIGN_REF(small_vector_allocator<U BOOST_MOVE_I OtherVoidAllocator BOOST_MOVE_I Options>) other) BOOST_NOEXCEPT_OR_NOTHROW
   {  return static_cast<small_vector_allocator&>(this->allocator_type::operator=(other.as_base()));  }

   //!Move assignment from related small_vector_allocator.
   //!Never throws
   template<class U, class OtherVoidAllocator>
   BOOST_CONTAINER_FORCEINLINE small_vector_allocator &
      operator=(BOOST_RV_REF(small_vector_allocator<U BOOST_MOVE_I OtherVoidAllocator BOOST_MOVE_I Options>) other) BOOST_NOEXCEPT_OR_NOTHROW
   {  return static_cast<small_vector_allocator&>(this->allocator_type::operator=(::boost::move(other.as_base())));  }

   //!Move assignment from allocator_type.
   //!Never throws
   BOOST_CONTAINER_FORCEINLINE small_vector_allocator &
      operator=(const allocator_type &other) BOOST_NOEXCEPT_OR_NOTHROW
   {  return static_cast<small_vector_allocator&>(this->allocator_type::operator=(other));  }

   //!Allocates storage from the standard-conforming allocator
   BOOST_CONTAINER_FORCEINLINE pointer allocate(size_type count, const_void_pointer hint = const_void_pointer())
   {  return allocator_traits_type::allocate(this->as_base(), count, hint);  }

   //!Deallocates previously allocated memory.
   //!Never throws
   void deallocate(pointer ptr, size_type n) BOOST_NOEXCEPT_OR_NOTHROW
   {
      if(!this->is_internal_storage(ptr))
         allocator_traits_type::deallocate(this->as_base(), ptr, n);
   }

   //!Returns the maximum number of elements that could be allocated.
   //!Never throws
   BOOST_CONTAINER_FORCEINLINE size_type max_size() const BOOST_NOEXCEPT_OR_NOTHROW
   {  return allocator_traits_type::max_size(this->as_base());   }

   small_vector_allocator select_on_container_copy_construction() const
   {  return small_vector_allocator(allocator_traits_type::select_on_container_copy_construction(this->as_base())); }

   bool storage_is_unpropagable(pointer p) const
   {  return this->is_internal_storage(p) || allocator_traits_type::storage_is_unpropagable(this->as_base(), p);  }

   //!Swaps two allocators, does nothing
   //!because this small_vector_allocator is stateless
   BOOST_CONTAINER_FORCEINLINE friend void swap(small_vector_allocator &l, small_vector_allocator &r) BOOST_NOEXCEPT_OR_NOTHROW
   {  boost::adl_move_swap(l.as_base(), r.as_base());  }

   //!An small_vector_allocator always compares to true, as memory allocated with one
   //!instance can be deallocated by another instance (except for unpropagable storage)
   BOOST_CONTAINER_FORCEINLINE friend bool operator==(const small_vector_allocator &l, const small_vector_allocator &r) BOOST_NOEXCEPT_OR_NOTHROW
   {  return allocator_traits_type::equal(l.as_base(), r.as_base());  }

   //!An small_vector_allocator always compares to false, as memory allocated with one
   //!instance can be deallocated by another instance
   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(const small_vector_allocator &l, const small_vector_allocator &r) BOOST_NOEXCEPT_OR_NOTHROW
   {  return !(l == r);   }

   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   /*
   //!An advanced function that offers in-place expansion shrink to fit and new allocation
   //!capabilities. Memory allocated with this function can only be deallocated with deallocate()
   //!or deallocate_many().
   //!This function is available only with Version == 2
   pointer allocation_command(allocation_type command,
                         size_type limit_size,
                         size_type &prefer_in_recvd_out_size,
                         pointer &reuse)
   {  return allocator_traits_type::allocation_command(command, limit_size, prefer_in_recvd_out_size, reuse);  }

   //!Returns maximum the number of objects the previously allocated memory
   //!pointed by p can hold.
   //!Memory must not have been allocated with
   //!allocate_one or allocate_individual.
   //!This function is available only with Version == 2
   size_type size(pointer p) const BOOST_NOEXCEPT_OR_NOTHROW
   {  return allocator_traits_type::size(p);  }
   */
   private:
   /*
   //!Allocates just one object. Memory allocated with this function
   //!must be deallocated only with deallocate_one().
   //!Throws bad_alloc if there is no enough memory
   //!This function is available only with Version == 2
   using allocator_type::allocate_one;
   using allocator_type::allocate_individual;
   using allocator_type::deallocate_one;
   using allocator_type::deallocate_individual;
   using allocator_type::allocate_many;
   using allocator_type::deallocate_many;*/

   typedef vector_alloc_holder< small_vector_allocator, size_type >  vector_alloc_holder_t;
   typedef typename dtl::vector_for_small_vector<T, allocator_type, Options>::type vector_base;
   typedef small_vector_base<value_type, allocator_type, Options>    derived_type;

   BOOST_CONTAINER_FORCEINLINE bool is_internal_storage(const_pointer p) const
   {  return this->internal_storage() == p;  }

   BOOST_CONTAINER_FORCEINLINE
   const_pointer internal_storage() const
   {
      const vector_alloc_holder_t &v_holder = static_cast<const vector_alloc_holder_t &>(*this);
      const vector_base &v_base = reinterpret_cast<const vector_base &>(v_holder);
      const derived_type &d_base = static_cast<const derived_type &>(v_base);
      return d_base.internal_storage();
   }

   BOOST_CONTAINER_FORCEINLINE
   pointer internal_storage()
   {
      vector_alloc_holder_t &v_holder = static_cast<vector_alloc_holder_t &>(*this);
      vector_base &v_base = reinterpret_cast<vector_base &>(v_holder);
      derived_type &d_base = static_cast<derived_type &>(v_base);
      return d_base.internal_storage();
   }
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
};

//! This class consists of common code from all small_vector<T, N> types that don't depend on the
//! "N" template parameter. This class is non-copyable and non-destructible, so this class typically
//! used as reference argument to functions that read or write small vectors. Since `small_vector<T, N>`
//! derives from `small_vector_base<T>`, the conversion to `small_vector_base` is implicit
//! <pre>
//!
//! //Clients can pass any small_vector<Foo, N>.
//! void read_any_small_vector_of_foo(const small_vector_base<Foo> &in_parameter);
//!
//! void modify_any_small_vector_of_foo(small_vector_base<Foo> &in_out_parameter);
//!
//! void some_function()
//! {
//! 
//!    small_vector<Foo, 8> myvector;
//!
//!    read_any_small_vector_of_foo(myvector);   // Reads myvector
//!
//!    modify_any_small_vector_of_foo(myvector); // Modifies myvector
//! 
//! }
//! </pre>
//!
//! All `boost::container:vector` member functions are inherited. See `vector` documentation for details.
//!
template <class T, class SecondaryAllocator, class Options>
class small_vector_base
   : public dtl::vector_for_small_vector<T, SecondaryAllocator, Options>::type
{
   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKEDVECTOR
   public:
   //Make it public as it will be inherited by small_vector and container
   //must have this public member
   typedef typename real_allocator<T, SecondaryAllocator>::type                  secondary_allocator_t;
   typedef typename allocator_traits<secondary_allocator_t>::
      template portable_rebind_alloc<void>::type                                 void_allocator_t;
   typedef typename dtl::get_small_vector_opt<Options>::type                     options_t;
   typedef typename dtl::vector_for_small_vector
      <T, SecondaryAllocator, Options>::type                                     base_type;
   typedef typename allocator_traits<secondary_allocator_t>::pointer             pointer;
   typedef typename allocator_traits<secondary_allocator_t>::const_pointer       const_pointer;
   typedef typename allocator_traits<secondary_allocator_t>::void_pointer        void_pointer;
   typedef typename allocator_traits<secondary_allocator_t>::const_void_pointer  const_void_pointer;
   typedef small_vector_allocator<T, void_allocator_t, Options>                  allocator_type;

   private: 
   BOOST_COPYABLE_AND_MOVABLE(small_vector_base)

   friend class small_vector_allocator<T, void_allocator_t, Options>;

   BOOST_CONTAINER_FORCEINLINE
   const_pointer internal_storage() const BOOST_NOEXCEPT_OR_NOTHROW
   {
      typedef typename boost::intrusive::pointer_traits<const_pointer>::template
         rebind_pointer<const unsigned char>::type const_char_pointer;
      const_void_pointer void_p = boost::intrusive::pointer_traits<const_char_pointer>::
         pointer_to(*m_storage_start.data);
      return boost::intrusive::pointer_traits<const_pointer>::static_cast_from(void_p);
   }

   BOOST_CONTAINER_FORCEINLINE
   pointer internal_storage() BOOST_NOEXCEPT_OR_NOTHROW
   {
      typedef typename boost::intrusive::pointer_traits<pointer>::template
         rebind_pointer<unsigned char>::type char_pointer;
      void_pointer void_p = boost::intrusive::pointer_traits<char_pointer>::
         pointer_to(*m_storage_start.data);
      return boost::intrusive::pointer_traits<pointer>::static_cast_from(void_p);
   }

         base_type &as_base()       { return static_cast<base_type&>(*this); }
   const base_type &as_base() const { return static_cast<const base_type&>(*this); }

   static const std::size_t final_alignment =
      options_t::inplace_alignment ? options_t::inplace_alignment : dtl::alignment_of<T>::value;
   public:

   typedef typename dtl::aligned_storage
      <sizeof(T), final_alignment>::type storage_type;

   protected:

   BOOST_CONTAINER_FORCEINLINE explicit small_vector_base(initial_capacity_t, std::size_t initial_capacity)
      : base_type(initial_capacity_t(), this->internal_storage(), initial_capacity)
   {}

   template<class AllocFwd>
   BOOST_CONTAINER_FORCEINLINE explicit small_vector_base(initial_capacity_t, std::size_t capacity, BOOST_FWD_REF(AllocFwd) a)
      : base_type(initial_capacity_t(), this->internal_storage(), capacity, ::boost::forward<AllocFwd>(a))
   {}

   //~small_vector_base(){}

   private:
   //The only member
   storage_type m_storage_start;

   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   public:
   BOOST_CONTAINER_FORCEINLINE small_vector_base& operator=(BOOST_COPY_ASSIGN_REF(small_vector_base) other)
   {  return static_cast<small_vector_base&>(this->base_type::operator=(static_cast<base_type const&>(other)));  }

   BOOST_CONTAINER_FORCEINLINE small_vector_base& operator=(BOOST_RV_REF(small_vector_base) other)
   {  return static_cast<small_vector_base&>(this->base_type::operator=(BOOST_MOVE_BASE(base_type, other))); }

   BOOST_CONTAINER_FORCEINLINE void swap(small_vector_base &other)
   {  return this->base_type::swap(other);  }

   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   protected:
   void move_construct_impl(base_type &x, const allocator_type &a)
   {
      if(base_type::is_propagable_from(x.get_stored_allocator(), x.data(), a, true)){
         this->steal_resources(x);
      }
      else{
         this->assign( boost::make_move_iterator(boost::movelib::iterator_to_raw_pointer(x.begin()))
                     , boost::make_move_iterator(boost::movelib::iterator_to_raw_pointer(x.end  ()))
                     );
      }
   }
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
};

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

/////////////////////////////////////////////////////
//
//          small_vector_storage_calculator
//
/////////////////////////////////////////////////////
template<std::size_t Needed, std::size_t Hdr, std::size_t SSize, bool NeedsZero = (0u == Needed || Needed <= Hdr)>
struct small_vector_storage_calculator_helper
{
   static const std::size_t value = (Needed - Hdr - 1u)/SSize + 1u;
};

template<std::size_t Needed, std::size_t Hdr, std::size_t SSize>
struct small_vector_storage_calculator_helper<Needed, Hdr, SSize, true>
{
   static const std::size_t value = 0u;
};

template<class Storage, class Allocator, class T, std::size_t N, class Options>
struct small_vector_storage_calculator
{
   typedef small_vector_base<T, Allocator, Options> svh_type;
   typedef typename real_allocator<T, Allocator>::type value_allocator_t;
   typedef typename allocator_traits<value_allocator_t>::template portable_rebind_alloc<void>::type void_allocator_t;
   typedef typename dtl::vector_for_small_vector<T, void_allocator_t, Options>::type  svhb_type;

   static const std::size_t s_align = dtl::alignment_of<Storage>::value;
   static const std::size_t s_size = sizeof(Storage);
   static const std::size_t svh_sizeof = sizeof(svh_type);
   static const std::size_t svhb_sizeof = sizeof(svhb_type);
   static const std::size_t s_start = ((svhb_sizeof-1)/s_align+1)*s_align;
   static const std::size_t header_bytes = svh_sizeof-s_start;
   static const std::size_t needed_bytes = sizeof(T)*N;
   static const std::size_t needed_extra_storages =
      small_vector_storage_calculator_helper<needed_bytes, header_bytes, s_size>::value;
};

/////////////////////////////////////////////////////
//
//          small_vector_storage_definer
//
/////////////////////////////////////////////////////
template<class Storage, std::size_t N>
struct small_vector_storage
{
   Storage m_rest_of_storage[N];
};

template<class Storage>
struct small_vector_storage<Storage, 0>
{};

template<class T, class Allocator, std::size_t N, class Options>
struct small_vector_storage_definer
{
   typedef T value_type;
   typedef typename small_vector_base<value_type, Allocator, Options>::storage_type storage_type;
   static const std::size_t needed_extra_storages =
      small_vector_storage_calculator<storage_type, Allocator, value_type, N, Options>::needed_extra_storages;
   typedef small_vector_storage<storage_type, needed_extra_storages> type;
};

#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

//! small_vector is a vector-like container optimized for the case when it contains few elements.
//! It contains some preallocated elements in-place, which can avoid the use of dynamic storage allocation
//! when the actual number of elements is below that preallocated threshold.
//!
//! `small_vector<T, N, Allocator, Options>` is convertible to `small_vector_base<T, Allocator, Options>` that is independent
//! from the preallocated element capacity, so client code does not need to be templated on that N argument.
//!
//! All `boost::container::vector` member functions are inherited. See `vector` documentation for details.
//!
//! \tparam T The type of object that is stored in the small_vector
//! \tparam N The number of preallocated elements stored inside small_vector. It shall be less than Allocator::max_size();
//! \tparam Allocator The allocator used for memory management when the number of elements exceeds N. Use void
//!   for the default allocator
//! |tparam Options A type produced from \c boost::container::small_vector_options.
template <class T, std::size_t N, class Allocator BOOST_CONTAINER_DOCONLY(= void), class Options BOOST_CONTAINER_DOCONLY(= void) >
class small_vector : public small_vector_base<T, Allocator, Options>
   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   , private small_vector_storage_definer<T, Allocator, N, Options>::type
   #endif
{
   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   typedef small_vector_base<T, Allocator, Options>   base_type;
   typedef typename small_vector_storage_definer
      <T, Allocator, N, Options>::type                remaining_storage_holder;

   BOOST_COPYABLE_AND_MOVABLE(small_vector)

   typedef allocator_traits<typename base_type::allocator_type> allocator_traits_type;

   public:
   typedef small_vector_storage_calculator
      < typename small_vector_base<T, Allocator, Options>::storage_type
      , Allocator, T, N, Options> storage_test;

   static const std::size_t needed_extra_storages =  storage_test::needed_extra_storages;
   static const std::size_t needed_bytes =  storage_test::needed_bytes;
   static const std::size_t header_bytes =  storage_test::header_bytes;
   static const std::size_t s_start =  storage_test::s_start;

   typedef typename base_type::allocator_type   allocator_type;
   typedef typename base_type::size_type        size_type;
   typedef typename base_type::value_type       value_type;

   BOOST_CONTAINER_FORCEINLINE static std::size_t internal_capacity()
   {  return (sizeof(small_vector) - storage_test::s_start)/sizeof(T);  }

   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   //! @brief The capacity/max size of the container
   static const size_type static_capacity = N;

   public:
   BOOST_CONTAINER_FORCEINLINE small_vector()
      BOOST_NOEXCEPT_IF(dtl::is_nothrow_default_constructible<allocator_type>::value)
      : base_type(initial_capacity_t(), internal_capacity())
   {}

   BOOST_CONTAINER_FORCEINLINE explicit small_vector(const allocator_type &a)
      : base_type(initial_capacity_t(), internal_capacity(), a)
   {}

   BOOST_CONTAINER_FORCEINLINE explicit small_vector(size_type n)
      : base_type(initial_capacity_t(), internal_capacity())
   {  this->resize(n); }

   BOOST_CONTAINER_FORCEINLINE small_vector(size_type n, const allocator_type &a)
      : base_type(initial_capacity_t(), internal_capacity(), a)
   {  this->resize(n); }

   BOOST_CONTAINER_FORCEINLINE small_vector(size_type n, default_init_t)
      : base_type(initial_capacity_t(), internal_capacity())
   {  this->resize(n, default_init_t()); }

   BOOST_CONTAINER_FORCEINLINE small_vector(size_type n, default_init_t, const allocator_type &a)
      : base_type(initial_capacity_t(), internal_capacity(), a)
   {  this->resize(n, default_init_t()); }

   BOOST_CONTAINER_FORCEINLINE small_vector(size_type n, const value_type &v)
      : base_type(initial_capacity_t(), internal_capacity())
   {  this->resize(n, v); }

   BOOST_CONTAINER_FORCEINLINE small_vector(size_type n, const value_type &v, const allocator_type &a)
      : base_type(initial_capacity_t(), internal_capacity(), a)
   {  this->resize(n, v); }

   template <class InIt>
   BOOST_CONTAINER_FORCEINLINE small_vector(InIt first, InIt last
      BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I typename dtl::disable_if_c
         < dtl::is_convertible<InIt BOOST_MOVE_I size_type>::value
         BOOST_MOVE_I dtl::nat >::type * = 0)
      )
      : base_type(initial_capacity_t(), internal_capacity())
   {  this->assign(first, last); }

   template <class InIt>
   BOOST_CONTAINER_FORCEINLINE small_vector(InIt first, InIt last, const allocator_type& a
      BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I typename dtl::disable_if_c
         < dtl::is_convertible<InIt BOOST_MOVE_I size_type>::value
         BOOST_MOVE_I dtl::nat >::type * = 0)
      )
      : base_type(initial_capacity_t(), internal_capacity(), a)
   {  this->assign(first, last); }

   BOOST_CONTAINER_FORCEINLINE small_vector(const small_vector &other)
      : base_type( initial_capacity_t(), internal_capacity()
                 , allocator_traits_type::select_on_container_copy_construction(other.get_stored_allocator()))
   {  this->assign(other.cbegin(), other.cend());  }

   BOOST_CONTAINER_FORCEINLINE small_vector(const small_vector &other, const allocator_type &a)
      : base_type(initial_capacity_t(), internal_capacity(), a)
   {  this->assign(other.cbegin(), other.cend());  }

   BOOST_CONTAINER_FORCEINLINE explicit small_vector(const base_type &other)
      : base_type( initial_capacity_t(), internal_capacity()
                 , allocator_traits_type::select_on_container_copy_construction(other.get_stored_allocator()))
   {  this->assign(other.cbegin(), other.cend());  }

   BOOST_CONTAINER_FORCEINLINE explicit small_vector(BOOST_RV_REF(base_type) other)
      : base_type(initial_capacity_t(), internal_capacity(), ::boost::move(other.get_stored_allocator()))
   {  this->move_construct_impl(other, other.get_stored_allocator());   }

   BOOST_CONTAINER_FORCEINLINE small_vector(BOOST_RV_REF(small_vector) other)
      BOOST_NOEXCEPT_IF(boost::container::dtl::is_nothrow_move_assignable<value_type>::value)
      : base_type(initial_capacity_t(), internal_capacity(), ::boost::move(other.get_stored_allocator()))
   {  this->move_construct_impl(other, other.get_stored_allocator());   }

   BOOST_CONTAINER_FORCEINLINE small_vector(BOOST_RV_REF(small_vector) other, const allocator_type &a)
      : base_type(initial_capacity_t(), internal_capacity(), a)
   {  this->move_construct_impl(other, a);   }

   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   BOOST_CONTAINER_FORCEINLINE small_vector(std::initializer_list<value_type> il, const allocator_type& a = allocator_type())
      : base_type(initial_capacity_t(), internal_capacity(), a)
   {
      this->assign(il.begin(), il.end());
   }
   #endif

   BOOST_CONTAINER_FORCEINLINE small_vector& operator=(BOOST_COPY_ASSIGN_REF(small_vector) other)
   {  return static_cast<small_vector&>(this->base_type::operator=(static_cast<base_type const&>(other)));  }

   BOOST_CONTAINER_FORCEINLINE small_vector& operator=(BOOST_RV_REF(small_vector) other)
      BOOST_NOEXCEPT_IF(boost::container::dtl::is_nothrow_move_assignable<value_type>::value
         && (allocator_traits_type::propagate_on_container_move_assignment::value
             || allocator_traits_type::is_always_equal::value))
   {  return static_cast<small_vector&>(this->base_type::operator=(BOOST_MOVE_BASE(base_type, other))); }

   BOOST_CONTAINER_FORCEINLINE small_vector& operator=(const base_type &other)
   {  return static_cast<small_vector&>(this->base_type::operator=(other));  }

   BOOST_CONTAINER_FORCEINLINE small_vector& operator=(BOOST_RV_REF(base_type) other)
   {  return static_cast<small_vector&>(this->base_type::operator=(boost::move(other))); }

   BOOST_CONTAINER_FORCEINLINE void swap(small_vector &other)
   {  return this->base_type::swap(other);  }
};

}}

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
/*
namespace boost {

//!has_trivial_destructor_after_move<> == true_type
//!specialization for optimizations
template <class T, class Allocator>
struct has_trivial_destructor_after_move<boost::container::vector<T, Allocator> >
{
   typedef typename ::boost::container::allocator_traits<Allocator>::pointer pointer;
   static const bool value = ::boost::has_trivial_destructor_after_move<Allocator>::value &&
                             ::boost::has_trivial_destructor_after_move<pointer>::value;
};

}
*/
#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

#include <boost/container/detail/config_end.hpp>

#endif //   #ifndef  BOOST_CONTAINER_CONTAINER_SMALL_VECTOR_HPP
