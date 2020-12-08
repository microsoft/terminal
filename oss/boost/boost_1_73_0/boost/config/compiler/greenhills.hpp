//  (C) Copyright John Maddock 2001. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for most recent version.

//  Greenhills C++ compiler setup:

#define BOOST_COMPILER "Greenhills C++ version " BOOST_STRINGIZE(__ghs)

#include <boost/config/compiler/common_edg.hpp>

//
// versions check:
// we don't support Greenhills prior to version 0:
#if __ghs < 0
#  error "Compiler not supported or configured - please reconfigure"
#endif
//
// last known and checked version is 0:
#if (__ghs > 0)
#  if defined(BOOST_ASSERT_CONFIG)
#     error "boost: Unknown compiler version - please run the configure tests and report the results"
#  endif
#endif


