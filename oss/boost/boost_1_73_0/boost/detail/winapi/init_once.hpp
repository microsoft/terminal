/*
 * Copyright 2017 Andrey Semashev
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 *
 * This header is deprecated, use boost/winapi/init_once.hpp instead.
 */

#ifndef BOOST_DETAIL_WINAPI_INIT_ONCE_HPP
#define BOOST_DETAIL_WINAPI_INIT_ONCE_HPP

#include <boost/config/header_deprecated.hpp>

BOOST_HEADER_DEPRECATED("<boost/winapi/init_once.hpp>")

#include <boost/winapi/init_once.hpp>
#include <boost/detail/winapi/detail/deprecated_namespace.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#define BOOST_DETAIL_WINAPI_INIT_ONCE_STATIC_INIT BOOST_WINAPI_INIT_ONCE_STATIC_INIT

#endif // BOOST_DETAIL_WINAPI_INIT_ONCE_HPP
