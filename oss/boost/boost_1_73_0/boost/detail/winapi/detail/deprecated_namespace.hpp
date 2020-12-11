/*
 * Copyright 2017 Andrey Semashev
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 *
 * This header is deprecated, it provides the deprecated namespace for backward compatibility.
 */

#ifndef BOOST_DETAIL_WINAPI_DETAIL_DEPRECATED_NAMESPACE_HPP_INCLUDED_
#define BOOST_DETAIL_WINAPI_DETAIL_DEPRECATED_NAMESPACE_HPP_INCLUDED_

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace winapi {}
namespace detail {
namespace winapi {
using namespace boost::winapi;
} // namespace winapi
} // namespace detail
} // namespace boost

#endif // BOOST_DETAIL_WINAPI_DETAIL_DEPRECATED_NAMESPACE_HPP_INCLUDED_
