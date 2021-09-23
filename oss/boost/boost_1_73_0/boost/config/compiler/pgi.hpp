//  (C) Copyright Noel Belcourt 2007.
//  Copyright 2017, NVIDIA CORPORATION.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for most recent version.

//  PGI C++ compiler setup:

#define BOOST_COMPILER_VERSION __PGIC__##__PGIC_MINOR__
#define BOOST_COMPILER "PGI compiler version " BOOST_STRINGIZE(BOOST_COMPILER_VERSION)

// PGI is mostly GNU compatible.  So start with that.
#include <boost/config/compiler/gcc.hpp>

// Now adjust for things that are different.

// __float128 is a typedef, not a distinct type.
#undef BOOST_HAS_FLOAT128

// __int128 is not supported.
#undef BOOST_HAS_INT128
