/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Joaquin M Lopez Munoz  2006-2013
// (C) Copyright Ion Gaztanaga          2014-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_DETAIL_EBO_HOLDER_HPP
#define BOOST_INTRUSIVE_DETAIL_EBO_HOLDER_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/intrusive/detail/workaround.hpp>
#include <boost/move/utility_core.hpp>

namespace boost {
namespace intrusive {
namespace detail {

#if defined(BOOST_MSVC) || defined(__BORLANDC_)
#define BOOST_INTRUSIVE_TT_DECL __cdecl
#else
#define BOOST_INTRUSIVE_TT_DECL
#endif

#if defined(_MSC_EXTENSIONS) && !defined(__BORLAND__) && !defined(_WIN64) && !defined(_M_ARM) && !defined(_M_ARM64) && !defined(UNDER_CE)
#define BOOST_INTRUSIVE_TT_TEST_MSC_FUNC_SIGS
#endif

template <typename T>
struct is_unary_or_binary_function_impl
{  static const bool value = false; };

// see boost ticket #4094
// avoid duplicate definitions of is_unary_or_binary_function_impl
#ifndef BOOST_INTRUSIVE_TT_TEST_MSC_FUNC_SIGS

template <typename R>
struct is_unary_or_binary_function_impl<R (*)()>
{  static const bool value = true;  };

template <typename R>
struct is_unary_or_binary_function_impl<R (*)(...)>
{  static const bool value = true;  };

#else // BOOST_INTRUSIVE_TT_TEST_MSC_FUNC_SIGS

template <typename R>
struct is_unary_or_binary_function_impl<R (__stdcall*)()>
{  static const bool value = true;  };

#ifndef _MANAGED

template <typename R>
struct is_unary_or_binary_function_impl<R (__fastcall*)()>
{  static const bool value = true;  };

#endif

template <typename R>
struct is_unary_or_binary_function_impl<R (__cdecl*)()>
{  static const bool value = true;  };

template <typename R>
struct is_unary_or_binary_function_impl<R (__cdecl*)(...)>
{  static const bool value = true;  };

#endif

// see boost ticket #4094
// avoid duplicate definitions of is_unary_or_binary_function_impl
#ifndef BOOST_INTRUSIVE_TT_TEST_MSC_FUNC_SIGS

template <typename R, class T0>
struct is_unary_or_binary_function_impl<R (*)(T0)>
{  static const bool value = true;  };

template <typename R, class T0>
struct is_unary_or_binary_function_impl<R (*)(T0...)>
{  static const bool value = true;  };

#else // BOOST_INTRUSIVE_TT_TEST_MSC_FUNC_SIGS

template <typename R, class T0>
struct is_unary_or_binary_function_impl<R (__stdcall*)(T0)>
{  static const bool value = true;  };

#ifndef _MANAGED

template <typename R, class T0>
struct is_unary_or_binary_function_impl<R (__fastcall*)(T0)>
{  static const bool value = true;  };

#endif

template <typename R, class T0>
struct is_unary_or_binary_function_impl<R (__cdecl*)(T0)>
{  static const bool value = true;  };

template <typename R, class T0>
struct is_unary_or_binary_function_impl<R (__cdecl*)(T0...)>
{  static const bool value = true;  };

#endif

// see boost ticket #4094
// avoid duplicate definitions of is_unary_or_binary_function_impl
#ifndef BOOST_INTRUSIVE_TT_TEST_MSC_FUNC_SIGS

template <typename R, class T0, class T1>
struct is_unary_or_binary_function_impl<R (*)(T0, T1)>
{  static const bool value = true;  };

template <typename R, class T0, class T1>
struct is_unary_or_binary_function_impl<R (*)(T0, T1...)>
{  static const bool value = true;  };

#else // BOOST_INTRUSIVE_TT_TEST_MSC_FUNC_SIGS

template <typename R, class T0, class T1>
struct is_unary_or_binary_function_impl<R (__stdcall*)(T0, T1)>
{  static const bool value = true;  };

#ifndef _MANAGED

template <typename R, class T0, class T1>
struct is_unary_or_binary_function_impl<R (__fastcall*)(T0, T1)>
{  static const bool value = true;  };

#endif

template <typename R, class T0, class T1>
struct is_unary_or_binary_function_impl<R (__cdecl*)(T0, T1)>
{  static const bool value = true;  };

template <typename R, class T0, class T1>
struct is_unary_or_binary_function_impl<R (__cdecl*)(T0, T1...)>
{  static const bool value = true;  };
#endif

template <typename T>
struct is_unary_or_binary_function_impl<T&>
{  static const bool value = false; };

template<typename T>
struct is_unary_or_binary_function : is_unary_or_binary_function_impl<T>
{};

template<typename T, typename Tag = void, bool = is_unary_or_binary_function<T>::value>
class ebo_functor_holder
{
   BOOST_COPYABLE_AND_MOVABLE(ebo_functor_holder)

