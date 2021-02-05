#//////////////////////////////////////////////////////////////////////////////
#//
#// (C) Copyright Ion Gaztanaga 2015-2015.
#// Distributed under the Boost Software License, Version 1.0.
#// (See accompanying file LICENSE_1_0.txt or copy at
#// http://www.boost.org/LICENSE_1_0.txt)
#//
#// See http://www.boost.org/libs/move for documentation.
#//
#//////////////////////////////////////////////////////////////////////////////
#
#if defined(_LIBCPP_VERSION)
   #if defined(__clang__)
      #define BOOST_MOVE_STD_NS_GCC_DIAGNOSTIC_PUSH
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wc++11-extensions"
   #endif
   #define BOOST_MOVE_STD_NS_BEG _LIBCPP_BEGIN_NAMESPACE_STD
   #define BOOST_MOVE_STD_NS_END _LIBCPP_END_NAMESPACE_STD
#elif defined(BOOST_GNU_STDLIB) && defined(_GLIBCXX_BEGIN_NAMESPACE_VERSION)  //GCC >= 4.6
   #define BOOST_MOVE_STD_NS_BEG namespace std _GLIBCXX_VISIBILITY(default) { _GLIBCXX_BEGIN_NAMESPACE_VERSION
   #define BOOST_MOVE_STD_NS_END _GLIBCXX_END_NAMESPACE_VERSION  } // namespace
#elif defined(BOOST_GNU_STDLIB) && defined(_GLIBCXX_BEGIN_NAMESPACE)  //GCC >= 4.2
   #define BOOST_MOVE_STD_NS_BEG _GLIBCXX_BEGIN_NAMESPACE(std)
   #define BOOST_MOVE_STD_NS_END _GLIBCXX_END_NAMESPACE
#else
   #define BOOST_MOVE_STD_NS_BEG namespace std{
   #define BOOST_MOVE_STD_NS_END }
#endif

