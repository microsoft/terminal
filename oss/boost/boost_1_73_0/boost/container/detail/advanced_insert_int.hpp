//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2008-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_ADVANCED_INSERT_INT_HPP
#define BOOST_CONTAINER_ADVANCED_INSERT_INT_HPP

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
// container/detail
#include <boost/container/detail/copy_move_algo.hpp>
#include <boost/container/detail/destroyers.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/type_traits.hpp>
#include <boost/container/detail/iterator.hpp>
#include <boost/container/detail/iterators.hpp>
#include <boost/move/detail/iterator_to_raw_pointer.hpp>
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/move/detail/fwd_macros.hpp>
#endif
// move
#include <boost/move/utility_core.hpp>
// other
#include <boost/assert.hpp>
#include <boost/core/no_exceptions_support.hpp>

namespace boost { namespace container { namespace dtl {

template<class Allocator, class FwdIt, class Iterator>
struct move_insert_range_proxy
{
   typedef typename allocator_traits<Allocator>::size_type size_type;
   typedef typename allocator_traits<Allocator>::value_type value_type;

   explicit move_insert_range_proxy(FwdIt first)
      :  first_(first)
   {}

   void uninitialized_copy_n_and_update(Allocator &a, Iterator p, size_type n)
   {
      this->first_ = ::boost::container::uninitialized_move_alloc_n_source
         (a, this->first_, n, p);
   }

   void copy_n_and_update(Allocator &, Iterator p, size_type n)
   {
      this->first_ = ::boost::container::move_n_source(this->first_, n, p);
   }

   FwdIt first_;
};


template<class Allocator, class FwdIt, class Iterator>
struct insert_range_proxy
{
   typedef typename allocator_traits<Allocator>::size_type size_type;
   typedef typename allocator_traits<Allocator>::value_type value_type;

   explicit insert_range_proxy(FwdIt first)
      :  first_(first)
   {}

   void uninitialized_copy_n_and_update(Allocator &a, Iterator p, size_type n)
   {
      this->first_ = ::boost::container::uninitialized_copy_alloc_n_source(a, this->first_, n, p);
   }

   void copy_n_and_update(Allocator &, Iterator p, size_type n)
   {
      this->first_ = ::boost::container::copy_n_source(this->first_, n, p);
   }

   FwdIt first_;
};


template<class Allocator, class Iterator>
struct insert_n_copies_proxy
{
   typedef typename allocator_traits<Allocator>::size_type size_type;
   typedef typename allocator_traits<Allocator>::value_type value_type;

   explicit insert_n_copies_proxy(const value_type &v)
      :  v_(v)
   {}

   void uninitialized_copy_n_and_update(Allocator &a, Iterator p, size_type n) const
   {  boost::container::uninitialized_fill_alloc_n(a, v_, n, p);  }

   void copy_n_and_update(Allocator &, Iterator p, size_type n) const
   {
      for (; 0 < n; --n, ++p){
         *p = v_;
      }
   }

   const value_type &v_;
};

template<class Allocator, class Iterator>
struct insert_value_initialized_n_proxy
{
   typedef ::boost::container::allocator_traits<Allocator> alloc_traits;
   typedef typename allocator_traits<Allocator>::size_type size_type;
   typedef typename allocator_traits<Allocator>::value_type value_type;

   void uninitialized_copy_n_and_update(Allocator &a, Iterator p, size_type n) const
   {  boost::container::uninitialized_value_init_alloc_n(a, n, p);  }

   void copy_n_and_update(Allocator &a, Iterator p, size_type n) const
   {
      for (; 0 < n; --n, ++p){
         typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;
         value_type *vp = reinterpret_cast<value_type *>(v.data);
         alloc_traits::construct(a, vp);
         value_destructor<Allocator> on_exit(a, *vp); (void)on_exit;
         *p = ::boost::move(*vp);
      }
   }
};

template<class Allocator, class Iterator>
struct insert_default_initialized_n_proxy
{
   typedef ::boost::container::allocator_traits<Allocator> alloc_traits;
   typedef typename allocator_traits<Allocator>::size_type size_type;
   typedef typename allocator_traits<Allocator>::value_type value_type;

   void uninitialized_copy_n_and_update(Allocator &a, Iterator p, size_type n) const
   {  boost::container::uninitialized_default_init_alloc_n(a, n, p);  }

