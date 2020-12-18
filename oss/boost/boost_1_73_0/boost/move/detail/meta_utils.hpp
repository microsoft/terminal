//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2012-2015.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

//! \file

#ifndef BOOST_MOVE_DETAIL_META_UTILS_HPP
#define BOOST_MOVE_DETAIL_META_UTILS_HPP

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif
#include <boost/move/detail/config_begin.hpp>
#include <boost/move/detail/workaround.hpp>  //forceinline
#include <boost/move/detail/meta_utils_core.hpp>
#include <cstddef>   //for std::size_t

//Small meta-typetraits to support move

namespace boost {

//Forward declare boost::rv
template <class T> class rv;

namespace move_detail {

//////////////////////////////////////
//          is_different
//////////////////////////////////////
template<class T, class U>
struct is_different
{
   static const bool value = !is_same<T, U>::value;
};

//////////////////////////////////////
//             apply
//////////////////////////////////////
template<class F, class Param>
struct apply
{
   typedef typename F::template apply<Param>::type type;
};

//////////////////////////////////////
//             bool_
//////////////////////////////////////

template< bool C_ >
struct bool_ : integral_constant<bool, C_>
{
     operator bool() const { return C_; }
   bool operator()() const { return C_; }
};

typedef bool_<true>        true_;
typedef bool_<false>       false_;

//////////////////////////////////////
//              nat
//////////////////////////////////////
struct nat{};
struct nat2{};
struct nat3{};

//////////////////////////////////////
//          yes_type/no_type
//////////////////////////////////////
typedef char yes_type;

struct no_type
{
   char _[2];
};

//////////////////////////////////////
//            natify
//////////////////////////////////////
template <class T> struct natify{};

//////////////////////////////////////
//          remove_reference
//////////////////////////////////////
template<class T>
struct remove_reference
{
   typedef T type;
};

template<class T>
struct remove_reference<T&>
{
   typedef T type;
};

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template<class T>
struct remove_reference<T&&>
{
   typedef T type;
};

#else

template<class T>
struct remove_reference< rv<T> >
{
   typedef T type;
};

template<class T>
struct remove_reference< rv<T> &>
{
   typedef T type;
};

template<class T>
struct remove_reference< const rv<T> &>
{
   typedef T type;
};

#endif

//////////////////////////////////////
//             remove_pointer
//////////////////////////////////////

template< class T > struct remove_pointer                    { typedef T type;   };
template< class T > struct remove_pointer<T*>                { typedef T type;   };
template< class T > struct remove_pointer<T* const>          { typedef T type;   };
template< class T > struct remove_pointer<T* volatile>       { typedef T type;   };
template< class T > struct remove_pointer<T* const volatile> { typedef T type;   };

//////////////////////////////////////
//             add_pointer
//////////////////////////////////////
template< class T >
struct add_pointer
{
   typedef typename remove_reference<T>::type* type;
};

//////////////////////////////////////
//             add_const
//////////////////////////////////////
template<class T>
struct add_const
{
   typedef const T type;
};

template<class T>
struct add_const<T&>
{
   typedef const T& type;
};

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template<class T>
struct add_const<T&&>
{
   typedef T&& type;
};

#endif

//////////////////////////////////////
//      add_lvalue_reference
//////////////////////////////////////
template<class T>
struct add_lvalue_reference
{  typedef T& type;  };

template<class T> struct add_lvalue_reference<T&>                 {  typedef T& type;  };
template<>        struct add_lvalue_reference<void>               {  typedef void type;   };
template<>        struct add_lvalue_reference<const void>         {  typedef const void type;  };
template<>        struct add_lvalue_reference<volatile void>      {  typedef volatile void type;   };
template<>        struct add_lvalue_reference<const volatile void>{  typedef const volatile void type;   };

template<class T>
struct add_const_lvalue_reference
{
   typedef typename remove_reference<T>::type         t_unreferenced;
   typedef typename add_const<t_unreferenced>::type   t_unreferenced_const;
   typedef typename add_lvalue_reference
      <t_unreferenced_const>::type                    type;
};

//////////////////////////////////////
//             is_lvalue_reference
//////////////////////////////////////
template<class T>
struct is_lvalue_reference
{
    static const bool value = false;
};

template<class T>
struct is_lvalue_reference<T&>
{
    static const bool value = true;
};


//////////////////////////////////////
//             identity
//////////////////////////////////////
template <class T>
struct identity
{
   typedef T type;
   typedef typename add_const_lvalue_reference<T>::type reference;
   reference operator()(reference t)
   {  return t;   }
};

//////////////////////////////////////
//          is_class_or_union
//////////////////////////////////////
template<class T>
struct is_class_or_union
{
   struct twochar { char dummy[2]; };
   template <class U>
   static char is_class_or_union_tester(void(U::*)(void));
   template <class U>
   static twochar is_class_or_union_tester(...);
   static const bool value = sizeof(is_class_or_union_tester<T>(0)) == sizeof(char);
};

//////////////////////////////////////
//             addressof
//////////////////////////////////////
template<class T>
struct addr_impl_ref
{
   T & v_;
   BOOST_MOVE_FORCEINLINE addr_impl_ref( T & v ): v_( v ) {}
   BOOST_MOVE_FORCEINLINE operator T& () const { return v_; }

