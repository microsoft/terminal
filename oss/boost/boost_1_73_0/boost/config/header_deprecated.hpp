#ifndef BOOST_CONFIG_HEADER_DEPRECATED_HPP_INCLUDED
#define BOOST_CONFIG_HEADER_DEPRECATED_HPP_INCLUDED

//  Copyright 2017 Peter Dimov.
//
//  Distributed under the Boost Software License, Version 1.0.
//
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//
//  BOOST_HEADER_DEPRECATED("<alternative>")
//
//  Expands to the equivalent of
//    BOOST_PRAGMA_MESSAGE("This header is deprecated. Use <alternative> instead.")
//
//  Note that this header is C compatible.

#include <boost/config/pragma_message.hpp>

#if defined(BOOST_ALLOW_DEPRECATED_HEADERS)
# define BOOST_HEADER_DEPRECATED(a)
#else
# define BOOST_HEADER_DEPRECATED(a) BOOST_PRAGMA_MESSAGE("This header is deprecated. Use " a " instead.")
#endif

#endif // BOOST_CONFIG_HEADER_DEPRECATED_HPP_INCLUDED
