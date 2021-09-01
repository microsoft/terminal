#ifndef BOOST_CORE_IS_SAME_HPP_INCLUDED
#define BOOST_CORE_IS_SAME_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

// is_same<T1,T2>::value is true when T1 == T2
//
// Copyright 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>

namespace boost
{

namespace core
{

template< class T1, class T2 > struct is_same
{
    BOOST_STATIC_CONSTANT( bool, value = false );
};

template< class T > struct is_same< T, T >
{
    BOOST_STATIC_CONSTANT( bool, value = true );
};

} // namespace core

} // namespace boost

#endif // #ifndef BOOST_CORE_IS_SAME_HPP_INCLUDED
