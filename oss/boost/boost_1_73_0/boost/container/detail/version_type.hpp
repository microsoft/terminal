//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//
//       This code comes from N1953 document by Howard E. Hinnant
//
//////////////////////////////////////////////////////////////////////////////


#ifndef BOOST_CONTAINER_DETAIL_VERSION_TYPE_HPP
#define BOOST_CONTAINER_DETAIL_VERSION_TYPE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/type_traits.hpp>

namespace boost{
namespace container {
namespace dtl {

template <class T, unsigned V>
struct version_type
    : public dtl::integral_constant<unsigned, V>
{
    typedef T type;
};

namespace impl{

template <class T>
struct extract_version
{
   typedef typename T::version type;
};

template <class T>
struct has_version
{
   private:
   struct two {char _[2];};
   template <class U> static two test(...);
   template <class U> static char test(const typename U::version*);
   public:
   static const bool value = sizeof(test<T>(0)) == 1;
   void dummy(){}
};

template <class T, bool = has_version<T>::value>
struct version
{
   static const unsigned value = 1;
};

template <class T>
struct version<T, true>
{
   static const unsigned value = extract_version<T>::type::value;
};

}  //namespace impl

template <class T>
struct version
   : public dtl::integral_constant<unsigned, impl::version<T>::value>
{};

template<class T, unsigned N>
struct is_version
{
   static const bool value =
      is_same< typename version<T>::type, integral_constant<unsigned, N> >::value;
};

}  //namespace dtl {

typedef dtl::integral_constant<unsigned, 0> version_0;
typedef dtl::integral_constant<unsigned, 1> version_1;
typedef dtl::integral_constant<unsigned, 2> version_2;

}  //namespace container {
}  //namespace boost{

#include <boost/container/detail/config_end.hpp>

#endif   //#define BOOST_CONTAINER_DETAIL_VERSION_TYPE_HPP
