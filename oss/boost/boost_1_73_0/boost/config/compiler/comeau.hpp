//  (C) Copyright John Maddock 2001. 
//  (C) Copyright Douglas Gregor 2001. 
//  (C) Copyright Peter Dimov 2001. 
//  (C) Copyright Aleksey Gurtovoy 2003. 
//  (C) Copyright Beman Dawes 2003. 
//  (C) Copyright Jens Maurer 2003. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for most recent version.

//  Comeau C++ compiler setup:

#include <boost/config/compiler/common_edg.hpp>

#if (__COMO_VERSION__ <= 4245)

#  if defined(_MSC_VER) && _MSC_VER <= 1300
#     if _MSC_VER > 100
         // only set this in non-strict mode:
#        define BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
#     endif
#  endif

// Void returns don't work when emulating VC 6 (Peter Dimov)
// TODO: look up if this doesn't apply to the whole 12xx range
#  if defined(_MSC_VER) && (_MSC_VER < 1300)
#     define BOOST_NO_VOID_RETURNS
#  endif

#endif  // version 4245

//
// enable __int64 support in VC emulation mode
//
#  if defined(_MSC_VER) && (_MSC_VER >= 1200)
#     define BOOST_HAS_MS_INT64
#  endif

#define BOOST_COMPILER "Comeau compiler version " BOOST_STRINGIZE(__COMO_VERSION__)

//
// versions check:
// we don't know Comeau prior to version 4245:
#if __COMO_VERSION__ < 4245
#  error "Compiler not configured - please reconfigure"
#endif
//
// last known and checked version is 4245:
#if (__COMO_VERSION__ > 4245)
#  if defined(BOOST_ASSERT_CONFIG)
#     error "boost: Unknown compiler version - please run the configure tests and report the results"
#  endif
#endif