   void copy_n_and_update(Allocator &a, Iterator p, size_type n) const
   {
      if(!is_pod<value_type>::value){
         for (; 0 < n; --n, ++p){
            typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;
            value_type *vp = reinterpret_cast<value_type *>(v.data);
            alloc_traits::construct(a, vp, default_init);
            value_destructor<Allocator> on_exit(a, *vp); (void)on_exit;
            *p = ::boost::move(*vp);
         }
      }
   }
};

template<class Allocator, class Iterator>
struct insert_copy_proxy
{
   typedef boost::container::allocator_traits<Allocator> alloc_traits;
   typedef typename alloc_traits::size_type size_type;
   typedef typename alloc_traits::value_type value_type;

   explicit insert_copy_proxy(const value_type &v)
      :  v_(v)
   {}

   void uninitialized_copy_n_and_update(Allocator &a, Iterator p, size_type n) const
   {
      BOOST_ASSERT(n == 1);  (void)n;
      alloc_traits::construct( a, boost::movelib::iterator_to_raw_pointer(p), v_);
   }

   void copy_n_and_update(Allocator &, Iterator p, size_type n) const
   {
      BOOST_ASSERT(n == 1);  (void)n;
      *p = v_;
   }

   const value_type &v_;
};


template<class Allocator, class Iterator>
struct insert_move_proxy
{
   typedef boost::container::allocator_traits<Allocator> alloc_traits;
   typedef typename alloc_traits::size_type size_type;
   typedef typename alloc_traits::value_type value_type;

   BOOST_CONTAINER_FORCEINLINE explicit insert_move_proxy(value_type &v)
      :  v_(v)
   {}

   BOOST_CONTAINER_FORCEINLINE void uninitialized_copy_n_and_update(Allocator &a, Iterator p, size_type n) const
   {
      BOOST_ASSERT(n == 1);  (void)n;
      alloc_traits::construct( a, boost::movelib::iterator_to_raw_pointer(p), ::boost::move(v_) );
   }

   BOOST_CONTAINER_FORCEINLINE void copy_n_and_update(Allocator &, Iterator p, size_type n) const
   {
      BOOST_ASSERT(n == 1);  (void)n;
      *p = ::boost::move(v_);
   }

   value_type &v_;
};

template<class It, class Allocator>
insert_move_proxy<Allocator, It> get_insert_value_proxy(BOOST_RV_REF(typename boost::container::iterator_traits<It>::value_type) v)
{
   return insert_move_proxy<Allocator, It>(v);
}

template<class It, class Allocator>
insert_copy_proxy<Allocator, It> get_insert_value_proxy(const typename boost::container::iterator_traits<It>::value_type &v)
{
   return insert_copy_proxy<Allocator, It>(v);
}

}}}   //namespace boost { namespace container { namespace dtl {

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

#include <boost/container/detail/variadic_templates_tools.hpp>
#include <boost/move/utility_core.hpp>

namespace boost {
namespace container {
namespace dtl {

template<class Allocator, class Iterator, class ...Args>
struct insert_nonmovable_emplace_proxy
{
   typedef boost::container::allocator_traits<Allocator>   alloc_traits;
   typedef typename alloc_traits::size_type        size_type;
   typedef typename alloc_traits::value_type       value_type;

   typedef typename build_number_seq<sizeof...(Args)>::type index_tuple_t;

   explicit insert_nonmovable_emplace_proxy(BOOST_FWD_REF(Args)... args)
      : args_(args...)
   {}

   void uninitialized_copy_n_and_update(Allocator &a, Iterator p, size_type n)
   {  this->priv_uninitialized_copy_some_and_update(a, index_tuple_t(), p, n);  }

   private:
   template<std::size_t ...IdxPack>
   void priv_uninitialized_copy_some_and_update(Allocator &a, const index_tuple<IdxPack...>&, Iterator p, size_type n)
   {
      BOOST_ASSERT(n == 1); (void)n;
      alloc_traits::construct( a, boost::movelib::iterator_to_raw_pointer(p), ::boost::forward<Args>(get<IdxPack>(this->args_))... );
   }