   private:
   addr_impl_ref & operator=(const addr_impl_ref &);
};

template<class T>
struct addressof_impl
{
   BOOST_MOVE_FORCEINLINE static T * f( T & v, long )
   {
      return reinterpret_cast<T*>(
         &const_cast<char&>(reinterpret_cast<const volatile char &>(v)));
   }

   BOOST_MOVE_FORCEINLINE static T * f( T * v, int )
   {  return v;  }
};

template<class T>
BOOST_MOVE_FORCEINLINE T * addressof( T & v )
{
   return ::boost::move_detail::addressof_impl<T>::f
      ( ::boost::move_detail::addr_impl_ref<T>( v ), 0 );
}

//////////////////////////////////////
//          has_pointer_type
//////////////////////////////////////
template <class T>
struct has_pointer_type
{
   struct two { char c[2]; };
   template <class U> static two test(...);
   template <class U> static char test(typename U::pointer* = 0);
   static const bool value = sizeof(test<T>(0)) == 1;
};

//////////////////////////////////////
//           is_convertible
//////////////////////////////////////
#if defined(_MSC_VER) && (_MSC_VER >= 1400)

//use intrinsic since in MSVC
//overaligned types can't go through ellipsis
template <class T, class U>
struct is_convertible
{
   static const bool value = __is_convertible_to(T, U);
};

#else

template <class T, class U>
class is_convertible
{
   typedef typename add_lvalue_reference<T>::type t_reference;
   typedef char true_t;
   class false_t { char dummy[2]; };
   static false_t dispatch(...);
   static true_t  dispatch(U);
   static t_reference       trigger();
   public:
   static const bool value = sizeof(dispatch(trigger())) == sizeof(true_t);
};

#endif

template <class T, class U, bool IsSame = is_same<T, U>::value>
struct is_same_or_convertible
   : is_convertible<T, U>
{};

template <class T, class U>
struct is_same_or_convertible<T, U, true>
{
   static const bool value = true;
};

template<
      bool C
    , typename F1
    , typename F2
    >
struct eval_if_c
    : if_c<C,F1,F2>::type
{};

template<
      typename C
    , typename T1
    , typename T2
    >
struct eval_if
    : if_<C,T1,T2>::type
{};


#if defined(BOOST_GCC) && (BOOST_GCC <= 40000)
#define BOOST_MOVE_HELPERS_RETURN_SFINAE_BROKEN
#endif

template<class T, class U, class R = void>
struct enable_if_convertible
   : enable_if< is_convertible<T, U>, R>
{};

template<class T, class U, class R = void>
struct disable_if_convertible
   : disable_if< is_convertible<T, U>, R>
{};

template<class T, class U, class R = void>
struct enable_if_same_or_convertible
   : enable_if< is_same_or_convertible<T, U>, R>
{};

template<class T, class U, class R = void>
struct disable_if_same_or_convertible
   : disable_if< is_same_or_convertible<T, U>, R>
{};

//////////////////////////////////////////////////////////////////////////////
//
//                         and_
//
//////////////////////////////////////////////////////////////////////////////
template<bool, class B = true_, class C = true_, class D = true_>
struct and_impl
   : and_impl<B::value, C, D>
{};

template<>
struct and_impl<true, true_, true_, true_>
{
   static const bool value = true;
};

template<class B, class C, class D>
struct and_impl<false, B, C, D>
{
   static const bool value = false;
};

template<class A, class B, class C = true_, class D = true_>
struct and_
   : and_impl<A::value, B, C, D>
{};

//////////////////////////////////////////////////////////////////////////////
//
//                            or_
//
//////////////////////////////////////////////////////////////////////////////
template<bool, class B = false_, class C = false_, class D = false_>
struct or_impl
   : or_impl<B::value, C, D>
{};

template<>
struct or_impl<false, false_, false_, false_>
{
   static const bool value = false;
};

template<class B, class C, class D>
struct or_impl<true, B, C, D>
{
   static const bool value = true;
};

template<class A, class B, class C = false_, class D = false_>
struct or_
   : or_impl<A::value, B, C, D>
{};

//////////////////////////////////////////////////////////////////////////////
//
//                         not_
//
//////////////////////////////////////////////////////////////////////////////
template<class T>
struct not_
{
   static const bool value = !T::value;
};

//////////////////////////////////////////////////////////////////////////////
//
// enable_if_and / disable_if_and / enable_if_or / disable_if_or
//
//////////////////////////////////////////////////////////////////////////////

template<class R, class A, class B, class C = true_, class D = true_>
struct enable_if_and
   : enable_if_c< and_<A, B, C, D>::value, R>
{};

template<class R, class A, class B, class C = true_, class D = true_>
struct disable_if_and
   : disable_if_c< and_<A, B, C, D>::value, R>
{};

template<class R, class A, class B, class C = false_, class D = false_>
struct enable_if_or
   : enable_if_c< or_<A, B, C, D>::value, R>
{};

template<class R, class A, class B, class C = false_, class D = false_>
struct disable_if_or
   : disable_if_c< or_<A, B, C, D>::value, R>
{};

//////////////////////////////////////////////////////////////////////////////
//
//                      has_move_emulation_enabled_impl
//
//////////////////////////////////////////////////////////////////////////////
template<class T>
struct has_move_emulation_enabled_impl
   : is_convertible< T, ::boost::rv<T>& >
{};

template<class T>
struct has_move_emulation_enabled_impl<T&>
{  static const bool value = false;  };

template<class T>
struct has_move_emulation_enabled_impl< ::boost::rv<T> >
{  static const bool value = false;  };

//////////////////////////////////////////////////////////////////////////////
//
//                            is_rv_impl
//
//////////////////////////////////////////////////////////////////////////////

template <class T>
struct is_rv_impl
{  static const bool value = false;  };

template <class T>
struct is_rv_impl< rv<T> >
{  static const bool value = true;  };

template <class T>
struct is_rv_impl< const rv<T> >
{  static const bool value = true;  };

// Code from Jeffrey Lee Hellrung, many thanks

template< class T >
struct is_rvalue_reference
{  static const bool value = false;  };

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template< class T >
struct is_rvalue_reference< T&& >
{  static const bool value = true;  };

#else // #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template< class T >
struct is_rvalue_reference< boost::rv<T>& >
{  static const bool value = true;  };

template< class T >
struct is_rvalue_reference< const boost::rv<T>& >
{  static const bool value = true;  };

#endif // #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template< class T >
struct add_rvalue_reference
{ typedef T&& type; };

#else // #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

namespace detail_add_rvalue_reference
{
   template< class T
            , bool emulation = has_move_emulation_enabled_impl<T>::value
            , bool rv        = is_rv_impl<T>::value  >
   struct add_rvalue_reference_impl { typedef T type; };

