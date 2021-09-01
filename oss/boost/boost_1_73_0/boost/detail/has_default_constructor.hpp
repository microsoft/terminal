
//  (C) Copyright Matthias Troyerk 2006.
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).
//
//  See http://www.boost.org/libs/type_traits for most recent version including documentation.

#ifndef BOOST_DETAIL_HAS_DEFAULT_CONSTRUCTOR_HPP_INCLUDED
#define BOOST_DETAIL_HAS_DEFAULT_CONSTRUCTOR_HPP_INCLUDED

#include <boost/type_traits/has_trivial_constructor.hpp>

namespace boost { namespace detail {

/// type trait to check for a default constructor
///
/// The default implementation just checks for a trivial constructor.
/// Using some compiler magic it might be possible to provide a better default

template <class T>
struct has_default_constructor
 : public has_trivial_constructor<T>
{};

} } // namespace boost::detail


#endif // BOOST_DETAIL_HAS_DEFAULT_CONSTRUCTOR_HPP_INCLUDED