   protected:
   tuple<Args&...> args_;
};

template<class Allocator, class Iterator, class ...Args>
struct insert_emplace_proxy
   :  public insert_nonmovable_emplace_proxy<Allocator, Iterator, Args...>
{
   typedef insert_nonmovable_emplace_proxy<Allocator, Iterator, Args...> base_t;
   typedef boost::container::allocator_traits<Allocator>   alloc_traits;
   typedef typename base_t::value_type             value_type;
   typedef typename base_t::size_type              size_type;
   typedef typename base_t::index_tuple_t          index_tuple_t;

   explicit insert_emplace_proxy(BOOST_FWD_REF(Args)... args)
      : base_t(::boost::forward<Args>(args)...)
   {}

   void copy_n_and_update(Allocator &a, Iterator p, size_type n)
   {  this->priv_copy_some_and_update(a, index_tuple_t(), p, n);  }

   private:

   template<std::size_t ...IdxPack>
   void priv_copy_some_and_update(Allocator &a, const index_tuple<IdxPack...>&, Iterator p, size_type n)
   {
      BOOST_ASSERT(n ==1); (void)n;
      typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;
      value_type *vp = reinterpret_cast<value_type *>(v.data);
      alloc_traits::construct(a, vp,
         ::boost::forward<Args>(get<IdxPack>(this->args_))...);
      BOOST_TRY{
         *p = ::boost::move(*vp);
      }
      BOOST_CATCH(...){
         alloc_traits::destroy(a, vp);
         BOOST_RETHROW
      }
      BOOST_CATCH_END
      alloc_traits::destroy(a, vp);
   }
};

//Specializations to avoid an unneeded temporary when emplacing from a single argument o type value_type
template<class Allocator, class Iterator>
struct insert_emplace_proxy<Allocator, Iterator, typename boost::container::allocator_traits<Allocator>::value_type>
   : public insert_move_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy(typename boost::container::allocator_traits<Allocator>::value_type &&v)
   : insert_move_proxy<Allocator, Iterator>(v)
   {}
};

//We use "add_const" here as adding "const" only confuses MSVC12(and maybe later) provoking
//compiler error C2752 ("more than one partial specialization matches").
//Any problem is solvable with an extra layer of indirection? ;-)
template<class Allocator, class Iterator>
struct insert_emplace_proxy<Allocator, Iterator
   , typename boost::container::dtl::add_const<typename boost::container::allocator_traits<Allocator>::value_type>::type
   >
   : public insert_copy_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy(const typename boost::container::allocator_traits<Allocator>::value_type &v)
   : insert_copy_proxy<Allocator, Iterator>(v)
   {}
};

template<class Allocator, class Iterator>
struct insert_emplace_proxy<Allocator, Iterator, typename boost::container::allocator_traits<Allocator>::value_type &>
   : public insert_copy_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy(const typename boost::container::allocator_traits<Allocator>::value_type &v)
   : insert_copy_proxy<Allocator, Iterator>(v)
   {}
};

template<class Allocator, class Iterator>
struct insert_emplace_proxy<Allocator, Iterator
   , typename boost::container::dtl::add_const<typename boost::container::allocator_traits<Allocator>::value_type>::type &
   >
   : public insert_copy_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy(const typename boost::container::allocator_traits<Allocator>::value_type &v)
   : insert_copy_proxy<Allocator, Iterator>(v)
   {}
};

}}}   //namespace boost { namespace container { namespace dtl {

#else // !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

#include <boost/container/detail/value_init.hpp>