   template< class T, bool emulation>
   struct add_rvalue_reference_impl< T, emulation, true > { typedef T & type; };

   template< class T, bool rv >
   struct add_rvalue_reference_impl< T, true, rv > { typedef ::boost::rv<T>& type; };
} // namespace detail_add_rvalue_reference

template< class T >
struct add_rvalue_reference
   : detail_add_rvalue_reference::add_rvalue_reference_impl<T>
{ };

template< class T >
struct add_rvalue_reference<T &>
{  typedef T & type; };

#endif // #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template< class T > struct remove_rvalue_reference { typedef T type; };

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   template< class T > struct remove_rvalue_reference< T&& >                  { typedef T type; };
#else // #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   template< class T > struct remove_rvalue_reference< rv<T> >                { typedef T type; };
   template< class T > struct remove_rvalue_reference< const rv<T> >          { typedef T type; };
   template< class T > struct remove_rvalue_reference< volatile rv<T> >       { typedef T type; };
   template< class T > struct remove_rvalue_reference< const volatile rv<T> > { typedef T type; };
   template< class T > struct remove_rvalue_reference< rv<T>& >               { typedef T type; };
   template< class T > struct remove_rvalue_reference< const rv<T>& >         { typedef T type; };
   template< class T > struct remove_rvalue_reference< volatile rv<T>& >      { typedef T type; };
   template< class T > struct remove_rvalue_reference< const volatile rv<T>& >{ typedef T type; };
#endif // #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

// Ideas from Boost.Move review, Jeffrey Lee Hellrung:
//
//- TypeTraits metafunctions is_lvalue_reference, add_lvalue_reference, and remove_lvalue_reference ?
//  Perhaps add_reference and remove_reference can be modified so that they behave wrt emulated rvalue
//  references the same as wrt real rvalue references, i.e., add_reference< rv<T>& > -> T& rather than
//  rv<T>& (since T&& & -> T&).
//
//- Add'l TypeTraits has_[trivial_]move_{constructor,assign}...?
//
//- An as_lvalue(T& x) function, which amounts to an identity operation in C++0x, but strips emulated
//  rvalue references in C++03.  This may be necessary to prevent "accidental moves".

}  //namespace move_detail {
}  //namespace boost {

#include <boost/move/detail/config_end.hpp>

#endif //#ifndef BOOST_MOVE_DETAIL_META_UTILS_HPP
