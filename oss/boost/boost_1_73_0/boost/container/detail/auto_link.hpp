//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_DETAIL_AUTO_LINK_HPP_INCLUDED
#define BOOST_CONTAINER_DETAIL_AUTO_LINK_HPP_INCLUDED

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
# pragma once
#endif

//Define BOOST_CONTAINER_DYNAMIC_LINKING which is independent from BOOST_*_NO_LIB
//and is needed is some tests that need to disable some checks (like operator new replacements)
//that don't work across DLL boundaries
#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_CONTAINER_DYN_LINK)
#  define BOOST_CONTAINER_DYNAMIC_LINKING
#endif

//
// Automatically link to the correct build variant where possible.
//
#if !defined(BOOST_ALL_NO_LIB) && !defined(BOOST_CONTAINER_NO_LIB) && !defined(BOOST_CONTAINER_SOURCE)
//
// Set the name of our library, this will get undef'ed by auto_link.hpp
// once it's done with it:
//
#define BOOST_LIB_NAME boost_container

//
// If we're importing code from a dll, then tell auto_link.hpp about it:
//
#if defined(BOOST_CONTAINER_DYNAMIC_LINKING)
#  define BOOST_DYN_LINK
#endif

//
// And include the header that does the work:
//
#include <boost/config/auto_link.hpp>
#endif  // auto-linking disabled

#endif   //#ifndef BOOST_CONTAINER_DETAIL_AUTO_LINK_HPP_INCLUDED