namespace boost {
namespace container {
namespace dtl {

#define BOOST_CONTAINER_ADVANCED_INSERT_INT_CODE(N) \
template< class Allocator, class Iterator BOOST_MOVE_I##N BOOST_MOVE_CLASS##N >\
struct insert_nonmovable_emplace_proxy##N\
{\
   typedef boost::container::allocator_traits<Allocator> alloc_traits;\
   typedef typename alloc_traits::size_type size_type;\
   typedef typename alloc_traits::value_type value_type;\
   \
   explicit insert_nonmovable_emplace_proxy##N(BOOST_MOVE_UREF##N)\
      BOOST_MOVE_COLON##N BOOST_MOVE_FWD_INIT##N {}\
   \
   void uninitialized_copy_n_and_update(Allocator &a, Iterator p, size_type n)\
   {\
      BOOST_ASSERT(n == 1); (void)n;\
      alloc_traits::construct(a, boost::movelib::iterator_to_raw_pointer(p) BOOST_MOVE_I##N BOOST_MOVE_MFWD##N);\
   }\
   \
   void copy_n_and_update(Allocator &, Iterator, size_type)\
   {  BOOST_ASSERT(false);   }\
   \
   protected:\
   BOOST_MOVE_MREF##N\
};\
\
template< class Allocator, class Iterator BOOST_MOVE_I##N BOOST_MOVE_CLASS##N >\
struct insert_emplace_proxy_arg##N\
   : insert_nonmovable_emplace_proxy##N< Allocator, Iterator BOOST_MOVE_I##N BOOST_MOVE_TARG##N >\
{\
   typedef insert_nonmovable_emplace_proxy##N\
      < Allocator, Iterator BOOST_MOVE_I##N BOOST_MOVE_TARG##N > base_t;\
   typedef typename base_t::value_type value_type;\
   typedef typename base_t::size_type size_type;\
   typedef boost::container::allocator_traits<Allocator> alloc_traits;\
   \
   explicit insert_emplace_proxy_arg##N(BOOST_MOVE_UREF##N)\
      : base_t(BOOST_MOVE_FWD##N){}\
   \
   void copy_n_and_update(Allocator &a, Iterator p, size_type n)\
   {\
      BOOST_ASSERT(n == 1); (void)n;\
      typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;\
      BOOST_ASSERT((((size_type)(&v)) % alignment_of<value_type>::value) == 0);\
      value_type *vp = reinterpret_cast<value_type *>(v.data);\
      alloc_traits::construct(a, vp BOOST_MOVE_I##N BOOST_MOVE_MFWD##N);\
      BOOST_TRY{\
         *p = ::boost::move(*vp);\
      }\
      BOOST_CATCH(...){\
         alloc_traits::destroy(a, vp);\
         BOOST_RETHROW\
      }\
      BOOST_CATCH_END\
      alloc_traits::destroy(a, vp);\
   }\
};\
//
BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_ADVANCED_INSERT_INT_CODE)
#undef BOOST_CONTAINER_ADVANCED_INSERT_INT_CODE

#if defined(BOOST_NO_CXX11_RVALUE_REFERENCES)

//Specializations to avoid an unneeded temporary when emplacing from a single argument o type value_type
template<class Allocator, class Iterator>
struct insert_emplace_proxy_arg1<Allocator, Iterator, ::boost::rv<typename boost::container::allocator_traits<Allocator>::value_type> >
   : public insert_move_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy_arg1(typename boost::container::allocator_traits<Allocator>::value_type &v)
   : insert_move_proxy<Allocator, Iterator>(v)
   {}
};

template<class Allocator, class Iterator>
struct insert_emplace_proxy_arg1<Allocator, Iterator, typename boost::container::allocator_traits<Allocator>::value_type>
   : public insert_copy_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy_arg1(const typename boost::container::allocator_traits<Allocator>::value_type &v)
   : insert_copy_proxy<Allocator, Iterator>(v)
   {}
};

#else //e.g. MSVC10 & MSVC11

//Specializations to avoid an unneeded temporary when emplacing from a single argument o type value_type
template<class Allocator, class Iterator>
struct insert_emplace_proxy_arg1<Allocator, Iterator, typename boost::container::allocator_traits<Allocator>::value_type>
   : public insert_move_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy_arg1(typename boost::container::allocator_traits<Allocator>::value_type &&v)
   : insert_move_proxy<Allocator, Iterator>(v)
   {}
};

//We use "add_const" here as adding "const" only confuses MSVC10&11 provoking
//compiler error C2752 ("more than one partial specialization matches").
//Any problem is solvable with an extra layer of indirection? ;-)
template<class Allocator, class Iterator>
struct insert_emplace_proxy_arg1<Allocator, Iterator
   , typename boost::container::dtl::add_const<typename boost::container::allocator_traits<Allocator>::value_type>::type
   >
   : public insert_copy_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy_arg1(const typename boost::container::allocator_traits<Allocator>::value_type &v)
   : insert_copy_proxy<Allocator, Iterator>(v)
   {}
};

template<class Allocator, class Iterator>
struct insert_emplace_proxy_arg1<Allocator, Iterator, typename boost::container::allocator_traits<Allocator>::value_type &>
   : public insert_copy_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy_arg1(const typename boost::container::allocator_traits<Allocator>::value_type &v)
   : insert_copy_proxy<Allocator, Iterator>(v)
   {}
};

template<class Allocator, class Iterator>
struct insert_emplace_proxy_arg1<Allocator, Iterator
   , typename boost::container::dtl::add_const<typename boost::container::allocator_traits<Allocator>::value_type>::type &
   >
   : public insert_copy_proxy<Allocator, Iterator>
{
   explicit insert_emplace_proxy_arg1(const typename boost::container::allocator_traits<Allocator>::value_type &v)
   : insert_copy_proxy<Allocator, Iterator>(v)
   {}
};

#endif

}}}   //namespace boost { namespace container { namespace dtl {

#endif   // !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

#include <boost/container/detail/config_end.hpp>

#endif //#ifndef BOOST_CONTAINER_ADVANCED_INSERT_INT_HPP
