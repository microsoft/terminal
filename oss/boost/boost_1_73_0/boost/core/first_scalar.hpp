/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef BOOST_CORE_FIRST_SCALAR_HPP
#define BOOST_CORE_FIRST_SCALAR_HPP

#include <boost/config.hpp>
#include <cstddef>

namespace boost {
namespace detail {

template<class T>
struct make_scalar {
    typedef T type;
};

template<class T, std::size_t N>
struct make_scalar<T[N]> {
    typedef typename make_scalar<T>::type type;
};

} /* detail */

template<class T>
BOOST_CONSTEXPR inline T*
first_scalar(T* p) BOOST_NOEXCEPT
{
    return p;
}

template<class T, std::size_t N>
BOOST_CONSTEXPR inline typename detail::make_scalar<T>::type*
first_scalar(T (*p)[N]) BOOST_NOEXCEPT
{
    return boost::first_scalar(&(*p)[0]);
}

} /* boost */

#endif