   public:
   typedef T functor_type;

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder()
      : t_()
   {}

   BOOST_INTRUSIVE_FORCEINLINE explicit ebo_functor_holder(const T &t)
      : t_(t)
   {}

   BOOST_INTRUSIVE_FORCEINLINE explicit ebo_functor_holder(BOOST_RV_REF(T) t)
      : t_(::boost::move(t))
   {}

   template<class Arg1, class Arg2>
   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder(BOOST_FWD_REF(Arg1) arg1, BOOST_FWD_REF(Arg2) arg2)
      : t_(::boost::forward<Arg1>(arg1), ::boost::forward<Arg2>(arg2))
   {}

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder(const ebo_functor_holder &x)
      : t_(x.t_)
   {}

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder(BOOST_RV_REF(ebo_functor_holder) x)
      : t_(x.t_)
   {}

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder& operator=(BOOST_COPY_ASSIGN_REF(ebo_functor_holder) x)
   {
      this->get() = x.get();
      return *this;
   }

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder& operator=(BOOST_RV_REF(ebo_functor_holder) x)
   {
      this->get() = ::boost::move(x.get());
      return *this;
   }

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder& operator=(const T &x)
   {
      this->get() = x;
      return *this;
   }

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder& operator=(BOOST_RV_REF(T) x)
   {
      this->get() = ::boost::move(x);
      return *this;
   }

   BOOST_INTRUSIVE_FORCEINLINE T&       get(){return t_;}
   BOOST_INTRUSIVE_FORCEINLINE const T& get()const{return t_;}

   private:
   T t_;
};

template<typename T, typename Tag>
class ebo_functor_holder<T, Tag, false>
   :  public T
{
   BOOST_COPYABLE_AND_MOVABLE(ebo_functor_holder)

   public:
   typedef T functor_type;

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder()
      : T()
   {}

   BOOST_INTRUSIVE_FORCEINLINE explicit ebo_functor_holder(const T &t)
      : T(t)
   {}

   BOOST_INTRUSIVE_FORCEINLINE explicit ebo_functor_holder(BOOST_RV_REF(T) t)
      : T(::boost::move(t))
   {}

   template<class Arg1, class Arg2>
   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder(BOOST_FWD_REF(Arg1) arg1, BOOST_FWD_REF(Arg2) arg2)
      : T(::boost::forward<Arg1>(arg1), ::boost::forward<Arg2>(arg2))
   {}

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder(const ebo_functor_holder &x)
      : T(static_cast<const T&>(x))
   {}

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder(BOOST_RV_REF(ebo_functor_holder) x)
      : T(BOOST_MOVE_BASE(T, x))
   {}

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder& operator=(BOOST_COPY_ASSIGN_REF(ebo_functor_holder) x)
   {
      const ebo_functor_holder&r = x;
      this->get() = r;
      return *this;
   }

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder& operator=(BOOST_RV_REF(ebo_functor_holder) x)
   {
      this->get() = ::boost::move(x.get());
      return *this;
   }

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder& operator=(const T &x)
   {
      this->get() = x;
      return *this;
   }

   BOOST_INTRUSIVE_FORCEINLINE ebo_functor_holder& operator=(BOOST_RV_REF(T) x)
   {
      this->get() = ::boost::move(x);
      return *this;
   }

   BOOST_INTRUSIVE_FORCEINLINE T&       get(){return *this;}
   BOOST_INTRUSIVE_FORCEINLINE const T& get()const{return *this;}
};

}  //namespace detail {
}  //namespace intrusive {
}  //namespace boost {

#endif   //#ifndef BOOST_INTRUSIVE_DETAIL_EBO_HOLDER_HPP
