//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_DETAIL_COPY_MOVE_ALGO_HPP
#define BOOST_CONTAINER_DETAIL_COPY_MOVE_ALGO_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

// container
#include <boost/container/allocator_traits.hpp>
// container/detail
#include <boost/container/detail/iterator.hpp>
#include <boost/move/detail/iterator_to_raw_pointer.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/type_traits.hpp>
#include <boost/container/detail/construct_in_place.hpp>

// move
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/iterator.hpp>
#include <boost/move/utility_core.hpp>
// other
#include <boost/core/no_exceptions_support.hpp>
// std
#include <cstring> //for memmove/memcpy

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
//pair memcpy optimizations rightfully detected by GCC
#  if defined(BOOST_GCC) && (BOOST_GCC >= 80000)
#     pragma GCC diagnostic ignored "-Wclass-memaccess"
#  endif
//GCC 8 seems a bit confused about array access error with static_vector
//when out of bound exceptions are being thrown.
#  if defined(BOOST_GCC) && (BOOST_GCC >= 80000) && (BOOST_GCC < 80200)
#     pragma GCC diagnostic ignored "-Wstringop-overflow"
#  endif
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif

namespace boost {
namespace container {
namespace dtl {

template<class I>
struct are_elements_contiguous
{
   static const bool value = false;
};

/////////////////////////
//    raw pointers
/////////////////////////

template<class T>
struct are_elements_contiguous<T*>
{
   static const bool value = true;
};

/////////////////////////
//    move iterators
/////////////////////////

template<class It>
struct are_elements_contiguous< ::boost::move_iterator<It> >
   : are_elements_contiguous<It>
{};

}  //namespace dtl {

/////////////////////////
//    predeclarations
/////////////////////////

template <class Pointer, bool IsConst>
class vec_iterator;

}  //namespace container {

namespace interprocess {

template <class PointedType, class DifferenceType, class OffsetType, std::size_t OffsetAlignment>
class offset_ptr;

}  //namespace interprocess {

namespace container {

namespace dtl {

/////////////////////////
//vector_[const_]iterator
/////////////////////////

template <class Pointer, bool IsConst>
struct are_elements_contiguous<boost::container::vec_iterator<Pointer, IsConst> >
{
   static const bool value = true;
};

/////////////////////////
//    offset_ptr
/////////////////////////

template <class PointedType, class DifferenceType, class OffsetType, std::size_t OffsetAlignment>
struct are_elements_contiguous< ::boost::interprocess::offset_ptr<PointedType, DifferenceType, OffsetType, OffsetAlignment> >
{
   static const bool value = true;
};

template <typename I, typename O>
struct are_contiguous_and_same
   : boost::move_detail::and_
      < are_elements_contiguous<I>
      , are_elements_contiguous<O>
      , is_same< typename remove_const< typename ::boost::container::iterator_traits<I>::value_type >::type
               , typename ::boost::container::iterator_traits<O>::value_type
               >
      >
{};

template <typename I, typename O>
struct is_memtransfer_copy_assignable
   : boost::move_detail::and_
      < are_contiguous_and_same<I, O>
      , dtl::is_trivially_copy_assignable< typename ::boost::container::iterator_traits<I>::value_type >
      >
{};

template <typename I, typename O>
struct is_memtransfer_copy_constructible
   : boost::move_detail::and_
      < are_contiguous_and_same<I, O>
      , dtl::is_trivially_copy_constructible< typename ::boost::container::iterator_traits<I>::value_type >
      >
{};

template <typename I, typename O, typename R>
struct enable_if_memtransfer_copy_constructible
   : enable_if<dtl::is_memtransfer_copy_constructible<I, O>, R>
{};

template <typename I, typename O, typename R>
struct disable_if_memtransfer_copy_constructible
   : disable_if<dtl::is_memtransfer_copy_constructible<I, O>, R>
{};

template <typename I, typename O, typename R>
struct enable_if_memtransfer_copy_assignable
   : enable_if<dtl::is_memtransfer_copy_assignable<I, O>, R>
{};

template <typename I, typename O, typename R>
struct disable_if_memtransfer_copy_assignable
   : disable_if<dtl::is_memtransfer_copy_assignable<I, O>, R>
{};

template
   <typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline F memmove(I f, I l, F r) BOOST_NOEXCEPT_OR_NOTHROW
{
   typedef typename boost::container::iterator_traits<I>::value_type value_type;
   value_type *const dest_raw = boost::movelib::iterator_to_raw_pointer(r);
   const value_type *const beg_raw = boost::movelib::iterator_to_raw_pointer(f);
   const value_type *const end_raw = boost::movelib::iterator_to_raw_pointer(l);
   if(BOOST_LIKELY(beg_raw != end_raw && dest_raw && beg_raw)){
      const typename boost::container::iterator_traits<I>::difference_type n = end_raw - beg_raw;
      std::memmove(dest_raw, beg_raw, sizeof(value_type)*n);
      boost::container::iterator_advance(r, n);
   }
   return r;
}

template
   <typename I, // I models InputIterator
    typename U, // U models unsigned integral constant
    typename F> // F models ForwardIterator
F memmove_n(I f, U n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{
   typedef typename boost::container::iterator_traits<I>::value_type value_type;
   if(BOOST_LIKELY(n)){
      std::memmove(boost::movelib::iterator_to_raw_pointer(r), boost::movelib::iterator_to_raw_pointer(f), sizeof(value_type)*n);
      boost::container::iterator_advance(r, n);
   }

   return r;
}

template
   <typename I, // I models InputIterator
    typename U, // U models unsigned integral constant
    typename F> // F models ForwardIterator
I memmove_n_source(I f, U n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{
   if(BOOST_LIKELY(n)){
      typedef typename boost::container::iterator_traits<I>::value_type value_type;
      std::memmove(boost::movelib::iterator_to_raw_pointer(r), boost::movelib::iterator_to_raw_pointer(f), sizeof(value_type)*n);
      boost::container::iterator_advance(f, n);
   }
   return f;
}

template
   <typename I, // I models InputIterator
    typename U, // U models unsigned integral constant
    typename F> // F models ForwardIterator
I memmove_n_source_dest(I f, U n, F &r) BOOST_NOEXCEPT_OR_NOTHROW
{
   typedef typename boost::container::iterator_traits<I>::value_type value_type;
   if(BOOST_LIKELY(n)){
      std::memmove(boost::movelib::iterator_to_raw_pointer(r), boost::movelib::iterator_to_raw_pointer(f), sizeof(value_type)*n);
      boost::container::iterator_advance(f, n);
      boost::container::iterator_advance(r, n);
   }
   return f;
}

template <typename O>
struct is_memzero_initializable
{
   typedef typename ::boost::container::iterator_traits<O>::value_type value_type;
   static const bool value = are_elements_contiguous<O>::value &&
      (  dtl::is_integral<value_type>::value || dtl::is_enum<value_type>::value
      #if defined(BOOST_CONTAINER_MEMZEROED_POINTER_IS_NULL)
      || dtl::is_pointer<value_type>::value
      #endif
      #if defined(BOOST_CONTAINER_MEMZEROED_FLOATING_POINT_IS_ZERO)
      || dtl::is_floating_point<value_type>::value
      #endif
      #if defined(BOOST_CONTAINER_MEMZEROED_FLOATING_POINT_IS_ZERO) && defined(BOOST_CONTAINER_MEMZEROED_POINTER_IS_NULL)
      || dtl::is_pod<value_type>::value
      #endif
      );
};

template <typename O, typename R>
struct enable_if_memzero_initializable
   : enable_if_c<dtl::is_memzero_initializable<O>::value, R>
{};

template <typename O, typename R>
struct disable_if_memzero_initializable
   : enable_if_c<!dtl::is_memzero_initializable<O>::value, R>
{};

template <typename I, typename R>
struct enable_if_trivially_destructible
   : enable_if_c < dtl::is_trivially_destructible
                  <typename boost::container::iterator_traits<I>::value_type>::value
               , R>
{};

template <typename I, typename R>
struct disable_if_trivially_destructible
   : enable_if_c <!dtl::is_trivially_destructible
                  <typename boost::container::iterator_traits<I>::value_type>::value
               , R>
{};

}  //namespace dtl {

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_move_alloc
//
//////////////////////////////////////////////////////////////////////////////


//! <b>Effects</b>:
//!   \code
//!   for (; f != l; ++r, ++f)
//!      allocator_traits::construct(a, &*r, boost::move(*f));
//!   \endcode
//!
//! <b>Returns</b>: r
template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_constructible<I, F, F>::type
   uninitialized_move_alloc(Allocator &a, I f, I l, F r)
{
   F back = r;
   BOOST_TRY{
      while (f != l) {
         allocator_traits<Allocator>::construct(a, boost::movelib::iterator_to_raw_pointer(r), boost::move(*f));
         ++f; ++r;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return r;
}

template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_constructible<I, F, F>::type
   uninitialized_move_alloc(Allocator &, I f, I l, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove(f, l, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_move_alloc_n
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; n--; ++r, ++f)
//!      allocator_traits::construct(a, &*r, boost::move(*f));
//!   \endcode
//!
//! <b>Returns</b>: r
template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_constructible<I, F, F>::type
   uninitialized_move_alloc_n(Allocator &a, I f, typename boost::container::allocator_traits<Allocator>::size_type n, F r)
{
   F back = r;
   BOOST_TRY{
      while (n--) {
         allocator_traits<Allocator>::construct(a, boost::movelib::iterator_to_raw_pointer(r), boost::move(*f));
         ++f; ++r;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return r;
}

template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_constructible<I, F, F>::type
   uninitialized_move_alloc_n(Allocator &, I f, typename boost::container::allocator_traits<Allocator>::size_type n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n(f, n, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_move_alloc_n_source
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; n--; ++r, ++f)
//!      allocator_traits::construct(a, &*r, boost::move(*f));
//!   \endcode
//!
//! <b>Returns</b>: f (after incremented)
template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_constructible<I, F, I>::type
   uninitialized_move_alloc_n_source(Allocator &a, I f, typename boost::container::allocator_traits<Allocator>::size_type n, F r)
{
   F back = r;
   BOOST_TRY{
      while (n--) {
         allocator_traits<Allocator>::construct(a, boost::movelib::iterator_to_raw_pointer(r), boost::move(*f));
         ++f; ++r;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return f;
}

template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_constructible<I, F, I>::type
   uninitialized_move_alloc_n_source(Allocator &, I f, typename boost::container::allocator_traits<Allocator>::size_type n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n_source(f, n, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_copy_alloc
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; f != l; ++r, ++f)
//!      allocator_traits::construct(a, &*r, *f);
//!   \endcode
//!
//! <b>Returns</b>: r
template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_constructible<I, F, F>::type
   uninitialized_copy_alloc(Allocator &a, I f, I l, F r)
{
   F back = r;
   BOOST_TRY{
      while (f != l) {
         allocator_traits<Allocator>::construct(a, boost::movelib::iterator_to_raw_pointer(r), *f);
         ++f; ++r;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return r;
}

template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_constructible<I, F, F>::type
   uninitialized_copy_alloc(Allocator &, I f, I l, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove(f, l, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_copy_alloc_n
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; n--; ++r, ++f)
//!      allocator_traits::construct(a, &*r, *f);
//!   \endcode
//!
//! <b>Returns</b>: r
template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_constructible<I, F, F>::type
   uninitialized_copy_alloc_n(Allocator &a, I f, typename boost::container::allocator_traits<Allocator>::size_type n, F r)
{
   F back = r;
   BOOST_TRY{
      while (n--) {
         allocator_traits<Allocator>::construct(a, boost::movelib::iterator_to_raw_pointer(r), *f);
         ++f; ++r;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return r;
}

template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_constructible<I, F, F>::type
   uninitialized_copy_alloc_n(Allocator &, I f, typename boost::container::allocator_traits<Allocator>::size_type n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n(f, n, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_copy_alloc_n_source
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; n--; ++r, ++f)
//!      allocator_traits::construct(a, &*r, *f);
//!   \endcode
//!
//! <b>Returns</b>: f (after incremented)
template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_constructible<I, F, I>::type
   uninitialized_copy_alloc_n_source(Allocator &a, I f, typename boost::container::allocator_traits<Allocator>::size_type n, F r)
{
   F back = r;
   BOOST_TRY{
      while (n) {
         boost::container::construct_in_place(a, boost::movelib::iterator_to_raw_pointer(r), f);
         ++f; ++r; --n;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return f;
}

template
   <typename Allocator,
    typename I, // I models InputIterator
    typename F> // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_constructible<I, F, I>::type
   uninitialized_copy_alloc_n_source(Allocator &, I f, typename boost::container::allocator_traits<Allocator>::size_type n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n_source(f, n, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_value_init_alloc_n
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; n--; ++r, ++f)
//!      allocator_traits::construct(a, &*r);
//!   \endcode
//!
//! <b>Returns</b>: r
template
   <typename Allocator,
    typename F> // F models ForwardIterator
inline typename dtl::disable_if_memzero_initializable<F, F>::type
   uninitialized_value_init_alloc_n(Allocator &a, typename boost::container::allocator_traits<Allocator>::size_type n, F r)
{
   F back = r;
   BOOST_TRY{
      while (n--) {
         allocator_traits<Allocator>::construct(a, boost::movelib::iterator_to_raw_pointer(r));
         ++r;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return r;
}

template
   <typename Allocator,
    typename F> // F models ForwardIterator
inline typename dtl::enable_if_memzero_initializable<F, F>::type
   uninitialized_value_init_alloc_n(Allocator &, typename boost::container::allocator_traits<Allocator>::size_type n, F r)
{
   typedef typename boost::container::iterator_traits<F>::value_type value_type;
   std::memset((void*)boost::movelib::iterator_to_raw_pointer(r), 0, sizeof(value_type)*n);
   boost::container::iterator_advance(r, n);
   return r;
}

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_default_init_alloc_n
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; n--; ++r, ++f)
//!      allocator_traits::construct(a, &*r);
//!   \endcode
//!
//! <b>Returns</b>: r
template
   <typename Allocator,
    typename F> // F models ForwardIterator
inline F uninitialized_default_init_alloc_n(Allocator &a, typename boost::container::allocator_traits<Allocator>::size_type n, F r)
{
   F back = r;
   BOOST_TRY{
      while (n--) {
         allocator_traits<Allocator>::construct(a, boost::movelib::iterator_to_raw_pointer(r), default_init);
         ++r;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return r;
}

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_fill_alloc
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; f != l; ++r, ++f)
//!      allocator_traits::construct(a, &*r, *f);
//!   \endcode
//!
//! <b>Returns</b>: r
template
   <typename Allocator,
    typename F, // F models ForwardIterator
    typename T>
inline void uninitialized_fill_alloc(Allocator &a, F f, F l, const T &t)
{
   F back = f;
   BOOST_TRY{
      while (f != l) {
         allocator_traits<Allocator>::construct(a, boost::movelib::iterator_to_raw_pointer(f), t);
         ++f;
      }
   }
   BOOST_CATCH(...){
      for (; back != l; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
}


//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_fill_alloc_n
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; n--; ++r, ++f)
//!      allocator_traits::construct(a, &*r, v);
//!   \endcode
//!
//! <b>Returns</b>: r
template
   <typename Allocator,
    typename T,
    typename F> // F models ForwardIterator
inline F uninitialized_fill_alloc_n(Allocator &a, const T &v, typename boost::container::allocator_traits<Allocator>::size_type n, F r)
{
   F back = r;
   BOOST_TRY{
      while (n--) {
         allocator_traits<Allocator>::construct(a, boost::movelib::iterator_to_raw_pointer(r), v);
         ++r;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(back));
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return r;
}

//////////////////////////////////////////////////////////////////////////////
//
//                               copy
//
//////////////////////////////////////////////////////////////////////////////

template
<typename I,   // I models InputIterator
typename F>    // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_assignable<I, F, F>::type
   copy(I f, I l, F r)
{
   while (f != l) {
      *r = *f;
      ++f; ++r;
   }
   return r;
}

template
<typename I,   // I models InputIterator
typename F>    // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_assignable<I, F, F>::type
   copy(I f, I l, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove(f, l, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                               copy_n
//
//////////////////////////////////////////////////////////////////////////////

template
<typename I,   // I models InputIterator
typename U,   // U models unsigned integral constant
typename F>   // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_assignable<I, F, F>::type
   copy_n(I f, U n, F r)
{
   while (n) {
      --n;
      *r = *f;
      ++f; ++r;
   }
   return r;
}

template
<typename I,   // I models InputIterator
typename U,   // U models unsigned integral constant
typename F>   // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_assignable<I, F, F>::type
   copy_n(I f, U n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n(f, n, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                            copy_n_source
//
//////////////////////////////////////////////////////////////////////////////

template
<typename I,   // I models InputIterator
typename U,   // U models unsigned integral constant
typename F>   // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_assignable<I, F, I>::type
   copy_n_source(I f, U n, F r)
{
   while (n--) {
      boost::container::assign_in_place(r, f);
      ++f; ++r;
   }
   return f;
}

template
<typename I,   // I models InputIterator
typename U,   // U models unsigned integral constant
typename F>   // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_assignable<I, F, I>::type
   copy_n_source(I f, U n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n_source(f, n, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                            copy_n_source_dest
//
//////////////////////////////////////////////////////////////////////////////

template
<typename I,   // I models InputIterator
typename U,   // U models unsigned integral constant
typename F>   // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_assignable<I, F, I>::type
   copy_n_source_dest(I f, U n, F &r)
{
   while (n--) {
      *r = *f;
      ++f; ++r;
   }
   return f;
}

template
<typename I,   // I models InputIterator
typename U,   // U models unsigned integral constant
typename F>   // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_assignable<I, F, I>::type
   copy_n_source_dest(I f, U n, F &r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n_source_dest(f, n, r);  }

//////////////////////////////////////////////////////////////////////////////
//
//                         move
//
//////////////////////////////////////////////////////////////////////////////

template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_assignable<I, F, F>::type
   move(I f, I l, F r)
{
   while (f != l) {
      *r = ::boost::move(*f);
      ++f; ++r;
   }
   return r;
}

template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_assignable<I, F, F>::type
   move(I f, I l, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove(f, l, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                         move_n
//
//////////////////////////////////////////////////////////////////////////////

template
<typename I,   // I models InputIterator
typename U,   // U models unsigned integral constant
typename F>   // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_assignable<I, F, F>::type
   move_n(I f, U n, F r)
{
   while (n--) {
      *r = ::boost::move(*f);
      ++f; ++r;
   }
   return r;
}

template
<typename I,   // I models InputIterator
typename U,   // U models unsigned integral constant
typename F>   // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_assignable<I, F, F>::type
   move_n(I f, U n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n(f, n, r); }


//////////////////////////////////////////////////////////////////////////////
//
//                         move_backward
//
//////////////////////////////////////////////////////////////////////////////

template
<typename I,   // I models BidirectionalIterator
typename F>    // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_assignable<I, F, F>::type
   move_backward(I f, I l, F r)
{
   while (f != l) {
      --l; --r;
      *r = ::boost::move(*l);
   }
   return r;
}

template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_assignable<I, F, F>::type
   move_backward(I f, I l, F r) BOOST_NOEXCEPT_OR_NOTHROW
{
   typedef typename boost::container::iterator_traits<I>::value_type value_type;
   const typename boost::container::iterator_traits<I>::difference_type n = boost::container::iterator_distance(f, l);
   r -= n;
   std::memmove((boost::movelib::iterator_to_raw_pointer)(r), (boost::movelib::iterator_to_raw_pointer)(f), sizeof(value_type)*n);
   return r;
}

//////////////////////////////////////////////////////////////////////////////
//
//                         move_n_source_dest
//
//////////////////////////////////////////////////////////////////////////////

template
<typename I    // I models InputIterator
,typename U    // U models unsigned integral constant
,typename F>   // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_assignable<I, F, I>::type
   move_n_source_dest(I f, U n, F &r)
{
   while (n--) {
      *r = ::boost::move(*f);
      ++f; ++r;
   }
   return f;
}

template
<typename I    // I models InputIterator
,typename U    // U models unsigned integral constant
,typename F>   // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_assignable<I, F, I>::type
   move_n_source_dest(I f, U n, F &r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n_source_dest(f, n, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                         move_n_source
//
//////////////////////////////////////////////////////////////////////////////

template
<typename I    // I models InputIterator
,typename U    // U models unsigned integral constant
,typename F>   // F models ForwardIterator
inline typename dtl::disable_if_memtransfer_copy_assignable<I, F, I>::type
   move_n_source(I f, U n, F r)
{
   while (n--) {
      *r = ::boost::move(*f);
      ++f; ++r;
   }
   return f;
}

template
<typename I    // I models InputIterator
,typename U    // U models unsigned integral constant
,typename F>   // F models ForwardIterator
inline typename dtl::enable_if_memtransfer_copy_assignable<I, F, I>::type
   move_n_source(I f, U n, F r) BOOST_NOEXCEPT_OR_NOTHROW
{  return dtl::memmove_n_source(f, n, r); }

//////////////////////////////////////////////////////////////////////////////
//
//                               destroy_alloc_n
//
//////////////////////////////////////////////////////////////////////////////

template
   <typename Allocator
   ,typename I   // I models InputIterator
   ,typename U>  // U models unsigned integral constant
inline typename dtl::disable_if_trivially_destructible<I, void>::type
   destroy_alloc_n(Allocator &a, I f, U n)
{
   while(n){
      --n;
      allocator_traits<Allocator>::destroy(a, boost::movelib::iterator_to_raw_pointer(f));
      ++f;
   }
}

template
   <typename Allocator
   ,typename I   // I models InputIterator
   ,typename U>  // U models unsigned integral constant
inline typename dtl::enable_if_trivially_destructible<I, void>::type
   destroy_alloc_n(Allocator &, I, U)
{}

//////////////////////////////////////////////////////////////////////////////
//
//                         deep_swap_alloc_n
//
//////////////////////////////////////////////////////////////////////////////

template
   <std::size_t MaxTmpBytes
   ,typename Allocator
   ,typename F // F models ForwardIterator
   ,typename G // G models ForwardIterator
   >
inline typename dtl::disable_if_memtransfer_copy_assignable<F, G, void>::type
   deep_swap_alloc_n( Allocator &a, F short_range_f, typename allocator_traits<Allocator>::size_type n_i
                    , G large_range_f, typename allocator_traits<Allocator>::size_type n_j)
{
   typename allocator_traits<Allocator>::size_type n = 0;
   for (; n != n_i ; ++short_range_f, ++large_range_f, ++n){
      boost::adl_move_swap(*short_range_f, *large_range_f);
   }
   boost::container::uninitialized_move_alloc_n(a, large_range_f, n_j - n_i, short_range_f);  // may throw
   boost::container::destroy_alloc_n(a, large_range_f, n_j - n_i);
}

static const std::size_t DeepSwapAllocNMaxStorage = std::size_t(1) << std::size_t(11); //2K bytes

template
   <std::size_t MaxTmpBytes
   ,typename Allocator
   ,typename F // F models ForwardIterator
   ,typename G // G models ForwardIterator
   >
inline typename dtl::enable_if_c
   < dtl::is_memtransfer_copy_assignable<F, G>::value && (MaxTmpBytes <= DeepSwapAllocNMaxStorage) && false
   , void>::type
   deep_swap_alloc_n( Allocator &a, F short_range_f, typename allocator_traits<Allocator>::size_type n_i
                    , G large_range_f, typename allocator_traits<Allocator>::size_type n_j)
{
   typedef typename allocator_traits<Allocator>::value_type value_type;
   typedef typename dtl::aligned_storage
      <MaxTmpBytes, dtl::alignment_of<value_type>::value>::type storage_type;
   storage_type storage;

   const std::size_t n_i_bytes = sizeof(value_type)*n_i;
   void *const large_ptr = static_cast<void*>(boost::movelib::iterator_to_raw_pointer(large_range_f));
   void *const short_ptr = static_cast<void*>(boost::movelib::iterator_to_raw_pointer(short_range_f));
   void *const stora_ptr = static_cast<void*>(boost::movelib::iterator_to_raw_pointer(storage.data));
   std::memcpy(stora_ptr, large_ptr, n_i_bytes);
   std::memcpy(large_ptr, short_ptr, n_i_bytes);
   std::memcpy(short_ptr, stora_ptr, n_i_bytes);
   boost::container::iterator_advance(large_range_f, n_i);
   boost::container::iterator_advance(short_range_f, n_i);
   boost::container::uninitialized_move_alloc_n(a, large_range_f, n_j - n_i, short_range_f);  // may throw
   boost::container::destroy_alloc_n(a, large_range_f, n_j - n_i);
}

template
   <std::size_t MaxTmpBytes
   ,typename Allocator
   ,typename F // F models ForwardIterator
   ,typename G // G models ForwardIterator
   >
inline typename dtl::enable_if_c
   < dtl::is_memtransfer_copy_assignable<F, G>::value && true//(MaxTmpBytes > DeepSwapAllocNMaxStorage)
   , void>::type
   deep_swap_alloc_n( Allocator &a, F short_range_f, typename allocator_traits<Allocator>::size_type n_i
                    , G large_range_f, typename allocator_traits<Allocator>::size_type n_j)
{
   typedef typename allocator_traits<Allocator>::value_type value_type;
   typedef typename dtl::aligned_storage
      <DeepSwapAllocNMaxStorage, dtl::alignment_of<value_type>::value>::type storage_type;
   storage_type storage;
   const std::size_t sizeof_storage = sizeof(storage);

   std::size_t n_i_bytes = sizeof(value_type)*n_i;
   char *large_ptr = static_cast<char*>(static_cast<void*>(boost::movelib::iterator_to_raw_pointer(large_range_f)));
   char *short_ptr = static_cast<char*>(static_cast<void*>(boost::movelib::iterator_to_raw_pointer(short_range_f)));
   char *stora_ptr = static_cast<char*>(static_cast<void*>(storage.data));

   std::size_t szt_times = n_i_bytes/sizeof_storage;
   const std::size_t szt_rem = n_i_bytes%sizeof_storage;

   //Loop unrolling using Duff's device, as it seems it helps on some architectures
   const std::size_t Unroll = 4;
   std::size_t n = (szt_times + (Unroll-1))/Unroll;
   const std::size_t branch_number = (!szt_times)*Unroll + (szt_times % Unroll);
   switch(branch_number){
      case 4:
         break;
      case 0: do{
         std::memcpy(stora_ptr, large_ptr, sizeof_storage);
         std::memcpy(large_ptr, short_ptr, sizeof_storage);
         std::memcpy(short_ptr, stora_ptr, sizeof_storage);
         large_ptr += sizeof_storage;
         short_ptr += sizeof_storage;
         BOOST_FALLTHROUGH;
      case 3:
         std::memcpy(stora_ptr, large_ptr, sizeof_storage);
         std::memcpy(large_ptr, short_ptr, sizeof_storage);
         std::memcpy(short_ptr, stora_ptr, sizeof_storage);
         large_ptr += sizeof_storage;
         short_ptr += sizeof_storage;
         BOOST_FALLTHROUGH;
      case 2:
         std::memcpy(stora_ptr, large_ptr, sizeof_storage);
         std::memcpy(large_ptr, short_ptr, sizeof_storage);
         std::memcpy(short_ptr, stora_ptr, sizeof_storage);
         large_ptr += sizeof_storage;
         short_ptr += sizeof_storage;
         BOOST_FALLTHROUGH;
      case 1:
         std::memcpy(stora_ptr, large_ptr, sizeof_storage);
         std::memcpy(large_ptr, short_ptr, sizeof_storage);
         std::memcpy(short_ptr, stora_ptr, sizeof_storage);
         large_ptr += sizeof_storage;
         short_ptr += sizeof_storage;
         } while(--n);
   }
   std::memcpy(stora_ptr, large_ptr, szt_rem);
   std::memcpy(large_ptr, short_ptr, szt_rem);
   std::memcpy(short_ptr, stora_ptr, szt_rem);
   boost::container::iterator_advance(large_range_f, n_i);
   boost::container::iterator_advance(short_range_f, n_i);
   boost::container::uninitialized_move_alloc_n(a, large_range_f, n_j - n_i, short_range_f);  // may throw
   boost::container::destroy_alloc_n(a, large_range_f, n_j - n_i);
}


//////////////////////////////////////////////////////////////////////////////
//
//                         copy_assign_range_alloc_n
//
//////////////////////////////////////////////////////////////////////////////

template
   <typename Allocator
   ,typename I // F models InputIterator
   ,typename O // G models OutputIterator
   >
void copy_assign_range_alloc_n( Allocator &a, I inp_start, typename allocator_traits<Allocator>::size_type n_i
                              , O out_start, typename allocator_traits<Allocator>::size_type n_o )
{
   if (n_o < n_i){
      inp_start = boost::container::copy_n_source_dest(inp_start, n_o, out_start);     // may throw
      boost::container::uninitialized_copy_alloc_n(a, inp_start, n_i - n_o, out_start);// may throw
   }
   else{
      out_start = boost::container::copy_n(inp_start, n_i, out_start);  // may throw
      boost::container::destroy_alloc_n(a, out_start, n_o - n_i);
   }
}

//////////////////////////////////////////////////////////////////////////////
//
//                         move_assign_range_alloc_n
//
//////////////////////////////////////////////////////////////////////////////

template
   <typename Allocator
   ,typename I // F models InputIterator
   ,typename O // G models OutputIterator
   >
void move_assign_range_alloc_n( Allocator &a, I inp_start, typename allocator_traits<Allocator>::size_type n_i
                              , O out_start, typename allocator_traits<Allocator>::size_type n_o )
{
   if (n_o < n_i){
      inp_start = boost::container::move_n_source_dest(inp_start, n_o, out_start);  // may throw
      boost::container::uninitialized_move_alloc_n(a, inp_start, n_i - n_o, out_start);  // may throw
   }
   else{
      out_start = boost::container::move_n(inp_start, n_i, out_start);  // may throw
      boost::container::destroy_alloc_n(a, out_start, n_o - n_i);
   }
}

}  //namespace container {
}  //namespace boost {

//#pragma GCC diagnostic ignored "-Wclass-memaccess"
#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic pop
#endif

#endif   //#ifndef BOOST_CONTAINER_DETAIL_COPY_MOVE_ALGO_HPP
